// CP2 internals access functions
// (read/write registers, memory, etc.)

#include <stdio.h>
#include "machine.h"
#include "insn_code.h"
#include "system.h"
#include "prockern.h"
#include "prockern_access.h"

#define K128CP2_INITSTATE_FILENAME "k128cp2_initstate.xml"

// machine
extern Machine_T *k128cp2_machine;

// reg read raw
regval_t reg_read_raw  (regid_t id) {

	regval_t rv;

	rv.val.ui64 = 0;

	switch (id.type) {
		case REGID_TYPE_FCCR    :  rv.val.fccr = reg_fccr (id.section);           break;
		case REGID_TYPE_FCSR    :  rv.val.fcsr = reg_fcsr (id.section);           break;
		case REGID_TYPE_FPR     :  rv.val.fpr  = reg_fpr  (id.section, id.regno); break;
		case REGID_TYPE_GPR     :  rv.val.gpr  = reg_gpr  (id.regno);             break;
		case REGID_TYPE_CTRLREG :
			switch (id.regno) {
				case CTRLREG_PC         : rv.val.ctrlreg = kern.pc          ; break;
				case CTRLREG_COMM       : rv.val.ctrlreg = kern.comm        ; break;
				case CTRLREG_CONTROL    : rv.val.ctrlreg = kern.control.ui32; break;
				case CTRLREG_STATUS     : rv.val.ctrlreg = kern.status.ui32 ; break;
				case CTRLREG_PSP        :
					rv.val.ctrlreg = kern.psp;
					if (CORRECT_READ_WRITE) rv.val.ctrlreg = kern.psp_cur; // alex
					break;
				case CTRLREG_LC         :
					rv.val.ctrlreg = kern.lc;
					if (CORRECT_READ_WRITE) rv.val.ctrlreg = kern.lc_cur; // alex
				break;
				case CTRLREG_LA         :
					rv.val.ctrlreg = kern.la.ui32;
					if (CORRECT_READ_WRITE) rv.val.ctrlreg = kern.la_cur.ui32; // alex
					break;
				case CTRLREG_LSP        :
					rv.val.ctrlreg = kern.lsp;
					if (CORRECT_READ_WRITE) rv.val.ctrlreg = kern.lsp_cur; // alex
					break;
				case CTRLREG_RIND       : rv.val.ctrlreg = kern.rind.ui32   ; break;
				case CTRLREG_RSTEP      : rv.val.ctrlreg = kern.rstep       ; break;
				case CTRLREG_RMASK      : rv.val.ctrlreg = kern.rmask       ; break;
				case CTRLREG_CLOCKCOUNT : rv.val.ctrlreg = kern.clockcount  ; break;
				case CTRLREG_STOPCODE   : rv.val.ctrlreg = kern.stopcode    ; break;
				default: sim_error_internal(); break;
			}
			break;
		case REGID_TYPE_IREG    :  rv.val.ireg   = reg_ireg   (id.regno);    break;
		case REGID_TYPE_ADDRAN  :  rv.val.addran = reg_addran (id.regno);    break;
		case REGID_TYPE_ADDRNN  :  rv.val.addrnn = reg_addrnn (id.regno);    break;
		case REGID_TYPE_ADDRMN  :  rv.val.addrmn = reg_addrmn (id.regno);    break;
		default: sim_error_internal(); break;
	}

	// return result
	return rv;
}

