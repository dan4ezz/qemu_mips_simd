#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include "system.h"
#include "insn_code.h"
#include "machine.h"
#include "softfloat.h"
#include "options.h"
#include "prockern.h"
#include "prockern_access.h"
#include "common.h"

static char float32_is_nan(unsigned int a) {
    return ((uint32_t)(a << 1) > 0xFF000000);
}

// machine
extern Machine_T *k128cp2_machine;
int float_flag_denorm = 64;

float_status f_status;

// instruction emulation
#define instr_def(instr_name) void execute_instr_##instr_name (instr_t instr)


#define execute_instr_for_all_sections(a) \
	for (i=0; i<NUMBER_OF_EXESECT; i++) { \
		execute_instr_one_section_##a; \
	}
#define instr_onesect_def(a) execute_instr_one_section_##a
//


// delayed address register writing
void addrreg_result_write (regid_type_t type, int regno, regval_t val) {

	regid_t  regid;

	// set regid
	regid = ((regid_t) {
		.section = REGID_SECT_NONE,
		.type    = type,
		.regno   = regno,
		.half    = REGID_HALF_NONE
	});

	// call delayed_reg_write
	delayed_reg_write(&regid, &val, INSTR_DELAY_ADDRREG, TRUE);
}

//
int if_update_fcsr(instr_t instr) {

	int rv;

	switch(instr_opcode(instr)){
		case INSTR_OPCODE_NOP:
		case INSTR_OPCODE_CLEAR:
		case INSTR_OPCODE_SWAPHL:
		case INSTR_OPCODE_SWAP64:
		case INSTR_OPCODE_MTRANS:
		case INSTR_OPCODE_RDSEC:
		case INSTR_OPCODE_LI0:
		case INSTR_OPCODE_LI1:
		case INSTR_OPCODE_LI2:
		case INSTR_OPCODE_LI3:
		case INSTR_OPCODE_LSHIFT:
		case INSTR_OPCODE_LSHIFTI:
		case INSTR_OPCODE_ASHIFT:
		case INSTR_OPCODE_ASHIFTI:
		case INSTR_OPCODE_SPLIT8:
		case INSTR_OPCODE_SPLIT16:
		case INSTR_OPCODE_SPLIT32:
		case INSTR_OPCODE_JOIN8:
		case INSTR_OPCODE_JOIN16:
		case INSTR_OPCODE_JOIN32:
		case INSTR_OPCODE_EXTSIGN8H:
		case INSTR_OPCODE_EXTSIGN8W:
		case INSTR_OPCODE_EXTSIGN16W:
		case INSTR_OPCODE_COPY:
		case INSTR_OPCODE_ADD:
		case INSTR_OPCODE_ABS:
		case INSTR_OPCODE_ADDI:
		case INSTR_OPCODE_SUB:
		case INSTR_OPCODE_SUBI:
		case INSTR_OPCODE_INCI:
		case INSTR_OPCODE_DECI:
		case INSTR_OPCODE_AND:
		case INSTR_OPCODE_NEG:
		case INSTR_OPCODE_OR:
		case INSTR_OPCODE_XOR:
		case INSTR_OPCODE_NOT:
		case INSTR_OPCODE_CCOND:
		case INSTR_OPCODE_MFC:
		case INSTR_OPCODE_MTC:
		case INSTR_OPCODE_GETCOEFF: rv = 0; break;
		default: rv = 1; break;

	}

	return rv;
}

//
bool_t is_denorm(uint32_t a) {

	return (
        ((a & ( (0xffffffffUL          >> (31-((30)-(23))) ) << (23) )) == 0) && // exponent
        ((a & ( (0xffffffffUL          >> (31-((22)-(0))) ) << (0) )) != 0)    // fraction
    );

}

uint32_t adjust_result_rs(uint32_t f)
{

	if( (f_status.float_exception_flags & float_flag_underflow) || is_denorm(f) ){
		f_status.float_exception_flags |= float_flag_underflow | float_flag_inexact;
			f &= ~bits32(30,0);
	}

	if( (f_status.float_exception_flags & float_flag_overflow) ){
			f = ((f & bit32(31)) | 0x7f800000);
	}

	if( float32_is_nan(f) ){
		f_status.float_exception_flags |= float_flag_invalid;
		f = 0x7fffffff;
	}

	return f;
}

//
uint32_t adjust_result(uint32_t f) {

	int ctrl_rm;

	// read rounding mode from control register
	ctrl_rm = rawread_ctrlreg (CTRLREG_CONTROL);
	ctrl_rm = ctrl_rm & REG_CONTROL_RM;

	if( (f_status.float_exception_flags & float_flag_underflow) || is_denorm(f) ){
		f_status.float_exception_flags |= float_flag_underflow | float_flag_inexact;
		switch (ctrl_rm) {
			case REG_CONTROL_RM_RN : f &= ~bits32(30,0); break;
			case REG_CONTROL_RM_RZ : f &= ~bits32(30,0); break;
			case REG_CONTROL_RM_RP : f = ( ((f & bit32(31)) == 0) ? bit32(23) : bit32(31)            ); break;
			case REG_CONTROL_RM_RM : f = ( ((f & bit32(31)) == 0) ?         0 : bit32(31) | bit32(23)); break;
			default: sim_error("Invalid rounding mode in control register: RM=0x%02x\n", ctrl_rm); break;
		}
	}

	if( (f_status.float_exception_flags & float_flag_overflow) ){
//		f_status.float_exception_flags |= float_flag_overflow | float_flag_inexact;
		switch (ctrl_rm) {
			case REG_CONTROL_RM_RN : f = ((f & bit32(31)) | 0x7f800000); break;
			case REG_CONTROL_RM_RZ : f = ((f & bit32(31)) | 0x7f7fffff); break;
			case REG_CONTROL_RM_RP : f = ( ((f & bit32(31)) == 0) ? 0x7f800000 : 0xff7fffff ); break;
			case REG_CONTROL_RM_RM : f = ( ((f & bit32(31)) == 0) ? 0x7f7fffff : 0xff800000 ); break;
			default: sim_error("Invalid rounding mode in control register: RM=0x%02x\n", ctrl_rm); break;
		}
	}

	if( float32_is_nan(f) ){
		f_status.float_exception_flags |= float_flag_invalid;
		return 0x7fffffff;
	}

	return f;
}

//
int adjust_exception_flags(int flags) {

	//if 'f' denormalized then raise Underflow and Inexact exceptions and set 'f' to zero with sign
//	if( is_denorm(f.ui32.hi) || is_denorm(f.ui32.lo) )
//		f_status.float_exception_flags |= float_flag_underflow | float_flag_inexact;

	//if invalid then set to zero other bits
	if( flags & float_flag_invalid )
		flags &= float_flag_denorm | float_flag_invalid;
//		f_status.float_exception_flags = float_flag_invalid;

	return flags;
}

// qnan to nan
fpr_t adjust_operand(fpr_t f) {

	if( float32_is_nan( f.ui32.hi) ){
		f.ui32.hi = 0xffffffff;
		f_status.float_exception_flags |= float_flag_invalid;
	}
	if( float32_is_nan( f.ui32.lo) ){
		f.ui32.lo = 0xffffffff;
		f_status.float_exception_flags |= float_flag_invalid;
	}

	if( is_denorm(f.ui32.hi) ){
		f.ui32.hi &= 0x80000000;
		f_status.float_exception_flags |= float_flag_denorm;
	}
	if( is_denorm(f.ui32.lo) ){
		f.ui32.lo &= 0x80000000;
		f_status.float_exception_flags |= float_flag_denorm;
	}

	return f;

}

// qnan to nan
fpr_t qnan_to_snan(fpr_t f) {

	if( float32_is_nan( f.ui32.hi) )
		f.ui32.hi &= 0xffbfffff;
	if( float32_is_nan( f.ui32.lo) )
		f.ui32.lo &= 0xffbfffff;

	return f;

}

// delayed register writing calls
void float_result_write (int sect, int regno, fpr_t fprval, bool_t bypass) {

	regid_t  regid;
	regval_t regval;

	// set regid
	regid.section = sect;
	regid.type    = REGID_TYPE_FPR;
	regid.regno   = regno;
	regid.half    = REGID_HALF_BOTH;

	// set regval
	regval = REGVAL (fpr, fprval, NOMASK64, NONE);

	// call delayed_reg_write
	delayed_reg_write (&regid, &regval, INSTR_DELAY_FPU, bypass);
}

void float_result_write_lo (int sect, int regno, fpr_t fprval, bool_t bypass) {

	regid_t  regid;
	regval_t regval;

	// set regid
	regid.section = sect;
	regid.type    = REGID_TYPE_FPR;
	regid.regno   = regno;
	regid.half    = REGID_HALF_LO;

	// set regval
	regval = REGVAL (fpr, fprval, NOMASK64, NONE);

	// call delayed_reg_write
	delayed_reg_write(&regid, &regval, INSTR_DELAY_FPU, bypass);
}

void float_result_write_hi (int sect, int regno, fpr_t fprval, bool_t bypass) {

	regid_t  regid;
	regval_t regval;

	// set regid
	regid.section = sect;
	regid.type    = REGID_TYPE_FPR;
	regid.regno   = regno;
	regid.half    = REGID_HALF_HI;

	// set regval
	regval = REGVAL (fpr, fprval, NOMASK64, NONE);

	// call delayed_reg_write
	delayed_reg_write(&regid, &regval, INSTR_DELAY_FPU, bypass);
}


// clear cause bits in fcsr
// FIXME: записывать регистр fcsr на том же такте,
// что и сам результат арифметической инструкции
#define flush_fcsr_cause(_sect,_op)	kern.exesect[_sect].fcsr.fields.cause = 0;

// write exception bits from softfloat variable to fcsr
#define update_fcsr(_sect,_op) update_fcsr_from_softfloat(_sect);

// Update fcsr register and fpe field of status register
// with floating point exceptions from softfloat.
void  update_fcsr_from_softfloat (sectid_t sect) {

	regid_t  regid;
	regval_t regval;
	fcsr_t       fcsr;

	regid  = REGID (sect, FCSR, REGNO_NONE, NONE);
	regval = reg_read_on_bypass(regid, reg_clockcount-1, READWRITE_CTRLREG_SRCID_NONE);
	fcsr = regval.val.fcsr;

	// clear cause bits in fcsr
	fcsr.fields.cause = 0;

	// write cause bits
#define exc_code f_status.float_exception_flags
	fcsr.bits.cause_I = (((exc_code & float_flag_inexact   ) != 0) ? 1 : 0);
	fcsr.bits.cause_U = (((exc_code & float_flag_underflow ) != 0) ? 1 : 0);
	fcsr.bits.cause_O = (((exc_code & float_flag_overflow  ) != 0) ? 1 : 0);
	fcsr.bits.cause_Z = (((exc_code & float_flag_divbyzero ) != 0) ? 1 : 0);
	fcsr.bits.cause_V = (((exc_code & float_flag_invalid   ) != 0) ? 1 : 0);
	fcsr.bits.cause_E = (((exc_code & float_flag_denorm    ) != 0) ? 1 : 0);
#undef exc_code

	// accumulate flag bits: 'or' existing flags with cause flags
	fcsr.fields.flags |= fcsr.fields.cause;

	// write register
	regid  = REGID  (sect, FCSR, REGNO_NONE, NONE);
	regval = REGVAL (fcsr, fcsr, FCSRMASK, NONE);
	delayed_reg_write (&regid, &regval, INSTR_DELAY_FCSR, BYPASS_YES);

}


