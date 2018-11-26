/*
 * QEMU Duna board support
 *
 * Copyright (C) 2014 Antony Pavlov, Dmitry Smagin
 * Copyright (C) 2017 Aleksey Kuleshov <rndfax@yandex.ru>
 * Copyright (C) 2017 Anton Bondarev <anton.bondarev2310@gmail.com>
 * Copyright (C) 2017 Alex Kalmuk <alexkalmuk@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"

#include <net/net.h>

#include "elf.h"
#include "exec/address-spaces.h"
#include "hw/block/flash.h"
#include "hw/block/sc64-nand.h"
#include "hw/block/sc64-onfi.h"
#include "hw/boards.h"
#include "hw/char/serial.h"
#include "hw/char/sc64-fuart.h"
#include "hw/can/sc64-can.h"
#include "hw/display/vdc35-vgafb.h"
#include "hw/dma/sc64_idma.h"
#include "hw/dma/sc64_dma.h"
#include "hw/empty_slot.h"
#include "hw/gpio/sc64-gpio.h"
#include "hw/i2c/i2c.h"
#include "hw/ide/sc64-sata.h"
#include "hw/loader.h"
#include "hw/mips/bios.h"
#include "hw/mips/cpudevs.h"
#include "hw/mips/mips.h"
#include "hw/mips/sc64-devicebus.h"
#include "hw/mips/sccr-sysbus.h"
#include "hw/net/tulip-sysbus.h"
#include "hw/net/vg15-sysbus.h"
#include "hw/pci-host/sc64-pci.h"
#include "hw/pci/msi.h"
#include "hw/rapidio/sc64-rio.h"
#include "hw/mips/sc64-mpu.h"
#include "hw/ssi/sc64-spi-sysbus.h"
#include "hw/ssi/ssi.h"
#include "hw/sysbus.h"             /* SysBusDevice */
#include "hw/timer/sc64-timer.h"
#include "hw/usb/hcd-ehci.h"
#include "hw/usb/hcd-uhci.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "sysemu/block-backend.h"
#include "chardev/char-fe.h"
#include "sysemu/qtest.h"
#include "sysemu/sysemu.h"
#include "hw/mips/cp2.h"

#define ENVP_ADDR          0x800000700l
#define ENVP_SIZE          0x400

/* Hardware addresses */
#define SC64_RIO_ADDRESS     0x1a400000ULL
#define FLASH_ADDRESS        0x1fc00000ULL
#define FLASH_USR_ADDRESS    0x1c000000ULL
#define SERIAL0_ADDRESS      0x1ff70000ULL
#define SERIAL1_ADDRESS      0x1ff70008ULL
#define SC64_SPI_ADDRESS     0x1a600000ULL
#define SC64_IDMA_ADDRESS    0x1b506000ULL
#define SC64_MONITOR_ADDRESS 0x1b50d000ULL
#define SC64_FUART0_ADDRESS  0x1b500000ULL
#define SC64_FAURT1_ADDRESS  0x1b501000ULL
#define SC64_CAN0_ADDRESS    0x1b507000ULL
#define SC64_CAN1_ADDRESS    0x1b508000ULL
#define SC64_CAN2_ADDRESS    0x1b509000ULL
#define SC64_CAN3_ADDRESS    0x1b50a000ULL
#define SC64_DEVICE_BUS_ADDRESS 0x1b50c000ULL

const char *sc64_can_name[] = {
    "sc64-can0",
    "sc64-can1",
    "sc64-can2",
    "sc64-can3",
};

#define NUM_SPI_FLASHES      2

#undef BIOS_SIZE
#define BIOS_SIZE     (512 * 1024)

typedef struct {
    SysBusDevice parent_obj;
} DunaState;

#define TYPE_MIPS_VM8  "mips-vm8"
#define TYPE_MIPS_VM10 "mips-vm10"

static struct _loaderparams {
    const char *kernel_filename;
    const char *kernel_cmdline;
    const char *initrd_filename;
} loaderparams;