// reg read
// (with notifying observer)
regval_t reg_read (regid_t id, uint64_t clck, readwrite_ctrlreg_srcid_t srcid) {

    regid_t  regid;
	regval_t rv, rv1;
	event_src_t evsrc;

	// dispatch id
	switch (id.type) {
		case REGID_TYPE_FCCR    :  evsrc = EVENT_SRC_FCCR;        break;
		case REGID_TYPE_FCSR    :  evsrc = EVENT_SRC_FCSR;        break;
		case REGID_TYPE_FPR     :  evsrc = EVENT_SRC_FPR;         break;
		case REGID_TYPE_GPR     :  evsrc = EVENT_SRC_GPR;         break;
		case REGID_TYPE_CTRLREG :  evsrc = EVENT_SRC_CTRLREG;     break;
		case REGID_TYPE_ADDRAN  :  evsrc = EVENT_SRC_ADDRAN;      break;
		case REGID_TYPE_ADDRNN  :  evsrc = EVENT_SRC_ADDRNN;      break;
		case REGID_TYPE_ADDRMN  :  evsrc = EVENT_SRC_ADDRMN;      break;
		case REGID_TYPE_IREG    :  evsrc = EVENT_SRC_IREG;        break;
		default: sim_error_internal(); break;
	}

	// read reg
	rv = reg_read_raw (id);

	// notify observer
	send_newevent (clck, evsrc, EVENT_TYPE_READ, id.section, id.regno, rv.val.ui64, 1);

	// flush comm2cp2 or comm2k64 bits of status register on COMM reg read
	if (
		(id.type  == REGID_TYPE_CTRLREG) &&
		(id.regno == CTRLREG_COMM)
	) {
		switch (srcid) {
			case READWRITE_CTRLREG_SRCID_CP2 :
				regid  = REGID  (0, CTRLREG, CTRLREG_STATUS, NONE);
				rv1 = REGVAL (ctrlreg, 0, REG_STATUS_COMMK64.ui32, CP2);
				delayed_reg_write (&regid, &rv1, 1, BYPASS_NO);
				break;
			case READWRITE_CTRLREG_SRCID_K64 :
				regid  = REGID  (0, CTRLREG, CTRLREG_STATUS, NONE);
				rv1 = REGVAL (ctrlreg, 0, REG_STATUS_COMMCP2.ui32, K64);
				delayed_reg_write (&regid, &rv1, 0, BYPASS_NO);
				break;
			default: sim_error_internal(); break;
		}
	}

	return rv;

}


// reg write raw
void reg_write_raw (regid_t id, regval_t data) {

	regid_t  regid;
	regval_t rv;

#define write_masked(_reg,_data,_mask) (_reg) = ((_reg) & (~(_mask))) | ((_data) & (_mask));
	// dispatch id
	switch (id.type) {
		case REGID_TYPE_FCCR    :  write_masked( reg_fccr (id.section), data.val.fccr, data.mask); break;
		case REGID_TYPE_FCSR    :  write_masked( reg_fcsr (id.section).ui32, data.val.fcsr.ui32, data.mask); break;
		case REGID_TYPE_FPR     :
			switch (id.half) {
				case REGID_HALF_BOTH :  reg_fpr (id.section, id.regno)         = data.val.fpr;         break;
				case REGID_HALF_HI   :  reg_fpr (id.section, id.regno).ui32.hi = data.val.fpr.ui32.hi; break;
				case REGID_HALF_LO   :  reg_fpr (id.section, id.regno).ui32.lo = data.val.fpr.ui32.lo; break;
				default: sim_error_internal(); break;
			}
			break;
		case REGID_TYPE_GPR     :  write_masked( reg_gpr (id.regno), data.val.gpr, data.mask); break;
		case REGID_TYPE_CTRLREG:
			switch (id.regno) {
				case CTRLREG_COMM    : write_masked(kern.comm    , data.val.ctrlreg, data.mask); break;
				case CTRLREG_CONTROL : {
					write_masked(kern.control.ui32 , data.val.ctrlreg, data.mask);
					if( data.val.ctrlreg & REG_CONTROL_RS )
						machine_reset ();
					if( data.val.ctrlreg & REG_CONTROL_ST ){
					    kern.stop_pending_counter = 7;
					    kern.stop_pending = TRUE;
					    kern.run_flag = FALSE;
						regid  = REGID  (0, CTRLREG, CTRLREG_STATUS, NONE);
						rv = REGVAL (ctrlreg, 0, REG_STATUS_RUN.ui32, CP2);
						delayed_reg_write (&regid, &rv, INSTR_DELAY_RUNSTOP, BYPASS_NO);
						regid  = REGID  (0, CTRLREG, CTRLREG_CONTROL, NONE);
						rv = REGVAL (ctrlreg, 0, REG_CONTROL_ST, CP2);
						delayed_reg_write (&regid, &rv, 2, BYPASS_NO);
					}
				}
				break;
				case CTRLREG_STATUS  : write_masked (kern.status.ui32  , data.val.ctrlreg, data.mask); break;
				case CTRLREG_LC      : write_masked (kern.lc           , data.val.ctrlreg, data.mask); break;
				case CTRLREG_LA      : write_masked (kern.la.ui32      , data.val.ctrlreg, data.mask); break;
				case CTRLREG_LSP     : write_masked (kern.lsp          , data.val.ctrlreg, data.mask); break;
				case CTRLREG_PSP     : write_masked (kern.psp          , data.val.ctrlreg, data.mask); break;
				case CTRLREG_RIND    : write_masked (kern.rind.ui32    , data.val.ctrlreg, data.mask); break;
				case CTRLREG_RSTEP   : write_masked (kern.rstep        , data.val.ctrlreg, data.mask); break;
				case CTRLREG_RMASK   : write_masked (kern.rmask        , data.val.ctrlreg, data.mask); break;
				case CTRLREG_PC      : write_masked (kern.pc           , data.val.ctrlreg, data.mask); break;
				default: sim_error_internal(); break;
			};
			break;
		case REGID_TYPE_ADDRAN   :  write_masked( reg_addran (id.regno), data.val.addran, data.mask); break;
		case REGID_TYPE_ADDRNN   :  write_masked( reg_addrnn (id.regno), data.val.addrnn, data.mask); break;
		case REGID_TYPE_ADDRMN   :  write_masked( reg_addrmn (id.regno), data.val.addrmn, data.mask); break;
		default: sim_error_internal(); break;
	}
#undef write_masked

}