// Update FPE fields of Status register: 'or' Flags fields from all sections
void update_reg_status_fpe (uint32_t exc_acc) {

	reg_status_t status;

	// fill FPE field 'or'ing Flags fields of fcsr register
	status.ui32 = 0;
//	for (i=0; i<NUMBER_OF_EXESECT; i++) {
//		status.fields.fpe |= rawread_fcsr(i).fields.flags;
//	}

	status.bits.fpe_I = (((exc_acc & float_flag_inexact   ) != 0) ? 1 : 0);
	status.bits.fpe_U = (((exc_acc & float_flag_underflow ) != 0) ? 1 : 0);
	status.bits.fpe_O = (((exc_acc & float_flag_overflow  ) != 0) ? 1 : 0);
	status.bits.fpe_Z = (((exc_acc & float_flag_divbyzero ) != 0) ? 1 : 0);
	status.bits.fpe_V = (((exc_acc & float_flag_invalid   ) != 0) ? 1 : 0);
	status.bits.fpe_E = (((exc_acc & float_flag_denorm    ) != 0) ? 1 : 0);

	// write status using mask
	write_ctrlreg (
		reg_clockcount,
		CTRLREG_STATUS,
		(status.ui32 | kern.status.ui32),
		REG_STATUS_FPE.ui32,
		CP2
	);
}


// nop instruction
void execute_instr_NOP(instr_t instr) {
	// do nothing
}



// float instructions
#include "instr_float.inc"


// index instructions
#include "instr_index.inc"



// 'sync' instruction
// set sync_pending flag.
// this flag is checked and updated in prockern_do_one_step
// before starting execution of new instructions
instr_def(SYNC) {
	kern.sync_pending_counter = 7;
	kern.sync_pending = TRUE;
}



// common part of 'run' and 'runi' instructions
void execute_instr_RUN_common (uint16_t start_addr) {

	regid_t  regid;
	regval_t rv;

	if( (reg_control.ui32 & REG_CONTROL_ST) != 0 )
		return;

	// jump at start_addr
	prockern_jump (start_addr);

	// set 'run' bit in status register
	// write Status register using mask
	regid  = REGID  (0, CTRLREG, CTRLREG_STATUS, NONE);
	rv = REGVAL (ctrlreg, REG_STATUS_RUN.ui32, REG_STATUS_RUN.ui32, CP2);
	delayed_reg_write (&regid, &rv, INSTR_DELAY_RUNSTOP, BYPASS_NO);

	// save state optionally
	prockern_save_state_onrun ();
}


// this instruction jumps to address=imm13 and sets 'run' bit in status register
instr_def(RUNI) {

	uint16_t start_addr;

	// read arguments
	start_addr = instr_2imm13(instr);

	// call common part of 'run' and 'runi' instructions
	execute_instr_RUN_common (start_addr);

}


// this instruction jumps to address=gs and sets 'run' bit in status register
instr_def(RUN) {

	uint16_t start_addr;

	// read arguments
	start_addr = read_gpr (reg_clockcount, instr_gs(instr));

	// call common part of 'run' and 'runi' instructions
	execute_instr_RUN_common (start_addr);

}



// common part of 'stop' and 'stopi' instructions
void execute_instr_STOP_common (uint64_t stop_code) {

  	regid_t  regid;
	regval_t rv;

	kern.stop_pending_counter = 7;
	//FIXME replace kern.sync_pending by stop_pending???
	kern.stop_pending = TRUE;
	kern.run_flag = FALSE;

	// flush Run bit in Status register
	// write Status register using mask
	regid  = REGID  (0, CTRLREG, CTRLREG_STATUS, NONE);
	rv = REGVAL (ctrlreg, 0, REG_STATUS_RUN.ui32, CP2);
	delayed_reg_write (&regid, &rv, INSTR_DELAY_RUNSTOP, BYPASS_NO);

	// write stop code
	reg_stopcode = stop_code;

	// print message
	if (k128cp2_machine -> opt_dump_inststop) {
		sim_printf ("dump_inststop: STOP %" PRId64, stop_code);
	}

	// haltdump option support
	prockern_haltdump ();

	// save state optionally
	prockern_save_state_onstop ();
}


// this instruction immediately sets halt bit in control register
instr_def(STOPI) {

	uint64_t stop_code;

	// read arguments
	stop_code = instr_2imm13(instr);

	// call common part of 'stop' and 'stopi' instructions
	execute_instr_STOP_common (stop_code);

}


// this instruction immediately sets halt bit in control register
instr_def(STOP) {

	uint64_t stop_code;

	// read arguments
	stop_code = read_gpr (reg_clockcount, instr_gs(instr));

	// call common part of 'stop' and 'stopi' instructions
	execute_instr_STOP_common (stop_code);

}


//
void execute_instr_JUMP (instr_t instr){

	if(kern.nulify)
		return;

	uint64_t addr;
	uint32_t gprno;
	regid_t regid;
	regval_t regval;

	// read arguments
	gprno   = instr_gs(instr);

	// read gpr register
	regid  = REGID (SNONE, GPR, gprno, BOTH);
	regval = reg_read_on_bypass(regid, reg_clockcount-1, READWRITE_CTRLREG_SRCID_CP2);
	addr = regval.val.ui64;

	if ( reg_status.fields.run != 0 )
		kern.delay_slot_flag  = TRUE;

	// send loop info to prockern
	prockern_jump (addr);
}


//
void execute_instr_JUMPI (instr_t instr){

	if(kern.nulify)
		return;

	uint32_t addr;

	// read arguments
	addr = instr_2imm13(instr);

	if ( reg_status.fields.run != 0 )
		kern.delay_slot_flag  = TRUE;

	// send loop info to prockern
	prockern_jump (addr);
}



//
instr_def(CALL) {

	uint64_t addr;
	uint32_t gprno;

	// read arguments
	gprno   = instr_gs(instr);

	// read gpr register
	addr = read_gpr (reg_clockcount, gprno);

	// send call info to prockern
	prockern_call_new (addr);

}



//
instr_def(CALLI) {

	uint32_t addr;

	// read arguments
	addr = instr_2imm13(instr);

	// send call info to prockern
	prockern_call_new (addr);

}



//
instr_def(RET) {

	// inform prockern
	prockern_call_ret ();

}

//
instr_def(ENDDO) {

	reg_la_t newla;
	reg_lc_t newlc;

	// read LSP register
//	lsp = read_ctrlreg (reg_clockcount, CTRLREG_LSP, CP2);

	// check lstack underflow
	if (kern.lsp_cur == 0) {

		// set LUE bit in Status register
		write_ctrlreg (
			reg_clockcount,
			CTRLREG_STATUS,
			REG_STATUS_LUE.ui32,
			REG_STATUS_LUE.ui32,
			CP2
		);

	} else {
		// decrement lsp
		kern.lsp_cur -= 1; // alex


		// pop LA, LC registers from lstack
		newla = kern.lstack[kern.lsp_cur].la;
		newlc = kern.lstack[kern.lsp_cur].lc;
		kern.la_cur = newla;
		kern.lc_cur = newlc;

		// decrement lsp
//		kern.lsp_cur -= 1;

		// write registers
		write_ctrlreg (reg_clockcount, CTRLREG_LSP, kern.lsp_cur        , NOMASK64, CP2);
		write_ctrlreg (reg_clockcount, CTRLREG_LA , newla.ui32 , NOMASK64, CP2);
		write_ctrlreg (reg_clockcount, CTRLREG_LC , newlc      , NOMASK64, CP2);
	}

	// print optionally lstack
	if (k128cp2_machine -> opt_dump_lstack) {
		prockern_dump_lstack ();
	}

}


//
instr_def(DO) {

	uint64_t nloops;
	uint32_t gprno, endaddr;

	// read arguments
	gprno   = instr_gs(instr);
	endaddr = instr_2imm13(instr);

	// read gpr register
	nloops = read_gpr (reg_clockcount-1, gprno);

/*	if( nloops == 0 )
		kern.nulify = TRUE;*/

	// send loop info to prockern
	prockern_loop_new (nloops, endaddr);
}


//
instr_def(DOI) {

	uint32_t nloops, endaddr;

	// read arguments
	nloops  = instr_cnt10(instr);
	endaddr = instr_2imm13(instr);

/*	if( nloops == 0 )
		kern.nulify = TRUE;*/

	// send loop info to prockern
	prockern_loop_new (nloops, endaddr);
}


//
instr_def(SETI) {

	uint32_t regno, regtype;
	uint64_t val64;
	regid_t  regid;
	regval_t regval;

	// read arguments
	regno   = instr_rn2(instr);
	regtype = instr_regtype(instr);
	val64   = instr_2imm16(instr);

	// set regid and regval
    switch(regtype) {
        case INSTR_SETI_REGTYPE_GPR : regid  = REGID (SNONE, GPR   , regno, BOTH); regval = REGVAL(gpr   , val64, NOMASK64, NONE); break;
        case INSTR_SETI_REGTYPE_AN  : regid  = REGID (SNONE, ADDRAN, regno, BOTH); regval = REGVAL(addran, val64, ADDRREGMASK, NONE); break;
        case INSTR_SETI_REGTYPE_NN  : regid  = REGID (SNONE, ADDRNN, regno, BOTH); regval = REGVAL(addrnn, val64, ADDRREGMASK, NONE); break;
        case INSTR_SETI_REGTYPE_MN  : regid  = REGID (SNONE, ADDRMN, regno, BOTH); regval = REGVAL(addrmn, val64, ADDRREGMASK, NONE); break;
        default: sim_error("unknown value regtyp=%d in 'seti' instruction",(int)regtype); break;
    }
	regid.regno   = regno;

	// call delayed_reg_write
	delayed_reg_write (&regid, &regval, INSTR_DELAY_SETI, BYPASS_YES);

}

//
instr_def(CLR) {

	uint32_t regno;

	// read arguments
	regno = instr_rn(instr);

	// write zeros to address registers
	addrreg_result_write(REGID_TYPE_ADDRAN, regno, REGVAL(addran,     0,NOMASK64,NONE));
	addrreg_result_write(REGID_TYPE_ADDRNN, regno, REGVAL(addrnn,     1,NOMASK64,NONE));
	addrreg_result_write(REGID_TYPE_ADDRMN, regno, REGVAL(addrmn,0x1fff,NOMASK64,NONE));
}