static void write_bootloader(CPUMIPSState *env, uint8_t *base,
                              int64_t run_addr, int64_t kernel_entry)
{
    uint32_t *p;

    /* Small bootloader */
    p = (uint32_t *)base;

    /*
     * Note: for now duna's kernel has its commandline hardcoded
     * so do nothing but a simple jump to kernel entry
     */
    stl_p(p++, 0x3c1f0000 | ((kernel_entry >> 16) & 0xffff));
    stl_p(p++, 0x37ff0000 | (kernel_entry & 0xffff));
    stl_p(p++, 0x03e00009);
    stl_p(p++, 0x00000000);
}

/* NOTE: duna's kernel uses weird format of cmdline where
 * all spaces are changed to underscores and underscores
 * are changed to double ones, so encode the string.
 */
static void append_cmd_tag(char *buf, const char *tag)
{
    char *p = buf + strlen(buf);

    while (*tag != 0) {
        if (*tag == ' ') {
            *p++ = '_';
            tag++;
        } else if (*tag == '_') {
            *p++ = '_';
            *p++ = '_';
            tag++;
        } else {
            *p++ = *tag++;
        }
    }

    *p = 0;
}

static int64_t load_kernel(const char *kernel_filename)
{
    int64_t kernel_entry, kernel_high;
    int big_endian;
    long initrd_size;
    ram_addr_t initrd_offset;
    char *prom_buf;

#ifdef TARGET_WORDS_BIGENDIAN
    big_endian = 1;
#else
    big_endian = 0;
#endif

    if (load_elf(kernel_filename, cpu_mips_kseg0_to_phys, NULL,
                 (uint64_t *)&kernel_entry, NULL, (uint64_t *)&kernel_high,
                 big_endian, EM_MIPS, 1, 0) < 0) {
        fprintf(stderr, "qemu: could not load kernel '%s'\n",
                kernel_filename);
        exit(1);
    }

    /* load initrd */
    initrd_size = 0;
    initrd_offset = 0;
    if (loaderparams.initrd_filename) {
        initrd_size = get_image_size (loaderparams.initrd_filename);
        if (initrd_size > 0) {
            initrd_offset = (kernel_high + ~INITRD_PAGE_MASK) & INITRD_PAGE_MASK;
            if (initrd_offset + initrd_size > ram_size) {
                fprintf(stderr,
                        "qemu: memory too small for initial ram disk '%s'\n",
                        loaderparams.initrd_filename);
                exit(1);
            }
            initrd_size = load_image_targphys(loaderparams.initrd_filename,
                                              initrd_offset,
                                              ram_size - initrd_offset);
        }
        if (initrd_size == (target_ulong) -1) {
            fprintf(stderr, "qemu: could not load initial ram disk '%s'\n",
                    loaderparams.initrd_filename);
            exit(1);
        }
    }

    /* Setup prom parameters. */
    prom_buf = g_malloc(ENVP_SIZE);

    /* First part of the command line */
    strcpy(prom_buf, "tn=duna_ose s=");

    if (initrd_size > 0) {
        char tmp[ENVP_SIZE];

        sprintf(tmp, "rd_start=0x%" PRIx64 " rd_size=%li %s",
                 cpu_mips_phys_to_kseg0(NULL, initrd_offset), initrd_size,
                 loaderparams.kernel_cmdline);
        append_cmd_tag(prom_buf, (const char *)tmp);
    } else {
        append_cmd_tag(prom_buf, loaderparams.kernel_cmdline);
    }

    rom_add_blob_fixed("prom", prom_buf, ENVP_SIZE,
                       cpu_mips_kseg0_to_phys(NULL, ENVP_ADDR));

    g_free(prom_buf);

    return kernel_entry;
}

static void main_cpu_reset(void *opaque)
{
    MIPSCPU *cpu = opaque;
    CPUMIPSState *env = &cpu->env;

    cpu_reset(CPU(cpu));

    if (loaderparams.kernel_filename) {
        env->CP0_Status &= ~(1 << CP0St_ERL);
    }
}

static MemoryRegion *duna_create_bios_map(hwaddr addr,
                                                const char *name, size_t size)
{
    MemoryRegion *map = g_new(MemoryRegion, 1);
    MemoryRegion *map_512 = g_new(MemoryRegion, 1);
    MemoryRegion *system_memory = get_system_memory();

    memory_region_init(map, NULL, name, size);
    memory_region_init_alias(map_512, NULL, name, map, 0, size);
    memory_region_add_subregion(system_memory, addr, map_512);
    memory_region_set_readonly(map, true);
    return map;
}

