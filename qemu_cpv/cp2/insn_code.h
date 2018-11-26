#ifndef __insn_code_h__
#define __insn_code_h__

#include "common.h"

//! Instruction
typedef uint64_t instr_t;


// instruction arguments types
// (for disasm)
typedef enum {
	INSN_ARGS_NOARGS,
	INSN_ARGS_FD,
	INSN_ARGS_FDFS,
	INSN_ARGS_FDFSFT,
	INSN_ARGS_FDFSFQ,
	INSN_ARGS_FDFSFTFQ,
	INSN_ARGS_FDIMM16,
	INSN_ARGS_FDFSIMM12,
	INSN_ARGS_FDIMM18,
	INSN_ARGS_FDIMM6,
	INSN_ARGS_CCONDFMT,
	INSN_ARGS_FT2IMM13,
	INSN_ARGS_FT2RNOFFS13,
	INSN_ARGS_FT2PRN,
	INSN_ARGS_FT2MRN,
	INSN_ARGS_RNOFFS13,
	INSN_ARGS_RN,
	INSN_ARGS_IMM13,
	INSN_ARGS_CNT10IMM13,
	INSN_ARGS_GS,
	INSN_ARGS_GSIMM13,
	INSN_ARGS_SETI,
	INSN_ARGS_MOVE,
	INSN_ARGS_MTFPR,
	INSN_ARGS_MFFPR,
	INSN_ARGS_FDFSIMM6,
	INSN_ARGS_MFCMTC,
	INSN_ARGS_CCOND,
    INSN_ARGS_COPY,
    INSN_ARGS_PWTOPS,
    INSN_ARGS_PSTOPW,
    INSN_ARGS_SINC,
	INSN_ARGS_GT,
    INSN_ARGS_QSDOT,
    INSN_ARGS_PSPRMSGN,
	INSN_ARGS_PSPRMSGN0,
	INSN_ARGS_PSPRMSGN1
} insn_arg_t;


// type for instruction tables
// (for disasm)
typedef struct {
	char       *name;    // name of instruction
	uint32_t     opcode;   // its opcode
	insn_arg_t args;     // which arguments it has
} insntab_t;


// instruction fields
#define instr_field(instr,start,length) (((instr) >> (start)) & bits64((length)-1,0))

#define instr_hi(instr)    instr_field(instr,32,32)
#define instr_lo(instr)    instr_field(instr, 0,32)

#define instr_opcode(instr)    instr_field(instr,56,8)
#define instr_fd(instr)        instr_field(instr,50,6)
#define instr_fs(instr)        instr_field(instr,44,6)
#define instr_ft(instr)        instr_field(instr,38,6)
#define instr_2ft(instr)       instr_field(instr,17,6)
#define instr_fq(instr)        instr_field(instr,32,6)
#define instr_imm6(instr)      instr_field(instr,32,6)
#define instr_imm12(instr)     instr_field(instr,32,12)
#define instr_imm16(instr)     instr_field(instr,32,16)
#define instr_imm18(instr)     instr_field(instr,32,18)
#define instr_2imm13(instr)    instr_field(instr, 0,13)
#define instr_2imm16(instr)    instr_field(instr, 0,16)
#define instr_fmt(instr)       instr_field(instr,53,2)
#define instr_cc1(instr)       instr_field(instr,50,3)
#define instr_cond(instr)      instr_field(instr,32,3)
#define instr_cc(instr)        instr_field(instr,29,3)
#define instr_elf_cond(instr)  instr_field(instr,32,2)
#define instr_opcode2(instr)   instr_field(instr,23,6)
#define instr_ft2(instr)       instr_field(instr,17,6)
#define instr_fd2(instr)       instr_field(instr,12,5)
#define instr_cnt10(instr)     instr_field(instr,13,10)
#define instr_gs(instr)        instr_field(instr,13,4)
#define instr_gr(instr)        instr_field(instr,13,4)
#define instr_gt(instr)        instr_field(instr,17,4)
#define instr_rn(instr)        instr_field(instr,13,4)
#define instr_rn2(instr)       instr_field(instr,17,4)
#define instr_offset13(instr)  instr_field(instr, 0,13)
#define instr_dreg(instr)      instr_field(instr,17,4)
#define instr_sreg(instr)      instr_field(instr,13,4)
#define instr_dtyp(instr)      instr_field(instr, 3,3)
#define instr_styp(instr)      instr_field(instr, 0,3)
#define instr_secn(instr)      instr_field(instr, 0,3)
#define instr_regtype(instr)   instr_field(instr,21,2)
#define instr_cntrlreg(instr)  instr_field(instr,32,4)
#define instr_hilo(instr)      instr_field(instr,43,1)
#define instr_mode(instr)      instr_field(instr,32,1)
#define instr_n1(instr)        instr_field(instr, 8,1)
#define instr_n2(instr)        instr_field(instr, 9,1)
#define instr_n3(instr)        instr_field(instr, 10,1)
#define instr_n4(instr)        instr_field(instr, 11,1)
#define instr_sel1(instr)      instr_field(instr, 0,2)
#define instr_sel2(instr)      instr_field(instr, 2,2)
#define instr_sel3(instr)      instr_field(instr, 4,2)
#define instr_sel4(instr)      instr_field(instr, 6,2)
#define instr_mfpr(instr)      instr_field(instr, 3,1)

