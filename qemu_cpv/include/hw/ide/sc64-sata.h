#ifndef _SC64_SATA_H
#define _SC64_SATA_H

#define SC64_SATA_CHANNELS_QUANTITY   2

extern void sc64_init_disks(DeviceState *ds, AddressSpace *dma_as,
                            DriveInfo *hds[SC64_SATA_CHANNELS_QUANTITY]);

#define SC64_SATA_PCMD_BAR        0x10
#define SC64_SATA_PCMD_BAR_VAL        0x00
#define SC64_SATA_PCNL_BAR        0x14
#define SC64_SATA_PCNL_BAR_VAL        0x20
#define SC64_SATA_SCMD_BAR        0x18
#define SC64_SATA_SCMD_BAR_VAL        0x40
#define SC64_SATA_SCNL_BAR        0x1C
#define SC64_SATA_SCNL_BAR_VAL        0x60
#define SC64_SATA_BAR             0x20
#define SC64_SATA_BAR_VAL             0x80
#define SC64_SATA_SIDPBA_BAR      0x24
#define SC64_SATA_SIDPBA_BAR_VAL      0xa0
#endif /* _SC64_SATA_H */