static void duna_load_kernel(MachineState *machine,
                                        CPUMIPSState *env, MemoryRegion *bios)
{
    const char *kernel_filename = machine->kernel_filename;
    const char *kernel_cmdline = machine->kernel_cmdline;
    const char *initrd_filename = machine->initrd_filename;
    uint64_t kernel_entry;

    if (kernel_filename == NULL) {
        return;
    }

    /* Fill kernel parameters data */
    loaderparams.kernel_filename = kernel_filename;
    loaderparams.kernel_cmdline = kernel_cmdline;
    loaderparams.initrd_filename = initrd_filename;

    /* Load kernel elf */
    kernel_entry = load_kernel(kernel_filename);

    /* Write a small bootloader to the flash location. */
    MemoryRegion *mr = g_new(MemoryRegion, 1);
    memory_region_init_ram_nomigrate(mr, NULL, "default-boot-fw", BIOS_SIZE,
                        NULL);
    memory_region_add_subregion_overlap(bios, 0, mr, -2);
    write_bootloader(env, memory_region_get_ram_ptr(mr),
                        0xbfc00000ULL, kernel_entry);
}

static void duna_powerdown_handler(void *opaque, int n, int level)
{
    if (level) {
        qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
    }
}

static MemoryRegion *duna_ram_init(uint64_t ram_size)
{
    MemoryRegion *system_memory = get_system_memory();
    const uint64_t MAXRAMSIZE = 8192ULL << 20;
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    MemoryRegion *ram_low = g_new(MemoryRegion, 1);
    MemoryRegion *ram_high = g_new(MemoryRegion, 1);

    if (ram_size > MAXRAMSIZE) {
        error_report(
                "qemu: Too much memory for this machine: "
                "%" PRIu64 " MB, maximum %" PRIu64 " MB",
                ram_size / (1 << 20), MAXRAMSIZE / (1 << 20));
        exit(1);
    }

    /* register the whole RAM */
    memory_region_init_ram_nomigrate(ram, NULL, "mips_machine.ram", ram_size,
                           &error_abort);

    /* register the bottom part of RAM as a mirror at low address */
    memory_region_init_alias(ram_low, NULL, "mips_machine_low.ram",
                             ram, 0, MIN(ram_size, (256 << 20)));
    memory_region_add_subregion(system_memory, 0, ram_low);

    /* register the whole of RAM at high address */
    memory_region_init_alias(ram_high, NULL, "mips_machine_high.ram",
                             ram, 0, ram_size);
    memory_region_add_subregion(system_memory, 0x40000000, ram_high);

    vmstate_register_ram_global(ram);

    return ram;
}

static void duna_usb_init(qemu_irq irq, AddressSpace *dma_as)
{
    DeviceState *ehci_dev;
    BusState *usbbus;
    DeviceState *uhci_dev;
    SysBusDevice *sbd;
    SRISAEHCIState *s;

    /* Create EHCI */
    ehci_dev = qdev_create(NULL, "srisa,sc64-ehci");
    s = SRISA_EHCI(ehci_dev);
    s->dma_as = dma_as;
    qdev_init_nofail(ehci_dev);

    sbd = SYS_BUS_DEVICE(ehci_dev);
    sysbus_mmio_map(sbd, 0, 0x1ba00200);
    sysbus_connect_irq(sbd, 0, irq);

    /* Create UHCI as companion to EHCI */
    usbbus = QLIST_FIRST(&ehci_dev->child_bus);
    uhci_dev = qdev_create(NULL, "sc64-uhci");
    sbd = SYS_BUS_DEVICE(uhci_dev);

    qdev_prop_set_string(uhci_dev, "masterbus", usbbus->name);

    qdev_init_nofail(uhci_dev);
    sysbus_mmio_map(sbd, 0, 0x1ba00000);
    sysbus_connect_irq(sbd, 0, irq);

    usb_uhci_sysbus_init(uhci_dev, dma_as);
}

static void duna_load_ramfiles(MachineState *machine, CPUMIPSState *env,
                                            MemoryRegion *bios)
{
    int result;
    int i;

    duna_load_kernel(machine, env, bios);

    /* load binary images to ram */
    for (i = 0; i < nb_ramfile; i++) {
        result = load_image_targphys(ramfile[i].name,
                                     ramfile[i].addr,
                                     get_image_size(ramfile[i].name));

        if (result == -1) {
            fprintf(stderr, "qemu: could not load ram image file '%s'\n",
                    ramfile[i].name);
            exit(1);
        }
    }
}