//
instr_def(MOVE) {

	uint32_t dreg, sreg, dtype, stype;
	uint64_t data;
	regid_t  regid;
	regval_t regval;
	int move_delay;
	//FIXME move delay alias should be here
	move_delay = INSTR_DELAY_MOVE;

	// read arguments
	dreg  = instr_dreg(instr);
	sreg  = instr_sreg(instr);
	dtype = instr_dtyp(instr);
	stype = instr_styp(instr);

	// read source
	switch(stype) {
		case INSTR_MOVE_TYPE_GPR    :
										regid  = REGID (SNONE, GPR, sreg, BOTH);
										regval = reg_read_on_bypass(regid, reg_clockcount-1, READWRITE_CTRLREG_SRCID_CP2);
										data = regval.val.ui64;
										break;
		case INSTR_MOVE_TYPE_CTRL   :
										regid  = REGID (SNONE, CTRLREG, sreg, BOTH);
										regval = reg_read_on_bypass(regid, reg_clockcount-1, READWRITE_CTRLREG_SRCID_CP2);
										data = regval.val.ui64;
										if (CORRECT_READ_WRITE) { // alex
										  	switch (sreg) {
											case CTRLREG_LC:	data = kern.lc_cur;	break;
											case CTRLREG_LSP:	data = kern.lsp_cur;	break;
											case CTRLREG_LA:	data = kern.la_cur.ui32;break;
											case CTRLREG_PSP:	data = kern.psp_cur;	break;
											}
										}
										break;
		case INSTR_MOVE_TYPE_ADDRAN :
										regid  = REGID (SNONE, ADDRAN, sreg, BOTH);
										regval = reg_read_on_bypass(regid, reg_clockcount-1, READWRITE_CTRLREG_SRCID_CP2);
										data = regval.val.ui64;
										break;
		case INSTR_MOVE_TYPE_ADDRNN :
										regid  = REGID (SNONE, ADDRNN, sreg, BOTH);
										regval = reg_read_on_bypass(regid, reg_clockcount-1, READWRITE_CTRLREG_SRCID_CP2);
										data = regval.val.ui64;
										break;
		case INSTR_MOVE_TYPE_ADDRMN :
										regid  = REGID (SNONE, ADDRMN, sreg, BOTH);
										regval = reg_read_on_bypass(regid, reg_clockcount-1, READWRITE_CTRLREG_SRCID_CP2);
										data = regval.val.ui64;
										break;
		case INSTR_MOVE_TYPE_IREG   :   data = read_ireg (reg_clockcount,    sreg);  break;//FIXME should be read from pipe
		default: sim_error("unknown value styp=%d in 'move' instruction",(int)stype); break;
	}

	// write destination
	// set regid
	switch(dtype) {
		case INSTR_MOVE_TYPE_GPR     :
			regid  = REGID (SNONE, GPR, dreg, BOTH);
			regval = REGVAL(gpr    , data, NOMASK64, NONE);
			break;
		case INSTR_MOVE_TYPE_CTRL    :
			regid  = REGID (SNONE, CTRLREG, dreg, BOTH);
			regval = REGVAL(ctrlreg, data, NOMASK64, NONE);
			move_delay = INSTR_DELAY_MOVE_CTRL;
			if (CORRECT_READ_WRITE) { // alex
			  	switch (dreg) {
				case CTRLREG_LC: kern.lc_cur       = kern.lstack[kern.lsp_cur].lc      = regval.val.ctrlreg;
					break;
				case CTRLREG_LA: kern.la_cur.ui32  = kern.lstack[kern.lsp_cur].la.ui32 = regval.val.ctrlreg;
					break;
				case CTRLREG_PSP:
				case CTRLREG_LSP:
					return;		// read only
				}
			}
			break;
		case INSTR_MOVE_TYPE_ADDRAN  :
			regid  = REGID (SNONE, ADDRAN, dreg, BOTH);
			regval = REGVAL(addran , data, ADDRREGMASK, NONE);
			break;
		case INSTR_MOVE_TYPE_ADDRNN  :
			regid  = REGID (SNONE, ADDRNN, dreg, BOTH);
			regval = REGVAL(addrnn , data, ADDRREGMASK, NONE);
			break;
		case INSTR_MOVE_TYPE_ADDRMN  :
			regid  = REGID (SNONE, ADDRMN, dreg, BOTH);
			regval = REGVAL(addrmn , data, ADDRREGMASK, NONE);
			break;
		case INSTR_MOVE_TYPE_IREG    :
			regid  = REGID (SNONE, IREG, dreg, BOTH);
			regval = REGVAL(ireg , data, NOMASK64, NONE);
			break;
		default: sim_error("unknown value dtyp=%d in 'move' instruction",(int)dtype); break;
	}
	regid.regno   = dreg;

	// call delayed_reg_write
	delayed_reg_write (&regid, &regval, move_delay, BYPASS_YES);

}

// move from gpr or ireg to fpr
instr_def(MTFPR) {

	uint32_t dreg, sreg, stype, secn;
	uint64_t data;
	regid_t  regid;
	regval_t regval;

	// read arguments
	dreg = instr_2ft(instr);
	sreg = instr_gr(instr);
	secn = instr_secn(instr);
	stype = instr_mfpr(instr);

	// read source
	switch(stype) {
		case INSTR_MFPR_TYPE_GPR:
					regid = REGID (SNONE, GPR, sreg, BOTH);
					regval = reg_read_on_bypass(regid, reg_clockcount-1, READWRITE_CTRLREG_SRCID_CP2);
					data = regval.val.ui64;
					break;
		case INSTR_MFPR_TYPE_IREG:
					data = read_ireg (reg_clockcount,    sreg);//FIXME should be read from pipe
					break;
	}

	// prepare for writing destination
	regid  = REGID (secn, FPR, dreg, BOTH);
	regval = REGVAL (fpr.ui64, data, NOMASK64, NONE);

	// write destination
#if NUMBER_OF_EXESECT != 4
#error NUMBER_OF_EXESECT != 4
#endif
	if (
		(secn == 0) ||
		(secn == 1) ||
		(secn == 2) ||
		(secn == 3)
	) {

		// write one section
		regid.section = secn;
		// call delayed_reg_write
		delayed_reg_write (&regid, &regval, INSTR_DELAY_MTFPR, BYPASS_NO);

	} else if (secn == INSTR_MOVE_SECN_ALL) {

		// write all sections
		for (secn=0; secn<NUMBER_OF_EXESECT; secn++) {
			regid.section = secn;
			delayed_reg_write (&regid, &regval, INSTR_DELAY_MTFPR, BYPASS_NO);
		}
	}

}


// move from fpr to gpr or ireg
instr_def(MFFPR) {

	uint32_t dreg, sreg, secn, dtype;
	uint64_t data;
	regid_t  regid;
	regval_t regval;

	// read arguments
	sreg = instr_2ft(instr);
	dreg = instr_gr(instr);
	secn = instr_secn(instr);
	dtype = instr_mfpr(instr);

	// check secn field
	if (secn >= NUMBER_OF_EXESECT) {
		sim_error("invalid value secn=%d in 'mffpr' instruction",(int)secn);
		return;
	}

	// read source
	regid = REGID (secn, FPR, sreg, BOTH);
	regval = reg_read_ext(regid, reg_clockcount-1, READWRITE_CTRLREG_SRCID_CP2);
	data = regval.val.ui64;

	// write destination
	switch(dtype){
		case INSTR_MFPR_TYPE_GPR:
					regid = REGID (SNONE, GPR, dreg, BOTH);
					regval = REGVAL(gpr, data, NOMASK64, NONE);;
					break;
		case INSTR_MFPR_TYPE_IREG:
					regid  = REGID (SNONE, IREG, dreg, BOTH);
					regval = REGVAL(ireg , data, NOMASK64, NONE);
					break;
	}

	// call delayed_reg_write
	delayed_reg_write (&regid, &regval, INSTR_DELAY_MFFPR, BYPASS_YES);

}

//
void execute_instr_CAL_COMMON (instr_t instr) {

	// place calculation operation in pipe
	prockern_cal_newop (instr);
}

//
void execute_instr_LMEM_COMMON (instr_t instr) {

	// place memory operation in pipe
	prockern_lmem_newop (instr);
}



// convert 3 flags (less, equal, nan) into one flag
// according condition code cond
bool_t ccond_lessequalnan_to_cond (uint32_t cond, bool_t less, bool_t equal, bool_t nan) {

	bool_t res;

	res = FALSE;
	switch (cond) {
		case INSTR_CCOND_COND_T  : res = TRUE;                     		      break;
		case INSTR_CCOND_COND_UN : res = nan;                   	   	      break;
		case INSTR_CCOND_COND_LT : res = less;                     	    	  break;
		case INSTR_CCOND_COND_LE : res = (less || equal);          		      break;
		case INSTR_CCOND_COND_EQ : res = equal;                    		      break;
		case INSTR_CCOND_COND_NE : res = (! equal);             		      break;
		case INSTR_CCOND_COND_GT : res = ((! less) && (! equal) && (! nan));  break;
		case INSTR_CCOND_COND_GE : res = ((! less) && (! nan));               break;
		default: sim_error ("invalid condition code = 0x%02x", (int)cond);    break;
	}

	return res;
}

// return the result of comparison of a and b
// in word format according to condition cond
bool_t ccond_compare_w(int32_t a, int32_t b, uint32_t cond) {

	bool_t less, equal;

	less  = (a < b);
	equal = (a == b);

	return ccond_lessequalnan_to_cond(cond, less, equal, FALSE);
}

#define is_nan(_a) float32_is_nan(_a)

// return the result of comparison of a and b
// in float format according to condition cond
bool_t ccond_compare_s (uint32_t a, uint32_t b, uint32_t cond) {

	bool_t less, equal, nan;

	// not a number ?
	if (is_nan(a) || is_nan(b)) {

		// NaN
		// FIXME raise exception ?
		less  = FALSE;
		equal = FALSE;
		nan   = TRUE;

	} else {

		// compare numbers with softfloat
		less  = float32_lt(a,b, &f_status);
		equal = float32_eq(a,b, &f_status);
		nan   = FALSE;

	}

	return ccond_lessequalnan_to_cond (cond, less, equal, nan);
}

// write result of comparison val
// into cc bit of fccr
void ccond_write_cc(fccr_t *fccr, uint32_t cc, bool_t val) {
	*fccr = ((*fccr) & (~bit32(cc))) | ((val==TRUE) ? bit32(cc) : 0) | 0x1;
}

// c.cond.fmt instruction
void execute_instr_CCOND (instr_t instr) {

	int      sectnum;
	regid_t  regid;
	regval_t regval;
	bool_t   res;
	uint64_t mask;

	// input regs
	int fsnum, ftnum;
	fpr_t fs, ft;
	fccr_t fccr;

	// instruction fields
	uint32_t fmt, cc, cond;

	// read instruction fields
	fsnum = instr_fs(instr);
	ftnum = instr_ft(instr);
	fmt   = instr_fmt(instr);
	cc    = instr_cc1(instr);
	cond  = instr_cond(instr);

	// cc must be in 0...7
	if (cc > 7) {
		sim_error("cc=%d, must be 0...7", (int) cc);
		return;
	}

	// loop for all sections
	for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

		// clear exception bits in fcsr
//		flush_fcsr_cause(sectnum,CCOND);

		// read arguments
		fs = read_fpr (reg_clockcount-1, sectnum, fsnum);
		ft = read_fpr (reg_clockcount-1, sectnum, ftnum);
		fccr = read_fccr (sectnum,reg_clockcount-1);

		// check condition code
		if ((fccr & bit32(instr_cc(instr))) == 0) {
			// skip this instruction for this section
			continue;
		}

		mask = NOMASK64;

		// perform operation
		switch (fmt) {
			case INSTR_CCOND_FMT_S  :
					fs = adjust_operand(fs);
					ft = adjust_operand(ft);
					res = ccond_compare_s (fs.ui32.lo, ft.ui32.lo, cond);
					ccond_write_cc(&fccr, cc, res);
					mask = bit64(cc);
					break;
			case INSTR_CCOND_FMT_W  :
					res = ccond_compare_w (fs.ui32.lo, ft.ui32.lo, cond);
					ccond_write_cc(&fccr, cc, res);
					mask = bit64(cc);
					break;
			case INSTR_CCOND_FMT_PS :
					fs = adjust_operand(fs);
					ft = adjust_operand(ft);
					res = ccond_compare_s (fs.ui32.lo, ft.ui32.lo, cond); ccond_write_cc (&fccr, cc,   res);
					res = ccond_compare_s (fs.ui32.hi, ft.ui32.hi, cond); ccond_write_cc (&fccr, cc+1, res);
					mask = bit64(cc) | bit64(cc+1);
					break;
			case INSTR_CCOND_FMT_PW :
					res = ccond_compare_w (fs.ui32.lo, ft.ui32.lo, cond); ccond_write_cc(&fccr, cc,   res);
					res = ccond_compare_w (fs.ui32.hi, ft.ui32.hi, cond); ccond_write_cc(&fccr, cc+1, res);
					mask = bit64(cc) | bit64(cc+1);
					break;
			default: sim_error("unknown fmt=0x%02x in c.cond.fmt instruction", (int) fmt); break;
		}

		// write result
		regid  = REGID (sectnum, FCCR, REGNO_NONE, NONE);
		regval = REGVAL (fccr, fccr, mask, NONE);
		delayed_reg_write(&regid, &regval, INSTR_DELAY_COND, BYPASS_NO);

	}
}