// reg write
// (with notifying observer)
void reg_write (regid_t id, regval_t data, uint64_t clck, readwrite_ctrlreg_srcid_t srcid) {

	event_src_t evsrc;
	uint64_t data64_read;
	uint32_t data32_read;
    regid_t  regid;
    regval_t rv;

	// save
	kern.written_regs[kern.written_regs_num].regid = id;
	kern.written_regs[kern.written_regs_num].regval= reg_read_raw(id);
	kern.written_regs_num++;

	// dispatch id
	switch (id.type) {
		case REGID_TYPE_FCCR    :  evsrc = EVENT_SRC_FCCR       ; break;
		case REGID_TYPE_FCSR    :  evsrc = EVENT_SRC_FCSR       ; break;
		case REGID_TYPE_FPR     :  evsrc = EVENT_SRC_FPR        ; break;
		case REGID_TYPE_GPR     :  evsrc = EVENT_SRC_GPR        ; break;
		case REGID_TYPE_CTRLREG :  evsrc = EVENT_SRC_CTRLREG    ; break;
		case REGID_TYPE_ADDRAN  :  evsrc = EVENT_SRC_ADDRAN     ; break;
		case REGID_TYPE_ADDRNN  :  evsrc = EVENT_SRC_ADDRNN     ; break;
		case REGID_TYPE_ADDRMN  :  evsrc = EVENT_SRC_ADDRMN     ; break;
		case REGID_TYPE_IREG    :  evsrc = EVENT_SRC_IREG       ; break;
		default: sim_error_internal(); break;
	}

	// write
#define write_masked(_reg,_data,_mask) (_reg) = ((_reg) & (~(_mask))) | ((_data) & (_mask));
#define write_or_masked(_reg,_data,_mask_write,_mask_or) (_reg) = (((_reg) | ((_data) & (_mask_or))) & (~(_mask_write))) | ((_data) & (_mask_write));

	// dispatch id
	switch (id.type) {
		case REGID_TYPE_FCCR    :
						write_masked( reg_fccr (id.section), data.val.fccr, data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, reg_fccr (id.section), 1);
						break;
		case REGID_TYPE_FCSR    :
						write_masked( reg_fcsr (id.section).ui32, data.val.fcsr.ui32, data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, reg_fcsr (id.section).ui32, 1);
						break;
		case REGID_TYPE_FPR     :
			switch (id.half) {
				case REGID_HALF_BOTH :
						reg_fpr (id.section, id.regno)         = data.val.fpr;
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, reg_fpr (id.section, id.regno).ui64, 1);
						break;
				case REGID_HALF_HI   :
						reg_fpr (id.section, id.regno).ui32.hi = data.val.fpr.ui32.hi;
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, reg_fpr (id.section, id.regno).ui64, 1);
						break;
				case REGID_HALF_LO   :
						reg_fpr (id.section, id.regno).ui32.lo = data.val.fpr.ui32.lo;
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, reg_fpr (id.section, id.regno).ui64, 1);
						break;
				default: sim_error_internal(); break;
			}
			break;
		case REGID_TYPE_GPR     :
						write_masked( reg_gpr (id.regno), data.val.gpr, data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, reg_gpr (id.regno), 1);
						break;
		case REGID_TYPE_CTRLREG:
			switch (id.regno) {
				case CTRLREG_PC      :
						data32_read = kern.pc;
						write_masked(kern.pc      , data.val.ctrlreg, data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, kern.pc, (data32_read != kern.pc));
						break;
				case CTRLREG_COMM    :
						data64_read = kern.comm;
						write_masked(kern.comm    , data.val.ctrlreg, data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, kern.comm, (data64_read != kern.comm));
						break;
				case CTRLREG_CONTROL : {
						data32_read = kern.control.ui32;
						write_masked(kern.control.ui32 , data.val.ctrlreg, data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, kern.control.ui32, (data32_read != kern.control.ui32) || data.val.ctrlreg & REG_CONTROL_RS );
					if( data.val.ctrlreg & REG_CONTROL_RS )
						machine_reset ();
					if( data.val.ctrlreg & REG_CONTROL_ST ){
					    kern.stop_pending_counter = 7;
					    kern.stop_pending = TRUE;
					    kern.run_flag = FALSE;
						regid  = REGID  (0, CTRLREG, CTRLREG_STATUS, NONE);
						rv = REGVAL (ctrlreg, 0, REG_STATUS_RUN.ui32, CP2);
						delayed_reg_write (&regid, &rv, INSTR_DELAY_RUNSTOP, BYPASS_NO);
						regid  = REGID  (0, CTRLREG, CTRLREG_CONTROL, NONE);
						rv = REGVAL (ctrlreg, 0, REG_CONTROL_ST, CP2);
						delayed_reg_write (&regid, &rv, 2, BYPASS_NO);
					}

				}
						break;
				case CTRLREG_STATUS  :
						data32_read = kern.status.ui32;
						write_masked (kern.status.ui32  , data.val.ctrlreg, data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, kern.status.ui32, (data32_read != kern.status.ui32));
						break;
				case CTRLREG_LC      :
						data32_read = kern.lc;
						write_masked (kern.lc           , data.val.ctrlreg, data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, kern.lc, (data32_read != kern.lc));
						break;
				case CTRLREG_LA      :
						data32_read = kern.la.ui32;
						write_masked (kern.la.ui32      , data.val.ctrlreg, data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, kern.la.ui32, (data32_read != kern.la.ui32));
						break;
				case CTRLREG_LSP     :
						data32_read = kern.lsp;
						write_masked (kern.lsp          , data.val.ctrlreg, data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, kern.lsp, (data32_read != kern.lsp));
						break;
				case CTRLREG_PSP     :
						write_masked (kern.psp          , data.val.ctrlreg, data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, kern.psp, 1);
						break;
				case CTRLREG_RIND    :
						write_masked (kern.rind.ui32    , data.val.ctrlreg, data.mask & RINDMASK);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, kern.rind.ui32, 1);
						break;
				case CTRLREG_RSTEP   :
						write_masked (kern.rstep        , data.val.ctrlreg, data.mask & RSTEPMASK);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, kern.rstep, 1);
						break;
				case CTRLREG_RMASK   :
						write_masked (kern.rmask        , data.val.ctrlreg, data.mask & RMASKMASK);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, kern.rmask, 1);
						break;
				default: sim_error_internal(); break;
			};
			break;
		case REGID_TYPE_ADDRAN   :
						write_masked( reg_addran (id.regno), data.val.addran, data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, reg_addran (id.regno), 1);
						break;
		case REGID_TYPE_ADDRNN   :
						write_masked( reg_addrnn (id.regno), data.val.addrnn, data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, reg_addrnn (id.regno), 1);
						break;
		case REGID_TYPE_ADDRMN   :
						write_masked( reg_addrmn (id.regno), data.val.addrmn, data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, reg_addrmn (id.regno), 1);
						break;
		case REGID_TYPE_IREG     :
						write_masked( reg_ireg   (id.regno), data.val.ireg  , data.mask);
						send_newevent (clck, evsrc, EVENT_TYPE_WRITE, id.section, id.regno, reg_ireg   (id.regno), 1);
						break;
		default: sim_error_internal(); break;
	}