static void duna_sata_init(AddressSpace *dma_as, qemu_irq irq)
{
    DeviceState *sata = sysbus_create_simple("sc64-sata", 0x1a800000, irq);
    DriveInfo *hds[SC64_SATA_CHANNELS_QUANTITY];
    int i;

    for (i = 0; i < SC64_SATA_CHANNELS_QUANTITY; i++) {
        hds[i] = drive_get_by_index(IF_IDE, i);
    }
    sc64_init_disks(sata, dma_as, hds);
}

static void duna_serials_init(qemu_irq irq1, qemu_irq irq2)
{
    int i;
    /* Make sure the first 1 serial ports are associated with a device. */
    for (i = 0; i < 2; i++) {
        if (!serial_hds[i]) {
            char label[32];
            snprintf(label, sizeof(label), "serial%d", i);
            serial_hds[i] = qemu_chr_new(label, "null");
        }
    }

    serial_mm_init(get_system_memory(), SERIAL0_ADDRESS, 0, irq1,
                   115200, serial_hds[0], DEVICE_NATIVE_ENDIAN);

    serial_mm_init(get_system_memory(), SERIAL1_ADDRESS, 0, irq2,
                   115200, serial_hds[1], DEVICE_NATIVE_ENDIAN);
}

static void vm10_fuarts_init(qemu_irq irq1, qemu_irq irq2,
    AddressSpace *dma_as)
{
    Chardev *fuart0 = qemu_chr_find("fuart0");
    Chardev *fuart1 = qemu_chr_find("fuart1");

    if (!fuart0 || !fuart1) {
        warn_report("FastUart: FastUarts are not specified. "
                    "Only internal loop is available.");

        /* Just create stubs, which will be not used in the loop mode */
        if (!fuart0) {
            fuart0 = qemu_chr_new("fuart0", "null");
        }
        if (!fuart1) {
            fuart1 = qemu_chr_new("fuart1", "null");
        }
    } else {
        warn_report("FastUart: Only internal and external "
                    "loops are currenly available.");
    }

    sc64_fuart_register("sc64-fuart", SC64_FUART0_ADDRESS, irq1,
                        fuart0, dma_as, 0);

    sc64_fuart_register("sc64-fuart", SC64_FAURT1_ADDRESS, irq2,
                        fuart1, dma_as, 1);
}

static qemu_irq *get_irqs_for_intc(CPUMIPSState *env)
{
    qemu_irq *cpu_irq;
    int i;

    cpu_irq = malloc(sizeof(qemu_irq) * 8);
    for (i = 0; i < 8; i++) {
        cpu_irq[i] = qemu_clone_irq(env->irq[i]);
    }

    return cpu_irq;
}

static CPUMIPSState *duna_cpu_init(const char *cpu_type)
{
    MIPSCPU *cpu;

    /* init CPUs */
    cpu = MIPS_CPU(cpu_create(cpu_type));

    /* Init internal devices */
    cpu_mips_irq_init_cpu(cpu);
    cpu_mips_clock_init(cpu);
    qemu_register_reset(main_cpu_reset, cpu);

    cpu = MIPS_CPU(first_cpu);

    return &cpu->env;
}

static void duna_create_machine(const char *machine_name)
{
    DeviceState *dev = qdev_create(NULL, machine_name);

    qdev_init_nofail(dev);
}

static void duna_boot_init(MachineState *machine, CPUMIPSState *env,
                                        AddressSpace *dma_as, qemu_irq spi_irq)
{
    MemoryRegion *cs0, *cs1;
    char *filename;

    cs0 = duna_create_bios_map(0x1fc00000, "SPI Flash CS0", 0x80000);
    cs1 = duna_create_bios_map(0x1c000000, "SPI Flash CS1", 0x2000000);

    sysbus_spi_register("sc64-spi", SC64_SPI_ADDRESS, spi_irq, dma_as, cs0, cs1);

    /* Set default firmware image. */
    if (bios_name == NULL) {
        bios_name = BIOS_FILENAME;
    }

    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, bios_name);
    if (filename) {
        void *buf = g_malloc(BIOS_SIZE);
        int bios_size = load_image_size(filename, buf, BIOS_SIZE);

        g_free(filename);

        if (bios_size < 0 && !qtest_enabled()) {
            error_report("Could not load default firmware from '%s'."
                                                " Using SPI cs0 image.", bios_name);
            g_free(buf);
        } else {
            MemoryRegion *mr = g_new(MemoryRegion, 1);
            memory_region_init_ram_ptr(mr, NULL, "default-fw", BIOS_SIZE, buf);
            memory_region_add_subregion_overlap(cs0, 0, mr, -1);
        }
    }

    duna_load_ramfiles(machine, env, cs0);
}

