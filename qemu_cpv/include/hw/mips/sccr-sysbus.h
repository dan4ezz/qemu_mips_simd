/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SCCR_SYSBUS_H_
#define _SCCR_SYSBUS_H_

/* definitions for MIPS_DUNA machine */
#define DUNA_VDC35_IRQ   0
#define DUNA_SATA_IRQ    1 /* INT0nB, PCI or SATA */
#define DUNA_TIMER4_IRQ  4
#define DUNA_I2C0_IRQ    5
#define DUNA_I2C1_IRQ    5
#define DUNA_UART0_IRQ   6
#define DUNA_SPI_IRQ     8
#define DUNA_NAND_IRQ    8 /* The same IRQ with SPI */
#define DUNA_TIMER3_IRQ  9
#define DUNA_TIMER2_IRQ  10
#define DUNA_TIMER1_IRQ  11
#define DUNA_TIMER0_IRQ  12
#define DUNA_PORTA2_IRQ  13
#define DUNA_PORTA3_IRQ  14
#define DUNA_UART1_IRQ   16
#define DUNA_OUTER_IRQ0  17
#define DUNA_PORTB2_IRQ  18
#define DUNA_PORTB3_IRQ  19
#define DUNA_USB_IRQ     20
#define DUNA_RIO_IRQ     21
#define DUNA_OUTER_IRQ1  22
#define DUNA_ETH0_IRQ    23

enum {
    SC64_TIMER_V1_0_IRQ,
    SC64_TIMER_V1_1_IRQ,
    SC64_TIMER_V1_2_IRQ,
    SC64_TIMER_V1_3_IRQ,
    SC64_TIMER_V1_4_IRQ,

    SC64_TIMER_V2_0_IRQ,
    SC64_TIMER_V2_1_IRQ,
    SC64_TIMER_V2_2_IRQ,
    SC64_TIMER_V2_3_IRQ,
    SC64_TIMER_V2_4_IRQ,
    SC64_TIMER_V2_5_IRQ,
    SC64_TIMER_V2_6_IRQ,
    SC64_TIMER_V2_7_IRQ,

    SC64_CAN0_IRQ,
    SC64_CAN1_IRQ,

    SC64_VDC35_IRQ,

    SC64_SATA_IRQ,

    SC64_I2C0_IRQ,
    SC64_I2C1_IRQ,

    SC64_UART0_IRQ,
    SC64_UART1_IRQ,

    SC64_FASTUART0_IRQ,
    SC64_FASTUART1_IRQ,

    SC64_SPI_IRQ,
    SC64_NAND_IRQ,

    SC64_PORTA2_IRQ,
    SC64_PORTA3_IRQ,
    SC64_PORTB2_IRQ,
    SC64_PORTB3_IRQ,

    SC64_OUTER0_IRQ,
    SC64_OUTER1_IRQ,

    SC64_USB_IRQ,

    SC64_MPU_IRQ,

    SC64_RIO_IRQ,

    SC64_ETH0_IRQ,
    SC64_ETH1_IRQ,

    SC64_IDMA_IRQ,

    SC64_PCI_A_IRQ,
    SC64_PCI_B_IRQ,
    SC64_PCI_C_IRQ,
    SC64_PCI_D_IRQ,

    SC64_PCIE0_A_IRQ,
    SC64_PCIE0_B_IRQ,
    SC64_PCIE0_C_IRQ,
    SC64_PCIE0_D_IRQ,
    SC64_PCIE0_MSIX_IRQ,

    SC64_PCIE1_A_IRQ,
    SC64_PCIE1_B_IRQ,
    SC64_PCIE1_C_IRQ,
    SC64_PCIE1_D_IRQ,
    SC64_PCIE1_MSIX_IRQ,

    SC64_TEMP_IRQ,

    SC64_MAX_IRQ,
};

#define SC64_CAN_NUM 4
#define SC64_CAN_PORT_NUM 4

extern const char *sc64_can_name[];

void sysbus_sccr_register(hwaddr addr);
qemu_irq *sysbus_sccr_v2_register(hwaddr addr, qemu_irq *pic24,
                                                            qemu_irq *pic48);

#endif