// instruction fields sign extended to 32bits
#define instr_offset13_sigext32(instr)  sign_ext32(instr_offset13(instr),12)
#define instr_imm6_sigext32(instr)      sign_ext32(instr_imm6(instr),5)
#define instr_imm12_sigext32(instr)     sign_ext32(instr_imm12(instr),11)
#define instr_imm18_sigext32(instr)     sign_ext32(instr_imm18(instr),17)
#define instr_2imm16_sigext64(instr)    sign_ext64(instr_2imm16(instr),15)

// instruction opcodes
#define INSTR_OPCODE_NOP      0x00
//#define INSTR_OPCODE_       0x01
#define INSTR_OPCODE_CCOND    0x02
#define INSTR_OPCODE_CLEAR    0x03
#define INSTR_OPCODE_COPY     0x04
#define INSTR_OPCODE_SWAPHL   0x05
#define INSTR_OPCODE_SWAP64   0x06
#define INSTR_OPCODE_RDSEC    0x07

#define INSTR_OPCODE_LI3      0x08
#define INSTR_OPCODE_LI2      0x09
#define INSTR_OPCODE_LI1      0x0a
#define INSTR_OPCODE_LI0      0x0b
//#define INSTR_OPCODE_       0x0c
//#define INSTR_OPCODE_       0x0d
#define INSTR_OPCODE_MFC      0x0e
#define INSTR_OPCODE_MTC      0x0f

#define INSTR_OPCODE_CADD     0x10
#define INSTR_OPCODE_CSUB     0x11
#define INSTR_OPCODE_CADDSUB  0x12
#define INSTR_OPCODE_CNEG     0x13
#define INSTR_OPCODE_CMUL     0x14
#define INSTR_OPCODE_CMADD    0x15
#define INSTR_OPCODE_CMSUB    0x16
#define INSTR_OPCODE_CMULI    0x17

#define INSTR_OPCODE_CMULNI   0x18
#define INSTR_OPCODE_CCONJ    0x19
#define INSTR_OPCODE_CFLY     0x1a
#define INSTR_OPCODE_CFLY2    0x1b
#define INSTR_OPCODE_CHMUL    0x1c
#define INSTR_OPCODE_CHMADD   0x1d
#define INSTR_OPCODE_CHMSUB   0x1e
//#define INSTR_OPCODE_       0x1f

#define INSTR_OPCODE_PSADD    0x20
#define INSTR_OPCODE_PSSUB    0x21
#define INSTR_OPCODE_PSADDSUB 0x22
#define INSTR_OPCODE_PSNEG    0x23
#define INSTR_OPCODE_PSMUL    0x24
#define INSTR_OPCODE_PSMADD   0x25
#define INSTR_OPCODE_PSMSUB   0x26
#define INSTR_OPCODE_PSABS    0x27
#define INSTR_OPCODE_MVMUL    0x28
#define INSTR_OPCODE_MTVMUL   0x29
#define INSTR_OPCODE_MVMADD   0x2a
#define INSTR_OPCODE_MTVMADD  0x2b
#define INSTR_OPCODE_MVMSUB   0x2c
#define INSTR_OPCODE_MTVMSUB  0x2d
#define INSTR_OPCODE_MTRANS   0x2e
#define INSTR_OPCODE_QSDOT    0x2f