// 'mfc' instruction
void execute_instr_MFC (instr_t instr) {

	int sectnum;
	uint32_t data32;
	int fdnum;
	uint32_t cntrlreg;
	regid_t  regid;
	regval_t regval;

	// get instr fields
	fdnum    = instr_fd(instr);
	cntrlreg = instr_cntrlreg(instr);

	// loop for all sections
	for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

		// check condition code
		if (instr_cc(instr) != 0) {
			data32 = read_fccr (sectnum,reg_clockcount-1);
			if ((data32 & bit32(instr_cc(instr))) == 0) {
				// skip this instruction for this section
				continue;
			}
		}

		// read control register
		switch (cntrlreg) {
			case INSTR_MFCMTC_CNTRLREG_FCSR :  data32 = read_fcsr (sectnum,reg_clockcount-1).ui32; break;
			case INSTR_MFCMTC_CNTRLREG_FCCR :  data32 = read_fccr (sectnum,reg_clockcount-1); break;
			default: sim_error("unknown value cntrlreg=0x%x in 'mfc' instruction",(int)cntrlreg); break;
		}

		// write result (with delayed_reg_write)
		regid  = REGID (sectnum, FPR, fdnum, BOTH);
		regval = ((regval_t) {
			.val.fpr.ui32.hi = 0,
			.val.fpr.ui32.lo = data32,
			.mask            = NOMASK64,
			.srcid           = READWRITE_CTRLREG_SRCID_NONE
		});
		delayed_reg_write (&regid, &regval, INSTR_DELAY_MFC, BYPASS_NO);
	}

}



// 'mtc' instruction
void execute_instr_MTC (instr_t instr) {

	int sectnum;
	uint32_t data32;
	int fdnum;
	uint32_t cntrlreg;
	regid_t  regid;
	regval_t regval;
	reg_status_t status;
	status.ui32 = 0;

	// get instr fields
	fdnum    = instr_fd(instr);
	cntrlreg = instr_cntrlreg(instr);

	// loop for all sections
	for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

		// check condition code
		if (instr_cc(instr) != 0) {
			data32 = read_fccr (sectnum,reg_clockcount-1);
			if ((data32 & bit32(instr_cc(instr))) == 0) {
				// skip this instruction for this section
				continue;
			}
		}

		// read fp register
		data32 = read_fpr (reg_clockcount-1,sectnum, fdnum) .ui32.lo;

		// set regid and regval
		switch(cntrlreg) {
			case INSTR_MFCMTC_CNTRLREG_FCSR :
				regid  = REGID (sectnum, FCSR, REGNO_NONE, NONE);
				regval = REGVAL (fcsr.ui32, data32, FCSRMASK, NONE);
				//set status FPE fields on FCSR write //FIXME delayed write ?
				status.ui32 |= ((data32 & 0xfc) << 14);
				break;
			case INSTR_MFCMTC_CNTRLREG_FCCR :
//sim_printf("myout fccr = %x", kern.exesect[sectnum].fccr);
				regid  = REGID (sectnum, FCCR, REGNO_NONE, NONE);
				regval = REGVAL (fccr, data32 | 0x1, FCCRMASK, NONE);
				break;
			default: sim_error("unknown value cntrlreg=0x%x in 'mtc' instruction",(int)cntrlreg); break;
		}

		// write result (with delayed_reg_write)
		delayed_reg_write (&regid, &regval, INSTR_DELAY_MTC, BYPASS_YES);
	}

	regid  = REGID (0, CTRLREG, CTRLREG_STATUS, NONE);
	regval = REGVAL (ctrlreg, status.ui32, REG_STATUS_FPE.ui32, CP2);
	delayed_reg_write (&regid, &regval, INSTR_DELAY_MTC, BYPASS_NO);

}

void execute_instr_MVMADD(instr_t instr)
{
	fpr_t fd, fs, fss, ft, ftmp;

	int fsnum = instr_fs(instr); // get fpr numbers
	int ftnum = instr_ft(instr);
	int fdnum = instr_fd(instr);

	uint32_t exc_sec_acc = 0;
	uint32_t exc_acc = 0;
	exc_sec_acc = exc_sec_acc;

	int sectnum;
	for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) // loop for all sections
		{
			fs  = read_fpr(reg_clockcount-1,sectnum,fsnum);   // read arguments
			fss = read_fpr(reg_clockcount-1,sectnum,fsnum+1);
			ft  = read_fpr(reg_clockcount-1,sectnum,ftnum);
			fd  = read_fpr(reg_clockcount-1,sectnum,fdnum);

			// check condition code
			if (instr_cc(instr) != 0){
				  uint32_t data32 = read_fccr (sectnum,reg_clockcount-1);
					if ((data32 & bit32(instr_cc(instr))) == 0)   // skip this instruction for this section
						continue;
			}

//			flush_fcsr_cause(sectnum,MVMADD);

			// perform operation
			exc_sec_acc = 0;
			f_status.float_exception_flags = 0;
			fs = adjust_operand(fs);
			ft = adjust_operand(ft);
			fss = adjust_operand(fss);
			fd = adjust_operand(fd);

			ftmp.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.lo);
			ftmp.ui32.lo = adjust_result(ftmp.ui32.lo);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.hi = float_add(ftmp.ui32.hi, ftmp.ui32.lo);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			fd.ui32.hi = float_add(fd.ui32.hi, ftmp.ui32.hi);
			fd.ui32.hi = adjust_result(fd.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.hi = float_mul(fss.ui32.hi, ft.ui32.hi);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.lo = float_mul(fss.ui32.lo, ft.ui32.lo);
			ftmp.ui32.lo = adjust_result(ftmp.ui32.lo);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.hi = float_add(ftmp.ui32.hi, ftmp.ui32.lo);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			fd.ui32.lo = float_add(fd.ui32.lo, ftmp.ui32.hi);
			fd.ui32.lo = adjust_result(fd.ui32.lo);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			exc_sec_acc = adjust_exception_flags(exc_sec_acc);
			f_status.float_exception_flags = exc_sec_acc;
			exc_acc |= exc_sec_acc;

//      fd.ui32.hi = float_add( fd.ui32.hi, float_add(float_mul(fs.ui32.hi , ft.ui32.hi), float_mul(fs.ui32.lo , ft.ui32.lo)) );
//      fd.ui32.lo = float_add( fd.ui32.lo, float_add(float_mul(fss.ui32.hi, ft.ui32.hi), float_mul(fss.ui32.lo, ft.ui32.lo)) );

		// update fcsr
		if(if_update_fcsr(instr) == 1)
		   	update_fcsr(sectnum,MVMADD);

		// write result
        float_result_write(sectnum,fdnum,fd,BYPASS_NO);
	}

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}

void execute_instr_MVMSUB(instr_t instr)
{
	fpr_t fd, fs, fss, ft, ftmp;

	int fsnum = instr_fs(instr); // get fpr numbers
	int ftnum = instr_ft(instr);
	int fdnum = instr_fd(instr);

	uint32_t exc_sec_acc = 0;
	uint32_t exc_acc = 0;
	exc_sec_acc = exc_sec_acc;

	int sectnum;
	for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) // loop for all sections
		{
			fs  = read_fpr(reg_clockcount-1,sectnum,fsnum);   // read arguments
			fss = read_fpr(reg_clockcount-1,sectnum,fsnum+1);
			ft  = read_fpr(reg_clockcount-1,sectnum,ftnum);
			fd  = read_fpr(reg_clockcount-1,sectnum,fdnum);

			// check condition code
			if (instr_cc(instr) != 0){
				  uint32_t data32 = read_fccr (sectnum,reg_clockcount-1);
					if ((data32 & bit32(instr_cc(instr))) == 0)   // skip this instruction for this section
						continue;
			}

//			flush_fcsr_cause(sectnum,MVMADD);

			// perform operation
			exc_sec_acc = 0;
			f_status.float_exception_flags = 0;
			fs = adjust_operand(fs);
			ft = adjust_operand(ft);
			fss = adjust_operand(fss);
			fd = adjust_operand(fd);

			ftmp.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.lo);
			ftmp.ui32.lo = adjust_result(ftmp.ui32.lo);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.hi = float_add(ftmp.ui32.hi, ftmp.ui32.lo);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			fd.ui32.hi = float_sub(fd.ui32.hi, ftmp.ui32.hi);
			fd.ui32.hi = adjust_result(fd.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.hi = float_mul(fss.ui32.hi, ft.ui32.hi);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.lo = float_mul(fss.ui32.lo, ft.ui32.lo);
			ftmp.ui32.lo = adjust_result(ftmp.ui32.lo);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.hi = float_add(ftmp.ui32.hi, ftmp.ui32.lo);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			fd.ui32.lo = float_sub(fd.ui32.lo, ftmp.ui32.hi);
			fd.ui32.lo = adjust_result(fd.ui32.lo);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			exc_sec_acc = adjust_exception_flags(exc_sec_acc);
			f_status.float_exception_flags = exc_sec_acc;
			exc_acc |= exc_sec_acc;

/*      fd.ui32.hi = float_sub( fd.ui32.hi, float_add(float_mul(fs.ui32.hi , ft.ui32.hi), float_mul(fs.ui32.lo , ft.ui32.lo)) );
      fd.ui32.lo = float_sub( fd.ui32.lo, float_add(float_mul(fss.ui32.hi, ft.ui32.hi), float_mul(fss.ui32.lo, ft.ui32.lo)) ); */

		// update fcsr
		if(if_update_fcsr(instr) == 1)
		   	update_fcsr(sectnum,MVMADD);

		// write result
        float_result_write(sectnum,fdnum,fd,BYPASS_NO);
	}

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}

void execute_instr_MTVMADD(instr_t instr)
{
	fpr_t fd, fs, fss, ft, ftmp;

	int fsnum = instr_fs(instr); // get fpr numbers
	int ftnum = instr_ft(instr);
	int fdnum = instr_fd(instr);

	uint32_t exc_sec_acc = 0;
	uint32_t exc_acc = 0;
	exc_sec_acc = exc_sec_acc;

	int sectnum;
	for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) // loop for all sections
		{
			fs  = read_fpr(reg_clockcount-1,sectnum,fsnum);   // read arguments
			fss = read_fpr(reg_clockcount-1,sectnum,fsnum+1);
			ft  = read_fpr(reg_clockcount-1,sectnum,ftnum);
			fd  = read_fpr(reg_clockcount-1,sectnum,fdnum);

			// check condition code
			if (instr_cc(instr) != 0){
				  uint32_t data32 = read_fccr (sectnum,reg_clockcount-1);
					if ((data32 & bit32(instr_cc(instr))) == 0)   // skip this instruction for this section
						continue;
			}

//			flush_fcsr_cause(sectnum,MTVMADD);

			// perform operation
			exc_sec_acc = 0;
			f_status.float_exception_flags = 0;
			fs = adjust_operand(fs);
			ft = adjust_operand(ft);
			fss = adjust_operand(fss);
			fd = adjust_operand(fd);

			ftmp.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.lo = float_mul(fss.ui32.hi, ft.ui32.lo);
			ftmp.ui32.lo = adjust_result(ftmp.ui32.lo);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.hi = float_add(ftmp.ui32.hi, ftmp.ui32.lo);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			fd.ui32.hi = float_add(fd.ui32.hi, ftmp.ui32.hi);
			fd.ui32.hi = adjust_result(fd.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.hi = float_mul(fs.ui32.lo, ft.ui32.hi);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.lo = float_mul(fss.ui32.lo, ft.ui32.lo);
			ftmp.ui32.lo = adjust_result(ftmp.ui32.lo);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.hi = float_add(ftmp.ui32.hi, ftmp.ui32.lo);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			fd.ui32.lo = float_add(fd.ui32.lo, ftmp.ui32.hi);
			fd.ui32.lo = adjust_result(fd.ui32.lo);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			exc_sec_acc = adjust_exception_flags(exc_sec_acc);
			f_status.float_exception_flags = exc_sec_acc;
			exc_acc |= exc_sec_acc;

		// update fcsr
		if(if_update_fcsr(instr) == 1)
		   	update_fcsr(sectnum,MTVMADD);

		// write result
        float_result_write(sectnum,fdnum,fd,BYPASS_NO);
	}

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}