#undef write_masked

	// raise commcp2 & commk64 bits of Status register on COMM reg write
	if (
		(id.type  == REGID_TYPE_CTRLREG) &&
		(id.regno == CTRLREG_COMM)
	) {
		switch (srcid) {
			case READWRITE_CTRLREG_SRCID_CP2 :
				regid  = REGID  (0, CTRLREG, CTRLREG_STATUS, NONE);
				rv = REGVAL (ctrlreg, REG_STATUS_COMMCP2.ui32, REG_STATUS_COMMCP2.ui32, CP2);
				delayed_reg_write (&regid, &rv, 1, BYPASS_NO);
				break;
			case READWRITE_CTRLREG_SRCID_K64 :
				regid  = REGID  (0, CTRLREG, CTRLREG_STATUS, NONE);
				rv = REGVAL (ctrlreg, REG_STATUS_COMMK64.ui32, REG_STATUS_COMMK64.ui32, K64);
				delayed_reg_write (&regid, &rv, 1, BYPASS_NO);
				break;
			default: sim_error_internal(); break;
		}
	}

}


// check values of sect and cp2addr
void check_sect_cp2addr (sectid_t sect, cp2addr_t cp2addr) {
	if ((sect) >= NUMBER_OF_EXESECT) {
		sim_error("trying to read lmem of section %d",(int)(sect));
	} else if ((cp2addr) >= (LMEM_SIZE / sizeof(uint64_t))) {
		sim_error("trying to read lmem at addr=0x%08x",(uint32_t)(cp2addr));
	}
}

