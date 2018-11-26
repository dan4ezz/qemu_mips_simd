#include "qemu/osdep.h"
#include "cpu.h"
#include "internal.h"
#include "exec/exec-all.h"
#include "exec/helper-proto.h"
#include "exec/cpu_ldst.h"
#include "qemu/bitops.h"

#include "cpv.h"

static const uint32_t ONE32 = 0x3F800000;
static const uint64_t ONE64 = 0x3ff0000000000000ULL;

#define MANTISSA_BITS32   23
#define EXP_BITS32        8
#define EXP_OFFSET32      127

#define MANTISSA_BITS64   52
#define EXP_BITS64        11
#define EXP_OFFSET64      1023

#define BIT64(nr)               ((uint64_t)1 << (nr))
#define BIT32(nr)               ((uint32_t)1 << (nr))

#define MDMX2_PFT(v)            (env->cpv[MDMX2_FT(v)])
#define MDMX2_PFS(v)            (env->cpv[MDMX2_FS(v)])
#define MDMX2_PFD(v)            (env->cpv[MDMX2_FD(v)])
/* vld/vsd */
#define MDMX_PFD(v)             MDMX2_PFD(v)

#define VSCR_W  (1 << 15)

#define CPV_DEBUG(FMT, ...)                                                 \
    do {                                                                    \
        if (unlikely(qemu_loglevel_mask(CPU_LOG_CPV))) {                    \
            char *instr_str = (char *)__func__ + strlen("helper_cpv_");     \
            qemu_log("%s: " FMT "\n", instr_str, ## __VA_ARGS__);           \
        }                                                                   \
    } while (0)

/* a1 <OP> a2 */
#define CPV_BINOP(DEST, BITS, OP, a1, a2)                                   \
    do {                                                                    \
        float_status *status = &env->active_tc.cpv_fp_status;               \
                                                                            \
        set_float_exception_flags(0, status);                               \
        DEST = float ## BITS ## _ ## OP(a1, a2, status);                    \
    } while (0)

/* a1 * a2 + NEGATE * a3 * a4 */
#define CPV_MULADD2(DEST, BITS, a1, a2, NEGATE, a3, a4)                     \
    do {                                                                    \
        float_status *status = &env->active_tc.cpv_fp_status;               \
                                                                            \
        set_float_exception_flags(0, status);                               \
        DEST = float ## BITS ## _ ## mul(a1, a2, status);                   \
        DEST = float ## BITS ## _muladd(a3, a4, DEST,                       \
                    NEGATE < 0 ? float_muladd_negate_product : 0, status);  \
    } while (0)

/* a1 + NEGATE1 * a2 * a3 + NEGATE2 * a4 * a5 */
#define CPV_MULADD3(DEST, BITS, a1, NEGATE1, a2, a3, NEGATE2, a4, a5)       \
    do {                                                                    \
        float_status *status = &env->active_tc.cpv_fp_status;               \
                                                                            \
        set_float_exception_flags(0, status);                               \
        DEST = float ## BITS ## _muladd(a2, a3, a1,                         \
                    NEGATE1 < 0 ? float_muladd_negate_product : 0, status); \
        DEST = float ## BITS ## _muladd(a4, a5, DEST,                       \
                    NEGATE2 < 0 ? float_muladd_negate_product : 0, status); \
    } while (0)

#define SWAP(val1, val2)                \
    do {                                \
        typeof(val1) swap_tmp = val2;   \
        val2 = val1;                    \
        val1 = swap_tmp;                \
    } while (0);

static void vcsr_set_vcc(CPUMIPSState *env, int i, int cond)
{
    if (cond) {
        env->vcsr |= BIT32(24 + i);
    } else {
        env->vcsr &= ~BIT32(24 + i);
    }
}

static bool vcsr_get_vcc(CPUMIPSState *env, int i)
{
    return ((env->vcsr & BIT32(24 + i)) != 0);
}

static inline bool
cpv_condition(uint32_t opcode, bool less, bool equal, bool unordered)
{
    bool gt = !(less || equal || unordered);
    return (cpv_cond_bit(3, opcode) && gt)
            || (cpv_cond_bit(2, opcode) && less)
            || (cpv_cond_bit(1, opcode) && equal)
            || (cpv_cond_bit(0, opcode) && unordered);
}

static void cpv_reg_store(CPUMIPSState *env, uint32_t reg, wr_t *val)
{
    wr_t *regp = &(env->cpv[reg]);
    *regp = *val;
    if (unlikely(qemu_loglevel_mask(CPU_LOG_CPV))) {
        qemu_log("Reg write CPV[%i] = %016lx%016lx\n", reg, val->d[1], val->d[0]);
    }
}

static void cpv_control_store(CPUMIPSState *env, uint32_t reg, uint32_t val)
{
    switch (reg) {
    case CPV_VCSR:
        env->vcsr = val;
        break;
    default:
        qemu_log("ERROR: cpv_control_store (reg = %x)\n", reg);
        break;
    }
}

static uint64_t calc_recip64(CPUMIPSState *env, uint64_t val)
{
    float_status *status = &env->active_tc.cpv_fp_status;
    return float64_div(ONE64, val, status);
}

static uint64_t calc_recip32(CPUMIPSState *env, uint64_t val)
{
    float_status *status = &env->active_tc.cpv_fp_status;
    return float32_div(ONE32, val, status);
}

static uint64_t calc_rsqrt64(CPUMIPSState *env, uint64_t val)
{
    float_status *status = &env->active_tc.cpv_fp_status;
    uint64_t sqrt = float64_sqrt(val, status);
    return float64_div(ONE64, sqrt, status);
}

static uint64_t calc_rsqrt32(CPUMIPSState *env, uint64_t val)
{
    float_status *status = &env->active_tc.cpv_fp_status;
    uint64_t sqrt = float32_sqrt(val, status);
    return float32_div(ONE32, sqrt, status);
}

static inline bool is_width_128(CPUMIPSState *env)
{
    return !(env->vcsr & VSCR_W);
}

static bool is_double_precision(CPUMIPSState *env, uint32_t opcode)
{
    uint8_t sd = MDMX2_SD(opcode);
    return is_width_128(env) && sd;
}

static bool is_double_precision2(CPUMIPSState *env, uint32_t opcode)
{
    uint32_t sd = opcode & (1 << 19);
    return is_width_128(env) && sd;
}

static inline int sections_64(CPUMIPSState *env)
{
    return is_width_128(env) ? 2 : 1;
}

void helper_cpv_vcmp(CPUMIPSState *env, uint32_t opcode)
{
    bool less, equal, unordered;
    int32_t *wd, *ws;
    int i;
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    int vcc_index = cpv_cc2(opcode) * 4;
    float_status *status = &env->active_tc.cpv_fp_status;

    CPV_DEBUG("opcode = %x, pfd.d[0] = %lx, pfs.d[0] = %lx, "
                "pfd.d[1] = %lx, pfs.d[1] = %lx",
                opcode, pfd.d[0], pfs.d[0], pfd.d[1], pfs.d[1]);

    less = false;
    equal = false;
    unordered = false;

    if (is_double_precision2(env, opcode)) {
        /* LO */
        if (float64_is_any_nan(pfd.d[0]) || float64_is_any_nan(pfs.d[0])) {
            unordered = true;
        } else {
            less  = float64_lt(pfs.d[0], pfd.d[0], status);
            equal = float64_eq(pfs.d[0], pfd.d[0], status);
        }
        vcsr_set_vcc(env, vcc_index,
            cpv_condition(opcode, less, equal, unordered));

        /* HI */
        if (float64_is_any_nan(pfd.d[1]) || float64_is_any_nan(pfs.d[1])) {
            unordered = true;
        } else {
            less  = float64_lt(pfs.d[1], pfd.d[1], status);
            equal = float64_eq(pfs.d[1], pfd.d[1], status);
        }
        vcsr_set_vcc(env, vcc_index + 1,
            cpv_condition(opcode, less, equal, unordered));
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i];

            /* LO */
            if (float32_is_any_nan(wd[0]) || float32_is_any_nan(ws[0])) {
                unordered = true;
            } else {
                less  = float32_lt(ws[0], wd[0], status);
                equal = float32_eq(ws[0], wd[0], status);
            }
            vcsr_set_vcc(env, vcc_index + 2 * i,
                cpv_condition(opcode, less, equal, unordered));

            /* HI */
            if (float32_is_any_nan(wd[1]) || float32_is_any_nan(wd[1])) {
                unordered = true;
            } else {
                less  = float32_lt(ws[1], wd[1], status);
                equal = float32_eq(ws[1], wd[1], status);
            }
            vcsr_set_vcc(env, vcc_index + 2 * i + 1,
                cpv_condition(opcode, less, equal, unordered));
        }
    }
}