#define INSTR_OPCODE_PSCOPYSIGN 0x30
#define INSTR_OPCODE_PSGETEXP   0x31
#define INSTR_OPCODE_PSGETMAN   0x32
#define INSTR_OPCODE_PSSCALE    0x33
#define INSTR_OPCODE_GETCOEFF   0x34

#define INSTR_OPCODE_ADD      0x40
#define INSTR_OPCODE_SUB      0x41
#define INSTR_OPCODE_ABS      0x42
#define INSTR_OPCODE_NEG      0x43
#define INSTR_OPCODE_ADDI     0x44
#define INSTR_OPCODE_SUBI     0x45
#define INSTR_OPCODE_INCI     0x46
#define INSTR_OPCODE_DECI     0x47

#define INSTR_OPCODE_AND      0x48
#define INSTR_OPCODE_OR       0x49
#define INSTR_OPCODE_XOR      0x4a
#define INSTR_OPCODE_NOT      0x4b
#define INSTR_OPCODE_LSHIFT   0x4c
#define INSTR_OPCODE_LSHIFTI  0x4d
#define INSTR_OPCODE_ASHIFT   0x4e
#define INSTR_OPCODE_ASHIFTI  0x4f

#define INSTR_OPCODE_PWTOPS   0x58
#define INSTR_OPCODE_PSTOPW   0x59

#define INSTR_OPCODE_SPLIT8   0x5a
#define INSTR_OPCODE_SPLIT16  0x5b
#define INSTR_OPCODE_SPLIT32  0x5c
#define INSTR_OPCODE_EXTSIGN8H  0x5d
#define INSTR_OPCODE_EXTSIGN8W  0x5e
#define INSTR_OPCODE_EXTSIGN16W 0x5f
#define INSTR_OPCODE_JOIN8    0x60
#define INSTR_OPCODE_JOIN16   0x61
#define INSTR_OPCODE_JOIN32   0x62
#define INSTR_OPCODE_UNPCK16WSTOPS  0x63

#define INSTR_OPCODE_RECIP    0x70
#define INSTR_OPCODE_SINC     0x71
#define INSTR_OPCODE_ATANC    0x72
#define INSTR_OPCODE_LOG2C    0x73
#define INSTR_OPCODE_EXP2     0x74
#define INSTR_OPCODE_RSQRT    0x75
#define INSTR_OPCODE_RRCOSSIN 0x78
#define INSTR_OPCODE_RRSIN    0x79
#define INSTR_OPCODE_RRCOS    0x7a
#define INSTR_OPCODE_RRLOG2   0x7b



// instruction opcode2s
#define INSTR_OPCODE2_NOP         0x00
#define INSTR_OPCODE2_MOVE        0x01
#define INSTR_OPCODE2_CLR         0x02
#define INSTR_OPCODE2_RET         0x03
#define INSTR_OPCODE2_ENDDO       0x04
#define INSTR_OPCODE2_SYNC        0x05
#define INSTR_OPCODE2_STOPI       0x08
#define INSTR_OPCODE2_STOP        0x09
#define INSTR_OPCODE2_RUNI        0x0a
#define INSTR_OPCODE2_RUN         0x0b
#define INSTR_OPCODE2_SETI        0x20
#define INSTR_OPCODE2_DOI         0x24
#define INSTR_OPCODE2_DO          0x25
#define INSTR_OPCODE2_JUMPI       0x28
#define INSTR_OPCODE2_JUMP        0x29
#define INSTR_OPCODE2_LWI         0x10
#define INSTR_OPCODE2_LDI         0x18
#define INSTR_OPCODE2_SWI         0x14
#define INSTR_OPCODE2_SDI         0x1c
#define INSTR_OPCODE2_LWNP        0x12
#define INSTR_OPCODE2_LDNP        0x1a
#define INSTR_OPCODE2_SWNP        0x16
#define INSTR_OPCODE2_SDNP        0x1e
#define INSTR_OPCODE2_LWNM        0x13
#define INSTR_OPCODE2_LDNM        0x1b
#define INSTR_OPCODE2_SWNM        0x17
#define INSTR_OPCODE2_SDNM        0x1f
#define INSTR_OPCODE2_LWO         0x11
#define INSTR_OPCODE2_LDO         0x19
#define INSTR_OPCODE2_SWO         0x15
#define INSTR_OPCODE2_SDO         0x1d
#define INSTR_OPCODE2_UPDADDR     0x21
#define INSTR_OPCODE2_UPDADDRNP   0x22
#define INSTR_OPCODE2_UPDADDRNM   0x23
#define INSTR_OPCODE2_CALLI       0x26
#define INSTR_OPCODE2_CALL        0x27
#define INSTR_OPCODE2_CHECK_DMA   0x2e
#define INSTR_OPCODE2_START_DMA   0x2f
#define INSTR_OPCODE2_MTFPR       0x30
#define INSTR_OPCODE2_MFFPR       0x31
#define INSTR_OPCODE2_PSPRMSGN0   0x32
#define INSTR_OPCODE2_PSPRMSGN1   0x33

