#include "qemu/osdep.h"
#include "cpu.h"
#include "internal.h"
#include "exec/exec-all.h"
#include "exec/helper-proto.h"
#include "exec/cpu_ldst.h"
#include "qemu/bitops.h"
#include "qemu/main-loop.h"
#include "qemu/log.h"

#include "hw/mips/cp2.h"

#define K128CP2_REG_FIFO       0
#define K128CP2_REG_COMM       1
#define K128CP2_REG_CONTROL    2
#define K128CP2_REG_STATUS     3
#define K128CP2_REG_INSTR      4
#define K128CP2_REG_STOPCODE   5

#define rt(i)        ((uint16_t) ((i >> 16) & 0x01f))
#define rd(i)        ((uint16_t) ((i >> 11) & 0x01f))
#define cp2_tf(i)    ((uint16_t) ((i >> 16) & 0x01))
#define cp2_br_cc(i) ((uint16_t) ((i >> 18) & 0x07))

#include "k128cp2.h"
#define MSG_LOG_WRITE(str, ...) \
    do { \
        if (unlikely(qemu_loglevel_mask(CPU_LOG_CP2))) { \
            char str_buf[1024]; \
            sprintf(str_buf, "cp2-helper: " str , ## __VA_ARGS__); \
            libk128cp2_print(cp2->cp2ptr, str_buf); \
        } \
    } while (0)

void helper_cp2_mtc2(CPUMIPSState *env, uint32_t opcode, uint32_t dtmc,
    uint32_t pc)
{
    uint64_t rt_reg;
    int rd_num;
    CP2State *cp2 = env->cp2;

    qemu_mutex_lock_iothread();

    cp2_do_work(cp2);

    rd_num = rd(opcode);
    rt_reg = env->active_tc.gpr[rt(opcode)];

    switch (rd_num) {
    case 31:
        cp2_store_in_buffer(cp2, rt_reg);
        break;
    case K128CP2_REG_FIFO:
    case K128CP2_REG_COMM:
    case K128CP2_REG_CONTROL:
        if (!dtmc && cp2_reg_is_64bit(cp2, rd_num)) {
            rt_reg = cp2_expand_from_buffer(cp2, rt_reg);
        }
        cp2_reg_write(cp2, rd_num, rt_reg);
        break;
    default:
        break;
    }

    MSG_LOG_WRITE("mtc2 rt=%d, rd=%d", rt(opcode), rd(opcode));

    /* This cp2_do_work is required for some baremetal tests,
     * because they check RUN flag immediately after mtc2 execution. */
    cp2_do_work(cp2);

    qemu_mutex_unlock_iothread();
}

void helper_cp2_mfc2(CPUMIPSState *env, uint32_t opcode, uint32_t dmfc,
    uint32_t pc)
{
    uint64_t data;
    int rt_num, rd_num, regnum;
    CP2State *cp2 = env->cp2;
    const uint64_t iram_size = (64 * 1024) / sizeof(uint64_t); /* in qwords */

    qemu_mutex_lock_iothread();

    cp2_do_work(cp2);

    rt_num = rt(opcode);
    rd_num = rd(opcode);
    regnum = rd_num & 0x0F;

    switch (regnum) {
    case K128CP2_REG_COMM:
    case K128CP2_REG_CONTROL:
    case K128CP2_REG_STATUS:
    case K128CP2_REG_STOPCODE:
        data = cp2_reg_read(cp2, regnum);
        break;
    case K128CP2_REG_INSTR:
    default:
        regnum = 4;
        /* CP2 PC */
        data = cp2_get_reg(cp2, CP2_SYSREG, 0, 0 /* = CTRLREG_PC */);
        data &= iram_size - 1;
        data = cp2_iread(cp2, data); /* read iram at PC address (=0,1,2,...) */
        break;
    }

    if (!dmfc) {
        if (rd_num & 0x10) {
            data = ((int64_t)data) >> 32;
        } else {
            data = (int64_t)((int32_t)data);
        }
    }
    env->active_tc.gpr[rt_num] = data;

    MSG_LOG_WRITE("mfc2 rt=%d, rd=%d, data=%lx", rt_num, rd_num, data);
    qemu_mutex_unlock_iothread();
}

void helper_cp2_ldc2(CPUMIPSState *env, uint32_t opcode, uint64_t value,
    uint32_t ldc2)
{
    CP2State *cp2 = env->cp2;
    int rt_num;

    rt_num = rt(opcode);
    if ((rt_num != 0) && (rt_num != 31)) {
        return;
    }

    qemu_mutex_lock_iothread();

    cp2_do_work(cp2);

    switch (rt_num) {
    case 31:
        cp2_store_in_buffer(cp2, value);
        break;
    case K128CP2_REG_FIFO:
        if (!ldc2 && cp2_reg_is_64bit(cp2, rt_num)) {
            value = cp2_expand_from_buffer(cp2, value);
        }
        cp2_ldc2(cp2, value);
        break;
    default:
        break;
    }

    MSG_LOG_WRITE("ldc2 rt=%d, value=%lx", rt_num, value);
    qemu_mutex_unlock_iothread();
}

target_ulong helper_cp2_bc2(CPUMIPSState *env, uint32_t opcode)
{
    CP2State *cp2 = env->cp2;
    int cc;
    bool c;

    qemu_mutex_lock_iothread();

    cp2_do_work(cp2);

    cc = cp2_br_cc(opcode);
    c = (cp2_fifo_full(cp2) >> cc) & 0x1;

    if (!cp2_tf(opcode)) {
        c = !c;
    }

    MSG_LOG_WRITE("bc2");
    qemu_mutex_unlock_iothread();

    return c;
}