static void duna_reset(void *arg)
{
    CPUState *cpu = arg;
    /* Magic: 0x0400 - CPU_INTERRUPT_RESET */
    cpu_interrupt(cpu, 0x0400);
}

static void duna_bootmap_enable(MemoryRegion *mr, bool enable)
{
    if (memory_region_is_mapped(mr)) {
        memory_region_del_subregion(get_system_memory(), mr);
    }

    if (enable) {
        memory_region_add_subregion_overlap(
                get_system_memory(), 0x1fc00000, mr, 1);
    }
}

static void sc64_pcie_register(AddressSpace *mpu_as, qemu_irq *intc_irqs)
{
    struct sc64_pcie_struct ps_0 = {
        .name = "pcie_0",
        .dma_as = &mpu_as[MPU_PCIE0],
        .irq = {
            intc_irqs[SC64_PCIE0_A_IRQ],
            intc_irqs[SC64_PCIE0_B_IRQ],
            intc_irqs[SC64_PCIE0_C_IRQ],
            intc_irqs[SC64_PCIE0_D_IRQ],
        },
        .msi = intc_irqs[SC64_PCIE0_MSIX_IRQ],
        .self_conf = 0x1b001000ULL,
        .out1 = 0x10000000ULL,
        .out2 = 0x1a000000ULL,
        .out3 = 0xd00000000ULL,
    };
    struct sc64_pcie_struct ps_1 = {
        .name = "pcie_1",
        .dma_as = &mpu_as[MPU_PCIE1],
        .irq = {
            intc_irqs[SC64_PCIE1_A_IRQ],
            intc_irqs[SC64_PCIE1_B_IRQ],
            intc_irqs[SC64_PCIE1_C_IRQ],
            intc_irqs[SC64_PCIE1_D_IRQ],
        },
        .msi = intc_irqs[SC64_PCIE1_MSIX_IRQ],
        .self_conf = 0x1b003000ULL,
        .out1 = 0xf00000000ULL,
        .out2 =  0xf09000000ULL,
        .out3 = 0xe00000000ULL,
    };

    if (qemu_chr_find("pcie_0_ep") != NULL) {
        ps_0.name = "pcie_0_ep";
        sc64_pcie_ep_register(&ps_0);
    } else {
        sc64_pcie_host_register(&ps_0,
                                    0x1e000000ULL, 0x20000000ULL, 0x10000000);
    }

    if (qemu_chr_find("pcie_1_ep") != NULL) {
        ps_0.name = "pcie_1_ep";
        sc64_pcie_ep_register(&ps_1);
    } else {
        sc64_pcie_host_register(&ps_1,
                                    0xf08000000ULL, 0x30000000ULL, 0x10000000);
    }
}

static void vm10_onfi_register(hwaddr addr, AddressSpace *dma_as, qemu_irq irq)
{
    DeviceState *dev;
    SysBusDevice *s;

    dev = sc64_onfi_register(dma_as);
    s = SYS_BUS_DEVICE(dev);

    /* This overlaps default NAND controller registers */
    sysbus_mmio_map_overlap(s, 0, addr, 1);
    sysbus_connect_irq(s, 0, irq);
    /* Device is disabled by default */
    memory_region_set_enabled(sysbus_mmio_get_region(s, 0), false);
}