void execute_instr_MTVMSUB(instr_t instr)
{
	fpr_t fd, fs, fss, ft, ftmp;

	int fsnum = instr_fs(instr); // get fpr numbers
	int ftnum = instr_ft(instr);
	int fdnum = instr_fd(instr);

	uint32_t exc_sec_acc = 0;
	uint32_t exc_acc = 0;
	exc_sec_acc = exc_sec_acc;

	int sectnum;
	for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) // loop for all sections
		{
			fs  = read_fpr(reg_clockcount-1,sectnum,fsnum);   // read arguments
			fss = read_fpr(reg_clockcount-1,sectnum,fsnum+1);
			ft  = read_fpr(reg_clockcount-1,sectnum,ftnum);
			fd  = read_fpr(reg_clockcount-1,sectnum,fdnum);

			// check condition code
			if (instr_cc(instr) != 0){
				  uint32_t data32 = read_fccr (sectnum,reg_clockcount-1);
					if ((data32 & bit32(instr_cc(instr))) == 0)   // skip this instruction for this section
						continue;
			}

//			flush_fcsr_cause(sectnum,MTVMSUB);

			// perform operation
			exc_sec_acc = 0;
			f_status.float_exception_flags = 0;
			fs = adjust_operand(fs);
			ft = adjust_operand(ft);
			fss = adjust_operand(fss);
			fd = adjust_operand(fd);

			ftmp.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.lo = float_mul(fss.ui32.hi, ft.ui32.lo);
			ftmp.ui32.lo = adjust_result(ftmp.ui32.lo);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.hi = float_add(ftmp.ui32.hi, ftmp.ui32.lo);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			fd.ui32.hi = float_sub(fd.ui32.hi, ftmp.ui32.hi);
			fd.ui32.hi = adjust_result(fd.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.hi = float_mul(fs.ui32.lo, ft.ui32.hi);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.lo = float_mul(fss.ui32.lo, ft.ui32.lo);
			ftmp.ui32.lo = adjust_result(ftmp.ui32.lo);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			ftmp.ui32.hi = float_add(ftmp.ui32.hi, ftmp.ui32.lo);
			ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			fd.ui32.lo = float_sub(fd.ui32.lo, ftmp.ui32.hi);
			fd.ui32.lo = adjust_result(fd.ui32.lo);
			exc_sec_acc |= f_status.float_exception_flags;
			f_status.float_exception_flags = 0;

			exc_sec_acc = adjust_exception_flags(exc_sec_acc);
			f_status.float_exception_flags = exc_sec_acc;
			exc_acc |= exc_sec_acc;

		// update fcsr
		if(if_update_fcsr(instr) == 1)
		   	update_fcsr(sectnum,MTVMSUB);

		// write result
        float_result_write(sectnum,fdnum,fd,BYPASS_NO);
	}

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}

// 'check_dma' instruction
void execute_instr_CHECK_DMA (instr_t instr) {

	uint32_t reg;
	uint64_t data;
	regid_t  regid;
	regval_t regval;

	// get instr fields
	reg = instr_gt(instr);

	// call dma interface function. FIXME: generate warning on else case.
	if (mch->dma_ifptr_check_dma != NULL)
		data = (mch->dma_ifptr_check_dma)(mch->dma_ctx);
	else
		data = 0;

	// write destination
	regid  = REGID (SNONE, GPR, reg, NONE);
	regval = REGVAL (gpr, data, NOMASK64, NONE);

	// call delayed_reg_write
	delayed_reg_write (&regid, &regval, INSTR_DELAY_CHECK_DMA, BYPASS_YES);

}

// 'start_dma' instruction
void execute_instr_START_DMA (instr_t instr) {

	kern.start_dma = 1;

}

void execute_instr_QSDOT(instr_t instr)
{
	fpr_t fd, fs, fss, ft, ftt, ftmp, ftmp1;

	int fsnum = instr_fs(instr); // get fpr numbers
	int ftnum = instr_ft(instr);
	int fdnum = instr_fd(instr);

	uint32_t exc_sec_acc = 0;
	uint32_t exc_acc = 0;
	exc_sec_acc = exc_sec_acc;

	int sectnum;
	for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++){ // loop for all sections
		fs  = read_fpr(reg_clockcount-1,sectnum,fsnum);   // read arguments
		fss = read_fpr(reg_clockcount-1,sectnum,fsnum+1);
		ft  = read_fpr(reg_clockcount-1,sectnum,ftnum);
		ftt = read_fpr(reg_clockcount-1,sectnum,ftnum+1);
		fd  = read_fpr(reg_clockcount-1,sectnum,fdnum);

//		flush_fcsr_cause(sectnum,QSDOT);

		// perform operation
		exc_sec_acc = 0;
		f_status.float_exception_flags = 0;
		fs = adjust_operand(fs);
		ft = adjust_operand(ft);
		fss = adjust_operand(fss);
		ftt = adjust_operand(ftt);

		ftmp.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
		ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
		exc_sec_acc |= f_status.float_exception_flags;
		f_status.float_exception_flags = 0;

		ftmp.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.lo);
		ftmp.ui32.lo = adjust_result(ftmp.ui32.lo);
		exc_sec_acc |= f_status.float_exception_flags;
		f_status.float_exception_flags = 0;

		ftmp.ui32.lo = float_add(ftmp.ui32.lo, ftmp.ui32.hi);
		ftmp.ui32.lo = adjust_result(ftmp.ui32.lo);
		exc_sec_acc |= f_status.float_exception_flags;
		f_status.float_exception_flags = 0;

		ftmp1.ui32.lo = float_mul(fss.ui32.lo, ftt.ui32.lo);
		ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
		exc_sec_acc |= f_status.float_exception_flags;
		f_status.float_exception_flags = 0;

		ftmp1.ui32.hi = float_mul(fss.ui32.hi, ftt.ui32.hi);
		ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
		exc_sec_acc |= f_status.float_exception_flags;
		f_status.float_exception_flags = 0;

		ftmp1.ui32.lo = float_add(ftmp1.ui32.lo, ftmp1.ui32.hi);
		ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
		exc_sec_acc |= f_status.float_exception_flags;
		f_status.float_exception_flags = 0;

		ftmp.ui32.lo = float_add(ftmp.ui32.lo, ftmp1.ui32.lo);
		ftmp.ui32.lo = adjust_result(ftmp.ui32.lo);
		exc_sec_acc |= f_status.float_exception_flags;
		f_status.float_exception_flags = 0;

		exc_sec_acc = adjust_exception_flags(exc_sec_acc);
		f_status.float_exception_flags = exc_sec_acc;
		exc_acc |= exc_sec_acc;

		update_fcsr(sectnum,QSDOT);

		if(instr_mode(instr) == 0)
			fd.ui32.lo = ftmp.ui32.lo;
		else
			fd.ui32.hi = ftmp.ui32.lo;

		float_result_write(sectnum,fdnum,fd,BYPASS_NO);
	}
	update_reg_status_fpe (exc_acc);
}

void execute_instr_UNPCK16WSTOPS(instr_t instr)
{
	fpr_t fd, fs, ft;
	uint32_t exc_acc = 0;
	uint32_t tmp = 0;

	int fsnum = instr_fs(instr); // get fpr numbers
	int ftnum = instr_ft(instr);
	int fdnum = instr_fd(instr);

	int sectnum;
	for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++){ // loop for all sections
		fd  = read_fpr(reg_clockcount-1,sectnum,fdnum);
		fs  = read_fpr(reg_clockcount-1,sectnum,fsnum);   // read arguments
		ft  = read_fpr(reg_clockcount-1,sectnum,ftnum);

		if (instr_cc(instr) != 0)  // check condition code
			{
				uint32_t data32 = read_fccr (sectnum,reg_clockcount-1);
		    	if ((data32 & bit32(instr_cc(instr))) == 0)   // skip this instruction for this section
				continue;
			}

		// perform operation
		{
			tmp = (ft.ui32.hi >> 16) & 0xffff;
			tmp = ((ft.ui32.hi&0x80000000) != 0) ? tmp | 0xffff0000 : tmp;
			fd.ui32.hi = int32_to_float32(tmp, &f_status);

			tmp = ft.ui32.hi & 0xffff;
			tmp = ((ft.ui32.hi&0x8000) != 0) ? tmp | 0xffff0000 : tmp;
			fd.ui32.lo = int32_to_float32(tmp, &f_status);

			tmp = (ft.ui32.lo >> 16) & 0xffff;
			tmp = ((ft.ui32.lo&0x80000000) != 0) ? tmp | 0xffff0000 : tmp;
			fs.ui32.hi = int32_to_float32(tmp, &f_status);

			tmp = ft.ui32.lo & 0xffff;
			tmp = ((ft.ui32.lo&0x8000) != 0) ? tmp | 0xffff0000 : tmp;
			fs.ui32.lo = int32_to_float32(tmp, &f_status);
		}
		update_fcsr(sectnum,UNPCK16WSTOPS);

		float_result_write(sectnum,fdnum,fd,BYPASS_NO);
		float_result_write(sectnum,fsnum,fs,BYPASS_NO);
	}
	update_reg_status_fpe (exc_acc);
}

