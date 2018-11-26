#ifndef _SC64_MPU_H_
#define _SC64_MPU_H_

#include "exec/memory.h"
#include "exec/address-spaces.h"

enum MpuDeviceType {
    MPU_SPI = 0,
    MPU_USB,
    MPU_RAPIDIO,
    MPU_SATA,
    MPU_ETH1,
    MPU_ETH0,
    MPU_NAND,
    MPU_PCIE1,
    MPU_PCIE0,
    MPU_DMA,
    MPU_IDMA,
};

AddressSpace *sc64_mpu_register(hwaddr addr, qemu_irq irq,
    AddressSpace *dma_as);

#endif
