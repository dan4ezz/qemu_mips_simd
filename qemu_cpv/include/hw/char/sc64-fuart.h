#ifndef _SC64_FUART_H_
#define _SC64_FUART_H_

DeviceState *sc64_fuart_register(const char *name, hwaddr addr,
    qemu_irq irq, Chardev *chr, AddressSpace *dma_as, int port_nr);

#endif
