#ifndef _SC64_FUART_INTERNAL_H_
#define _SC64_FUART_INTERNAL_H_

#include "hw/sysbus.h"
#include "hw/char/serial.h"
#include "chardev/char-serial.h"

#define FUART_FIFO_SIZE  2048
#define FUART_DMA_REGISTERS_REGION_SIZE 0x68
#define TYPE_SC64_FUART "sc64-fuart"
#define SC64_FUART(obj) OBJECT_CHECK(Sc64FuartState, (obj), TYPE_SC64_FUART)

typedef struct FifoMem {
    uint64_t base;
    uint32_t size;
    uint64_t head_ptr;
    uint64_t tail_ptr;
} FifoMem;

typedef struct FuartTxBuf {
    uint8_t buf[FUART_FIFO_SIZE];
    uint8_t *data;
    uint32_t len;
} FuartTxBuf;

typedef struct Sc64FuartState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    /* Current port number */
    int port_nr;

    SerialState *modem;

    MemoryRegion dma_mem;
    MemoryRegion modem_mem;

    QEMUSerialSetParams settings;
    /* Current levels of RTC, CTS and other */
    uint32_t signals;

    uint64_t dma_regs[FUART_DMA_REGISTERS_REGION_SIZE >> 3];

    FuartTxBuf txb;

    FifoMem rx_mem_fifo;

    QEMUTimer *rx_timer;
    QEMUTimer *mem_fifo_timer;
    QEMUTimer *tx_timer;
    QEMUTimer *cts_timer;
    uint32_t recv_fifo_timeout;
    uint32_t mem_fifo_timeout;
    uint32_t cts_signal_timeout;

    uint64_t cur_tx_descr_addr;

    uint32_t requested_irq;

    AddressSpace *dma_as;

    qemu_irq irq;
} Sc64FuartState;

void fuart_dma_receive(void *opaque, const uint8_t *buf, int size);
int fuart_dma_can_receive(void *opaque);

void fuart_modem_start_xmit(Sc64FuartState *s);
void fuart_modem_update_tiocm(Sc64FuartState *s);

#define SC64_FUART_PORTS_NR 2

extern Sc64FuartState *sc64_fuart_ports[SC64_FUART_PORTS_NR];

/* Defines from hw/char/serial.c */

#define UART_LCR_DLAB   0x80    /* Divisor latch access bit */

#define UART_IER_MSI    0x08    /* Enable Modem status interrupt */
#define UART_IER_RLSI   0x04    /* Enable receiver line status interrupt */
#define UART_IER_THRI   0x02    /* Enable Transmitter holding register int. */
#define UART_IER_RDI    0x01    /* Enable receiver data interrupt */

#define UART_IIR_NO_INT 0x01    /* No interrupts pending */
#define UART_IIR_ID 0x06    /* Mask for the interrupt ID */

#define UART_IIR_MSI    0x00    /* Modem status interrupt */
#define UART_IIR_THRI   0x02    /* Transmitter holding register empty */
#define UART_IIR_RDI    0x04    /* Receiver data interrupt */
#define UART_IIR_RLSI   0x06    /* Receiver line status interrupt */
#define UART_IIR_CTI    0x0C    /* Character Timeout Indication */

#define UART_IIR_FENF   0x80    /* Fifo enabled, but not functionning */
#define UART_IIR_FE     0xC0    /* Fifo enabled */

/*
 * These are the definitions for the Modem Control Register
 */
#define UART_MCR_LOOP   0x10    /* Enable loopback test mode */
#define UART_MCR_OUT2   0x08    /* Out2 complement */
#define UART_MCR_OUT1   0x04    /* Out1 complement */
#define UART_MCR_RTS    0x02    /* RTS complement */
#define UART_MCR_DTR    0x01    /* DTR complement */

/*
 * These are the definitions for the Modem Status Register
 */
#define UART_MSR_DCD    0x80    /* Data Carrier Detect */
#define UART_MSR_RI     0x40    /* Ring Indicator */
#define UART_MSR_DSR    0x20    /* Data Set Ready */
#define UART_MSR_CTS    0x10    /* Clear to Send */
#define UART_MSR_DDCD   0x08    /* Delta DCD */
#define UART_MSR_TERI   0x04    /* Trailing edge ring indicator */
#define UART_MSR_DDSR   0x02    /* Delta DSR */
#define UART_MSR_DCTS   0x01    /* Delta CTS */
#define UART_MSR_ANY_DELTA 0x0F /* Any of the delta bits! */

#define UART_LSR_TEMT   0x40    /* Transmitter empty */
#define UART_LSR_THRE   0x20    /* Transmit-hold-register empty */
#define UART_LSR_BI     0x10    /* Break interrupt indicator */
#define UART_LSR_FE     0x08    /* Frame error indicator */
#define UART_LSR_PE     0x04    /* Parity error indicator */
#define UART_LSR_OE     0x02    /* Overrun error indicator */
#define UART_LSR_DR     0x01    /* Receiver data ready */
#define UART_LSR_INT_ANY 0x1E   /* Any of the lsr-interrupt-triggering
                                 * status bits */

/* Interrupt trigger levels. The byte-counts are for 16550A -
 * in newer UARTs the byte-count for each ITL is higher. */

#define UART_FCR_ITL_1      0x00 /* 1 byte ITL */
#define UART_FCR_ITL_2      0x40 /* 4 bytes ITL */
#define UART_FCR_ITL_3      0x80 /* 8 bytes ITL */
#define UART_FCR_ITL_4      0xC0 /* 14 bytes ITL */

#define UART_FCR_DMS        0x08    /* DMA Mode Select */
#define UART_FCR_XFR        0x04    /* XMIT Fifo Reset */
#define UART_FCR_RFR        0x02    /* RCVR Fifo Reset */
#define UART_FCR_FE         0x01    /* FIFO Enable */

#endif /* _SC64_FUART_INTERNAL_H_ */
