#!/usr/bin/env python
# -*- coding: utf-8 -*-
# vim:ts=4:sw=4:softtabstop=4:smarttab:tw=78:expandtab
"""
vrioswitch.py --- эмулятор коммутатора RapidIO.

Опции:
  -h, --help
  -v, --verbose
  -P, --start-port <num>
"""

__author__ = """Antony Pavlov <antony@niisi.msk.ru>"""

import Queue
import threading
import time
import socket
import sys
import getopt

from vrio import *
import struct


RIO_HOST_DID_LOCK_CSR = 0x68

#from pyrio.agent.emulator import mregisters_emulator
class mregisters_emulator():
	"""
Хранилище регистров MAINTENANCE RapidIO.
Обеспечивает запись и чтение регистров из хранилище, а
также в некоторых чертах имитирует поведение реального
устройства при доступе к регистрам.
	"""
	def __init__(self):
		self.regs = {}

	def init_reg(self, offset, value):
		"""
Инициализация хранилища.
		"""
		self.regs[offset] = value

	def read(self, offset):
		"""
Чтение из хранилища.
		"""
		try:
			ret = self.regs[offset]
		except KeyError:
			ret = 0
		return ret

	def write(self, offset, value):
		"""
Запись в хранилище. При записи учитываются, что
регистры CAR --- только для чтения, а регистр
HOST_DID_LOCK_CSR имеет особую дисциплину доступа.
		"""
		if (offset == RIO_HOST_DID_LOCK_CSR):
			if self.regs[RIO_HOST_DID_LOCK_CSR] == 0xffff:
				self.regs[offset] = value
				return
			if self.regs[RIO_HOST_DID_LOCK_CSR] == value:
				self.regs[offset] = 0xffff
			return

		# Регистры CAR --- только для чтения
		if (offset < 0x40):
			return

		self.regs[offset] = value


### число записей в таблице маршрутизации
ROUTING_TABLE_ENTRY_NUMBER = 512