static void mips_vm10_init(MachineState *machine)
{
    uint64_t ram_size = machine->ram_size;
    MemoryRegion *ram;
    CPUMIPSState *env;
    qemu_irq *intc_irqs;
    DeviceState *i2c_dev, *gpio_dev, *timer_dev;
    I2CBus *i2c_bus;
    qemu_irq *duna_pic[2];
    AddressSpace *mpu_as;
    AddressSpace *dma_as = g_new(AddressSpace, 1);
    int i;

    duna_create_machine(TYPE_MIPS_VM10);

    env = duna_cpu_init(machine->cpu_type);

    /* allocate RAM */
    ram = duna_ram_init(ram_size);

    address_space_init(dma_as, ram, "DMA address space");

    /* Init interrupt controller */
    duna_pic[0] = sc64_intc_register(0x1b400020, get_irqs_for_intc(env));
    duna_pic[1] = sc64_intc_v2_register(0x01b502000, get_irqs_for_intc(env));

    /* Init system controller registers */
    intc_irqs = sysbus_sccr_v2_register(0x1b000000, duna_pic[0], duna_pic[1]);

    mpu_as = sc64_mpu_register(0x1b516000, intc_irqs[SC64_MPU_IRQ], dma_as);

    sysbus_dma_register(0x1b000000, &mpu_as[MPU_DMA]);

    sc64_pcie_register(mpu_as, intc_irqs);

    /* Init UART */
    duna_serials_init(intc_irqs[SC64_UART0_IRQ], intc_irqs[SC64_UART1_IRQ]);
    vm10_fuarts_init(intc_irqs[SC64_FASTUART0_IRQ],
                     intc_irqs[SC64_FASTUART1_IRQ], dma_as);

    gpio_dev = sc64_gpio_register(0x1b400250, 0x1b400130,
                    intc_irqs[SC64_PORTA2_IRQ], intc_irqs[SC64_PORTA3_IRQ],
                    intc_irqs[SC64_PORTB2_IRQ], intc_irqs[SC64_PORTB3_IRQ],
                    intc_irqs[SC64_OUTER0_IRQ], intc_irqs[SC64_OUTER1_IRQ]);

    qdev_connect_gpio_out(gpio_dev, 0,
                            qemu_allocate_irq(duna_powerdown_handler, NULL, 0));

    duna_usb_init(intc_irqs[SC64_USB_IRQ], &mpu_as[MPU_USB]);

    sysbus_vdc35_register(0x1b200000, intc_irqs[SC64_VDC35_IRQ], dma_as);

    /* [0x01b400110 - 0x1b400117] - first i2c bus */
    i2c_dev = sysbus_create_simple("sc64_i2c", 0x01b400110,
                                                    intc_irqs[SC64_I2C0_IRQ]);
    i2c_bus = (I2CBus *)qdev_get_child_bus(i2c_dev, "i2c");
    i2c_create_slave(i2c_bus, "i2c-ddc", 0x50);
    i2c_create_slave(i2c_bus, "ds1338", 0x68);

    /* [0x01b400120 - 0x1b400127] - second i2c bus */
    sysbus_create_simple("sc64_i2c", 0x01b400120, intc_irqs[SC64_I2C1_IRQ]);

    sc64_nand_flash_register(0x1b600000);
    vm10_onfi_register(0x1b600000, dma_as, intc_irqs[SC64_NAND_IRQ]);

    /* Init memory for IDMA */
    MemoryRegion *idma_mem_win = g_new(MemoryRegion, 3);
    MemoryRegion *idma_mem = g_new(MemoryRegion, 1);

    memory_region_init_alias(&idma_mem_win[0], NULL, "pcie-idma-win0",
                                get_system_memory(), 0x10000000, 0x8000000);
    memory_region_init_alias(&idma_mem_win[1], NULL, "pcie-idma-win1",
                                get_system_memory(), 0x1a000000, 0x400000);
    memory_region_init_alias(&idma_mem_win[2], NULL, "pcie-idma-win2",
                                get_system_memory(), 0xd00000000, 0x100000000);
    memory_region_init(idma_mem, NULL, "pcie-idma", UINT64_MAX);
    memory_region_add_subregion(idma_mem, 0x10000000, &idma_mem_win[0]);
    memory_region_add_subregion(idma_mem, 0x1a000000, &idma_mem_win[1]);
    memory_region_add_subregion(idma_mem, 0xd00000000, &idma_mem_win[2]);

    /* Init IDMA controller */
    for (i = 0; i < 8; i++) {
        sysbus_idma_register("sc64-idma", SC64_IDMA_ADDRESS + i * 0x40,
                             &mpu_as[MPU_IDMA], idma_mem, NULL,
                             intc_irqs[SC64_IDMA_IRQ]);
    }

    timer_dev = sysbus_create_varargs(TYPE_SC64_TIMER_V2, 0x1b503000,
                                                intc_irqs[SC64_TIMER_V2_0_IRQ],
                                                intc_irqs[SC64_TIMER_V2_1_IRQ],
                                                intc_irqs[SC64_TIMER_V2_2_IRQ],
                                                intc_irqs[SC64_TIMER_V2_3_IRQ],
                                                intc_irqs[SC64_TIMER_V2_4_IRQ],
                                                intc_irqs[SC64_TIMER_V2_5_IRQ],
                                                intc_irqs[SC64_TIMER_V2_6_IRQ],
                                                intc_irqs[SC64_TIMER_V2_7_IRQ],
                                                NULL);

    sysbus_create_varargs("sc64-timer", 0x1b400080,
                                                intc_irqs[SC64_TIMER_V1_0_IRQ],
                                                intc_irqs[SC64_TIMER_V1_1_IRQ],
                                                intc_irqs[SC64_TIMER_V1_2_IRQ],
                                                intc_irqs[SC64_TIMER_V1_3_IRQ],
                                                intc_irqs[SC64_TIMER_V1_4_IRQ],
                                                NULL);

    sysbus_vg15_register(VG15m, &nd_table[0], 0x1b800000,
                        intc_irqs[SC64_ETH0_IRQ], &mpu_as[MPU_ETH0]);
    sysbus_vg15_register(VG15m, &nd_table[1], 0x1b900000,
                        intc_irqs[SC64_ETH1_IRQ], &mpu_as[MPU_ETH1]);

    /* Init RapidIO controller. */
    sc64_rio_register(TYPE_SC64_RIO, SC64_RIO_ADDRESS, &mpu_as[MPU_RAPIDIO],
        intc_irqs[SC64_RIO_IRQ]);

    sysbus_create_simple("sc64-monitor", SC64_MONITOR_ADDRESS,
            intc_irqs[SC64_TEMP_IRQ]);

    sc64_can_register(SC64_CAN0_ADDRESS, intc_irqs[SC64_CAN0_IRQ],
            sc64_can_name[0], sc64_timestamp_get, timer_dev);
    sc64_can_register(SC64_CAN1_ADDRESS, intc_irqs[SC64_CAN1_IRQ],
            sc64_can_name[1], sc64_timestamp_get, timer_dev);
    sc64_can_register(SC64_CAN2_ADDRESS, intc_irqs[SC64_CAN0_IRQ],
            sc64_can_name[2], sc64_timestamp_get, timer_dev);
    sc64_can_register(SC64_CAN3_ADDRESS, intc_irqs[SC64_CAN1_IRQ],
            sc64_can_name[3], sc64_timestamp_get, timer_dev);

    duna_sata_init(&mpu_as[MPU_SATA], intc_irqs[SC64_SATA_IRQ]);

    duna_boot_init(machine, env, &mpu_as[MPU_SPI], intc_irqs[SC64_SPI_IRQ]);

    sc64_devicebus_register(SC64_DEVICE_BUS_ADDRESS,
            0xF20000000ULL, duna_reset, CPU(mips_env_get_cpu(env)),
            duna_bootmap_enable);
}