target_ulong helper_cpv_vbt(CPUMIPSState *env, uint32_t opcode)
{
    CPV_DEBUG("");
    uint8_t cc = cpv_br_cc(opcode);
    return vcsr_get_vcc(env, cc);
}

target_ulong helper_cpv_vbf(CPUMIPSState *env, uint32_t opcode)
{
    CPV_DEBUG("");
    uint8_t cc = cpv_br_cc(opcode);
    return !vcsr_get_vcc(env, cc);
}

void helper_cpv_vadd(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_BINOP(pfd.d[1], 64, add, pfs.d[1], pft.d[1]);
        CPV_BINOP(pfd.d[0], 64, add, pfs.d[0], pft.d[0]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];

            CPV_BINOP(wd[1], 32, add, ws[1], wt[1]);
            CPV_BINOP(wd[0], 32, add, ws[0], wt[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vsub(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_BINOP(pfd.d[1], 64, sub, pfs.d[1], pft.d[1]);
        CPV_BINOP(pfd.d[0], 64, sub, pfs.d[0], pft.d[0]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];

            CPV_BINOP(wd[1], 32, sub, ws[1], wt[1]);
            CPV_BINOP(wd[0], 32, sub, ws[0], wt[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_cmagsq2(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD2(pfd.d[1], 64, pfs.d[0], pfs.d[0], 1, pfs.d[1], pfs.d[1]);
        CPV_MULADD2(pfd.d[0], 64, pft.d[1], pft.d[1], 1, pft.d[0], pft.d[0]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD2(wd[1], 32, ws[0], ws[0], 1, ws[1], ws[1]);
            CPV_MULADD2(wd[0], 32, wt[1], wt[1], 1, wt[0], wt[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_chmul(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD2(pfd.d[1], 64, pfs.d[1], pft.d[1], 1, pfs.d[0], pft.d[0]);
        CPV_MULADD2(pfd.d[0], 64, pfs.d[0], pft.d[1], -1, pfs.d[1], pft.d[0]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD2(wd[1], 32, ws[1], wt[1], 1, ws[0], wt[0]);
            CPV_MULADD2(wd[0], 32, ws[0], wt[1], -1, ws[1], wt[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_cmul(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD2(pfd.d[1], 64, pfs.d[1], pft.d[1], -1, pfs.d[0], pft.d[0]);
        CPV_MULADD2(pfd.d[0], 64, pfs.d[1], pft.d[0], 1, pfs.d[0], pft.d[1]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD2(wd[1], 32, ws[1], wt[1], -1, ws[0], wt[0]);
            CPV_MULADD2(wd[0], 32, ws[1], wt[0], 1, ws[0], wt[1]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_chmadd(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD3(pfd.d[1], 64, pfd.d[1], 1, pfs.d[1], pft.d[1], 1, pfs.d[0], pft.d[0]);
        CPV_MULADD3(pfd.d[0], 64, pfd.d[0], -1, pfs.d[1], pft.d[0], 1, pfs.d[0], pft.d[1]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD3(wd[1], 32, wd[1], 1, ws[1], wt[1], 1, ws[0], wt[0]);
            CPV_MULADD3(wd[0], 32, wd[0], -1, ws[1], wt[0], 1, ws[0], wt[1]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_chmsub(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD3(pfd.d[1], 64, pfd.d[1], -1, pfs.d[1], pft.d[1], -1, pfs.d[0], pft.d[0]);
        CPV_MULADD3(pfd.d[0], 64, pfd.d[0], 1, pfs.d[1], pft.d[0], -1, pfs.d[0], pft.d[1]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD3(wd[1], 32, wd[1], -1, ws[1], wt[1], -1, ws[0], wt[0]);
            CPV_MULADD3(wd[0], 32, wd[0], 1, ws[1], wt[0], -1, ws[0], wt[1]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_cmadd(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD3(pfd.d[1], 64, pfd.d[1], 1, pfs.d[1], pft.d[1], -1, pfs.d[0], pft.d[0]);
        CPV_MULADD3(pfd.d[0], 64, pfd.d[0], 1, pfs.d[1], pft.d[0], 1, pfs.d[0], pft.d[1]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD3(wd[1], 32, wd[1], 1, ws[1], wt[1], -1, ws[0], wt[0]);
            CPV_MULADD3(wd[0], 32, wd[0], 1, ws[1], wt[0], 1, ws[0], wt[1]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_cmsub(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD3(pfd.d[1], 64, pfd.d[1], -1, pfs.d[1], pft.d[1], 1, pfs.d[0], pft.d[0]);
        CPV_MULADD3(pfd.d[0], 64, pfd.d[0], -1, pfs.d[1], pft.d[0], -1, pfs.d[0], pft.d[1]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD3(wd[1], 32, wd[1], -1, ws[1], wt[1], 1, ws[0], wt[0]);
            CPV_MULADD3(wd[0], 32, wd[0], -1, ws[1], wt[1], -1, ws[0], wt[1]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_mtvmul(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *ws1, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    wr_t pfs1 = env->cpv[MDMX2_FS(opcode) + 1];

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD2(pfd.d[1], 64, pfs.d[1], pft.d[1], 1, pfs1.d[1], pft.d[0]);
        CPV_MULADD2(pfd.d[0], 64, pfs1.d[0], pft.d[0], 1, pfs.d[0], pft.d[1]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; ws1 = &pfs1.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD2(wd[1], 32, ws[1], wt[1], 1, ws1[1], wt[0]);
            CPV_MULADD2(wd[0], 32, ws1[0], wt[0], 1, ws[0], wt[1]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_mvmul(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *ws1, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    wr_t pfs1 = env->cpv[MDMX2_FS(opcode) + 1];

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD2(pfd.d[1], 64, pfs.d[1], pft.d[1], 1, pfs.d[0], pft.d[0]);
        CPV_MULADD2(pfd.d[0], 64, pfs1.d[0], pft.d[0], 1, pfs1.d[1], pft.d[1]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; ws1 = &pfs1.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD2(wd[1], 32, ws[1], wt[1], 1, ws[0], wt[0]);
            CPV_MULADD2(wd[0], 32, ws1[0], wt[0], 1, ws1[1], wt[1]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vaddsub(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *wd1, *ws, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfd1 = env->cpv[MDMX2_FD(opcode) + 1];
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_BINOP(pfd.d[1], 64, add, pfs.d[1], pft.d[1]);
        CPV_BINOP(pfd.d[0], 64, add, pfs.d[0], pft.d[0]);

        CPV_BINOP(pfd1.d[1], 64, sub, pfs.d[1], pft.d[1]);
        CPV_BINOP(pfd1.d[0], 64, sub, pfs.d[0], pft.d[0]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; wd1 = &pfd1.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];

            CPV_BINOP(wd[1], 32, add, ws[1], wt[1]);
            CPV_BINOP(wd[0], 32, add, ws[0], wt[0]);

            CPV_BINOP(wd1[1], 32, sub, ws[1], wt[1]);
            CPV_BINOP(wd1[0], 32, sub, ws[0], wt[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
    cpv_reg_store(env, MDMX2_FD(opcode) + 1, &pfd1);
}

void helper_cpv_vmadd(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD2(pfd.d[1], 64, pfd.d[1], ONE64, 1, pfs.d[1], pft.d[1]);
        CPV_MULADD2(pfd.d[0], 64, pfd.d[0], ONE64, 1, pfs.d[0], pft.d[0]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD2(wd[1], 32, wd[1], ONE32, 1, ws[1], wt[1]);
            CPV_MULADD2(wd[0], 32, wd[0], ONE32, 1, ws[0], wt[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vmsub(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD2(pfd.d[1], 64, pfd.d[1], ONE64, -1, pfs.d[1], pft.d[1]);
        CPV_MULADD2(pfd.d[0], 64, pfd.d[0], ONE64, -1, pfs.d[0], pft.d[0]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD2(wd[1], 32, wd[1], ONE32, -1, ws[1], wt[1]);
            CPV_MULADD2(wd[0], 32, wd[0], ONE32, -1, ws[0], wt[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vmul(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_BINOP(pfd.d[1], 64, mul, pfs.d[1], pft.d[1]);
        CPV_BINOP(pfd.d[0], 64, mul, pfs.d[0], pft.d[0]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];

            CPV_BINOP(wd[1], 32, mul, ws[1], wt[1]);
            CPV_BINOP(wd[0], 32, mul, ws[0], wt[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_cmaddsub(CPUMIPSState *env, uint32_t opcode)
{
    int i;
    wr_t res, res1;
    int32_t *wd, *ws, *wt, *w_res, *w_res1;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    res = pfd;
    res1 = pfs;

    if (is_double_precision(env, opcode)) {
        CPV_MULADD3(res.d[1], 64, pfd.d[1], 1, pfs.d[1], pft.d[1], -1, pfs.d[0], pft.d[0]);
        CPV_MULADD3(res.d[0], 64, pfd.d[0], 1, pfs.d[1], pft.d[0], 1, pfs.d[0], pft.d[1]);

        CPV_MULADD3(res1.d[1], 64, pfd.d[1], -1, pfs.d[1], pft.d[1], 1, pfs.d[0], pft.d[0]);
        CPV_MULADD3(res1.d[0], 64, pfd.d[0], -1, pfs.d[1], pft.d[0], -1, pfs.d[0], pft.d[1]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];
            w_res = &res.w[2 * i]; w_res1 = &res1.w[2 * i];

            CPV_MULADD3(w_res[1], 32, wd[1], 1, ws[1], wt[1], -1, ws[0], wt[0]);
            CPV_MULADD3(w_res[0], 32, wd[0], 1, ws[1], wt[0], 1, ws[0], wt[1]);

            CPV_MULADD3(w_res1[1], 32, wd[1], -1, ws[1], wt[1], 1, ws[0], wt[0]);
            CPV_MULADD3(w_res1[0], 32, wd[0], -1, ws[1], wt[0], -1, ws[0], wt[1]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &res);
    cpv_reg_store(env, MDMX2_FS(opcode), &res1);
}

void helper_cpv_vmaddsub(CPUMIPSState *env, uint32_t opcode)
{
    int i;
    int32_t *wd, *ws, *wt, *w_res, *w_res1;
    wr_t res, res1;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);

    CPV_DEBUG("");

    res = pfd;
    res1 = pfs;

    if (is_double_precision(env, opcode)) {
        CPV_MULADD2(res.d[1], 64, pfd.d[1], ONE64, 1, pfs.d[1], pft.d[1]);
        CPV_MULADD2(res.d[0], 64, pfd.d[0], ONE64, 1, pfs.d[0], pft.d[0]);

        CPV_MULADD2(res1.d[1], 64, pfd.d[1], ONE64, -1, pfs.d[1], pft.d[1]);
        CPV_MULADD2(res1.d[0], 64, pfd.d[0], ONE64, -1, pfs.d[0], pft.d[0]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];
            w_res = &res.w[2 * i]; w_res1 = &res1.w[2 * i];

            CPV_MULADD2(w_res[1], 32, wd[1], ONE32, 1, ws[1], wt[1]);
            CPV_MULADD2(w_res[0], 32, wd[0], ONE32, 1, ws[0], wt[0]);

            CPV_MULADD2(w_res1[1], 32, wd[1], ONE32, -1, ws[1], wt[1]);
            CPV_MULADD2(w_res1[0], 32, wd[0], ONE32, -1, ws[0], wt[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &res);
    cpv_reg_store(env, MDMX2_FS(opcode), &res1);
}

void helper_cpv_vrecip(CPUMIPSState *env, uint32_t opcode)
{
    int i;
    int32_t *wd, *ws;
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    int var = cpv_var_4_3(opcode);

    CPV_DEBUG("");

    if (is_double_precision2(env, opcode)) {
        switch (var) {
        case 0:
            pfd.d[0] = calc_recip64(env, pfs.d[0]);
            break;
        case 1:
            pfd.d[1] = calc_recip64(env, pfs.d[0]);
            break;
        case 2:
            pfd.d[1] = calc_recip64(env, pfs.d[1]);
            break;
        case 3:
            pfd.d[0] = calc_recip64(env, pfs.d[1]);
            break;
        case 4:
            pfd.d[0] = calc_recip64(env, pfs.d[1]);
            pfd.d[1] = calc_recip64(env, pfs.d[1]);
            break;
        case 5:
            pfd.d[0] = calc_recip64(env, pfs.d[0]);
            pfd.d[1] = calc_recip64(env, pfs.d[1]);
            break;
        case 6:
            pfd.d[0] = calc_recip64(env, pfs.d[1]);
            pfd.d[1] = calc_recip64(env, pfs.d[0]);
            break;
        case 7:
            pfd.d[0] = calc_recip64(env, pfs.d[0]);
            pfd.d[1] = calc_recip64(env, pfs.d[0]);
            break;
        }
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i];

            switch (var) {
            case 0:
                wd[0] = calc_recip32(env, ws[0]);
                break;
            case 1:
                wd[1] = calc_recip32(env, ws[0]);
                break;
            case 2:
                wd[1] = calc_recip32(env, ws[1]);
                break;
            case 3:
                wd[0] = calc_recip32(env, ws[1]);
                break;
            case 4:
                wd[0] = calc_recip32(env, ws[1]);
                wd[1] = calc_recip32(env, ws[1]);
                break;
            case 5:
                wd[0] = calc_recip32(env, ws[0]);
                wd[1] = calc_recip32(env, ws[1]);
                break;
            case 6:
                wd[0] = calc_recip32(env, ws[1]);
                wd[1] = calc_recip32(env, ws[0]);
                break;
            case 7:
                wd[0] = calc_recip32(env, ws[0]);
                wd[1] = calc_recip32(env, ws[0]);
                break;
            }
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vrsqrt(CPUMIPSState *env, uint32_t opcode)
{
    int i;
    int32_t *wd, *ws;
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    int var = cpv_var_4_3(opcode);

    CPV_DEBUG("");

    if (is_double_precision2(env, opcode)) {
        switch (var) {
        case 0:
            pfd.d[0] = calc_rsqrt64(env, pfs.d[0]);
            break;
        case 1:
            pfd.d[1] = calc_rsqrt64(env, pfs.d[0]);
            break;
        case 2:
            pfd.d[1] = calc_rsqrt64(env, pfs.d[1]);
            break;
        case 3:
            pfd.d[0] = calc_rsqrt64(env, pfs.d[1]);
            break;
        case 4:
            pfd.d[0] = calc_rsqrt64(env, pfs.d[1]);
            pfd.d[1] = calc_rsqrt64(env, pfs.d[1]);
            break;
        case 5:
            pfd.d[0] = calc_rsqrt64(env, pfs.d[0]);
            pfd.d[1] = calc_rsqrt64(env, pfs.d[1]);
            break;
        case 6:
            pfd.d[0] = calc_rsqrt64(env, pfs.d[1]);
            pfd.d[1] = calc_rsqrt64(env, pfs.d[0]);
            break;
        case 7:
            pfd.d[0] = calc_rsqrt64(env, pfs.d[0]);
            pfd.d[1] = calc_rsqrt64(env, pfs.d[0]);
            break;
        }
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i];

            switch (var) {
            case 0:
                wd[0] = calc_rsqrt32(env, ws[0]);
                break;
            case 1:
                wd[1] = calc_rsqrt32(env, ws[0]);
                break;
            case 2:
                wd[1] = calc_rsqrt32(env, ws[1]);
                break;
            case 3:
                wd[0] = calc_rsqrt32(env, ws[1]);
                break;
            case 4:
                wd[0] = calc_rsqrt32(env, ws[1]);
                wd[1] = calc_rsqrt32(env, ws[1]);
                break;
            case 5:
                wd[0] = calc_rsqrt32(env, ws[0]);
                wd[1] = calc_rsqrt32(env, ws[1]);
                break;
            case 6:
                wd[0] = calc_rsqrt32(env, ws[1]);
                wd[1] = calc_rsqrt32(env, ws[0]);
                break;
            case 7:
                wd[0] = calc_rsqrt32(env, ws[0]);
                wd[1] = calc_rsqrt32(env, ws[0]);
                break;
            }
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_prm(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *ws1;
    int i;
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    wr_t pfs1 = env->cpv[MDMX2_FS(opcode) + 1];

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        switch (cpv_prmH(opcode)) {
        case 0x0:
            pfd.d[1] = pfs.d[0];
            break;
        case 0x1:
            pfd.d[1] = pfs.d[1];
            break;
        case 0x2:
            pfd.d[1] = pfs1.d[0];
            break;
        case 0x3:
            pfd.d[1] = pfs1.d[1];
            break;
        }
        if (cpv_sH(opcode)) {
            pfd.d[1] ^= BIT64(63);
        }

        switch (cpv_prmL(opcode)) {
        case 0x0:
            pfd.d[0] = pfs.d[0];
            break;
        case 0x1:
            pfd.d[0] = pfs.d[1];
            break;
        case 0x2:
            pfd.d[0] = pfs1.d[0];
            break;
        case 0x3:
            pfd.d[0] = pfs1.d[1];
            break;
        }
        if (cpv_sL(opcode)) {
            pfd.d[0] ^= BIT64(63);
        }
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; ws1 = &pfs1.w[2 * i];

            switch (cpv_prmH(opcode)) {
            case 0x0:
                wd[1] = ws[0];
                break;
            case 0x1:
                wd[1] = ws[1];
                break;
            case 0x2:
                wd[1] = ws1[0];
                break;
            case 0x3:
                wd[1] = ws1[1];
                break;
            }
            if (cpv_sH(opcode)) {
                wd[1] ^= BIT32(31);
            }

            switch (cpv_prmL(opcode)) {
            case 0x0:
                wd[0] = ws[0];
                break;
            case 0x1:
                wd[0] = ws[1];
                break;
            case 0x2:
                wd[0] = ws1[0];
                break;
            case 0x3:
                wd[0] = ws1[1];
                break;
            }
            if (cpv_sL(opcode)) {
                wd[0] ^= BIT32(31);
            }
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_mtvmsub(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *ws1, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    wr_t pfs1 = env->cpv[MDMX2_FS(opcode) + 1];

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD3(pfd.d[1], 64, pfd.d[1], -1, pfs.d[1], pft.d[1], -1, pfs1.d[1], pft.d[0]);
        CPV_MULADD3(pfd.d[0], 64, pfd.d[0], -1, pfs1.d[0], pft.d[0], -1, pfs.d[0], pft.d[1]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; ws1 = &pfs1.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD3(wd[1], 32, wd[1], -1, ws[1], wt[1], -1, ws1[1], wt[0]);
            CPV_MULADD3(wd[0], 32, wd[0], -1, ws1[0], wt[0], -1, ws[0], wt[1]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_mtvmadd(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *ws1, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    wr_t pfs1 = env->cpv[MDMX2_FS(opcode) + 1];

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD3(pfd.d[1], 64, pfd.d[1], 1, pfs.d[1], pft.d[1], 1, pfs1.d[1], pft.d[0]);
        CPV_MULADD3(pfd.d[0], 64, pfd.d[0], 1, pfs1.d[0], pft.d[0], 1, pfs.d[0], pft.d[1]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; ws1 = &pfs1.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD3(wd[1], 32, wd[1], 1, ws[1], wt[1], 1, ws1[1], wt[0]);
            CPV_MULADD3(wd[0], 32, wd[0], 1, ws1[0], wt[0], 1, ws[0], wt[1]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_mvmsub(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *ws1, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    wr_t pfs1 = env->cpv[MDMX2_FS(opcode) + 1];

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD3(pfd.d[1], 64, pfd.d[1], -1, pfs.d[1], pft.d[1], -1, pfs.d[0], pft.d[0]);
        CPV_MULADD3(pfd.d[0], 64, pfd.d[0], -1, pfs1.d[0], pft.d[0], -1, pfs1.d[1], pft.d[1]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; ws1 = &pfs1.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD3(wd[1], 32, wd[1], -1, ws[1], wt[1], -1, ws[0], wt[0]);
            CPV_MULADD3(wd[0], 32, wd[0], -1, ws1[0], wt[0], -1, ws1[1], wt[1]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_conv(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *ws1, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    wr_t pfs1 = env->cpv[MDMX2_FS(opcode) + 1];

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD3(pfd.d[1], 64, pfd.d[1], 1, pfs.d[1], pft.d[1], 1, pfs.d[0], pft.d[0]);
        CPV_MULADD3(pfd.d[0], 64, pfd.d[0], 1, pfs1.d[1], pft.d[0], 1, pfs.d[0], pft.d[1]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; ws1 = &pfs1.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD3(wd[1], 32, wd[1], 1, ws[1], wt[1], 1, ws[0], wt[0]);
            CPV_MULADD3(wd[0], 32, wd[0], 1, ws1[1], wt[0], 1, ws[0], wt[1]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_mvmadd(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *ws1, *wt;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    wr_t pfs1 = env->cpv[MDMX2_FS(opcode) + 1];

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_MULADD3(pfd.d[1], 64, pfd.d[1], 1, pfs.d[1], pft.d[1], 1, pfs.d[0], pft.d[0]);
        CPV_MULADD3(pfd.d[0], 64, pfd.d[0], 1, pfs1.d[1], pft.d[1], 1, pfs1.d[0], pft.d[0]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; ws1 = &pfs1.w[2 * i]; wt = &pft.w[2 * i];

            CPV_MULADD3(wd[1], 32, wd[1], 1, ws[1], wt[1], 1, ws[0], wt[0]);
            CPV_MULADD3(wd[0], 32, wd[0], 1, ws1[1], wt[1], 1, ws1[0], wt[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_smul(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd;
    int i;
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    int n = cpv_scalen(opcode);

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        n %= 2;
        CPV_BINOP(pfd.d[1], 64, mul, pfd.d[1], pfs.d[n]);
        CPV_BINOP(pfd.d[0], 64, mul, pfd.d[0], pfs.d[n]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i];
            if (!is_width_128(env)) {
                n %= 2;
            }
            CPV_BINOP(wd[1], 32, mul, wd[1], pfs.w[n]);
            CPV_BINOP(wd[0], 32, mul, wd[0], pfs.w[n]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vmul4(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws, *wt, *wd1, *ws1, *wt1;
    int i;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    wr_t pft1 = env->cpv[MDMX2_FT(opcode) + 1];
    wr_t pfd1 = env->cpv[MDMX2_FD(opcode) + 1];
    wr_t pfs1 = env->cpv[MDMX2_FS(opcode) + 1];

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        CPV_BINOP(pfd.d[1], 64, mul, pfs.d[1], pft.d[1]);
        CPV_BINOP(pfd.d[0], 64, mul, pfs.d[0], pft.d[0]);

        CPV_BINOP(pfd1.d[1], 64, mul, pfs1.d[1], pft1.d[1]);
        CPV_BINOP(pfd1.d[0], 64, mul, pfs1.d[0], pft1.d[0]);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];
            wd1 = &pfd1.w[2 * i]; ws1 = &pfs1.w[2 * i]; wt1 = &pft1.w[2 * i];

            CPV_BINOP(wd[1], 32, mul, ws[1], wt[1]);
            CPV_BINOP(wd[0], 32, mul, ws[0], wt[0]);

            CPV_BINOP(wd1[1], 32, mul, ws1[1], wt1[1]);
            CPV_BINOP(wd1[0], 32, mul, ws1[0], wt1[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
    cpv_reg_store(env, MDMX2_FD(opcode) + 1, &pfd1);
}

void helper_cpv_csd(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd;
    int i;
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    wr_t pfd1 = env->cpv[MDMX2_FD(opcode) + 1];
    wr_t pfs1 = env->cpv[MDMX2_FS(opcode) + 1];
    float_status *status = &env->active_tc.cpv_fp_status;

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        pfd.d[1] = float32_to_float64(pfs.w[1], status);
        pfd.d[0] = float32_to_float64(pfs.w[0], status);
        pfd1.d[1] = float32_to_float64(pfs.w[3], status);
        pfd1.d[0] = float32_to_float64(pfs.w[2], status);

        cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
        cpv_reg_store(env, MDMX2_FD(opcode) + 1, &pfd1);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i];

            wd[1] = float64_to_float32(pfs1.d[i], status);
            wd[0] = float64_to_float32(pfs.d[i], status);

            cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
        }
    }
}

static void chw_first_step(int32_t *fd, int32_t *fdd, int32_t *fs, int var)
{
    uint32_t val32;
    int32_t val64[2];
    uint64_t fs64 = *(uint64_t *)fs;

    if (var & 0x1) {    /* 8 bits */
        val32 = ((var & 0x4) == 0) ? fs[0] : fs[1];
        fd[0]  = (uint8_t) (val32);
        fd[1]  = (uint8_t) (val32 >>  8);
        fdd[0] = (uint8_t) (val32 >> 16);
        fdd[1] = (uint8_t) (val32 >> 24);
        if ((var & 0x8) == 0) { /* expand sign */
            fd[0]  = (int8_t) fd[0];
            fd[1]  = (int8_t) fd[1];
            fdd[0] = (int8_t) fdd[0];
            fdd[1] = (int8_t) fdd[1];
        }
    } else {        /* 16 bits */
        *(uint64_t *) val64 = ((var & 0x2) == 0) ? fs64 :
                        ((fs64 >> 8) & 0x00ff00ff00ff00ffll)
                        | ((fs64 << 8) & 0xff00ff00ff00ff00ll); /* swap bytes */
        fd[0]  = (uint16_t) (val64[0]);
        fd[1]  = (uint16_t) (val64[0] >> 16);
        fdd[0] = (uint16_t) (val64[1]);
        fdd[1] = (uint16_t) (val64[1] >> 16);
        if ((var & 0x8) == 0) { /* expand sign */
            fd[0]  = (int16_t) fd[0];
            fd[1]  = (int16_t) fd[1];
            fdd[0] = (int16_t) fdd[0];
            fdd[1] = (int16_t) fdd[1];
        }
    }
}

void helper_cpv_chw(CPUMIPSState *env, uint32_t opcode)
{
    int32_t td[2];
    int32_t td1[2];
    int32_t *wd, *wd1;
    int i;
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    wr_t pfd1 = env->cpv[MDMX2_FD(opcode) + 1];
    int var = cpv_var_19_4(opcode);
    float_status *status = &env->active_tc.cpv_fp_status;

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        chw_first_step(td, td1, &pfs.w[0], var);

        pfd.d[1] = int32_to_float64(td[1], status);
        pfd.d[0] = int32_to_float64(td[0], status);
        pfd1.d[1] = int32_to_float64(td1[1], status);
        pfd1.d[0] = int32_to_float64(td1[0], status);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; wd1 = &pfd1.w[2 * i];

            chw_first_step(td, td1, &pfs.w[2 * i], var);

            wd[1] = int32_to_float32(td[1], status);
            wd[0] = int32_to_float32(td[0], status);
            wd1[1] = int32_to_float32(td1[1], status);
            wd1[0] = int32_to_float32(td1[0], status);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
    cpv_reg_store(env, MDMX2_FD(opcode) + 1, &pfd1);
}

void helper_cpv_vmax(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws;
    int i;
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    float_status *status = &env->active_tc.cpv_fp_status;

    CPV_DEBUG("");

    if (is_double_precision2(env, opcode)) {
        pfd.d[1] = float64_max(pfd.d[1], pfs.d[1], status);
        pfd.d[0] = float64_max(pfd.d[0], pfs.d[0], status);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i];

            wd[1] = float32_max(wd[1], ws[1], status);
            wd[0] = float32_max(wd[0], ws[0], status);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vmin(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws;
    int i;
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    float_status *status = &env->active_tc.cpv_fp_status;

    CPV_DEBUG("");

    if (is_double_precision2(env, opcode)) {
        pfd.d[1] = float64_min(pfd.d[1], pfs.d[1], status);
        pfd.d[0] = float64_min(pfd.d[0], pfs.d[0], status);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i];

            wd[1] = float32_min(wd[1], ws[1], status);
            wd[0] = float32_min(wd[0], ws[0], status);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vleast(CPUMIPSState *env, uint32_t opcode)
{
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    wr_t pfs1 = env->cpv[MDMX2_FS(opcode) + 1];
    int var = cpv_var_4_3(opcode);
    float_status *status = &env->active_tc.cpv_fp_status;

    CPV_DEBUG("");

    if (is_width_128(env)) {
        if (is_double_precision2(env, opcode)) {
            pfd.d[1] = float64_min(pfs1.d[1], pfs1.d[0], status);
            pfd.d[0] = float64_min(pfs.d[1], pfs.d[0], status);
            if (var & 0x2) {
                SWAP(pfd.d[1], pfd.d[0]);
            }
        } else {
            if (var % 2) {
                pfd.w[3] = float32_min(pfs1.w[3], pfs1.w[1], status);
                pfd.w[2] = float32_min(pfs1.w[2], pfs1.w[0], status);
                pfd.w[1] = float32_min(pfs.w[3], pfs.w[1], status);
                pfd.w[0] = float32_min(pfs.w[2], pfs.w[0], status);
            } else {
                pfd.w[3] = float32_min(pfs1.w[3], pfs1.w[2], status);
                pfd.w[2] = float32_min(pfs1.w[1], pfs1.w[0], status);
                pfd.w[1] = float32_min(pfs.w[3], pfs.w[2], status);
                pfd.w[0] = float32_min(pfs.w[1], pfs.w[0], status);
            }
            if (var & 0x2) {
                SWAP(pfd.w[0], pfd.w[2]);
                SWAP(pfd.w[1], pfd.w[3]);
            }
        }
    } else {
        pfd.w[1] = float32_min(pfs1.w[1], pfs1.w[0], status);
        pfd.w[0] = float32_min(pfs.w[1], pfs.w[0], status);
        if (var & 0x2) {
            SWAP(pfd.w[1], pfd.w[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vmost(CPUMIPSState *env, uint32_t opcode)
{
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    wr_t pfs1 = env->cpv[MDMX2_FS(opcode) + 1];
    int var = cpv_var_4_3(opcode);
    float_status *status = &env->active_tc.cpv_fp_status;

    CPV_DEBUG("");

    if (is_width_128(env)) {
        if (is_double_precision2(env, opcode)) {
            pfd.d[1] = float64_max(pfs1.d[1], pfs1.d[0], status);
            pfd.d[0] = float64_max(pfs.d[1], pfs.d[0], status);
            if (var & 0x2) {
                SWAP(pfd.d[1], pfd.d[0]);
            }
        } else {
            if (var % 2) {
                pfd.w[3] = float32_max(pfs1.w[3], pfs1.w[1], status);
                pfd.w[2] = float32_max(pfs1.w[2], pfs1.w[0], status);
                pfd.w[1] = float32_max(pfs.w[3], pfs.w[1], status);
                pfd.w[0] = float32_max(pfs.w[2], pfs.w[0], status);
            } else {
                pfd.w[3] = float32_max(pfs1.w[3], pfs1.w[2], status);
                pfd.w[2] = float32_max(pfs1.w[1], pfs1.w[0], status);
                pfd.w[1] = float32_max(pfs.w[3], pfs.w[2], status);
                pfd.w[0] = float32_max(pfs.w[1], pfs.w[0], status);
            }
            if (var & 0x2) {
                SWAP(pfd.w[0], pfd.w[2]);
                SWAP(pfd.w[1], pfd.w[3]);
            }
        }
    } else {
        pfd.w[1] = float32_max(pfs1.w[1], pfs1.w[0], status);
        pfd.w[0] = float32_max(pfs.w[1], pfs.w[0], status);
        if (var & 0x2) {
            SWAP(pfd.w[1], pfd.w[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vswap(CPUMIPSState *env, uint32_t opcode)
{
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    int var = cpv_var_5_2(opcode);

    CPV_DEBUG("");

    if (is_width_128(env)) {
        if (is_double_precision2(env, opcode)) {
            pfd.d[1] = pfs.d[1];
            pfd.d[0] = pfs.d[0];
            if (var & 0x2) {
                SWAP(pfd.d[1], pfd.d[0]);
            }
        } else {
            if (var % 2) {
                pfd.w[3] = pfs.w[2];
                pfd.w[2] = pfs.w[3];
                pfd.w[1] = pfs.w[0];
                pfd.w[0] = pfs.w[1];
            } else {
                pfd.w[3] = pfs.w[3];
                pfd.w[2] = pfs.w[2];
                pfd.w[1] = pfs.w[1];
                pfd.w[0] = pfs.w[0];
            }
            if (var & 0x2) {
                SWAP(pfd.w[0], pfd.w[2]);
                SWAP(pfd.w[1], pfd.w[3]);
            }
        }
    } else {
        pfd.w[1] = pfs.w[1];
        pfd.w[0] = pfs.w[0];
        if (var & 0x2) {
            SWAP(pfd.w[1], pfd.w[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vsum(CPUMIPSState *env, uint32_t opcode)
{
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    wr_t pfs1 = env->cpv[MDMX2_FS(opcode) + 1];
    int var = cpv_var_19_4(opcode);

    CPV_DEBUG("");

    if (is_width_128(env)) {
        if (is_double_precision(env, opcode)) {
            CPV_BINOP(pfd.d[1], 64, add, pfs1.d[1], pfs1.d[0]);
            CPV_BINOP(pfd.d[0], 64, add, pfs.d[1], pfs.d[0]);
            if (var & 0x2) {
                SWAP(pfd.d[1], pfd.d[0]);
            }
        } else {
            if (var % 2) {
                CPV_BINOP(pfd.w[3], 32, add, pfs1.w[3], pfs1.w[1]);
                CPV_BINOP(pfd.w[2], 32, add, pfs1.w[2], pfs1.w[0]);
                CPV_BINOP(pfd.w[1], 32, add, pfs.w[3], pfs.w[1]);
                CPV_BINOP(pfd.w[0], 32, add, pfs.w[2], pfs.w[0]);
            } else {
                CPV_BINOP(pfd.w[3], 32, add, pfs1.w[3], pfs1.w[2]);
                CPV_BINOP(pfd.w[2], 32, add, pfs1.w[1], pfs1.w[0]);
                CPV_BINOP(pfd.w[1], 32, add, pfs.w[3], pfs.w[2]);
                CPV_BINOP(pfd.w[0], 32, add, pfs.w[1], pfs.w[0]);
            }
            if (var & 0x2) {
                SWAP(pfd.w[0], pfd.w[2]);
                SWAP(pfd.w[1], pfd.w[3]);
            }
        }
    } else {
        CPV_BINOP(pfd.w[1], 32, add, pfs1.w[1], pfs1.w[0]);
        CPV_BINOP(pfd.w[0], 32, add, pfs.w[1], pfs.w[0]);
        if (var & 0x2) {
            SWAP(pfd.w[1], pfd.w[0]);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_sgnmod(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws;
    int i;
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    const uint32_t sign32 = BIT32(31);
    const uint64_t sign64 = BIT64(63);

    CPV_DEBUG("");

    if (is_double_precision2(env, opcode)) {
        pfd.d[1] = pfs.d[1];
        pfd.d[0] = pfs.d[0];
        switch (cpv_modH(opcode)) {
        case 0x0:
            break;
        case 0x1:
            pfd.d[1] &= ~sign64;
            break;
        case 0x2:
            pfd.d[1] |= sign64;
            break;
        case 0x3:
            pfd.d[1] ^= sign64;
            break;
        }
        switch (cpv_modL(opcode)) {
        case 0x0:
            break;
        case 0x1:
            pfd.d[0] &= ~sign64;
            break;
        case 0x2:
            pfd.d[0] |= sign64;
            break;
        case 0x3:
            pfd.d[0] ^= sign64;
            break;
        }
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i];

            wd[1] = ws[1];
            wd[0] = ws[0];
            switch (cpv_modH(opcode)) {
            case 0x0:
                break;
            case 0x1:
                wd[1] &= ~sign32;
                break;
            case 0x2:
                wd[1] |= sign32;
                break;
            case 0x3:
                wd[1] ^= sign32;
                break;
            }
            switch (cpv_modL(opcode)) {
            case 0x0:
                break;
            case 0x1:
                wd[0] &= ~sign32;
                break;
            case 0x2:
                wd[0] |= sign32;
                break;
            case 0x3:
                wd[0] ^= sign32;
                break;
            }
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_ifmove(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws;
    int i;
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    int vcc_index = cpv_cc2(opcode) * 4;
    bool tf = cpv_tf2(opcode);

    CPV_DEBUG("");

    if (is_double_precision2(env, opcode)) {
        if (tf == vcsr_get_vcc(env, vcc_index)) {
            pfd.d[0] = pfs.d[0];
        }
        if (tf == vcsr_get_vcc(env, vcc_index + 1)) {
            pfd.d[1] = pfs.d[1];
        }
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i];

            if (tf == vcsr_get_vcc(env, vcc_index + 2 * i)) {
                wd[0] = ws[0];
            }
            if (tf == vcsr_get_vcc(env, vcc_index + 2 * i + 1)) {
                wd[1] = ws[1];
            }
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vscale(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws;
    int i;
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    uint64_t n = cpv_valuen(opcode);
    float_status *status = &env->active_tc.cpv_fp_status;

    CPV_DEBUG("");

    if (is_double_precision2(env, opcode)) {
        pfd.d[1] = float64_scalbn(pfs.d[1], n, status);
        pfd.d[0] = float64_scalbn(pfs.d[0], n, status);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i];

            wd[1] = float32_scalbn(ws[1], n, status);
            wd[0] = float32_scalbn(ws[0], n, status);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vzero(CPUMIPSState *env, uint32_t opcode)
{
    wr_t zero = {.d[0] = 0, .d[1] = 0};

    CPV_DEBUG("");

    cpv_reg_store(env, MDMX2_FD(opcode), &zero);
    cpv_reg_store(env, MDMX2_FS(opcode), &zero);
}

static uint32_t ifset32(bool cc, int var)
{
    uint32_t res;
    switch (var) {
    case 0:
        res = cc ? 0x3f800000 : 0x00000000;
        break;
    case 1:
        res = cc ? 0x3f800000 : 0xbf800000;
        break;
    case 2:
        res = cc ? 0xffffffff : 0x00000000;
        break;
    case 3:
        res = cc ? 0x7f800000 : 0xff800000;
        break;
    case 4:
        res = cc ? 0x00000000 : 0x80000000;
        break;
    case 5:
        res = cc ? 0x80000000 : 0x00000000;
        break;
    default:
        res = 0;
        break;
    }
    return res;
}

static uint64_t ifset64(bool cc, int var)
{
    uint64_t res;
    switch (var) {
    case 0:
        res = cc ? 0x3ff0000000000000LL : 0x0000000000000000LL;
        break;
    case 1:
        res = cc ? 0x3ff0000000000000LL : 0xbff0000000000000LL;
        break;
    case 2:
        res = cc ? 0xffffffffffffffffLL : 0x0000000000000000LL;
        break;
    case 3:
        res = cc ? 0x7ff0000000000000LL : 0xfff0000000000000LL;
        break;
    case 4:
        res = cc ? 0x0000000000000000LL : 0x8000000000000000LL;
        break;
    case 5:
        res = cc ? 0x8000000000000000LL : 0x0000000000000000LL;
        break;
    default:
        res = 0;
        break;
    }
    return res;
}

void helper_cpv_ifset(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd;
    int i;
    wr_t pfd = MDMX2_PFD(opcode);
    int vcc_index = cpv_cc2(opcode) * 4;
    int var = cpv_var_13_3(opcode);
    bool tf = cpv_tf2(opcode);

    CPV_DEBUG("");

    if (is_double_precision2(env, opcode)) {
        pfd.d[0] = ifset64(tf == vcsr_get_vcc(env, vcc_index), var);
        pfd.d[1] = ifset64(tf == vcsr_get_vcc(env, vcc_index + 1), var);
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i];

            wd[0] = ifset32(tf == vcsr_get_vcc(env, vcc_index + 2 * i), var);
            wd[1] = ifset32(tf == vcsr_get_vcc(env, vcc_index + 2 * i + 1), var);
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vget(CPUMIPSState *env, uint32_t opcode)
{
    int32_t *wd, *ws;
    int i;
    uint64_t mask64;
    uint32_t mask32;
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    int sem = cpv_sem(opcode);

    CPV_DEBUG("");

    if (sem == 0) {
        return;
    } else if (is_double_precision2(env, opcode)) {
        mask64 = 0;
        if (sem & BIT32(2)) { /* extract sign */
            mask64 |= BIT64(63);
        }
        if (sem & BIT32(1)) { /* extract exponent */
            mask64 |= MAKE_64BIT_MASK(MANTISSA_BITS64, EXP_BITS64);
        }
        if (sem & BIT32(0)) { /* extract mantissa */
            mask64 |= MAKE_64BIT_MASK(0, MANTISSA_BITS64);
        }

        pfd.d[0] = pfs.d[0] & mask64;
        pfd.d[1] = pfs.d[1] & mask64;
    } else {
        mask32 = 0;
        if (sem & BIT32(2)) { /* extract sign */
            mask32 |= BIT32(31);
        }
        if (sem & BIT32(1)) { /* extract exponent */
            mask32 |= MAKE_64BIT_MASK(MANTISSA_BITS32, EXP_BITS32);
        }
        if (sem & BIT32(0)) { /* extract mantissa */
            mask32 |= MAKE_64BIT_MASK(0, MANTISSA_BITS32);
        }

        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i];

            wd[0] = ws[0] & mask32;
            wd[1] = ws[1] & mask32;
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vctc(CPUMIPSState *env, uint32_t opcode)
{
    uint32_t res = env->active_tc.gpr[MDMX2_RT(opcode)];

    CPV_DEBUG("");

    switch (MDMX2_FD(opcode)) {
    case CPV_VCIR:
        return;
    case CPV_VCSR:
        res &= 0xff00001c | BIT32(15);
        break;
    default:
        break;
    }

    cpv_control_store(env, MDMX2_FD(opcode), res);
}

void helper_cpv_vcfc(CPUMIPSState *env, uint32_t opcode)
{
    uint32_t res;

    CPV_DEBUG("");

    switch (MDMX2_FD(opcode)) {
    case CPV_VCIR:
        res = env->vcir;
        break;
    case CPV_VCSR:
        res = env->vcsr;
        break;
    default:
        res = (uint32_t) -1;
        CPV_DEBUG("vcfc error, unknown FD\n");
        break;
    }

    env->active_tc.gpr[MDMX2_RT(opcode)] = res;
}

static void set_vcc_bits(CPUMIPSState *env, uint32_t bits)
{
    env->vcsr = (env->vcsr & MAKE_64BIT_MASK(0, 23)) | (bits << 24);
}

static uint32_t get_vcc_bits(CPUMIPSState *env)
{
    return env->vcsr >> 24;
}

void helper_cpv_vmtcc(CPUMIPSState *env, uint32_t opcode)
{
    CPV_DEBUG("");
    set_vcc_bits(env, env->active_tc.gpr[MDMX2_RT(opcode)] & cpv_mask(opcode));
}

void helper_cpv_vmfcc(CPUMIPSState *env, uint32_t opcode)
{
    CPV_DEBUG("");
    env->active_tc.gpr[MDMX2_RT(opcode)] = get_vcc_bits(env) & cpv_mask(opcode);
}

void helper_cpv_vmth(CPUMIPSState *env, uint32_t opcode)
{
    wr_t pfd = MDMX2_PFD(opcode);

    CPV_DEBUG("");
    pfd.d[1] = env->active_tc.gpr[MDMX2_RT(opcode)];
    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vmfh(CPUMIPSState *env, uint32_t opcode)
{
    CPV_DEBUG("");
    env->active_tc.gpr[MDMX2_RT(opcode)] = MDMX2_PFD(opcode).d[1];
}

void helper_cpv_vmtl(CPUMIPSState *env, uint32_t opcode)
{
    wr_t pfd = MDMX2_PFD(opcode);

    CPV_DEBUG("");
    pfd.d[0] = env->active_tc.gpr[MDMX2_RT(opcode)];
    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vmfl(CPUMIPSState *env, uint32_t opcode)
{
    CPV_DEBUG("");
    env->active_tc.gpr[MDMX2_RT(opcode)] = MDMX2_PFD(opcode).d[0];
}

void helper_cpv_vmthfp(CPUMIPSState *env, uint32_t opcode)
{
    wr_t pfd = MDMX2_PFD(opcode);

    CPV_DEBUG("");
    pfd.d[1] = env->active_fpu.fpr[MDMX2_RT(opcode)].d;
    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vmfhfp(CPUMIPSState *env, uint32_t opcode)
{
    CPV_DEBUG("");
    env->active_fpu.fpr[MDMX2_RT(opcode)].d = MDMX2_PFD(opcode).d[1];
}

void helper_cpv_vmtlfp(CPUMIPSState *env, uint32_t opcode)
{
    wr_t pfd = MDMX2_PFD(opcode);

    CPV_DEBUG("");
    pfd.d[0] = env->active_fpu.fpr[MDMX2_RT(opcode)].d;
    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

void helper_cpv_vmflfp(CPUMIPSState *env, uint32_t opcode)
{
    CPV_DEBUG("");
    env->active_fpu.fpr[MDMX2_RT(opcode)].d = MDMX2_PFD(opcode).d[0];
}

void helper_cpv_move(CPUMIPSState *env, uint32_t opcode, uint32_t type)
{
    int32_t *wd, *ws, *wt;
    wr_t pft = MDMX2_PFT(opcode);
    wr_t pfd = MDMX2_PFD(opcode);
    wr_t pfs = MDMX2_PFS(opcode);
    int i;

    CPV_DEBUG("");

    if (is_double_precision(env, opcode)) {
        switch (type) {
        case MOVE_HH:
            pfd.d[1] = pfs.d[1];
            pfd.d[0] = pft.d[1];
            break;
        case MOVE_HL:
            pfd.d[1] = pfs.d[1];
            pfd.d[0] = pft.d[0];
            break;
        case MOVE_LH:
            pfd.d[1] = pfs.d[0];
            pfd.d[0] = pft.d[1];
            break;
        case MOVE_LL:
            pfd.d[1] = pfs.d[0];
            pfd.d[0] = pft.d[0];
            break;
        }
    } else {
        for (i = 0; i < sections_64(env); i++) {
            wd = &pfd.w[2 * i]; ws = &pfs.w[2 * i]; wt = &pft.w[2 * i];

            switch (type) {
            case MOVE_HH:
                wd[1] = ws[1];
                wd[0] = wt[1];
                break;
            case MOVE_HL:
                wd[1] = ws[1];
                wd[0] = wt[0];
                break;
            case MOVE_LH:
                wd[1] = ws[0];
                wd[0] = wt[1];
                break;
            case MOVE_LL:
                wd[1] = ws[0];
                wd[0] = wt[0];
                break;
            }
        }
    }

    cpv_reg_store(env, MDMX2_FD(opcode), &pfd);
}

static uint64_t cpv_read_mem64(CPUMIPSState *env, uint64_t addr)
{
    TCGMemOpIdx oi = make_memop_idx(MO_TEQ, cpu_mmu_index(env, false));
    return helper_ret_ldq_mmu(env, addr, oi, GETPC());
}

static inline uint64_t cpv_read_mem128(CPUMIPSState *env, uint64_t addr,
    uint8_t offset)
{
    return cpv_read_mem64(env, addr + offset);
}

static void cpv_write_mem64(CPUMIPSState *env, uint64_t addr, uint64_t val)
{
    TCGMemOpIdx oi = make_memop_idx(MO_TEQ, cpu_mmu_index(env, false));
    helper_ret_stq_mmu(env, addr, val, oi, GETPC());
}

static inline void cpv_write_mem128(CPUMIPSState *env, uint64_t addr,
    uint64_t val, uint8_t offset)
{
    cpv_write_mem64(env, addr + offset, val);
}

static inline uint64_t cpv_get_addr(CPUMIPSState *env,
    uint64_t base, uint64_t offset, uint8_t align)
{
    return (env->active_tc.gpr[base] + (offset * align)) & ~(align - 1);
}

static inline uint64_t cpv_get_addr_x(CPUMIPSState *env,
    uint64_t base, uint64_t idx, uint8_t align)
{
    return (env->active_tc.gpr[base] + env->active_tc.gpr[idx]) & ~(align - 1);
}

void helper_cpv_vld(CPUMIPSState *env, uint32_t opcode, uint32_t type)
{
    uint64_t base, offset;
    uint64_t addr;
    uint32_t opcode_type;
    wr_t pfd = MDMX_PFD(opcode);
    wr_t pfd1 = env->cpv[MDMX_FD(opcode) + 1];

    CPV_DEBUG("");

    base = mdmx_base(opcode);
    offset = type == TYPE_X ? mdmx_index(opcode) : mdmx_offset(opcode);
    opcode_type = opcode & (type == TYPE_X ? 0xfc00e07f : 0xfc00000f);

    switch (opcode_type) {
    case OPC_VLD:
    case OPC_VLDX:
        addr = (type == 0 ? cpv_get_addr(env, base, offset, 8)
                          : cpv_get_addr_x(env, base, offset, 8));
        pfd.d[0] = cpv_read_mem64(env, addr);
        if (is_width_128(env)) {
            pfd.d[1] = pfd.d[0];
        }
        break;
    case OPC_VLDD:
    case OPC_VLDDX:
        addr = (type == 0 ? cpv_get_addr(env, base, offset, 16)
                          : cpv_get_addr_x(env, base, offset, 16));
        pfd.d[0] = cpv_read_mem128(env, addr, 0x0);
        pfd1.d[0] = cpv_read_mem128(env, addr, 0x8);
        if (is_width_128(env)) {
            pfd.d[1] = pfd.d[0];
            pfd1.d[1] = pfd1.d[0];
        }
        break;
    case OPC_VLDDH:
    case OPC_VLDDHX:
        addr = (type == 0 ? cpv_get_addr(env, base, offset, 16)
                          : cpv_get_addr_x(env, base, offset, 16));
        if (is_width_128(env)) {
            pfd.d[1] = cpv_read_mem128(env, addr, 0x0);
            pfd1.d[1] = cpv_read_mem128(env, addr, 0x8);
        } else {
            qemu_log("VLDDH/X exception\n");
            do_raise_exception(env, EXCP_CME, GETPC());
        }
        break;
    case OPC_VLDH:
    case OPC_VLDHX:
        addr = (type == 0 ? cpv_get_addr(env, base, offset, 8)
                          : cpv_get_addr_x(env, base, offset, 8));
        if (is_width_128(env)) {
            pfd.d[1] = cpv_read_mem64(env, addr);
        } else {
            qemu_log("VLDH/X exception\n");
            do_raise_exception(env, EXCP_CME, GETPC());
        }
        break;
    case OPC_VLDM:
    case OPC_VLDMX:
        addr = (type == 0 ? cpv_get_addr(env, base, offset, 16)
                          : cpv_get_addr_x(env, base, offset, 16));
        pfd.d[0] = cpv_read_mem128(env, addr, 0x8);
        if (is_width_128(env)) {
            pfd.d[1] = cpv_read_mem128(env, addr, 0x0);
        }
        break;
    case OPC_VLDQ:
    case OPC_VLDQX:
        addr = (type == 0 ? cpv_get_addr(env, base, offset, 32)
                          : cpv_get_addr_x(env, base, offset, 32));
        pfd.d[0] = cpv_read_mem128(env, addr, 0x8);
        pfd1.d[0] = cpv_read_mem128(env, addr + 0x10, 0x8);
        if (is_width_128(env)) {
            pfd.d[1] = cpv_read_mem128(env, addr, 0x0);
            pfd1.d[1] = cpv_read_mem128(env, addr + 0x10, 0x0);
        } else {
            qemu_log("VLDQ/X exception\n");
            do_raise_exception(env, EXCP_CME, GETPC());
        }
        break;
    /* X-specific instructions */
    case OPC_VLDDLX:
        addr = cpv_get_addr_x(env, base, offset, 16);
        pfd.d[0] = cpv_read_mem128(env, addr, 0x0);
        pfd1.d[0] = cpv_read_mem128(env, addr, 0x8);
        break;
    case OPC_VLDLX:
        addr = (type == 0 ? cpv_get_addr(env, base, offset, 8)
                          : cpv_get_addr_x(env, base, offset, 8));
        pfd.d[0] = cpv_read_mem64(env, addr);
        break;
    }

    cpv_reg_store(env, MDMX_FD(opcode), &pfd);
    cpv_reg_store(env, MDMX_FD(opcode) + 1, &pfd1);
}

void helper_cpv_vsd(CPUMIPSState *env, uint32_t opcode, uint32_t type)
{
    uint64_t base, offset;
    uint64_t addr;
    uint32_t opcode_type;
    wr_t pfd = MDMX_PFD(opcode);
    wr_t pfd1 = env->cpv[MDMX_FD(opcode) + 1];

    CPV_DEBUG("");

    base = mdmx_base(opcode);
    offset = type == TYPE_X ? mdmx_index(opcode) : mdmx_offset(opcode);
    opcode_type = opcode & (type == TYPE_X ? 0xfc00e07f : 0xfc00000f);

    switch (opcode_type) {
    case OPC_VSD:
    case OPC_VSDX:
        addr = (type == 0 ? cpv_get_addr(env, base, offset, 8)
                          : cpv_get_addr_x(env, base, offset, 8));
        cpv_write_mem64(env, addr, pfd.d[0]);
        break;
    case OPC_VSDD:
    case OPC_VSDDX:
        addr = (type == 0 ? cpv_get_addr(env, base, offset, 16)
                          : cpv_get_addr_x(env, base, offset, 16));
        cpv_write_mem128(env, addr, pfd.d[0], 0x0);
        cpv_write_mem128(env, addr, pfd1.d[0], 0x8);
        break;
    case OPC_VSDDH:
    case OPC_VSDDHX:
        addr = (type == 0 ? cpv_get_addr(env, base, offset, 16)
                          : cpv_get_addr_x(env, base, offset, 16));
        if (is_width_128(env)) {
            cpv_write_mem128(env, addr, pfd.d[1], 0x0);
            cpv_write_mem128(env, addr, pfd1.d[1], 0x8);
        } else {
            qemu_log("VSDDH/X exception\n");
            do_raise_exception(env, EXCP_CME, GETPC());
        }
        break;
    case OPC_VSDH:
    case OPC_VSDHX:
        addr = (type == 0 ? cpv_get_addr(env, base, offset, 8)
                          : cpv_get_addr_x(env, base, offset, 8));
        if (is_width_128(env)) {
            cpv_write_mem64(env, addr, pfd.d[1]);
        } else {
            qemu_log("VSDH/X exception\n");
            do_raise_exception(env, EXCP_CME, GETPC());
        }
        break;
    case OPC_VSDM:
    case OPC_VSDMX:
        addr = (type == 0 ? cpv_get_addr(env, base, offset, 16)
                          : cpv_get_addr_x(env, base, offset, 16));
        if (is_width_128(env)) {
            cpv_write_mem128(env, addr, pfd.d[0], 0x8);
            cpv_write_mem128(env, addr, pfd.d[1], 0x0);
        } else {
            qemu_log("VSDM/X exception\n");
            do_raise_exception(env, EXCP_CME, GETPC());
        }
        break;
    case OPC_VSDQ:
    case OPC_VSDQX:
        addr = (type == 0 ? cpv_get_addr(env, base, offset, 32)
                          : cpv_get_addr_x(env, base, offset, 32));
        if (is_width_128(env)) {
            cpv_write_mem128(env, addr, pfd.d[1], 0x0);
            cpv_write_mem128(env, addr, pfd.d[0], 0x8);
            cpv_write_mem128(env, addr + 0x10, pfd1.d[1], 0x0);
            cpv_write_mem128(env, addr + 0x10, pfd1.d[0], 0x8);
        } else {
            qemu_log("VSDQ/X exception\n");
            do_raise_exception(env, EXCP_CME, GETPC());
        }
        break;
    }
}