class switch_port():
    def __init__(self, name, portnum, router, tcp_port):
        self.qout = Queue.Queue()
        self.name = name
        self.portnum = portnum
        self.router = router
        self.tcp_port = tcp_port
        self.host = ''
        self.addr = None

        self.rx_num = 0
        self.tx_num = 0

        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.s.setblocking(1)
        self.s.bind((self.host, self.tcp_port))
        self.conn = None

        self.t = threading.Thread(target=self.in_thread)
        self.t.setDaemon(True)
        self.t.start()

        self.t = threading.Thread(target=self.out_thread)
        self.t.setDaemon(True)
        self.t.start()

    def print_status(self):
        if self.addr is not None:
            print("%s: connected by %s; rx/tx pkts = %d/%d" % \
                    (self.name, \
                    self.addr, \
                    self.rx_num, \
                    self.tx_num))
        else:
            print("%s: disconnected; rx/tx pkts = %d/%d" % \
                    (self.name, \
                    self.rx_num, \
                    self.tx_num))

    def in_thread(self):
        self.s.listen(1)
        self.conn, self.addr = self.s.accept()
        #print self.name, ": connected by ", self.addr
        self.router.port_link_up(self.portnum)

        while True:
            mesg = self.conn.recv(1)

            if not mesg:
                break

            if mesg[0] == "P":
                # parse data1
                pkt_hdr = self.conn.recv(12)
                #print "pkt_hdr <%s>" % (pkt_hdr,)
                #print "pkt_hdr len <%d>" % (len(pkt_hdr),)
                (ftype, tt, phy_res_1, phy_res_2, dstId, srcId, ll_len) = \
                        struct.unpack('!BBBBHHI', pkt_hdr)

                pkt_ll = self.conn.recv(ll_len - 4)

                self.rx_num = self.rx_num + 1

                if ftype == FTYPE_MAINTENANCE:
                    (hop_count,) = struct.unpack_from('!B', pkt_ll, 0)
                    if verbose == True:
                        print "MAINTENANCE (%d -> %d, %d) " % (srcId, dstId, hop_count)

                    (ttype, rdwrsize_status, TID) = \
                            struct.unpack_from('!xBBB', pkt_ll, 0)
                    if hop_count == 0:
                        if ttype == TTYPE_MAINT_READ_REQUEST:
                            (offset,) = struct.unpack_from('!xxxxI', pkt_ll, 0)
                            if verbose == True:
                                print "mregs.read(offset=%08x)" % (offset)
                            if offset == 0x74:
                                data = self.router.routing_table[self.router.mregs.read(0x70)]
                            elif offset == 0x14:
                                #data = self.portnum | (self.last_port << 8)
                                data = self.portnum | (4 << 8)
                            else:
                                data = self.router.mregs.read(offset)
                            wr_resp = mk_main_read_response(hop_count=255, status=0, TID=TID, data=data)
                            pk_hdr = mk_pkt_header(
                                    ftype=FTYPE_MAINTENANCE,
                                    tt=TT_8,
                                    dstId=srcId,
                                    srcId=dstId)

                            new_packet = pk_hdr + wr_resp
                            self.qout.put(new_packet)

                        elif ttype == TTYPE_MAINT_WRITE_REQUEST:
                            (offset, data) = struct.unpack_from('!xxxxII', pkt_ll, 0)
                            if verbose == True:
                                print "mregs.write(offset=%08x, data=%08x)" % (offset, data)
                            if offset == 0x74:
                                self.router.routing_table[self.router.mregs.read(0x70)] = data
                            else:
                                self.router.mregs.write(offset, data)
                            wr_resp = mk_main_write_response(hop_count=255, status=0, TID=TID)
                            pk_hdr = mk_pkt_header(
                                    ftype=FTYPE_MAINTENANCE,
                                    tt=TT_8,
                                    dstId=srcId,
                                    srcId=dstId)
                            new_packet = pk_hdr + wr_resp
                            self.qout.put(new_packet)
                        else:
                            print "skipping maintenance packet"
                    else:
                        if verbose == True:
                            if ttype == TTYPE_MAINT_READ_REQUEST:
                                (offset,) = struct.unpack_from('!xxxxI', pkt_ll, 0)
                                print "READ REQUEST(offset=%08x)" % (offset)
                            elif ttype == TTYPE_MAINT_WRITE_REQUEST:
                                (offset, data) = struct.unpack_from('!xxxxII', pkt_ll, 0)
                                print "WRITE REQUEST(offset=%08x, data=%08x)" % (offset, data)
                            elif ttype == TTYPE_MAINT_WRITE_RESPONSE:
                                print "WRITE RESPONSE"
                            elif ttype == TTYPE_MAINT_READ_RESPONSE:
                                (data,) = struct.unpack_from('!xxxxI', pkt_ll, 0)
                                print "READ RESPONSE(data=%08x)" % (data)

                        # repack maintenance packet
                        hop_count = hop_count - 1
                        pkt_ll = bytearray(pkt_ll)
                        struct.pack_into('!B', pkt_ll, 0, hop_count)
                        self.router.route(dstId, pkt_hdr, pkt_ll)
                else:
                    self.router.route(dstId, pkt_hdr, pkt_ll)
                if verbose == True:
                    print

            if mesg[0] == 'q' or mesg[0] == '\d':
                break

            if mesg[0] == "H":
                self.conn.send("Virtual RapidIO switch\n")
                self.conn.send("port %s\n" % (self.name,))

            if mesg[0] == "C":
                self.conn.send("C")

        self.conn.close()

    def out_thread(self):
        while True:
            packet = self.qout.get()
            #print "%s: out_thread" % (self.name,)
            if self.conn is not None:
                self.conn.send("P" + packet)
                self.tx_num = self.tx_num + 1