//
instr_def(PSPRMSGN0)
{
	fpr_t fd, fdd, ft, ftt;
	regid_t  regid;
	regval_t regval;

	uint32_t tmp;

	int ftnum = instr_ft2(instr); // get fpr numbers
	int fdnum = instr_fd2(instr);

	int sectnum;
	for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) // loop for all sections
		{
			ft  = read_fpr(reg_clockcount-1,sectnum,ftnum);   // read arguments
			ftt = read_fpr(reg_clockcount-1,sectnum,ftnum+1);
			fd  = read_fpr(reg_clockcount-1,sectnum,fdnum);
			fdd = read_fpr(reg_clockcount-1,sectnum,fdnum+1);

		if (instr_cc(instr) != 0)  // check condition code
			{
				uint32_t data32 = read_fccr (sectnum,reg_clockcount-1);
		    	if ((data32 & bit32(instr_cc(instr))) == 0)   // skip this instruction for this section
				continue;
			}

		if(instr_sel1(instr) == 0)
			tmp = ft.ui32.lo;
		else if(instr_sel1(instr) == 1)
			tmp = ft.ui32.hi;
		else if(instr_sel1(instr) == 2)
			tmp = ftt.ui32.lo;
		else
			tmp = ftt.ui32.hi;
		if(instr_n1(instr) == 1)
			fd.ui32.lo = (tmp ^ 0x80000000);
		else
			fd.ui32.lo = tmp;

		if(instr_sel2(instr) == 0)
			tmp = ft.ui32.lo;
		else if(instr_sel2(instr) == 1)
			tmp = ft.ui32.hi;
		else if(instr_sel2(instr) == 2)
			tmp = ftt.ui32.lo;
		else
			 tmp = ftt.ui32.hi;
		if(instr_n2(instr) == 1)
			fd.ui32.hi = (tmp ^ 0x80000000);
		else
			fd.ui32.hi = tmp;

		if(instr_sel3(instr) == 0)
			tmp = ft.ui32.lo;
		else if(instr_sel3(instr) == 1)
			tmp = ft.ui32.hi;
		else if(instr_sel3(instr) == 2)
			tmp = ftt.ui32.lo;
		else
			tmp = ftt.ui32.hi;
		if(instr_n3(instr) == 1)
			fdd.ui32.lo = (tmp ^ 0x80000000);
		else
			fdd.ui32.lo = tmp;

		if(instr_sel4(instr) == 0)
			tmp = ft.ui32.lo;
		else if(instr_sel4(instr) == 1)
			tmp = ft.ui32.hi;
		else if(instr_sel4(instr) == 2)
			 tmp = ftt.ui32.lo;
		else
			 tmp = ftt.ui32.hi;
		if(instr_n4(instr) == 1)
			fdd.ui32.hi = (tmp ^ 0x80000000);
		else
			fdd.ui32.hi = tmp;

		// write regs
		regid.section = sectnum;
		regid.type    = REGID_TYPE_FPR;
		regid.regno   = fdnum;
		regid.half    = REGID_HALF_BOTH;
		regval = REGVAL (fpr, fd, NOMASK64, NONE);

		delayed_reg_write (&regid, &regval, INSTR_DELAY_PSPRMSGN, BYPASS_NO);

 		regid.section = sectnum;
		regid.type    = REGID_TYPE_FPR;
		regid.regno   = fdnum+1;
		regid.half    = REGID_HALF_BOTH;
		regval = REGVAL (fpr, fdd, NOMASK64, NONE);

		delayed_reg_write (&regid, &regval, INSTR_DELAY_PSPRMSGN, BYPASS_NO);
   }
}

instr_def(PSPRMSGN1)
{
	fpr_t fd, fdd, ft, ftt;
	regid_t  regid;
	regval_t regval;

	uint32_t tmp;

	int ftnum = instr_ft2(instr); // get fpr numbers
	int fdnum = instr_fd2(instr) + 32;

	int sectnum;
	for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) // loop for all sections
		{
			ft  = read_fpr(reg_clockcount-1,sectnum,ftnum);   // read arguments
			ftt = read_fpr(reg_clockcount-1,sectnum,ftnum+1);
			fd  = read_fpr(reg_clockcount-1,sectnum,fdnum);
			fdd = read_fpr(reg_clockcount-1,sectnum,fdnum+1);

		if (instr_cc(instr) != 0)  // check condition code
			{
				uint32_t data32 = read_fccr (sectnum,reg_clockcount-1);
		    	if ((data32 & bit32(instr_cc(instr))) == 0)   // skip this instruction for this section
				continue;
			}

		if(instr_sel1(instr) == 0)
			tmp = ft.ui32.lo;
		else if(instr_sel1(instr) == 1)
			tmp = ft.ui32.hi;
		else if(instr_sel1(instr) == 2)
			tmp = ftt.ui32.lo;
		else
			tmp = ftt.ui32.hi;
		if(instr_n1(instr) == 1)
			fd.ui32.lo = (tmp ^ 0x80000000);
		else
			fd.ui32.lo = tmp;

		if(instr_sel2(instr) == 0)
			tmp = ft.ui32.lo;
		else if(instr_sel2(instr) == 1)
			tmp = ft.ui32.hi;
		else if(instr_sel2(instr) == 2)
			tmp = ftt.ui32.lo;
		else
			tmp = ftt.ui32.hi;
		if(instr_n2(instr) == 1)
			fd.ui32.hi = (tmp ^ 0x80000000);
		else
			fd.ui32.hi = tmp;

		if(instr_sel3(instr) == 0)
			tmp = ft.ui32.lo;
		else if(instr_sel3(instr) == 1)
			tmp = ft.ui32.hi;
		else if(instr_sel3(instr) == 2)
			tmp = ftt.ui32.lo;
		else
			tmp = ftt.ui32.hi;
		if(instr_n3(instr) == 1)
			fdd.ui32.lo = (tmp ^ 0x80000000);
		else
			fdd.ui32.lo = tmp;

		if(instr_sel4(instr) == 0)
			tmp = ft.ui32.lo;
		else if(instr_sel4(instr) == 1)
			tmp = ft.ui32.hi;
		else if(instr_sel4(instr) == 2)
			 tmp = ftt.ui32.lo;
		else
			 tmp = ftt.ui32.hi;
		if(instr_n4(instr) == 1)
			fdd.ui32.hi = (tmp ^ 0x80000000);
		else
			fdd.ui32.hi = tmp;

		// write regs
		regid.section = sectnum;
		regid.type    = REGID_TYPE_FPR;
		regid.regno   = fdnum;
		regid.half    = REGID_HALF_BOTH;
		regval = REGVAL (fpr, fd, NOMASK64, NONE);

		delayed_reg_write (&regid, &regval, INSTR_DELAY_PSPRMSGN, BYPASS_NO);

 		regid.section = sectnum;
		regid.type    = REGID_TYPE_FPR;
		regid.regno   = fdnum+1;
		regid.half    = REGID_HALF_BOTH;
		regval = REGVAL (fpr, fdd, NOMASK64, NONE);

		delayed_reg_write (&regid, &regval, INSTR_DELAY_PSPRMSGN, BYPASS_NO);
   }
}

void execute_instr_GETCOEFF

 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   //no regs

   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;







   // get fpr numbers
   fdnum = instr_fd(instr);






   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

	regid_t  regid;
 regval_t regval;
 reg_rind_t rind;
 union f32 { float f; uint32_t u; };
 union f32 sin_a32, cos_a32;

 regval = reg_read_on_bypass(REGID(SNONE, CTRLREG, CTRLREG_RIND, NONE), reg_clockcount-1, READWRITE_CTRLREG_SRCID_CP2);
 rind.ui32 = regval.val.ctrlreg;
// sim_printf("myout rind = %x", rind.ui32);

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments





	// check condition code
	if (instr_cc(instr) != 0) {
		data32 = read_fccr (sectnum,reg_clockcount-1);
		if ((data32 & bit32(instr_cc(instr))) == 0) {
			// skip this instruction for this section
			continue;
		}
	}

	// clear exception bits in fcsr
//	if(if_update_fcsr(instr) == 1)
//		flush_fcsr_cause(sectnum,GETCOEFF);

	// perform operation
 double angle_d;
 float sin_a, cos_a;
 angle_d = (rind.fields.b13 ? (0x2000 - rind.fields.addr) : rind.fields.addr) & 0x1FFF;
 if ((rind.ui32 & 0x3FFF) == 0x2000) {
	angle_d = 0x2000;
 }
 angle_d *= 2 * M_PI / 0x10000;
 sin_a = sin(angle_d);
 cos_a = cos(angle_d);
 sin_a32.f = sin_a;
 cos_a32.f = cos_a;
 fd.ui32.hi = (rind.fields.b13 ^ rind.fields.b14) ? (sin_a32.u) : (cos_a32.u);
 fd.ui32.lo = (rind.fields.b13 ^ rind.fields.b14) ? (cos_a32.u) : (sin_a32.u);
 if (rind.fields.b14 ^ rind.fields.b15 ) {fd.ui32.hi = fd.ui32.hi ^ 0x80000000;}
 if (rind.fields.b15 ^ kern.control.fields.i) {fd.ui32.lo = fd.ui32.lo ^ 0x80000000;}








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,GETCOEFF);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);



  }
  rind.ui32 = ((rind.ui32 + kern.rstep)&kern.rmask) | (rind.ui32 & ~kern.rmask);
  regid  = REGID (SNONE, CTRLREG, CTRLREG_RIND, NONE);
  regval = REGVAL(ctrlreg, rind.ui32 , NOMASK64, NONE);
  delayed_reg_write (&regid, &regval, INSTR_DELAY_MOVE_CTRL, BYPASS_YES);

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}



void execute_instr_CFLY

 (instr_t instr) {

   int sectnum;
   uint32_t data32;

   int fdnum;
   int fsnum;

   fpr_t fd;
   fpr_t fs;

   fpr_t ftt;
   fpr_t ftmp1;
   fpr_t ftmp2;
   fpr_t ftmp3;
   fpr_t ftmp4;
   fpr_t ftmp5;







   // get fpr numbers



   fdnum = instr_fd(instr);
   fsnum = instr_fs(instr);




   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

 regid_t  regid;
 regval_t regval;
 reg_rind_t rind;
 reg_rstep_t rstep;
 reg_rmask_t rmask;
 union f32 { float f; uint32_t u; };
 union f32 sin_a32, cos_a32;
 double angle_d;
 float sin_a, cos_a;
 regval = reg_read_on_bypass(REGID(SNONE, CTRLREG, CTRLREG_RIND, NONE), reg_clockcount-1, READWRITE_CTRLREG_SRCID_CP2);
 rind.ui32 = regval.val.ctrlreg;


   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments


     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);
   fs = read_fpr(reg_clockcount-1,sectnum,fsnum);




	// check condition code
	if (instr_cc(instr) != 0) {
		data32 = read_fccr (sectnum,reg_clockcount-1);
		if ((data32 & bit32(instr_cc(instr))) == 0) {
			// skip this instruction for this section
			continue;
		}
	}

	// clear exception bits in fcsr
//	if(if_update_fcsr(instr) == 1)
//		flush_fcsr_cause(sectnum,CFLY);

	// perform operation
	exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 angle_d = (rind.fields.b13 ? (0x2000 - rind.fields.addr) : rind.fields.addr) & 0x1FFF;
 if ((rind.ui32 & 0x3FFF) == 0x2000) {
	angle_d = 0x2000;
 }
 angle_d *= 2 * M_PI / 0x10000;
 sin_a = sin(angle_d);
 cos_a = cos(angle_d);
 sin_a32.f = sin_a;
 cos_a32.f = cos_a;
 ftt.ui32.hi = (rind.fields.b13 ^ rind.fields.b14) ? (sin_a32.u) : (cos_a32.u);
 ftt.ui32.lo = (rind.fields.b13 ^ rind.fields.b14) ? (cos_a32.u) : (sin_a32.u);
 if (rind.fields.b14 ^ rind.fields.b15 ) {ftt.ui32.hi = ftt.ui32.hi ^ 0x80000000;}
 if (rind.fields.b15 ^ kern.control.fields.i) {ftt.ui32.lo = ftt.ui32.lo ^ 0x80000000;}
 fs = adjust_operand(fs);
 ftt = adjust_operand(ftt);
 fd = adjust_operand(fd);
