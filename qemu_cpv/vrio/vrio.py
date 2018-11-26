# -*- coding: utf-8 -*-
# vim:ts=4:sw=4:softtabstop=4:smarttab:tw=78:expandtab
"""
Константы и библиотечные функции для vrio.
"""

import struct

FTYPE_MAINTENANCE = 8

TT_8 = 0
TT_16 = 1

TTYPE_MAINT_READ_REQUEST = 0
TTYPE_MAINT_WRITE_REQUEST = 1
TTYPE_MAINT_READ_RESPONSE = 2
TTYPE_MAINT_WRITE_RESPONSE = 3
TTYPE_MAINT_PORT_WRITE_REQUEST = 4


def mk_pkt_header(ftype, tt, dstId, srcId):
    return struct.pack('!BBBBHH',
            ftype,
            tt,
            0,  # phys_res_1
            0,  # phys_res_2
            dstId,
            srcId)

##
## #----
## #[23:0] offset
## #[63:32] reserved_data
## #[31:0] data
## #----
##


def mk_main_read_request(hop_count, TID, offset):
    ttype = TTYPE_MAINT_READ_REQUEST
    rdsize = 4
    ll_len = 12  # <<logical level>> length
    return struct.pack('!IBBBBI', ll_len,  # FIXME: pad to 16
            hop_count,
            ttype,
            rdsize,
            TID,
            offset)


def mk_main_read_response(hop_count, status, TID, data):
    ttype = TTYPE_MAINT_READ_RESPONSE
    ll_len = 12  # <<logical level>> length
    return struct.pack('!IBBBBI', ll_len,  # FIXME: pad to 16
            hop_count,
            ttype,
            status,
            TID,
            data)


def mk_main_write_request(hop_count, TID, offset, data):
    ttype = TTYPE_MAINT_WRITE_REQUEST
    wrsize = 4
    ll_len = 16  # <<logical level>> length
    return struct.pack('!IBBBBII', ll_len,
            hop_count,
            ttype,
            wrsize,
            TID,
            offset,
            data)


def mk_main_write_response(hop_count, status, TID):
    ttype = TTYPE_MAINT_WRITE_RESPONSE
    ll_len = 8  # <<logical level>> length
    return struct.pack('!IBBBB',  # FIXME: pad to 16
            ll_len,
            hop_count,
            ttype,
            status,
            TID)


def recv_rio_pkt(data1, data2):
    # [3:0] ftype [1:0] tt [15:0] srcId [15:0] dstId
    #data = recv(8)
    (ftype, tt, phy_res_1, phy_res_2, dstId, srcId) \
        = struct.unpack('!BBBBHH', data1)
    print(("ftype = %d" % (ftype,)))
    print(("tt = %d" % (tt,)))
    print(("dstId = %d" % (dstId,)))
    print(("srcId = %d" % (srcId,)))

    if ftype == FTYPE_MAINTENANCE:
        (ll_len, hop_count, ttype, rdwrsize_status, TID) \
            = \
            struct.unpack('!IBBBBxxxxxxxx', data2)

        if hop_count == 0:
            if ttype == TTYPE_MAINT_READ_REQUEST:
                wr_resp = mk_main_read_response(hop_count=255, \
                        status=0, TID=TID, data=0)
                pk_hdr = mk_pkt_header(ftype=8, tt=0, \
                        srcId=dstId, dstId=srcId)

            if ttype == TTYPE_MAINT_WRITE_REQUEST:
                wr_resp = mk_main_write_response(hop_count=255, \
                        status=0, TID=TID)
                pk_hdr = mk_pkt_header(ftype=8, \
                        tt=0, srcId=dstId, dstId=srcId)