class switch_emulator():
    def __init__(self, start_tcp_port=6000, debug=False):
        self.ports = {}
        self.start_tcp_port = start_tcp_port
        self.last_port = 0
        self.routing_table = map(lambda x: 0, range(ROUTING_TABLE_ENTRY_NUMBER))

        self.cur_rt_index = 0

        self.mregs = mregisters_emulator()
        self.mregs.init_reg(0x0000, 0x00010074)
        self.mregs.init_reg(0x0004, 0x00000001)
        self.mregs.init_reg(0x0008, 0x0001000d)
        self.mregs.init_reg(0x0010, 0x1000051f)
        self.mregs.init_reg(0x0014, 0x10000400)
        self.mregs.init_reg(0x0018, 0x00000004)
        self.mregs.init_reg(0x001c, 0x00000000)
        self.mregs.init_reg(0x0020, 0x00000000)
        self.mregs.init_reg(0x0024, 0x00000000)
        self.mregs.init_reg(0x0028, 0x00000000)
        self.mregs.init_reg(0x002c, 0x00000000)
        self.mregs.init_reg(0x0030, 0x00000000)
        self.mregs.init_reg(0x0034, 0x000001ff)
        self.mregs.init_reg(0x0038, 0x00000008)
        self.mregs.init_reg(0x003c, 0x00000000)
        self.mregs.init_reg(0x0040, 0x00000000)
        self.mregs.init_reg(0x0044, 0x00000000)
        self.mregs.init_reg(0x0048, 0x00000000)
        self.mregs.init_reg(0x004c, 0x00000000)
        self.mregs.init_reg(0x0050, 0x00000000)
        self.mregs.init_reg(0x0054, 0x00000000)
        self.mregs.init_reg(0x0058, 0x00000000)
        self.mregs.init_reg(0x005c, 0x00000000)
        self.mregs.init_reg(0x0060, 0x00000000)
        self.mregs.init_reg(0x0064, 0x00000000)
        self.mregs.init_reg(0x0068, 0x0000ffff)
        self.mregs.init_reg(0x006c, 0x00000000)
        self.mregs.init_reg(0x0070, 0x00000000)
        self.mregs.init_reg(0x0074, 0x00000000)
        self.mregs.init_reg(0x0078, 0x000000ff)
        self.mregs.init_reg(0x007c, 0x00000000)
        self.mregs.init_reg(0x0080, 0x00000000)
        self.mregs.init_reg(0x0084, 0x00000000)
        self.mregs.init_reg(0x0088, 0x00000000)
        self.mregs.init_reg(0x008c, 0x00000000)
        self.mregs.init_reg(0x0090, 0x00000000)
        self.mregs.init_reg(0x0094, 0x00000000)
        self.mregs.init_reg(0x0098, 0x00000000)
        self.mregs.init_reg(0x009c, 0x00000000)
        self.mregs.init_reg(0x00a0, 0x00000000)
        self.mregs.init_reg(0x00a4, 0x00000000)
        self.mregs.init_reg(0x00a8, 0x00000000)
        self.mregs.init_reg(0x00ac, 0x00000000)
        self.mregs.init_reg(0x00b0, 0x00000000)
        self.mregs.init_reg(0x00b4, 0x00000000)
        self.mregs.init_reg(0x00b8, 0x00000000)
        self.mregs.init_reg(0x00bc, 0x00000000)
        self.mregs.init_reg(0x00c0, 0x00000000)
        self.mregs.init_reg(0x00c4, 0x00000000)
        self.mregs.init_reg(0x00c8, 0x00000000)
        self.mregs.init_reg(0x00cc, 0x00000000)
        self.mregs.init_reg(0x00d0, 0x00000000)
        self.mregs.init_reg(0x00d4, 0x00000000)
        self.mregs.init_reg(0x00d8, 0x00000000)
        self.mregs.init_reg(0x00dc, 0x00000000)
        self.mregs.init_reg(0x00e0, 0x00000000)
        self.mregs.init_reg(0x00e4, 0x00000000)
        self.mregs.init_reg(0x00e8, 0x00000000)
        self.mregs.init_reg(0x00ec, 0x00000000)
        self.mregs.init_reg(0x00f0, 0x00000000)
        self.mregs.init_reg(0x00f4, 0x00000000)
        self.mregs.init_reg(0x00f8, 0x00000000)
        self.mregs.init_reg(0x00fc, 0x00000000)
        self.mregs.init_reg(0x0100, 0x00000009)
        self.mregs.init_reg(0x0120, 0xffffff00)
        self.mregs.init_reg(0x0124, 0x00000000)
        self.mregs.init_reg(0x0158, 0x00000001)
        self.mregs.init_reg(0x015c, 0x00000000)
        self.mregs.init_reg(0x0178, 0x00000001)
        self.mregs.init_reg(0x017c, 0x00000000)
        self.mregs.init_reg(0x0198, 0x00000001)
        self.mregs.init_reg(0x019c, 0x00000000)
        self.mregs.init_reg(0x01b8, 0x00000001)
        self.mregs.init_reg(0x01bc, 0x00000000)

    def add_port(self):
        p = switch_port("port%d" % self.last_port, \
                        self.last_port, \
                        self, \
                        self.start_tcp_port + self.last_port)
        self.ports[self.last_port] = p
        self.last_port = self.last_port + 1

    def port_link_up(self, portnum):
        self.mregs.write(0x158 + portnum * 0x20, 0x2)

    def route(self, dstId, pkt_hdr, pkt_ll):
        new_packet = pkt_hdr + pkt_ll
        #print("dstid = %d\n" % (dstId,))
        port = self.ports[self.routing_table[dstId]]
        #print("route: %d to port %s \n" % (dstId, port.name,))
        port.qout.put(new_packet)

    def print_ports_status(self):
        for i in self.ports.values():
            i.print_status()

start_tcp_port = 2020
verbose = False


def usage():
    print __doc__

try:
    opts, args = getopt.getopt(sys.argv[1:], "hcvP:", \
        ["help", "cycle", "verbose", "agent-port="])
except getopt.GetoptError, err:
    print str(err)
    usage()
    sys.exit(2)

for o, a in opts:
    if o in ("-h", "--help"):
        usage()
        sys.exit()
    elif o in ("-v", "--verbose"):
        verbose = True
    elif o in ("-P", "--start-port"):
        start_tcp_port = int(a)
    else:
        assert False, "unhandled option"

sw = switch_emulator(start_tcp_port, debug=verbose)
sw.add_port()
sw.add_port()
sw.add_port()
sw.add_port()

while True:
    if verbose == False:
        print "\033[H\033[J"
        print "Switch:", start_tcp_port
        sw.print_ports_status()
    time.sleep(1)