// lmem read raw
uint64_t rawread_lmem_ui64 (sectid_t sect, cp2addr_t addr) {
	uint64_t rv;
	check_sect_cp2addr (sect, addr);
	rv = lmem_ui64 (sect, addr);
	return rv;
}

// lmem read
// (with notifying observer)
uint64_t read_lmem_ui64 (uint64_t clck, sectid_t sect, cp2addr_t addr) {
	uint64_t rv;
	rv = rawread_lmem_ui64 (sect, addr);
	send_newevent (clck, EVENT_SRC_LMEM, EVENT_TYPE_READ, sect, addr, rv, 1);
	return rv;
}

// lmem write raw
void rawwrite_lmem_ui64 (sectid_t sect, cp2addr_t addr, uint64_t data64) {
	check_sect_cp2addr (sect, addr);
	lmem_ui32 (sect, addr*2)   = (uint32_t) (data64 >> 32);
	lmem_ui32 (sect, addr*2+1) = (uint32_t) (data64 & 0xffffffff);
}

// lmem write
// (with notifying observer)
void write_lmem_ui64 (uint64_t clck, sectid_t sect, cp2addr_t addr, uint64_t data64) {
	rawwrite_lmem_ui64 (sect, addr, data64);
	send_newevent (clck, EVENT_SRC_LMEM, EVENT_TYPE_WRITE, sect, addr, data64, 1);
}


// iram read raw
uint64_t rawread_iram_ui64 (cp2addr_t addr) {
	uint64_t rv;
	if (addr >= (IRAM_SIZE / sizeof(uint64_t))) {
		sim_error("trying to read iram at addr=0x%08x",(uint32_t)(addr));
	} else {
		rv = iram_ui64 (addr);
//		if (swap_bytes) (_val64) = swap_dword(_val64);
	}
	return rv;
}

// iram read
// (with notifying observer)
uint64_t read_iram_ui64 (uint64_t clck, cp2addr_t addr) {
	uint64_t rv;
	rv = rawread_iram_ui64 (addr);
	send_newevent (clck, EVENT_SRC_IRAM, EVENT_TYPE_READ, 0, addr, rv, 1);
	return rv;
}

// read iram starting from addr.
void rawread_iram (uint64_t *prg_ptr, uint64_t prg_len, uint64_t addr) {

	int i;

	// read iram
	for (i=0; i<prg_len; i++)
		prg_ptr[i] = rawread_iram_ui64 (addr+i);

}

// iram write raw
void rawwrite_iram_ui64 (cp2addr_t addr, uint64_t data64) {
	if (addr >= (IRAM_SIZE / sizeof(uint64_t))) {
		sim_error("trying to write iram at addr=0x%08x",(uint32_t)(addr));
	} else {
		iram_ui64 (addr) = data64;
	}
}

// Write iram starting from current pc. Then shift pc to the end of program.
// This is used with target linux and simulates loop with ldc2 instruction.
void rawwrite_iram (uint64_t *prg_ptr, uint64_t prg_len) {

	int i;

	// fill iram shifting pc
	for (i=0; i<prg_len; i++) {
		rawwrite_iram_ui64 (reg_pc, prg_ptr[i]);
		reg_pc += 1;
	}
}

// iram write
// (with notifying observer)
void write_iram_ui64 (uint64_t clck, cp2addr_t addr, uint64_t data64) {
	rawwrite_iram_ui64 (addr, data64);
	send_newevent (clck, EVENT_SRC_IRAM, EVENT_TYPE_WRITE, 0, addr, data64, 1);
}

// start_dma read
int start_dma_read() {

	if( kern.start_dma == 1 ) {
		kern.start_dma = 0;
		return 1;
	}
	else
		return 0;

}