static void mips_vm8_init(MachineState *machine)
{
    uint64_t ram_size = machine->ram_size;
    MemoryRegion *ram;
    CPUMIPSState *env;
    qemu_irq *intc_irqs;
    DeviceState *i2c_dev, *gpio_dev;
    I2CBus *i2c_bus;
    CP2State *cp2;
    AddressSpace *dma_as = g_new(AddressSpace, 1);

    duna_create_machine(TYPE_MIPS_VM8);

    env = duna_cpu_init(machine->cpu_type);

    /* allocate RAM */
    ram = duna_ram_init(ram_size);

    address_space_init(dma_as, ram, "DMA address space");

    cp2 = sc64_cp2_register(0x1ff00000, dma_as);
    env->cp2 = cp2;

    /* Init system controller registers */
    sysbus_sccr_register(0x1b000000);
    sysbus_dma_register(0x1b000000, dma_as);

    /* Init interrupt controller */
    intc_irqs = sc64_intc_register(0x1b400020, get_irqs_for_intc(env));

    sc64_pci_register(intc_irqs, dma_as);

    /* Init UART */
    duna_serials_init(intc_irqs[DUNA_UART0_IRQ], intc_irqs[DUNA_UART1_IRQ]);

    gpio_dev = sc64_gpio_register(0x1b400250, 0x1b400130,
                        intc_irqs[DUNA_PORTA2_IRQ], intc_irqs[DUNA_PORTA3_IRQ],
                        intc_irqs[DUNA_PORTB2_IRQ], intc_irqs[DUNA_PORTB3_IRQ],
                        intc_irqs[DUNA_OUTER_IRQ0], intc_irqs[DUNA_OUTER_IRQ1]);

    qdev_connect_gpio_out(gpio_dev, 0,
                            qemu_allocate_irq(duna_powerdown_handler, NULL, 0));

    duna_usb_init(intc_irqs[DUNA_USB_IRQ], dma_as);

    sysbus_vdc35_register(0x1b200000, intc_irqs[DUNA_VDC35_IRQ], dma_as);

    /* [0x01b400110 - 0x1b400117] - first i2c bus */
    i2c_dev = sysbus_create_simple("sc64_i2c",
                                        0x01b400110, intc_irqs[DUNA_I2C0_IRQ]);
    i2c_bus = (I2CBus *)qdev_get_child_bus(i2c_dev, "i2c");
    i2c_create_slave(i2c_bus, "i2c-ddc", 0x50);
    i2c_create_slave(i2c_bus, "ds1338", 0x68);

    /* [0x01b400120 - 0x1b400127] - second i2c bus */
    sysbus_create_simple("sc64_i2c", 0x01b400120, intc_irqs[DUNA_I2C1_IRQ]);

    sc64_nand_flash_register(0x1b600000);

    sysbus_create_varargs("sc64-timer", 0x1b400080,
                                                    intc_irqs[DUNA_TIMER0_IRQ],
                                                    intc_irqs[DUNA_TIMER1_IRQ],
                                                    intc_irqs[DUNA_TIMER2_IRQ],
                                                    intc_irqs[DUNA_TIMER3_IRQ],
                                                    intc_irqs[DUNA_TIMER4_IRQ],
                                                    NULL);

    sysbus_vg15_register(VG15e, &nd_table[1], 0x1b800000,
                                            intc_irqs[DUNA_ETH0_IRQ], dma_as);
    sysbus_tulip_register(0x1b900000, intc_irqs[DUNA_ETH0_IRQ], dma_as);

    /* Init RapidIO controller. */
    sc64_rio_register(TYPE_SC64_RIO, SC64_RIO_ADDRESS, dma_as,
        intc_irqs[DUNA_RIO_IRQ]);

    duna_sata_init(dma_as, intc_irqs[DUNA_SATA_IRQ]);

    duna_boot_init(machine, env, dma_as, intc_irqs[DUNA_SPI_IRQ]);
}

