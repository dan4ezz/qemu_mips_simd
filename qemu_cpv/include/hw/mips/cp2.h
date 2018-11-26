#ifndef MIPS_CP2_H
#define MIPS_CP2_H

#include "cpu.h"
#include "qemu/thread.h"

typedef struct CP2State {
    /* Control registers */
    uint64_t control_regs[11];
    void *cp2ptr; /* Inited in CP2 library */
    uint64_t cp2_clock_count; /* Used to call libk128cp2_clock */
    void *dma_ptr;
    uint32_t reg31;
    struct QemuThread thread;
} CP2State;

CP2State *sc64_cp2_register(hwaddr addr, AddressSpace *as);

void cp2_do_work(CP2State *cp2);

bool cp2_fifo_full(CP2State *cp2);
void cp2_ldc2(CP2State *cp2, uint64_t data);
void cp2_reg_write(CP2State *cp2, int reg, uint64_t value);
uint64_t cp2_reg_read(CP2State *cp2, int reg);

static inline void cp2_store_in_buffer(CP2State *cp2, uint32_t val)
{
    cp2->reg31 = val;
}

static inline uint64_t cp2_expand_from_buffer(CP2State *cp2, uint32_t val)
{
    return val | ((uint64_t)cp2->reg31 << 32);
}

static inline bool cp2_reg_is_64bit(CP2State *cp2, int regnum)
{
    return (regnum == 0) || (regnum == 1) || (regnum == 4);
}

enum cp2regtype {
    CP2_SYSREG,
    CP2_FPSYSREG,
    CP2_ARF,
    CP2_GPR,
    CP2_FPR,
    CP2_FIFO,
    CP2_PSTACK_PC,
    CP2_LSTACK_LC,
    CP2_LSTACK_LA
};

uint64_t cp2_get_reg(CP2State *cp2, enum cp2regtype type, uint8_t sect,
    uint8_t num);
uint64_t cp2_iread(CP2State *cp2, uint16_t addr);

#endif /* MIPS_CP2_H */
