#ifndef _SC64_PCI_H_
#define _SC64_PCI_H_

#include "hw/sysbus.h"

void sc64_pci_register(qemu_irq *pic, AddressSpace *dma_as);

#define SC64_PCIE_SPACES_CNT     3
#define SC64_PCIE_WINDOWS_CNT    8

typedef struct SC64PCIEBlock {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    uint32_t regs[0x1804 / 4];

    MemoryRegion out_spaces[SC64_PCIE_SPACES_CNT];
    MemoryRegion out_windows[SC64_PCIE_WINDOWS_CNT];
    MemoryRegion out_windows_io[SC64_PCIE_WINDOWS_CNT];

    MemoryRegion in_spaces[SC64_PCIE_SPACES_CNT];
    MemoryRegion in_windows[SC64_PCIE_WINDOWS_CNT];

    MemoryRegion pci_reg_host;
    MemoryRegion pci_mem;
    MemoryRegion pci_io;

    AddressSpace pci_dma_as;
    AddressSpace *dma_as;

    qemu_irq irq;

    void (*space_updated)(void *opaque, int bar, uint64_t size, int flags);
    void *opaque;

    PCIDevice *bridge;
} SC64PCIEBlock;

struct sc64_pcie_struct {
    const char *name;
    AddressSpace *dma_as;
    qemu_irq irq[4];
    qemu_irq msi;
    hwaddr self_conf;
    hwaddr out1, out2, out3;
};

void sc64_pcie_host_register(const struct sc64_pcie_struct *ps,
                        hwaddr leg_conf, hwaddr ext_conf, uint32_t ext_size);
void sc64_pcie_ep_register(const struct sc64_pcie_struct *ps);

SC64PCIEBlock *sc64_pcie_block_register(const struct sc64_pcie_struct *ps);
SC64PCIEBlock *sc64_pcie_block_register_ops(const struct sc64_pcie_struct *ps,
        void (*space_updated)(void *opaque, int bar, uint64_t size, int flags),
        void *opaque,
        const MemoryRegionOps *pci_mem_ops,
        const MemoryRegionOps *pci_io_ops,
        void *ops_opaque);

#endif