static const TypeInfo mips_duna_device = {
    .name          = TYPE_MIPS_VM8,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DunaState),
};

static const TypeInfo mips_vm108_device = {
    .name          = TYPE_MIPS_VM10,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(DunaState),
};

static void mips_duna_machine_init(MachineClass *mc)
{
    mc->desc = "VM8 board";
    mc->init = mips_vm8_init;
    mc->max_cpus = 1;
    mc->is_default = 0;
    mc->pci_allow_0_address = true;
    mc->default_cpu_type = MIPS_CPU_TYPE_NAME("K64RIO");
}

static void mips_vm108_machine_init(MachineClass *mc)
{
    mc->desc = "VM10 board";
    mc->init = mips_vm10_init;
    mc->max_cpus = 1;
    mc->is_default = 0;
    mc->pci_allow_0_address = true;
    mc->default_cpu_type = MIPS_CPU_TYPE_NAME("K64RIO");
    mc->has_dynamic_sysbus = 1;
    msi_nonbroken = 1;
}

DEFINE_MACHINE("vm8", mips_duna_machine_init);
DEFINE_MACHINE("vm10", mips_vm108_machine_init);

static void mips_duna_register_types(void)
{
    type_register_static(&mips_duna_device);
    type_register_static(&mips_vm108_device);
}

type_init(mips_duna_register_types)