// ftmp1.ui32.hi = float_add(fd.ui32.hi,float_sub(float_mul(fs.ui32.hi, ftt.ui32.hi),float_mul(fs.ui32.lo,ftt.ui32.lo)));

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ftt.ui32.hi);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp2.ui32.hi = float_mul(fs.ui32.lo,ftt.ui32.lo);
 ftmp2.ui32.hi = adjust_result(ftmp2.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp3.ui32.hi = float_sub(ftmp1.ui32.hi, ftmp2.ui32.hi);
 ftmp3.ui32.hi = adjust_result(ftmp3.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp4.ui32.hi = float_add(fd.ui32.hi, ftmp3.ui32.hi);
 ftmp4.ui32.hi = adjust_result(ftmp4.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;


// ftmp1.ui32.lo = float_add(fd.ui32.lo,float_add(float_mul(fs.ui32.hi, ftt.ui32.lo),float_mul(fs.ui32.lo,ftt.ui32.hi)));

 ftmp1.ui32.lo = float_mul(fs.ui32.hi, ftt.ui32.lo);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp2.ui32.lo = float_mul(fs.ui32.lo,ftt.ui32.hi);
 ftmp2.ui32.lo = adjust_result(ftmp2.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp3.ui32.lo = float_add(ftmp1.ui32.lo, ftmp2.ui32.lo);
 ftmp3.ui32.lo = adjust_result(ftmp3.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp4.ui32.lo = float_add(fd.ui32.lo, ftmp3.ui32.lo);
 ftmp4.ui32.lo = adjust_result(ftmp4.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;


// ftmp2.ui32.hi = float_sub(fd.ui32.hi,float_sub(float_mul(fs.ui32.hi, ftt.ui32.hi),float_mul(fs.ui32.lo,ftt.ui32.lo)));

 ftmp5.ui32.hi = float_sub(fd.ui32.hi, ftmp3.ui32.hi);
 ftmp5.ui32.hi = adjust_result(ftmp5.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;


// ftmp2.ui32.lo = float_sub(fd.ui32.lo,float_add(float_mul(fs.ui32.hi, ftt.ui32.lo),float_mul(fs.ui32.lo,ftt.ui32.hi)));

 ftmp5.ui32.lo = float_sub(fd.ui32.lo, ftmp3.ui32.lo);
 ftmp5.ui32.lo = adjust_result(ftmp5.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;

/* if(sectnum==3){
  rstep = read_ctrlreg (reg_clockcount-1, CTRLREG_RSTEP, CP2);
  rmask = read_ctrlreg (reg_clockcount-1, CTRLREG_RMASK, CP2);
  rind.ui32 = ((rind.ui32 + rstep)&rmask) | (rind.ui32 & ~rmask);
  regid  = REGID (SNONE, CTRLREG, CTRLREG_RIND, NONE);
  regval = REGVAL(ctrlreg, rind.ui32, NOMASK64, NONE);
  delayed_reg_write (&regid, &regval, INSTR_DELAY_MOVE_CTRL, BYPASS_YES);
 }*/
 fd = ftmp4;
 fs = ftmp5;










	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CFLY);

     // write result

     float_result_write(sectnum,fdnum,fd,BYPASS_NO);
   float_result_write(sectnum,fsnum,fs,BYPASS_NO);



  }

  rstep = read_ctrlreg (reg_clockcount-1, CTRLREG_RSTEP, CP2);
  rmask = read_ctrlreg (reg_clockcount-1, CTRLREG_RMASK, CP2);
  rind.ui32 = ((rind.ui32 + rstep)&rmask) | (rind.ui32 & ~rmask);
  regid  = REGID (SNONE, CTRLREG, CTRLREG_RIND, NONE);
  regval = REGVAL(ctrlreg, rind.ui32, NOMASK64, NONE);
  delayed_reg_write (&regid, &regval, INSTR_DELAY_MOVE_CTRL, BYPASS_YES);



	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}

void execute_instr_SINC (instr_t instr) {

   int sectnum;
   uint32_t data32;

   int fsnum;
   int fdnum;

   fpr_t fs;
   fpr_t fd;

   // get fpr numbers
   fsnum = instr_fs(instr);
   fdnum = instr_fd(instr);

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);



	// check condition code
	if (instr_cc(instr) != 0) {
		data32 = read_fccr (sectnum,reg_clockcount-1);
		if ((data32 & bit32(instr_cc(instr))) == 0) {
			// skip this instruction for this section
			continue;
		}
	}

	// perform operation
	exc_sec_acc = 0;
	f_status.float_exception_flags = 0;
	fs = adjust_operand(fs);
	switch(instr_elf_cond(instr)){
		case 0: fd.f.lo = k128cp2elfun_sinc(fs.f.lo); fd.ui32.lo = adjust_result_rs(fd.ui32.lo);
				float_result_write_lo(sectnum,fdnum,fd,BYPASS_NO); break;
		case 1: fd.f.lo = k128cp2elfun_sinc(fs.f.hi); fd.ui32.lo = adjust_result_rs(fd.ui32.lo);
				float_result_write_lo(sectnum,fdnum,fd,BYPASS_NO); break;
		case 2: fd.f.hi = k128cp2elfun_sinc(fs.f.lo); fd.ui32.hi = adjust_result_rs(fd.ui32.hi);
				float_result_write_hi(sectnum,fdnum,fd,BYPASS_NO); break;
		case 3: fd.f.hi = k128cp2elfun_sinc(fs.f.hi); fd.ui32.hi = adjust_result_rs(fd.ui32.hi);
				float_result_write_hi(sectnum,fdnum,fd,BYPASS_NO); break;
	}
	exc_sec_acc |= f_status.float_exception_flags;
	f_status.float_exception_flags = 0;
	exc_sec_acc = adjust_exception_flags(exc_sec_acc);
	f_status.float_exception_flags = exc_sec_acc;
	exc_acc |= exc_sec_acc;

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,SINC);

	}
	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}

void execute_instr_EXP2 (instr_t instr) {

   int sectnum;
   uint32_t data32;

   int fsnum;
   int fdnum;

   fpr_t fs;
   fpr_t fd;
   fpr_t ftmp;

   // get fpr numbers
   fsnum = instr_fs(instr);
   fdnum = instr_fd(instr);

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);



	// check condition code
	if (instr_cc(instr) != 0) {
		data32 = read_fccr (sectnum,reg_clockcount-1);
		if ((data32 & bit32(instr_cc(instr))) == 0) {
			// skip this instruction for this section
			continue;
		}
	}

	// perform operation
	exc_sec_acc = 0;
	ftmp.ui32.hi = 0;
	ftmp.ui32.lo = 0;
	f_status.float_exception_flags = 0;
	switch(instr_elf_cond(instr)){
		case 0: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_exp2(fs.f.lo);
				fd.ui32.lo = adjust_result_rs(fd.ui32.lo); float_result_write_lo(sectnum,fdnum,fd,BYPASS_NO); break;
		case 1: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_exp2(fs.f.hi);
				fd.ui32.lo = adjust_result_rs(fd.ui32.lo); float_result_write_lo(sectnum,fdnum,fd,BYPASS_NO); break;
		case 2: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_exp2(fs.f.lo);
				fd.ui32.hi = adjust_result_rs(fd.ui32.hi); float_result_write_hi(sectnum,fdnum,fd,BYPASS_NO); break;
		case 3: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_exp2(fs.f.hi);
				fd.ui32.hi = adjust_result_rs(fd.ui32.hi); float_result_write_hi(sectnum,fdnum,fd,BYPASS_NO); break;
	}
	exc_sec_acc |= f_status.float_exception_flags;
	f_status.float_exception_flags = 0;
	exc_sec_acc = adjust_exception_flags(exc_sec_acc);
	f_status.float_exception_flags = exc_sec_acc;
	exc_acc |= exc_sec_acc;

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,EXP2);

	}
	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}

void execute_instr_LOG2C (instr_t instr) {

   int sectnum;
   uint32_t data32;

   int fsnum;
   int fdnum;

   fpr_t fs;
   fpr_t fd;
   fpr_t ftmp;

   // get fpr numbers
   fsnum = instr_fs(instr);
   fdnum = instr_fd(instr);

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);



	// check condition code
	if (instr_cc(instr) != 0) {
		data32 = read_fccr (sectnum,reg_clockcount-1);
		if ((data32 & bit32(instr_cc(instr))) == 0) {
			// skip this instruction for this section
			continue;
		}
	}

	// perform operation
	exc_sec_acc = 0;
	ftmp.ui32.hi = 0;
	ftmp.ui32.lo = 0;
	f_status.float_exception_flags = 0;
	switch(instr_elf_cond(instr)){
		case 0: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_log2c(fs.f.lo);
				fd.ui32.lo = adjust_result_rs(fd.ui32.lo); float_result_write_lo(sectnum,fdnum,fd,BYPASS_NO); break;
		case 1: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_log2c(fs.f.hi);
				fd.ui32.lo = adjust_result_rs(fd.ui32.lo); float_result_write_lo(sectnum,fdnum,fd,BYPASS_NO); break;
		case 2: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_log2c(fs.f.lo);
				fd.ui32.hi = adjust_result_rs(fd.ui32.hi); float_result_write_hi(sectnum,fdnum,fd,BYPASS_NO); break;
		case 3: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_log2c(fs.f.hi);
				fd.ui32.hi = adjust_result_rs(fd.ui32.hi); float_result_write_hi(sectnum,fdnum,fd,BYPASS_NO); break;
	}
	exc_sec_acc |= f_status.float_exception_flags;
	f_status.float_exception_flags = 0;
	exc_sec_acc = adjust_exception_flags(exc_sec_acc);
	f_status.float_exception_flags = exc_sec_acc;
	exc_acc |= exc_sec_acc;

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,LOG2C);

	}
	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}

void execute_instr_ATANC(instr_t instr) {

   int sectnum;
   uint32_t data32;

   int fsnum;
   int fdnum;

   fpr_t fs;
   fpr_t fd;
   fpr_t ftmp;

   // get fpr numbers
   fsnum = instr_fs(instr);
   fdnum = instr_fd(instr);

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);



	// check condition code
	if (instr_cc(instr) != 0) {
		data32 = read_fccr (sectnum,reg_clockcount-1);
		if ((data32 & bit32(instr_cc(instr))) == 0) {
			// skip this instruction for this section
			continue;
		}
	}

	// perform operation
	exc_sec_acc = 0;
	ftmp.ui32.hi = 0;
	ftmp.ui32.lo = 0;
	f_status.float_exception_flags = 0;
	switch(instr_elf_cond(instr)){
		case 0: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_atanc(fs.f.lo);
				fd.ui32.lo = adjust_result_rs(fd.ui32.lo); float_result_write_lo(sectnum,fdnum,fd,BYPASS_NO); break;
		case 1: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_atanc(fs.f.hi);
				fd.ui32.lo = adjust_result_rs(fd.ui32.lo); float_result_write_lo(sectnum,fdnum,fd,BYPASS_NO); break;
		case 2: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_atanc(fs.f.lo);
				fd.ui32.hi = adjust_result_rs(fd.ui32.hi); float_result_write_hi(sectnum,fdnum,fd,BYPASS_NO); break;
		case 3: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_atanc(fs.f.hi);
				fd.ui32.hi = adjust_result_rs(fd.ui32.hi); float_result_write_hi(sectnum,fdnum,fd,BYPASS_NO); break;
}
	exc_sec_acc |= f_status.float_exception_flags;
	f_status.float_exception_flags = 0;
	exc_sec_acc = adjust_exception_flags(exc_sec_acc);
	f_status.float_exception_flags = exc_sec_acc;
	exc_acc |= exc_sec_acc;

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,ATANC);

	}
	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}