// field values for mtfpr/mffpr instructions
#define INSTR_MFPR_TYPE_GPR      0x0
#define INSTR_MFPR_TYPE_IREG     0x1


// field values for 'move' instruction
#define INSTR_MOVE_TYPE_GPR      0x00
#define INSTR_MOVE_TYPE_FPR      0x01
#define INSTR_MOVE_TYPE_CTRL     0x02
#define INSTR_MOVE_TYPE_ADDRAN   0x03
#define INSTR_MOVE_TYPE_ADDRNN   0x04
#define INSTR_MOVE_TYPE_ADDRMN   0x05
#define INSTR_MOVE_TYPE_IREG     0x06
#define INSTR_MOVE_SECN_0    0x00
#define INSTR_MOVE_SECN_1    0x01
#define INSTR_MOVE_SECN_2    0x02
#define INSTR_MOVE_SECN_3    0x03
#define INSTR_MOVE_SECN_ALL  0x04

#define CTRLREG_PC         0x00
#define CTRLREG_COMM       0x01
#define CTRLREG_CONTROL    0x02
#define CTRLREG_STATUS     0x03
#define CTRLREG_PSP        0x04
#define CTRLREG_LC         0x05
#define CTRLREG_LA         0x06
#define CTRLREG_LSP        0x07
#define CTRLREG_RIND       0x08
#define CTRLREG_RSTEP      0x09
#define CTRLREG_RMASK      0x0a
#define CTRLREG_STOPCODE   0x0b
#define CTRLREG_CLOCKCOUNT 0x100

// field values of control register

// field values for 'c.cond.fmt' instruction
#define INSTR_CCOND_FMT_S    0x00
#define INSTR_CCOND_FMT_W    0x01
#define INSTR_CCOND_FMT_PS   0x02
#define INSTR_CCOND_FMT_PW   0x03
#define INSTR_CCOND_COND_T   0x00
#define INSTR_CCOND_COND_UN  0x01
#define INSTR_CCOND_COND_LT  0x02
#define INSTR_CCOND_COND_LE  0x03
#define INSTR_CCOND_COND_EQ  0x04
#define INSTR_CCOND_COND_NE  0x05
#define INSTR_CCOND_COND_GT  0x06
#define INSTR_CCOND_COND_GE  0x07

// register types for 'seti' instruction
#define INSTR_SETI_REGTYPE_GPR  0x00
#define INSTR_SETI_REGTYPE_AN   0x01
#define INSTR_SETI_REGTYPE_NN   0x02
#define INSTR_SETI_REGTYPE_MN   0x03

// control register types for 'mfc','mtc' instructions
#define INSTR_MFCMTC_CNTRLREG_FCSR  0x00
#define INSTR_MFCMTC_CNTRLREG_FCCR  0x01




// =====  Functions  =====

//
int insntab_get_opcode_by_name  (insntab_t *table, uint64_t *opcode, char *name);
int insntab_get_index_by_name   (insntab_t *table, char *name);
int insntab_get_index_by_opcode (insntab_t *table, uint64_t opcode);

// disasm
void  disasm               (char *str, instr_t instr);
void  disasm_cc            (char *str, instr_t instr);
void  disasm_subinstr      (char *str, insntab_t table[], instr_t instr, uint64_t opcode);
void  disasm_subinstr_args (char *str, instr_t instr, insn_arg_t arg_type);


#endif // __insn_code_h__