void execute_instr_RECIP(instr_t instr) {

   int sectnum;
   uint32_t data32;

   int fsnum;
   int fdnum;

   fpr_t fs;
   fpr_t fd;

   // get fpr numbers
   fsnum = instr_fs(instr);
   fdnum = instr_fd(instr);

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);



	// check condition code
	if (instr_cc(instr) != 0) {
		data32 = read_fccr (sectnum,reg_clockcount-1);
		if ((data32 & bit32(instr_cc(instr))) == 0) {
			// skip this instruction for this section
			continue;
		}
	}

	// perform operation
	exc_sec_acc = 0;
	f_status.float_exception_flags = 0;
	fs = adjust_operand(fs);
	switch(instr_elf_cond(instr)){
		case 0: fd.f.lo = k128cp2elfun_recip(fs.f.lo); fd.ui32.lo = adjust_result_rs(fd.ui32.lo);
				float_result_write_lo(sectnum,fdnum,fd,BYPASS_NO); break;
		case 1: fd.f.lo = k128cp2elfun_recip(fs.f.hi); fd.ui32.lo = adjust_result_rs(fd.ui32.lo);
				float_result_write_lo(sectnum,fdnum,fd,BYPASS_NO); break;
		case 2: fd.f.hi = k128cp2elfun_recip(fs.f.lo); fd.ui32.hi = adjust_result_rs(fd.ui32.hi);
				float_result_write_hi(sectnum,fdnum,fd,BYPASS_NO); break;
		case 3: fd.f.hi = k128cp2elfun_recip(fs.f.hi); fd.ui32.hi = adjust_result_rs(fd.ui32.hi);
				float_result_write_hi(sectnum,fdnum,fd,BYPASS_NO); break;
	}
	exc_sec_acc |= f_status.float_exception_flags;
	f_status.float_exception_flags = 0;
	exc_sec_acc = adjust_exception_flags(exc_sec_acc);
	f_status.float_exception_flags = exc_sec_acc;
	exc_acc |= exc_sec_acc;

		// update fcsr
		if(if_update_fcsr(instr) == 1)
			update_fcsr(sectnum,RECIP);

	}
	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}

void execute_instr_RSQRT(instr_t instr) {

   int sectnum;
   uint32_t data32;

   int fsnum;
   int fdnum;

   fpr_t fs;
   fpr_t fd;
   fpr_t ftmp;

   // get fpr numbers
   fsnum = instr_fs(instr);
   fdnum = instr_fd(instr);

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);



	// check condition code
	if (instr_cc(instr) != 0) {
		data32 = read_fccr (sectnum,reg_clockcount-1);
		if ((data32 & bit32(instr_cc(instr))) == 0) {
			// skip this instruction for this section
			continue;
		}
	}

	// perform operation
	exc_sec_acc = 0;
	ftmp.ui32.hi = 0;
	ftmp.ui32.lo = 0;
	f_status.float_exception_flags = 0;
	switch(instr_elf_cond(instr)){
		case 0: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_recip(k128cp2elfun_sqrt(fs.f.lo)); fd.ui32.lo = adjust_result_rs(fd.ui32.lo); float_result_write_lo(sectnum,fdnum,fd,BYPASS_NO); break;
		case 1: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_recip(k128cp2elfun_sqrt(fs.f.hi)); fd.ui32.lo = adjust_result_rs(fd.ui32.lo); float_result_write_lo(sectnum,fdnum,fd,BYPASS_NO); break;
		case 2: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_recip(k128cp2elfun_sqrt(fs.f.lo)); fd.ui32.hi = adjust_result_rs(fd.ui32.hi); float_result_write_hi(sectnum,fdnum,fd,BYPASS_NO); break;
		case 3: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_recip(k128cp2elfun_sqrt(fs.f.hi)); fd.ui32.hi = adjust_result_rs(fd.ui32.hi); float_result_write_hi(sectnum,fdnum,fd,BYPASS_NO); break;
}
	exc_sec_acc |= f_status.float_exception_flags;
	f_status.float_exception_flags = 0;
	exc_sec_acc = adjust_exception_flags(exc_sec_acc);
	f_status.float_exception_flags = exc_sec_acc;
	exc_acc |= exc_sec_acc;

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,RSQRT);

	}
	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}

// 'instdump' option support
void instdump (instr_t instr) {

	// string for instdump
	static char str[400];

	if(!kern.nulify)
		sprintf(str,"instdump:");
	else
		sprintf(str,"instdumpnlf:");
//		return;

	// id
	sprintf (str+strlen(str), " clck=%" PRId64 " id=%" PRId64,  (uint64_t) kern.k64clock, (uint64_t) reg_clockcount);

	// instruction address or "fifo"
	if (kern.instr_from_k64fifo) {
		sprintf(str+strlen(str), " fifo");
	} else {
		sprintf(str+strlen(str), " %04x", (int) kern.newpc);
	}

	// instr codes
	sprintf (str+strlen(str), " %08x %08x",
		(int) instr_hi(instr),
		(int) instr_lo(instr)
	);

	// instr opcodes
/*
	sprintf (str+strlen(str), " opcode=%02x opcode2=%02x ",
		(int) instr_opcode(instr),
		(int) instr_opcode2(instr)
	);
*/

	// disassemble
	disasm  (str+strlen(str), instr);

	// output
	sim_print_string (str);

}


// update softfloat rounding mode from control register
void update_softfloat_rounding_mode () {

	int ctrl_rm;

	// read rounding mode from control register
	ctrl_rm = rawread_ctrlreg (CTRLREG_CONTROL);
	ctrl_rm = ctrl_rm & REG_CONTROL_RM;

	// update softfloat rounding mode
	switch (ctrl_rm) {
		case REG_CONTROL_RM_RN : f_status.float_rounding_mode = float_round_nearest_even; break;
		case REG_CONTROL_RM_RZ : f_status.float_rounding_mode = float_round_to_zero     ; break;
		case REG_CONTROL_RM_RP : f_status.float_rounding_mode = float_round_up          ; break;
		case REG_CONTROL_RM_RM : f_status.float_rounding_mode = float_round_down        ; break;
		default: sim_error("Invalid rounding mode in control register: RM=0x%02x\n", ctrl_rm); break;
	}

}


// prepare softfloat:
// - set rounding mode from control register
// - clear exception bits
void softfloat_prepare () {

	// clear softfloat exception bits
	f_status.float_exception_flags = 0;

	// set rounding mode from control register
	update_softfloat_rounding_mode ();
}


// clear softfloat:
// - clear exception bits
void softfloat_clear () {

	// clear softfloat exception bits
	f_status.float_exception_flags = 0;
}


//
void execute_instr_vliw (instr_t instr) {

	// instdump support
	if (k128cp2_machine -> opt_dump_inst) {
		instdump(instr);
	}

	// execute instrs
	execute_instr_hi (instr);
	execute_instr_lo (instr);

}


//
void execute_instr_hi      (instr_t instr) {

	// prepare softfloat
	softfloat_prepare ();

//#define case_code(instr_name)  case INSTR_OPCODE_##instr_name:  execute_instr_##instr_name(instr); break;
#define case_code(instr_name)  case INSTR_OPCODE_##instr_name:
	// decode & execute instruction
	switch (instr_opcode(instr)) {
		case_code(NOP);
		case_code(CMADD);
		case_code(CMSUB);
		case_code(CADD);
		case_code(CMUL);
		case_code(CMULI);
		case_code(CMULNI);
		case_code(CNEG);
		case_code(CCONJ);
		case_code(CSUB);
		case_code(CLEAR);
		case_code(SWAPHL);
		case_code(SWAP64);
		case_code(CFLY);
		case_code(CFLY2);
		case_code(CADDSUB);
		case_code(PSADDSUB);
		case_code(MTRANS);
		case_code(MVMUL);
		case_code(MTVMUL);
		case_code(PSADD);
		case_code(PSSUB);
		case_code(PSABS);
		case_code(PSMUL);
		case_code(PSMADD);
		case_code(PSMSUB);
		case_code(PSNEG);
		case_code(RDSEC);
		case_code(LI0);
		case_code(LI1);
		case_code(LI2);
		case_code(LI3);
		case_code(LSHIFT);
		case_code(LSHIFTI);
		case_code(ASHIFT);
		case_code(ASHIFTI);
		case_code(PSCOPYSIGN);
		case_code(SPLIT8);
		case_code(SPLIT16);
		case_code(SPLIT32);
		case_code(JOIN8);
		case_code(JOIN16);
		case_code(JOIN32);
		case_code(EXTSIGN8H);
		case_code(EXTSIGN8W);
		case_code(EXTSIGN16W);
		case_code(COPY);
		case_code(ADD);
		case_code(ABS);
		case_code(NEG);
		case_code(ADDI);
		case_code(SUB);
		case_code(SUBI);
		case_code(INCI);
		case_code(DECI);
		case_code(AND);
		case_code(OR);
		case_code(XOR);
		case_code(NOT);
		case_code(CCOND);
		case_code(MFC);
		case_code(MTC);
		case_code(PWTOPS);
		case_code(PSTOPW);
		case_code(PSGETEXP);
		case_code(PSGETMAN);
		case_code(PSSCALE);
		case_code(GETCOEFF);
		case_code(MVMADD);
		case_code(MVMSUB);
		case_code(MTVMADD);
		case_code(MTVMSUB);
		case_code(SINC);
		case_code(RECIP);
		case_code(RSQRT);
		case_code(RRSIN);
		case_code(RRCOS);
		case_code(RRCOSSIN);
		case_code(RRLOG2);
		case_code(EXP2);
		case_code(LOG2C);
		case_code(ATANC);
		case_code(CHMUL);
		case_code(CHMADD);
		case_code(CHMSUB);
		case_code(QSDOT);
		case_code(UNPCK16WSTOPS);
		execute_instr_CAL_COMMON(instr); break;
		default: sim_error ("unknown opcode 0x%02x",(int)instr_opcode(instr)); break;
	};
#undef case_code

	// clear softfloat
	softfloat_clear ();

}


//
void execute_instr_lo      (instr_t instr) {

#define case_code(instr_name)  case INSTR_OPCODE2_##instr_name:  execute_instr_##instr_name(instr); break;
//#define case_code(instr_name)  case INSTR_OPCODE2_##instr_name:
	// decode & execute instruction
	switch (instr_opcode2(instr)) {
		case_code(RUN);
		case_code(RUNI);
		case_code(ENDDO);
		case_code(STOP);
		case_code(STOPI);
		case_code(SYNC);
		case_code(START_DMA);
		case INSTR_OPCODE2_RET:
		case INSTR_OPCODE2_CALL:
		case INSTR_OPCODE2_CALLI:
		case INSTR_OPCODE2_JUMP:
		case INSTR_OPCODE2_JUMPI:
		case INSTR_OPCODE2_CHECK_DMA:
		case INSTR_OPCODE2_DO:
		case INSTR_OPCODE2_DOI:
		case INSTR_OPCODE2_NOP:
		case INSTR_OPCODE2_SETI:
		case INSTR_OPCODE2_MOVE:
		case INSTR_OPCODE2_MTFPR:
		case INSTR_OPCODE2_MFFPR:
		case INSTR_OPCODE2_PSPRMSGN0  :
		case INSTR_OPCODE2_PSPRMSGN1  :
		case INSTR_OPCODE2_CLR        :
		case INSTR_OPCODE2_LWI        :
		case INSTR_OPCODE2_LDI        :
		case INSTR_OPCODE2_SWI        :
		case INSTR_OPCODE2_SDI        :
		case INSTR_OPCODE2_LWNP       :
		case INSTR_OPCODE2_LDNP       :
		case INSTR_OPCODE2_SWNP       :
		case INSTR_OPCODE2_SDNP       :
		case INSTR_OPCODE2_LWNM       :
		case INSTR_OPCODE2_LDNM       :
		case INSTR_OPCODE2_SWNM       :
		case INSTR_OPCODE2_SDNM       :
		case INSTR_OPCODE2_LWO        :
		case INSTR_OPCODE2_LDO        :
		case INSTR_OPCODE2_SWO        :
		case INSTR_OPCODE2_SDO        :
		case INSTR_OPCODE2_UPDADDR    :
		case INSTR_OPCODE2_UPDADDRNP  :
		case INSTR_OPCODE2_UPDADDRNM  :
			execute_instr_LMEM_COMMON(instr);
			break;
		default: sim_error ("unknown opcode2 0x%02x",(int)instr_opcode2(instr)); break;
	};
#undef case_code

}

