#include <stdio.h>
#include <math.h>
#include <string.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "system.h"
#include "machine.h"
#include "options.h"
#include "insn_code.h"
#include "softfloat.h"
#include "prockern.h"
#include "instr.h"
#include "instr_float.h"
#include "prockern_access.h"

#define ADDR_BITREV_MOD 0x0000
#define ADDR_LINE_MOD 0x1FFF

// machine
extern Machine_T *k128cp2_machine;


//
void prockern_init () {

	// fill kern struct with zeroes
	memset(&kern, 0, sizeof(kern));

	// init delayed register writing queue
	delayed_reg_write_init ();

	// init local memory operations support
	lmem_init ();
	cal_init ();

	// init k64fifo support
	k64fifo_init ();

	// set flags
	kern.run_flag = FALSE;
	kern.jump_flag       = FALSE;
	kern.loop_jump       = FALSE;
	kern.call_jump       = FALSE;
	kern.delay_slot_flag = FALSE;
	kern.stop_pending    = FALSE;
	kern.stop_pending_counter = 0;
	kern.sync_pending    = FALSE;
	kern.sync_pending_counter = 0;
	kern.nulify          = FALSE;
	kern.written_regs_num = 0;

}


// load state from xml file
int prockern_load_state_from_file (char *state_filename) {

	// open xml doc on file
	if (open_xmldoc_on_file (state_filename) != 0 ) {
		return 1;
	}

	// load state
	prockern_load_state	();

	// close xml file
	close_xmldoc ();

	// return
	return 0;
}


// load state from string
int prockern_load_state_from_string (char *str) {

	// open xml doc on string
	if( open_xmldoc_on_string (str) != 0 ) {
		sim_warning ("cannot open xml doc on string");
		return 1;
	}

	// load state
	prockern_load_state	();

	// close xml file
	close_xmldoc ();

	// return
	return 0;
}


// load state
// provided that xml doc is already opened
// (see prockern_load_state_from_file() or prockern_load_state_from_string() )
void prockern_load_state () {

	// load iram
	xmldoc_load_iram (kern.iram,IRAM_SIZE,"//state//iram");

	// load data ram
#if (NUMBER_OF_EXESECT != 4)
#error Number of sections must be 4
#endif
	xmldoc_load_lmem (kern.exesect[0].lmem, LMEM_SIZE, "//state//section0//lmem");
	xmldoc_load_lmem (kern.exesect[1].lmem, LMEM_SIZE, "//state//section1//lmem");
	xmldoc_load_lmem (kern.exesect[2].lmem, LMEM_SIZE, "//state//section2//lmem");
	xmldoc_load_lmem (kern.exesect[3].lmem, LMEM_SIZE, "//state//section3//lmem");

	// load registers state
	xmldoc_load_regs ();
}





#define xml_writer_newline() { \
	rc = xmlTextWriterWriteFormatRaw (xmlwriter, "\n"); \
	sim_assert (rc >= 0); \
}


// save state to xml file
void prockern_save_state (char *state_filename)
{
    int rc;
    xmlTextWriterPtr xmlwriter;

    LIBXML_TEST_VERSION

    /* Create a new XmlWriter for uri, with no compression. */
    xmlwriter = xmlNewTextWriterFilename (state_filename, 0);
	sim_assert (xmlwriter != NULL);

	// start xml document
    rc = xmlTextWriterStartDocument (xmlwriter, NULL, NULL, NULL);
	sim_assert (rc >= 0);

	// start element "state"
    rc = xmlTextWriterStartElement (xmlwriter, BAD_CAST "state");
	sim_assert (rc >= 0);

	// save iram
	save_state_iram (xmlwriter);

	// save regs
	save_state_regs (xmlwriter);

	// save sections
#if (NUMBER_OF_EXESECT != 4)
#error Number of sections must be 4
#endif
	save_state_section (xmlwriter, 0);
	save_state_section (xmlwriter, 1);
	save_state_section (xmlwriter, 2);
	save_state_section (xmlwriter, 3);

	// close element "state"
	xml_writer_newline ();
    rc = xmlTextWriterEndElement (xmlwriter);
	sim_assert (rc >= 0);

	// end xml document
    rc = xmlTextWriterEndDocument (xmlwriter);
	sim_assert (rc >= 0);

	// free xmlwriter
	xmlFreeTextWriter (xmlwriter);

	// cleanup xml library
    xmlCleanupParser();
}


// save iram
void save_state_iram (xmlTextWriterPtr xmlwriter) {

    int i, rc;

	// start element "iram"
	xml_writer_newline ();
    rc = xmlTextWriterStartElement (xmlwriter, BAD_CAST "iram");
	sim_assert (rc >= 0);

	// write iram
	for (i=0; i<IRAM_SIZE/sizeof(uint64_t); i++) {
		xml_writer_newline ();
		rc = xmlTextWriterWriteFormatRaw (xmlwriter, "%016" PRIx64, rawread_iram_ui64(i) );
		sim_assert (rc >= 0);
	}

	// close element "iram"
	xml_writer_newline ();
    rc = xmlTextWriterEndElement (xmlwriter);
	sim_assert (rc >= 0);
}


// save regs
void save_state_regs (xmlTextWriterPtr xmlwriter) {

    int i, rc;
	char rname[20];

	xml_writer_newline ();

/* alex
	// save pc
//	save_state_onereg (xmlwriter, "pc", 0x11ULL);

	// save status
//	save_state_onereg (xmlwriter, "status", 0x22ULL);

	// save control
//	save_state_onereg (xmlwriter, "control", 0x33ULL);
*/

	save_state_onereg (xmlwriter, "pc",	rawread_ctrlreg(CTRLREG_PC	));
	save_state_onereg (xmlwriter, "status", rawread_ctrlreg(CTRLREG_STATUS	));
	save_state_onereg (xmlwriter, "control",rawread_ctrlreg(CTRLREG_CONTROL	));
	save_state_onereg (xmlwriter, "comm",	rawread_ctrlreg(CTRLREG_COMM	));
	save_state_onereg (xmlwriter, "psp",	rawread_ctrlreg(CTRLREG_PSP	));
	save_state_onereg (xmlwriter, "lc",	rawread_ctrlreg(CTRLREG_LC	));
	save_state_onereg (xmlwriter, "la",	rawread_ctrlreg(CTRLREG_LA	));
	save_state_onereg (xmlwriter, "lsp",	rawread_ctrlreg(CTRLREG_LSP	));
	save_state_onereg (xmlwriter, "rind",	rawread_ctrlreg(CTRLREG_RIND	));
	save_state_onereg (xmlwriter, "rstep",	rawread_ctrlreg(CTRLREG_RSTEP	));
	save_state_onereg (xmlwriter, "rmask",	rawread_ctrlreg(CTRLREG_RMASK	));

// CTRLREG_CLOCKCOUNT CTRLREG_STOPCODE

	// save GPRs
	xml_writer_newline ();
	for (i=0; i<GPR_SIZE; i++) {
		sprintf (rname, "gpr%02d", i);
		save_state_onereg (xmlwriter, rname, rawread_gpr(i) );
	}

	// save ANs
	xml_writer_newline ();
	for (i=0; i<ADDRREG_SIZE; i++) {
		sprintf (rname, "an%02d", i);
		save_state_onereg (xmlwriter, rname, rawread_addran(i) );
	}

	// save NNs
	xml_writer_newline ();
	for (i=0; i<ADDRREG_SIZE; i++) {
		sprintf (rname, "nn%02d", i);
		save_state_onereg (xmlwriter, rname, rawread_addrnn(i) );
	}

	// save MNs
	xml_writer_newline ();
	for (i=0; i<ADDRREG_SIZE; i++) {
		sprintf (rname, "mn%02d", i);
		save_state_onereg (xmlwriter, rname, rawread_addrmn(i) );
	}

}

// save regs: save one reg
void save_state_onereg (xmlTextWriterPtr xmlwriter, char *regname, uint64_t regval) {

	int rc;

	xml_writer_newline ();
	rc = xmlTextWriterWriteFormatElement (xmlwriter, BAD_CAST regname, "%016" PRIx64, regval);
	sim_assert (rc >= 0);
}


// save one section
void save_state_section (xmlTextWriterPtr xmlwriter, int sec) {

	int i, rc;
	char name[20];

	// start element "sectionX"
	xml_writer_newline ();
	xml_writer_newline ();
	sprintf (name, "section%d", sec);
    rc = xmlTextWriterStartElement (xmlwriter, BAD_CAST name);
	sim_assert (rc >= 0);

	// save lram
	save_state_section_lmem (xmlwriter, sec);

	// save fccr
	save_state_onereg (xmlwriter, "fccr", rawread_fccr(sec) );

	// save fpr
	for (i=0; i<FPR_SIZE; i++) {
		sprintf (name, "fpr%02d", i);
		save_state_onereg (xmlwriter, name, rawread_fpr(sec,i).ui64);
	}

	// close element "sectionX"
	xml_writer_newline ();
    rc = xmlTextWriterEndElement (xmlwriter);
	sim_assert (rc >= 0);

}


// save lmem
void save_state_section_lmem (xmlTextWriterPtr xmlwriter, int sec) {

    int i, rc;

	// start element "lmem"
	xml_writer_newline ();
    rc = xmlTextWriterStartElement (xmlwriter, BAD_CAST "lmem");
	sim_assert (rc >= 0);

	// some write
	for (i=0; i<LMEM_SIZE/sizeof(uint64_t); i++) {
		xml_writer_newline ();
		rc = xmlTextWriterWriteFormatRaw (xmlwriter, "%08x%08x",
			lmem_ui32(sec,2*i), lmem_ui32(sec,2*i+1)
		);
		sim_assert (rc >= 0);
	}

	// close element "lmem"
	xml_writer_newline ();
    rc = xmlTextWriterEndElement (xmlwriter);
	sim_assert (rc >= 0);
}

#undef xml_writer_newline



//
bool_t delayed_reg_write_queue_empty () {
	return (kern.delayed_reg_write_queue_numentries == 0);
}


//
bool_t lmem_pipe_empty () {
	return (kern.lmem_pipe_numentries == 0);
}


// update sync_pending flag
void update_sync_pending () {

	// wait 7 clocks
	kern.sync_pending_counter --;
	if( kern.sync_pending_counter == 0 )
		kern.sync_pending = FALSE;
	// wait until all previous instructions finish executing
/*	kern.sync_pending = ! (
		lmem_pipe_empty () &&
		delayed_reg_write_queue_empty ()
	);*/
}

// update stop_pending flag
void update_stop_pending () {
	// wait 7 clocks
	kern.stop_pending_counter --;
	if( kern.stop_pending_counter == 0 )
		kern.stop_pending = FALSE;
}

//
bool_t prockern_k64fifo_empty () {
	return (kern.k64fifo_size <= 0);
}


//
void prockern_do_one_step (uint64_t k64clock) {

	instr_t next_instr = 0;
	bool_t  start_new_instr, shift_pipes, update_pc;
	int     ldc2flag = 0;

	// set k64clock
	kern.k64clock = k64clock;

	// 'sync' instruction support
	// do not execute anything until all previous instructions finish executing
	// update and check flag
	if (kern.sync_pending) update_sync_pending ();
	if (kern.stop_pending) update_stop_pending ();
	if (kern.sync_pending || kern.stop_pending) {

		// still waiting
		start_new_instr = FALSE;
		shift_pipes     = TRUE;
		update_pc       = FALSE;

		goto do_step1;

	}

	// not in run mode?
	if (kern.run_flag == 0) {

		if( kern.status.fields.fe == 0 ) {

			// read next instruction from fifo
			if ( !prockern_k64fifo_empty() ){
				get_next_instr_from_k64fifo (&next_instr, &ldc2flag);
				kern.instr_from_k64fifo = TRUE;
			}

			// this fifo entry was written by dmtc2 or ldc2?
			if (ldc2flag == 0) {
				// dmtc2 => execute this instruction
				start_new_instr = TRUE;
				shift_pipes     = TRUE;
				update_pc       = FALSE;
			} else {
				// ldc2 => store this instruction into iram
				execute_k64ldc2(next_instr);
				start_new_instr = FALSE;
				shift_pipes     = FALSE;
				update_pc       = TRUE;
			}
			if ( prockern_k64fifo_empty() )
				write_ctrlreg (kern.k64clock, CTRLREG_STATUS, REG_STATUS_FE.ui32, REG_STATUS_FE.ui32, CP2);

		} else {
				if ( !prockern_k64fifo_empty() ) {
					//flush FE bit of Status register
					if( kern.status.fields.fe != 0 )
						write_ctrlreg (kern.k64clock, CTRLREG_STATUS, 0, REG_STATUS_FE.ui32, CP2);
				}
					// not in run mode & fifo is empty => only shift pipes
					start_new_instr = FALSE;
					shift_pipes     = TRUE;
					update_pc       = FALSE;
		}

	} else {
		// run mode
		// read next instruction from iram
		next_instr = get_next_instr_iram ();
		kern.instr_from_k64fifo = FALSE;
		start_new_instr = TRUE;
		shift_pipes     = TRUE;
		update_pc       = TRUE;
	}

do_step1:

	// start executing new instruction
	if (start_new_instr) {
		// execute instruction
		execute_instr_vliw (next_instr);
	}

	// shift pipes
	if (shift_pipes) {
		// shift cal pipe
		cal_pipe_shift ();

		// shift lmem pipe
		lmem_pipe_shift ();

		// shift fpu pipe
		delayed_reg_write_queue_shift ();
	}

	// update PC
	// if update_pc flag was set
	// or 'jump' instruction was executed
	// (for 'jump' instruction from k64 fifo support)
	if ( update_pc || ( (kern.jump_flag & !kern.call_jump) &
        !(kern.sync_pending || kern.stop_pending)) ) {
		set_next_pc ();
	}
	else{
		if(kern.call_jump){
			// if ldc2 and call_jump then don't update_pc.
			kern.jump_flag=FALSE;
			kern.call_jump=FALSE;
		}
	//FIXME what about ldc2 & loop_jump?
	}

	kern.written_regs_num = 0;
}



//
instr_t get_next_instr_iram () {

	uint64_t data64;
	instr_t instr;

	// read 64bit dword from iram
	data64 = read_iram_ui64 (reg_clockcount, kern.newpc);

//data64 = swap_dword(data64); // FIXME big/little endian?

	// store it into vliw instr
	instr = data64;
//	instr.hi.word = (uint32_t) (data64 >> 32);
//	instr.lo.word = (uint32_t) data64;

	return instr;
}


// store instruction into iram at pc address
void execute_k64ldc2 (instr_t instr) {

	uint64_t data;
	data = instr;
	write_iram_ui64 (kern.k64clock, reg_pc, data);

}



//
void prockern_dump_fpu (FILE *f)
{
	int i;
	uint32_t reg32;


	// print header
	fprintf(f, "FPU dump:\n");
	fprintf(f, "    |  Section 0  ||  Section 1  ||  Section 2  ||  Section 3  |\n");

	// print fccr registers
	fprintf(f, "fccr:");
	reg32 = rawread_fccr(0); fprintf(f, "| %02x |", (int) reg32);
	reg32 = rawread_fccr(1); fprintf(f, "| %02x |", (int) reg32);
	reg32 = rawread_fccr(2); fprintf(f, "| %02x |", (int) reg32);
	reg32 = rawread_fccr(3); fprintf(f, "| %02x |", (int) reg32);
	fprintf(f, "\n");

	// print FPR of all sections
	if (k128cp2_machine -> opt_dump_fpu_float) {

		// print fpr in float format
		#define reghi(_sect) rawread_fpr ((_sect),i) .f.hi
		#define reglo(_sect) rawread_fpr ((_sect),i) .f.lo
		for (i=0; i<FPR_SIZE; i++) {
			fprintf (f, "%2d: ",i);
			fprintf (f, "| (%10.1f, %-10.1f) |", reghi(0), reglo(0));
			fprintf (f, "| (%10.1f, %-10.1f) |", reghi(1), reglo(1));
			fprintf (f, "| (%10.1f, %-10.1f) |", reghi(2), reglo(2));
			fprintf (f, "| (%10.1f, %-10.1f) |", reghi(3), reglo(3));
			fprintf (f, "\n");
		}
		#undef reghi
		#undef reglo

	} else {

		// print fpr in hex format
		for (i=0; i<FPR_SIZE; i++) {
			fprintf (f, "%2d: ",i);
			fprintf (f, "| %016" PRIx64 " |", rawread_fpr(0,i).ui64);
			fprintf (f, "| %016" PRIx64 " |", rawread_fpr(1,i).ui64);
			fprintf (f, "| %016" PRIx64 " |", rawread_fpr(2,i).ui64);
			fprintf (f, "| %016" PRIx64 " |", rawread_fpr(3,i).ui64);
			fprintf (f, "\n");
		}
	}

	fflush(f);
}



//
void prockern_dump_ctrlreg (FILE *f)
{
	int i;
	uint64_t reg64;

 	// print CTRL
	fprintf(f, "CTRLREG dump:\n");
	for (i=0; i<10; i++) {
		reg64 = rawread_ctrlreg (i);
		fprintf(f, "%2d: %016" PRIx64 "\n", i, reg64);
	}

	fflush(f);
}


//
void prockern_dump_gpr (FILE *f)
{
	int i;
	uint64_t reg64;

 	// print GPR
	fprintf(f, "GPR dump:\n");
	for (i=0; i<GPR_SIZE; i++) {
		reg64 = rawread_gpr (i);
		fprintf(f, "%2d: %016" PRIx64 "\n", i, reg64);
	}

	fflush(f);
}


//
void prockern_dump_addrreg (FILE *f)
{
	int i;
	uint16_t r, n, m;

 	// print address register
	fprintf(f, "Address registers dump:\n");
	for (i=0; i<ADDRREG_SIZE; i++) {
		r = rawread_addran(i);
		n = rawread_addrnn(i);
		m = rawread_addrmn(i);
		fprintf(f, "%2d: %04x %04x %04x\n", i, r, n, m);
	}

	fflush(f);
}



// dump lstack
void prockern_dump_lstack ()
{
	int i;

	// read psp register
//	lsp = rawread_ctrlreg (CTRLREG_LSP);

	// print stack in reversed order
	sim_printf ("lstack dump (lsp=%d):", (int) kern.lsp_cur);
	for (i=kern.lsp_cur; i>0; i--) {
		#define ls kern.lstack[i]
		sim_printf ("  %2d: lc=%d  la.las=0x%04x  la.lae=0x%04x",
			i, ls.lc, ls.la.fields.las, ls.la.fields.lae
		);
		#undef ls
	}

}


// dump pstack
void prockern_dump_pstack ()
{
	int i, psp;

	// read psp register
	psp = rawread_ctrlreg (CTRLREG_PSP);

	// print pstack in reversed order
	sim_printf ("pstack dump:");
	for (i=psp; i>0; i--) {
		sim_printf ("  %2d: 0x%04x", i, kern.pstack[i].ret_pc);
	}

}


//
void prockern_dump_lmem (FILE *f, int sect)
{
	int i;
	uint64_t data;
	uint32_t datahi, datalo;

 	// print local memory of given section
	fprintf(f, "Local memory dump, section %d:\n", sect);
	for (i=0; i<LMEM_SIZE/sizeof(uint64_t); i++) {
		data = rawread_lmem_ui64 (sect, i);
		datalo = data;
		datahi = data >> 32;
		fprintf(f, "%04d: %016" PRIx64 " (%10.5f, %-10.5f)\n", i, data, hex2float(datahi), hex2float(datalo));
	}

	fflush(f);
}

//
void prockern_dump_lmem_pipe (FILE *f)
{
    int i;

    #define get_pindex(_i) ((kern.lmem_pipe_start + (_i)) % LMEM_PIPE_SIZE)
    #define ent(_i) kern.lmem_pipe[get_pindex(_i)]
    if(kern.lmem_pipe_numentries == 0)
        return;
    for(i=0;i<LMEM_PIPE_SIZE;i++){
        fprintf(f, "tkt=%d addr=%04x fn=%02d rn=%02d annv=%04x ofs=%04x data=%016" PRIx64 "\n", ent(i).tkt, ent(i).addr, ent(i).fpr_num, ent(i).rn, ent(i).annewval, ent(i).offset, ent(i).data[0].data0);
    }

    fflush(f);
    #undef ent
    #undef get_pindex
}


//
// adjust pc
void set_next_pc () {

    regid_t  regid;
	regval_t rv;
	uint32_t pc;

	pc = kern.newpc;

	// check if we in the delay slot
/*	if (kern.delay_slot_flag) {
		kern.delay_slot_flag = FALSE;
		pc += INSTR_SIZE;
		kern.newpc = pc;
		write_ctrlreg (reg_clockcount, CTRLREG_PC, pc, NOMASK64, CP2);
		goto _return;
	}*/

	//write loop control registers
		if(kern.loop_last_it == TRUE){
			//we should restore new lc value form lstack
			regid  = REGID  (0, CTRLREG, CTRLREG_LC, NONE);
			rv = REGVAL (ctrlreg, kern.lc_cur, NOMASK64, CP2);
			delayed_reg_write (&regid, &rv, 1, BYPASS_NO);
			//we should write LA only on last iteration, if not we write the same value - is this right?
	   		regid  = REGID (0, CTRLREG, CTRLREG_LA, NONE);
			rv = REGVAL (ctrlreg, kern.la_cur.ui32, NOMASK64, CP2);
			delayed_reg_write (&regid, &rv, 1, BYPASS_NO);
			//we should write LSP only on last iteration, if not we write the same value - is this right?
	   		regid  = REGID (0, CTRLREG, CTRLREG_LSP, NONE);
			rv = REGVAL (ctrlreg, kern.lsp_cur, NOMASK64, CP2);
			delayed_reg_write (&regid, &rv, 1, BYPASS_NO);
			kern.loop_last_it = FALSE;
		}
		if(kern.loop_jump == TRUE){
			//we are in the delaye slot of loop instruction and write new lc
			regid  = REGID  (0, CTRLREG, CTRLREG_LC, NONE);
			rv = REGVAL (ctrlreg, kern.lc_cur, NOMASK64, CP2);
			delayed_reg_write (&regid, &rv, 1, BYPASS_NO);
			kern.loop_jump = FALSE;
		}

	if(kern.call_jump == TRUE)
		kern.call_jump = FALSE;

	// check jump
	if (kern.jump_flag) {
		// jump
		kern.jump_flag = FALSE;
		pc         = kern.jump_newpc;
		write_ctrlreg (reg_clockcount, CTRLREG_PC, pc, NOMASK64, CP2);
		kern.newpc = pc;
		goto _return;
	}

	// check loop end
	pc = set_next_pc_process_loop_end ();

	write_ctrlreg (reg_clockcount, CTRLREG_PC, pc, NOMASK64, CP2);
	kern.newpc = pc;
_return:

	// iram overrun check
	if (pc >= IRAM_SIZE) {
		// FIXME: cyclic iram
		sim_print_string ("end of iram reached. halting...");
		// flush Run bit in Status register
		// write Status register using mask
		write_ctrlreg (
			reg_clockcount,
			CTRLREG_STATUS,
			0,
			REG_STATUS_RUN.ui32,
			CP2
		);
//		mch->halted = TRUE;
	}
}

// Process loop end.
// Handle lstack and LA, LC registers.
// Returns next PC value.
reg_pc_t set_next_pc_process_loop_end () {

	reg_pc_t newpc;

	// check loop end
	if ((kern.lsp_cur>0) && (kern.newpc == kern.la_cur.fields.lae)) {

		// decrement loop counter
//		sim_assert (kern.lc > 0);
//		kern.lc -= 1;
		kern.lc_cur -= 1;
		kern.lstack[kern.lsp_cur].lc = kern.lc_cur;

		// it was last iteration?
		if (kern.lc_cur == 0) {

			// decrement LSP register
			sim_assert (kern.lsp_cur > 0);
			kern.lsp_cur -= 1;

			kern.loop_last_it = TRUE;
			// pop LA, LC from lstack
			kern.la_cur = kern.lstack[kern.lsp_cur].la;
			kern.lc_cur = kern.lstack[kern.lsp_cur].lc;

			// set newpc to next instruction
			newpc = kern.newpc + INSTR_SIZE;

		} else {

			kern.loop_jump = TRUE;
			// it was not last iteration => jump to the loop start
			prockern_jump (kern.la_cur.fields.las);
			//but before execute instruction in the delay slot => pc++
			newpc = kern.newpc + INSTR_SIZE;

		}

		// print optionally lstack
		if (k128cp2_machine -> opt_dump_lstack) {
			prockern_dump_lstack ();
		}

	} else {

		// not loop end => step on next instruction
		newpc = kern.newpc + INSTR_SIZE;
	}

	return newpc;
}


//
void delayed_reg_write_init () {

	int i;

	// set start index of queue
	kern.delayed_reg_write_queue_start = 0;

	// free all entries
	for (i=0; i<DELAYED_REG_WRITE_QUEUE_SIZE; i++) {
		kern.delayed_reg_write_queue[i].entry_size = 0;
	}
	kern.delayed_reg_write_queue_numentries = 0;
}



// delayed register writing (emulate pipe)
// check for entry overflow
#define check_overflow(_index) \
	if (kern.delayed_reg_write_queue[(_index)].entry_size >= DELAYED_REG_WRITE_QUEUE_ENTRY_SIZE) { \
		sim_error_internal(); \
	}
#define get_qindex(_delay) ((kern.delayed_reg_write_queue_start + (_delay)) % DELAYED_REG_WRITE_QUEUE_SIZE)
#define qslot(_qindex,_slot) kern.delayed_reg_write_queue[_qindex].entry[_slot]

//
regval_t reg_read_on_bypass_raw (regid_t id) {

	regval_t rv;
	int i,j,qindex;

	// try to read from bypass
	// search all entries in reg_write_queue for register
	for(i=8;i>=0;i--){//FIXME 9 - depth of calc pipe

		qindex = get_qindex(i);
		for(j=0;j<kern.delayed_reg_write_queue[qindex].entry_size;j++){
			if( (qslot(qindex,j).regid.type == id.type) &&
				(qslot(qindex,j).regid.regno == id.regno) &&
				(qslot(qindex,j).regid.section == id.section) &&
				(qslot(qindex,j).bypass) == BYPASS_YES){
				rv.val = qslot(qindex,j).regval.val;

				goto return_result;
			}
		}
	}

	// read from register itself if failed to read from pipe
	rv = reg_read_raw(id);

	return_result:

	return rv;

}

//
regval_t reg_read_ext_raw (regid_t id) {

	regval_t rv;
	int i;

	//try to read from saved values of the registers writen on this clock
	for(i=0;i<kern.written_regs_num;i++){
		if( (kern.written_regs[i].regid.type == id.type) &&
			(kern.written_regs[i].regid.regno == id.regno) &&
			(kern.written_regs[i].regid.section == id.section) ){
			rv.val = kern.written_regs[i].regval.val;

			goto return_result;
		}
	}

	//read form register itself
	rv = reg_read_raw(id);

	return_result:

	return rv;
}

//
regval_t reg_read_ext (regid_t id, uint64_t clck, readwrite_ctrlreg_srcid_t srcid) {

	regval_t rv, rv1;
	regid_t regid;
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
		default: sim_error_internal(); break;
	}

	// read from register itself if failed to read from pipe
	rv = reg_read_ext_raw(id);

	//notify observer
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

//
regval_t reg_read_on_bypass (regid_t id, uint64_t clck, readwrite_ctrlreg_srcid_t srcid) {

	regval_t rv, rv1;
	regid_t regid;
	event_src_t evsrc;
	int qindex,i,j;

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

	rv = reg_read_on_bypass_raw(id);

	// try to send 'bypass event'
	for(i=8;i>=0;i--){//FIXME 9 - depth of calc pipe
		qindex = get_qindex(i);
		for(j=0;j<kern.delayed_reg_write_queue[qindex].entry_size;j++){
			if( (qslot(qindex,j).regid.type == id.type) &&
				(qslot(qindex,j).regid.regno == id.regno) &&
				(qslot(qindex,j).regid.section == id.section) &&
				(qslot(qindex,j).bypass) == BYPASS_YES){

				//notify observer
				send_newevent (clck, evsrc, EVENT_TYPE_READ_BYPASS, id.section, id.regno, rv.val.ui64, 1);
				goto further;
			}
		}
	}

	//notify observer
	send_newevent (clck, evsrc, EVENT_TYPE_READ, id.section, id.regno, rv.val.ui64, 1);

	further:

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

//
void delayed_reg_write (regid_t *regid_ptr, regval_t *regval_ptr, int delay, bool_t bypass) {

	int qindex, free_slot_index;

	// calculate queue index
	qindex = get_qindex(delay);

	// get a slot in the queue to write
	check_overflow(qindex);
	free_slot_index = kern.delayed_reg_write_queue[qindex].entry_size;

	// write at this slot
	qslot(qindex,free_slot_index).regid          = *regid_ptr;
	qslot(qindex,free_slot_index).regval         = *regval_ptr;
	qslot(qindex,free_slot_index).insn_start_clk = reg_clockcount-1;
	qslot(qindex,free_slot_index).bypass         = bypass;

	// increment entry size
	kern.delayed_reg_write_queue[qindex].entry_size += 1;
	kern.delayed_reg_write_queue_numentries += 1;

}

// do delayed writes from entry 0, empty this entry and shift queue
void delayed_reg_write_queue_shift () {

	int i, qindex;

	// get queue index
	qindex = get_qindex(0);

	// for all slots in this entry
	for (i=0; i<kern.delayed_reg_write_queue[qindex].entry_size; i++) {

		reg_write (
			qslot(qindex,i).regid,
			qslot(qindex,i).regval,
			qslot(qindex,i).insn_start_clk,
			READWRITE_CTRLREG_SRCID_CP2
		);
	}

/*
	int begin, end;
	begin = 900;
	end = 1000;
	int j;
	if((reg_clockcount < end) && (reg_clockcount > begin) ){
	for(i=8; i>=0; i--){//FIXME 9 - depth of calc pipe

		qindex = get_qindex(i);
		for(j=0;j<kern.delayed_reg_write_queue[qindex].entry_size;j++){
			//notify observer
			sim_printf("quedump qindex=%d  i=%d  j=%d  val=%x",qindex,i,j,qslot(qindex,j).regval.val);
			}
		}
	}
}
*/


	//raise kern.run_flag on next clock after "run" instr
	for (i=0; i<kern.delayed_reg_write_queue[(qindex+1) % DELAYED_REG_WRITE_QUEUE_SIZE].entry_size; i++) {
		if( ((qslot((qindex+1) % DELAYED_REG_WRITE_QUEUE_SIZE,i).regid.type == REGID_TYPE_CTRLREG) &&
			(qslot((qindex+1) % DELAYED_REG_WRITE_QUEUE_SIZE,i).regid.regno == CTRLREG_STATUS) &&
			(qslot((qindex+1) % DELAYED_REG_WRITE_QUEUE_SIZE,i).regval.val.ui64 & REG_STATUS_RUN.ui32) != 0) ){
			kern.run_flag = TRUE;
       }
	}

	// empty this entry
	kern.delayed_reg_write_queue_numentries -= kern.delayed_reg_write_queue[qindex].entry_size;
	kern.delayed_reg_write_queue[qindex].entry_size	= 0;

	// shift queue (increment start position)
	kern.delayed_reg_write_queue_start = get_qindex(1);

}



#undef check_overflow


// 'call', 'jump' and 'loop' support
void prockern_jump (reg_pc_t newpc) {

	// consistency check
	if (kern.jump_flag) {
		sim_error_internal();
	}

	// remember jump
	// pc will be adjusted in set_next_pc
	kern.jump_flag  = TRUE;
	kern.jump_newpc = newpc;
}

#define get_pindex(_i) ((kern.lmem_pipe_start + (_i)) % LMEM_PIPE_SIZE)
#define get_cpindex(_i) ((kern.cal_pipe_start + (_i)) % CAL_PIPE_SIZE)
// Start new loop
void prockern_loop_new  (uint64_t nloops, uint32_t endaddr) {

	uint32_t newlsp;
	reg_lc_t newlc = 0;
	reg_la_t newla = { .ui32 = 0};
	regid_t regid;
	regval_t regval;

	//we need only low bits
	nloops = nloops & 0x00000000ffffffffLL;

	// loop or jump?
	if (nloops == 0) {
		// FIXME have delay slot here? if yes, then uncomment next line
		// kern.delay_slot_flag  = TRUE;

		//nulify previous instruction
/*		pindex = get_cpindex(kern.cal_pipe_numentries-1);
		kern.cal_pipe[pindex].nlf        = TRUE;
		pindex = get_pindex(kern.lmem_pipe_numentries-1);
		kern.lmem_pipe[pindex].nlf       = TRUE;*/

		// jump
		prockern_jump(endaddr+2);
		return;
	}

	// read LSP register and set new lsp
	kern.lsp_cur++;
	newlsp  = kern.lsp_cur;

	// set new LA, LC values
	newla.fields.las = kern.newpc; // + INSTR_SIZE;
	newla.fields.lae = endaddr;
	newlc            = nloops;

	// lstack overflow check
	if (kern.lsp >= LOOP_MAX_DEPTH - 1) {

		//we should not increment lsp in case LOOP_MAX_DEPTH
		kern.lsp_cur--;

		// write LOE bit of Status register
		write_ctrlreg (
			reg_clockcount,
			CTRLREG_STATUS,
			REG_STATUS_LOE.ui32,
			REG_STATUS_LOE.ui32,
			CP2
		);

		// push LA, LC registers to lstack
		kern.lstack[newlsp-1].la = kern.la_cur;
		kern.lstack[newlsp-1].lc = kern.lc_cur;

	} else {

		kern.la_cur = newla;
		kern.lc_cur = newlc;

		// push LA, LC registers to lstack
		kern.lstack[newlsp].la = kern.la_cur;
		kern.lstack[newlsp].lc = kern.lc_cur;

		// write LSP
		regid  = REGID (0, CTRLREG, CTRLREG_LSP, NONE);
		regval = REGVAL (ctrlreg, newlsp, NOMASK64, NONE);
		delayed_reg_write (&regid, &regval, 2, BYPASS_NO);

	}

	// write LC
	regid  = REGID (0, CTRLREG, CTRLREG_LC, NONE);
	regval = REGVAL (ctrlreg, newlc, NOMASK64, NONE);
	delayed_reg_write (&regid, &regval, 2, BYPASS_NO);
	kern.lc_cur = newlc;

	// write LA
	regid  = REGID (0, CTRLREG, CTRLREG_LA, NONE);
	regval = REGVAL (ctrlreg, newla.ui32, NOMASK64, NONE);
	delayed_reg_write (&regid, &regval, 2, BYPASS_NO);
	kern.la_cur.ui32 = newla.ui32;

	// print optionally lstack
	if (k128cp2_machine -> opt_dump_lstack) {
		prockern_dump_lstack ();
	}

}


//
void prockern_call_new (uint32_t target_pc) {

	uint32_t newpsp;
	regid_t regid;
	regval_t regval;

	// read PSP register and set new psp
	kern.psp_cur++;
	newpsp = kern.psp_cur;

	kern.call_jump = TRUE;
	// pstack overflow check
	if (kern.psp >= CALL_MAX_DEPTH - 1) {

		//we should not increment psp in case CALL_MAX_DEPTH
		kern.psp_cur--;

		// write POE bit of Status register
		write_ctrlreg (
			reg_clockcount,
			CTRLREG_STATUS,
			REG_STATUS_POE.ui32,
			REG_STATUS_POE.ui32,
			CP2
		);

		// push return address to pstack
		// return address points to instruction after delay slot
		kern.pstack[newpsp-1].ret_pc = kern.newpc + INSTR_SIZE;

		// jump to the target_pc
		kern.delay_slot_flag  = TRUE;
		prockern_jump (target_pc);

	} else {

		// push return address to pstack
		// return address points to instruction after delay slot
		kern.pstack[newpsp].ret_pc = kern.newpc + INSTR_SIZE;

		// write PSP register
		regid  = REGID (0, CTRLREG, CTRLREG_PSP, NONE);
		regval = REGVAL (ctrlreg, newpsp, NOMASK64, NONE);
		delayed_reg_write (&regid, &regval, 1, BYPASS_NO);

		// jump to the target_pc
		kern.delay_slot_flag  = TRUE;
		prockern_jump (target_pc);

	}

	// optionally, dump pstack
	if (k128cp2_machine -> opt_dump_pstack) {
		prockern_dump_pstack ();
	}

}


//
void prockern_call_ret () {

	uint32_t psp;
	regid_t regid;
	regval_t regval;

	// read PSP register
	psp = kern.psp_cur;

	// check pstack underflow
	if (psp == 0) {

		// write PUE bit of Status register
		write_ctrlreg (
			reg_clockcount,
			CTRLREG_STATUS,
			REG_STATUS_PUE.ui32,
			REG_STATUS_PUE.ui32,
			CP2
		);

	} else {

		// jump to the return address
		kern.delay_slot_flag  = TRUE;
		prockern_jump (kern.pstack[psp].ret_pc);

		// decrement psp
		psp -= 1;
		kern.psp_cur--;

		// write PSP register
		regid  = REGID (0, CTRLREG, CTRLREG_PSP, NONE);
		regval = REGVAL (ctrlreg, psp, NOMASK64, NONE);
		delayed_reg_write (&regid, &regval, 1, BYPASS_NO);
	}

	// print optionally pstack
	if (k128cp2_machine -> opt_dump_pstack) {
		prockern_dump_pstack ();
	}

}


//
void lmem_init () {

	// empty pipe
	kern.lmem_pipe_start      = 0;
	kern.lmem_pipe_numentries = 0;
}

//
void cal_init () {
	// empty pipe
	kern.cal_pipe_start      = 0;
	kern.cal_pipe_numentries = 0;
}


//
void  prockern_cal_newop (instr_t instr) {

	// place this operation in lmem pipe
	cal_pipe_new_entry (instr);

}

//
void  prockern_lmem_newop (instr_t instr) {

	// place this operation in lmem pipe
	lmem_pipe_new_entry (instr);

}


// table with parameters of lmem instructions
#define UPDRN_YES TRUE
#define UPDRN_NO  FALSE
#define MEMOP_YES TRUE
#define MEMOP_NO  FALSE
const lmem_instr_params_t lmem_instr_params_table[] = {
	{"lwi"      ,  INSTR_OPCODE2_LWI      ,  LMEM_INSTRTYPE_LOAD   ,  LMEM_ADDRTYPE_IMM ,  LMEM_DATASIZE_WORD ,  UPDRN_NO  },
	{"ldi"      ,  INSTR_OPCODE2_LDI      ,  LMEM_INSTRTYPE_LOAD   ,  LMEM_ADDRTYPE_IMM ,  LMEM_DATASIZE_DWORD,  UPDRN_NO  },
	{"swi"      ,  INSTR_OPCODE2_SWI      ,  LMEM_INSTRTYPE_STORE  ,  LMEM_ADDRTYPE_IMM ,  LMEM_DATASIZE_WORD ,  UPDRN_NO  },
	{"sdi"      ,  INSTR_OPCODE2_SDI      ,  LMEM_INSTRTYPE_STORE  ,  LMEM_ADDRTYPE_IMM ,  LMEM_DATASIZE_DWORD,  UPDRN_NO  },
	{"lwnp"     ,  INSTR_OPCODE2_LWNP     ,  LMEM_INSTRTYPE_LOAD   ,  LMEM_ADDRTYPE_NP  ,  LMEM_DATASIZE_WORD ,  UPDRN_YES },
	{"ldnp"     ,  INSTR_OPCODE2_LDNP     ,  LMEM_INSTRTYPE_LOAD   ,  LMEM_ADDRTYPE_NP  ,  LMEM_DATASIZE_DWORD,  UPDRN_YES },
	{"swnp"     ,  INSTR_OPCODE2_SWNP     ,  LMEM_INSTRTYPE_STORE  ,  LMEM_ADDRTYPE_NP  ,  LMEM_DATASIZE_WORD ,  UPDRN_YES },
	{"sdnp"     ,  INSTR_OPCODE2_SDNP     ,  LMEM_INSTRTYPE_STORE  ,  LMEM_ADDRTYPE_NP  ,  LMEM_DATASIZE_DWORD,  UPDRN_YES },
	{"lwnm"     ,  INSTR_OPCODE2_LWNM     ,  LMEM_INSTRTYPE_LOAD   ,  LMEM_ADDRTYPE_NM  ,  LMEM_DATASIZE_WORD ,  UPDRN_YES },
	{"ldnm"     ,  INSTR_OPCODE2_LDNM     ,  LMEM_INSTRTYPE_LOAD   ,  LMEM_ADDRTYPE_NM  ,  LMEM_DATASIZE_DWORD,  UPDRN_YES },
	{"swnm"     ,  INSTR_OPCODE2_SWNM     ,  LMEM_INSTRTYPE_STORE  ,  LMEM_ADDRTYPE_NM  ,  LMEM_DATASIZE_WORD ,  UPDRN_YES },
	{"sdnm"     ,  INSTR_OPCODE2_SDNM     ,  LMEM_INSTRTYPE_STORE  ,  LMEM_ADDRTYPE_NM  ,  LMEM_DATASIZE_DWORD,  UPDRN_YES },
	{"lwo"      ,  INSTR_OPCODE2_LWO      ,  LMEM_INSTRTYPE_LOAD   ,  LMEM_ADDRTYPE_OFFS,  LMEM_DATASIZE_WORD ,  UPDRN_YES },
	{"ldo"      ,  INSTR_OPCODE2_LDO      ,  LMEM_INSTRTYPE_LOAD   ,  LMEM_ADDRTYPE_OFFS,  LMEM_DATASIZE_DWORD,  UPDRN_YES },
	{"swo"      ,  INSTR_OPCODE2_SWO      ,  LMEM_INSTRTYPE_STORE  ,  LMEM_ADDRTYPE_OFFS,  LMEM_DATASIZE_WORD ,  UPDRN_YES },
	{"sdo"      ,  INSTR_OPCODE2_SDO      ,  LMEM_INSTRTYPE_STORE  ,  LMEM_ADDRTYPE_OFFS,  LMEM_DATASIZE_DWORD,  UPDRN_YES },
	{"updaddr"  ,  INSTR_OPCODE2_UPDADDR  ,  LMEM_INSTRTYPE_UPDADDR,  LMEM_ADDRTYPE_OFFS,  LMEM_DATASIZE_WORD ,  UPDRN_YES },
	{"updaddrnp",  INSTR_OPCODE2_UPDADDRNP,  LMEM_INSTRTYPE_UPDADDR,  LMEM_ADDRTYPE_NP  ,  LMEM_DATASIZE_WORD ,  UPDRN_YES },
	{"updaddrnm",  INSTR_OPCODE2_UPDADDRNM,  LMEM_INSTRTYPE_UPDADDR,  LMEM_ADDRTYPE_NM  ,  LMEM_DATASIZE_WORD ,  UPDRN_YES },
	{NULL, 0, 3, 0, 0, 0}
};
#undef UPDRN_YES
#undef UPDRN_NO
#undef MEMOP_YES
#undef MEMOP_NO


// find entry in lmem instruction parameters table
const lmem_instr_params_t* lmem_get_instr_params(instr_t instr) {

	int i;

	i = 0;
	// run through table comparing opcodes
	while (
		(lmem_instr_params_table[i].name    != NULL) &&
		(lmem_instr_params_table[i].opcode2 != instr_opcode2(instr))
	) {
		// next entry
		i ++;
	}

	// return result (possibly NULL)
	return(&(lmem_instr_params_table[i]));
}


// read address register an
// from register itself, or bypass if possible
void read_addrreg_an_bypass (int tkt, addrreg_t rn, uint32_t *dataptr) {

	int i, j, qindex;
	uint32_t data;
	bool_t read_from_lmempipe = FALSE, read_from_queue = FALSE;
	regval_t rv;
	event_t ev;

	// try to read from bypass
	// search all entries in lmem pipe for "an" register writing
	i = 0;
	while (i<(kern.lmem_pipe_numentries-1)) {

		// check address register number, writing flag, stage
#define ent(_i) kern.lmem_pipe[get_pindex(_i)]
		if (
			(ent(i).rn == rn) &&
			(ent(i).params->updatean) &&
			(ent(i).stage == 3) &&
			(ent(i).nlf != 1)
		) {

			// read data to be written to "an" from pipe
			data = ent(i).annewval;

			read_from_lmempipe = TRUE;

			// notify observer
			ev.src   = EVENT_SRC_ADDRAN_BP;
			ev.type  = EVENT_TYPE_READ;
			ev.addr  = rn;
			ev.data  = data;
			ev.sect  = SNONE;
			ev.mask  = 0;
			ev.clck  = tkt;
			ev.mod   = 1;
			observer_newevent (&ev);

			// exit from search cycle
//			goto return_result;
			goto read_from_write_queue;
		}

		// next entry
		i += 1;
	}
#undef ent

read_from_write_queue:

	//try to read from registers write queue
	for(i=8;i>=0;i--){
		qindex = get_qindex(i);
		for(j=0;j<kern.delayed_reg_write_queue[qindex].entry_size;j++){
			if( (qslot(qindex,j).regid.type == REGID_TYPE_ADDRAN) &&
				(qslot(qindex,j).regid.regno == rn) &&
				(qslot(qindex,j).regid.section == REGID_SECT_NONE) &&
				(qslot(qindex,j).bypass) == BYPASS_YES){

				rv.val = qslot(qindex,j).regval.val;
				rv.mask = qslot(qindex,j).regval.mask;
				data = rv.val.addran & rv.mask;
				read_from_queue = TRUE;

				// notify observer
				ev.src   = EVENT_SRC_ADDRAN_BP;
				ev.type  = EVENT_TYPE_READ;
				ev.addr  = rn;
				ev.data  = data;
				ev.sect  = SNONE;
				ev.mask  = 0;
				ev.clck  = tkt;
				ev.mod   = 1;
				observer_newevent (&ev);

				goto check_read;
			}
		}
	}
check_read:

	if( read_from_queue && read_from_lmempipe ){
		sim_error_internal();
	}

	if( read_from_queue || read_from_lmempipe ){
		goto return_result;
	}


	// consistency check
/*	if (i != kern.lmem_pipe_numentries-1) {
		sim_error_internal();
	}*/

	// read from register itself if failed to read from pipes
	data = read_addran (tkt,rn);

	// return result
return_result:

	*dataptr = data;

}
#undef qslot
#undef get_qindex

bool_t check_nlf (instr_t instr){

	bool_t rv;
	regval_t regval;
	regid_t regid;

	switch(instr_opcode2(instr)){
		case INSTR_OPCODE2_DO:
								regid  = REGID (SNONE, GPR, instr_gs(instr), BOTH);
								regval = reg_read_on_bypass_raw(regid);
								if(regval.val.ui64 == 0)
									rv = TRUE;
								else
									rv = FALSE;
								break;
		case INSTR_OPCODE2_DOI:
								if(instr_cnt10(instr) == 0)
									rv = TRUE;
								else
									rv = FALSE;
								break;
		default: rv = FALSE;
	}
	return rv;

}


// add new entry into cal_pipe
void cal_pipe_new_entry (instr_t instr) {

	int pindex;

	// overflow check
	if (kern.lmem_pipe_numentries > CAL_PIPE_SIZE) {
		sim_error_internal();
	}

	// get pipe index
	pindex = get_cpindex(kern.cal_pipe_numentries);

	// write lmem_pipe entry
    kern.cal_pipe[pindex].tkt        = reg_clockcount;
	kern.cal_pipe[pindex].instr      = instr;
	kern.cal_pipe[pindex].opcode     = instr_opcode(instr);
	kern.cal_pipe[pindex].stage      = 0; // start
	kern.cal_pipe[pindex].nlf        = kern.nulify;

	//
	kern.cal_pipe_numentries += 1;

}

// add new entry into lmem_pipe
void lmem_pipe_new_entry (instr_t instr) {

	int rn, pindex;
	uint32_t addr;
	addrreg_t offs;
	const lmem_instr_params_t *params;

	// read instruction fields
	rn = instr_rn(instr);

	// overflow check
	if (kern.lmem_pipe_numentries > LMEM_PIPE_SIZE) {
		sim_error_internal();
	}

	// get pipe index
	pindex = get_pindex(kern.lmem_pipe_numentries);

	// get instruction parameters
	if ((params = lmem_get_instr_params(instr)) == NULL) {
		sim_error_internal();
		return;
	}

	if( params->name == 0 ){
    	kern.lmem_pipe[pindex].tkt        = reg_clockcount;
		kern.lmem_pipe[pindex].stage      = 0; // start
		kern.lmem_pipe[pindex].params     = params;
		kern.lmem_pipe[pindex].instr      = instr;
		kern.lmem_pipe[pindex].nlf        = kern.nulify;
		kern.lmem_pipe_numentries += 1;
		kern.nulify = check_nlf(instr);
		return;
	}

	// get address and set new an value if needed
	switch (params->addrtype) {
		// immediate operand
		case LMEM_ADDRTYPE_IMM :
			addr = instr_2imm13(instr);
			kern.lmem_pipe[pindex].addr = addr;
			offs = 0;
			break;
		// an + nn
		case LMEM_ADDRTYPE_NP :
			offs = 0;
			break;
		// an - nn
		case LMEM_ADDRTYPE_NM :
			offs = 0;
			break;
		// an + offset
		case LMEM_ADDRTYPE_OFFS :
			offs = instr_offset13_sigext32(instr);
			break;
		default: sim_error_internal(); break;
	}

	// write lmem_pipe entry
    kern.lmem_pipe[pindex].tkt        = reg_clockcount;
	kern.lmem_pipe[pindex].params     = params;
	kern.lmem_pipe[pindex].stage      = 0; // start
	kern.lmem_pipe[pindex].fpr_num    = instr_ft2(instr);
	kern.lmem_pipe[pindex].rn         = rn;
	kern.lmem_pipe[pindex].offset     = offs      ;
	kern.lmem_pipe[pindex].instr      = instr_opcode2(instr);
	kern.lmem_pipe[pindex].nlf        = kern.nulify;

	//
	kern.lmem_pipe_numentries += 1;

	kern.nulify = check_nlf(instr);

}

//
int bit_rev(int num, int order){

    int sum;
    int i;
    unsigned int pw, rpw;

    sum = 0;
    pw = 1;
    rpw = 1 << order;

    for(i = 0; i <= order; i++) {
        sum += (num & pw) ? rpw : 0;
        pw <<= 1;
        rpw >>= 1;
    }

    return sum;
}

int adjust_addr(int addr, int offs, int mode)
{
	if( mode == ADDR_BITREV_MOD ){
		if( offs >> 31 == 0)
			return bit_rev(bit_rev(addr,12) + bit_rev(abs(offs),12),12);
		else
			return bit_rev(bit_rev(addr,12) - bit_rev(abs(offs),12),12);
	}
    if( mode == ADDR_LINE_MOD )
        return addr + offs;
    else{
		return (addr & ~mode) + ((addr+offs) & mode);
	}

}

// shift calc. operations pipe
// (execute next stage for all entries)
void cal_pipe_shift () {

	int cpindex, i, finished;

	// execute stage for all entries in pipe
	i = 0;
	while (i<kern.cal_pipe_numentries) {

		// get index in pipe
		cpindex = get_cpindex(i);

		// execute stage for this entry
		finished = cal_pipe_execute_stage (& (kern.cal_pipe[cpindex]));


		// finished => empty this entry
		if (finished) {

			// consistency check
			// oldest entries must be finished earlier
			if (i != 0) {
				sim_error_internal();
			}

			// remove this entry (move pipe start index)
			kern.cal_pipe_start = get_cpindex(1);
			kern.cal_pipe_numentries -= 1;

		} else {
			// next entry
			i += 1;
		}
	}
}

// shift local memory operations pipe
// (execute next stage for all entries)
void lmem_pipe_shift () {

	int pindex, i, finished;

	// execute stage for all entries in pipe
	i = 0;
	while (i<kern.lmem_pipe_numentries) {

		// get index in pipe
		pindex = get_pindex(i);

		// execute stage for this entry
		finished = lmem_pipe_execute_stage (& (kern.lmem_pipe[pindex]));

		// finished => empty this entry
		if (finished) {

			// consistency check
			// oldest entries must be finished earlier
			if (i != 0) {
				sim_error_internal();
			}

			// remove this entry (move pipe start index)
			kern.lmem_pipe_start = get_pindex(1);
			kern.lmem_pipe_numentries -= 1;

		} else {
			// next entry
			i += 1;
		}
	}
}
#undef get_pindex
#undef get_cpindex


// update address registers
// called on WRITEFPR and WRITEMEM stages
void lmem_update_addrregs (lmem_pipe_entry_t *ent) {

	if (ent -> params -> updatean) {
		write_addran (ent->tkt, ent->rn, ent->annewval);
	}

}

#define lmem_pipe_stage_def(_st) void lmem_pipe_execute_stage_##_st(lmem_pipe_entry_t *ent)
//
lmem_pipe_stage_def(ADDR) {

	// read index registers and calculate address
	uint32_t addr,mode;
	addrreg_t annewval, offs;

	// get address and set new an value if needed
	switch (ent->params->addrtype) {
		// immediate operand
		case LMEM_ADDRTYPE_IMM :
			addr = ent->addr;
			annewval = ent->annewval;
			break;
		// an + nn
		case LMEM_ADDRTYPE_NP :
			read_addrreg_an_bypass (ent->tkt, ent->rn, &addr);
			offs = reg_read_on_bypass(REGID(SNONE, ADDRNN, ent->rn, BOTH), ent->tkt, READWRITE_CTRLREG_SRCID_CP2).val.addrnn;
			mode = reg_read_on_bypass(REGID(SNONE, ADDRMN, ent->rn, BOTH), ent->tkt, READWRITE_CTRLREG_SRCID_CP2).val.addrmn;
			annewval = adjust_addr(addr,(int) offs,mode);
			annewval &= 0x1fff;
			break;
		// an - nn
		case LMEM_ADDRTYPE_NM :
			read_addrreg_an_bypass (ent->tkt, ent->rn, &addr);
			offs = reg_read_on_bypass(REGID(SNONE, ADDRNN, ent->rn, BOTH), ent->tkt, READWRITE_CTRLREG_SRCID_CP2).val.addrnn;
			mode = reg_read_on_bypass(REGID(SNONE, ADDRMN, ent->rn, BOTH), ent->tkt, READWRITE_CTRLREG_SRCID_CP2).val.addrmn;
			annewval = adjust_addr(addr, (int)-offs,mode);
			annewval &= 0x1fff;
			break;
		// an + offset
		case LMEM_ADDRTYPE_OFFS :
			read_addrreg_an_bypass (ent->tkt, ent->rn, &addr);
			mode = reg_read_on_bypass(REGID(SNONE, ADDRMN, ent->rn, BOTH), ent->tkt, READWRITE_CTRLREG_SRCID_CP2).val.addrmn;
			offs = ent->offset;
			annewval = adjust_addr(addr,(int) offs,mode);
			annewval &= 0x1fff;
			break;
		default: sim_error_internal(); break;
	}

	ent->addr       = addr      ;
	ent->annewval   = annewval  ;

}



//
lmem_pipe_stage_def(READMEM) {

	int i;

	// read memory
	switch (ent -> params -> datasize) {

		case LMEM_DATASIZE_WORD :
			// read 64bit-word for each section
			for (i=0; i<NUMBER_OF_EXESECT; i++) {
				ent->data[i].data0 = read_lmem_ui64 (ent->tkt, i, ent->addr);
			}
			break;

		case LMEM_DATASIZE_DWORD :
			// read two 64bit-words for each section
			for (i=0; i<NUMBER_OF_EXESECT; i++) {
				ent->data[i].data0 = read_lmem_ui64 (ent->tkt, i, ( ent->addr>>1)<<1   );
				ent->data[i].data1 = read_lmem_ui64 (ent->tkt, i, ((ent->addr>>1)<<1)+1);
			}
			break;

		default: sim_error_internal(); break;
	}

}



//
lmem_pipe_stage_def(WRITEMEM) {

	int i;

	// write memory
	switch (ent -> params -> datasize) {

		case LMEM_DATASIZE_WORD :
			// write 64bit-word for each section
			for (i=0; i<NUMBER_OF_EXESECT; i++) {
				write_lmem_ui64(ent->tkt,i,ent->addr,ent->data[i].data0);
			}
			break;

		case LMEM_DATASIZE_DWORD :
			// write two 64bit-words for each section
			for (i=0; i<NUMBER_OF_EXESECT; i++) {
				write_lmem_ui64(ent->tkt,i,(ent->addr>>1)<<1,  ent->data[i].data0);
				write_lmem_ui64(ent->tkt,i,((ent->addr>>1)<<1)+1,ent->data[i].data1);
			}
			break;

		default: sim_error_internal(); break;
	}

}



//
lmem_pipe_stage_def(READFPR) {

	int i;
	regid_t regid;
	regval_t regval;

	// read register(s)
	switch (ent -> params -> datasize) {

		case LMEM_DATASIZE_WORD :
			// read one fpr for each section
			for (i=0; i<NUMBER_OF_EXESECT; i++) {
			    regid = REGID (i, FPR, ent->fpr_num, BOTH);
			    regval = reg_read_ext(regid, ent->tkt, READWRITE_CTRLREG_SRCID_NONE);
			    ent->data[i].data0 = regval.val.ui64;
//				ent->data[i].data0 = read_fpr (ent->tkt, i, ent->fpr_num) .ui64;
			}
			break;

		case LMEM_DATASIZE_DWORD :
			// read two fprs for each section
			for (i=0; i<NUMBER_OF_EXESECT; i++) {
			    regid = REGID (i, FPR, ent->fpr_num, BOTH);
			    regval = reg_read_ext(regid, ent->tkt, READWRITE_CTRLREG_SRCID_NONE);
			    ent->data[i].data0 = regval.val.ui64;
			    regid = REGID (i, FPR, ent->fpr_num+1, BOTH);
			    regval = reg_read_ext(regid, ent->tkt, READWRITE_CTRLREG_SRCID_NONE);
			    ent->data[i].data1 = regval.val.ui64;
//				ent->data[i].data0 = read_fpr (ent->tkt, i, ent->fpr_num)   .ui64;
//				ent->data[i].data1 = read_fpr (ent->tkt, i, ent->fpr_num+1) .ui64;
			}
			break;

		default: sim_error_internal(); break;
	}

}



//
lmem_pipe_stage_def(WRITEFPR) {

	int i;

	// write fpr(s)
	switch (ent -> params -> datasize) {

		case LMEM_DATASIZE_WORD :
			// write 64bit-word for each section
			for (i=0; i<NUMBER_OF_EXESECT; i++) {
				write_fpr (ent->tkt, i, ent->fpr_num, (fpr_t) {.ui64 = ent->data[i].data0});
			}
			break;

		case LMEM_DATASIZE_DWORD :
			// write two 64bit-words for each section
			for (i=0; i<NUMBER_OF_EXESECT; i++) {
				write_fpr (ent->tkt, i, ent->fpr_num,   (fpr_t) {.ui64 = ent->data[i].data0});
				write_fpr (ent->tkt, i, ent->fpr_num+1, (fpr_t) {.ui64 = ent->data[i].data1});
			}
			break;

		default: sim_error_internal(); break;
	}

}

#define case_code(instr_name)  case INSTR_OPCODE_##instr_name:  execute_instr_##instr_name(ent->instr); break;
int cal_pipe_execute_stage (cal_pipe_entry_t *ent) {

	//skip if instruction nulified
	if(ent->nlf)
		goto end;

	switch (ent->stage) {
		case 0: break; // do nothing
		case 1:	{
					// prepare softfloat
					softfloat_prepare ();

					switch(ent->opcode){
							case_code(NOP);
							case_code(CMADD);
							case_code(CMSUB);
							case_code(CMUL);
							case_code(CMULI);
							case_code(CMULNI);
							case_code(CNEG);
							case_code(CCONJ);
							case_code(CADD);
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
							case_code(MVMADD);
							case_code(MVMSUB);
							case_code(MTVMADD);
							case_code(MTVMSUB);
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
					}
					// clear softfloat
					softfloat_clear ();

				}
		case 2:	break; // do nothing
		case 3:	break; // do nothing
		case 4:	break; // do nothing
		case 5:	break; // do nothing
		case 6:	break; // do nothing
		case 7:	break; // do nothing
		case 8:	break; // do nothing
		default: sim_error_internal(); break;
	}

end:

	// switch to next stage
	ent -> stage += 1;

	// return last stage flag
	return (ent->stage == 9);

}
#undef case_code

// execute one stage for one local memory pipe entry
// returns: TRUE if it was the last stage for this entry
// (and the entry must be removed from pipe)
int lmem_pipe_execute_stage (lmem_pipe_entry_t *ent) {

	//skip if instruction nulified
	if(ent->nlf)
		goto end;

	// execute stage
	switch (ent->params->instrtype) {

		// update address register
		case LMEM_INSTRTYPE_UPDADDR:
			switch (ent->stage) {
				case 0: break; // do nothing
				case 1:	lmem_pipe_execute_stage_ADDR(ent); break; // do nothing
				case 2:	lmem_update_addrregs(ent); break;
				case 3:	break; // do nothing
				default:
sim_error_internal(); break;
			}
			break;

		// load instructions
		case LMEM_INSTRTYPE_LOAD:
			switch (ent->stage) {
				case 0: break; // do nothing
				case 1:	lmem_pipe_execute_stage_ADDR(ent); break; // do nothing
				case 2: lmem_pipe_execute_stage_READMEM(ent);
						lmem_update_addrregs(ent);
						break;
				case 3:
						lmem_pipe_execute_stage_WRITEFPR(ent);
						break;
				default:
sim_error_internal(); break;
			}
			break;

		// store instruction
		case LMEM_INSTRTYPE_STORE:
			switch (ent->stage) {
				case 0: break; // do nothing
				case 1: lmem_pipe_execute_stage_ADDR(ent);
						lmem_pipe_execute_stage_READFPR(ent);
						break;
				case 2:
						lmem_update_addrregs(ent);
						lmem_pipe_execute_stage_WRITEMEM(ent);
						break;
				case 3:	break; // do nothing
				default:
sim_error_internal(); break;
			}
			break;
		// other instructions
		case LMEM_INSTRTYPE_OTHER:
#define case_code(instr_name)  case INSTR_OPCODE2_##instr_name:  execute_instr_##instr_name(ent->instr); break;
			switch (ent->stage) {
				case 0: break; // do nothing
				case 1: switch(instr_opcode2(ent->instr)){
							case_code(NOP);
							case_code(CHECK_DMA);
							case_code(DOI);
							case_code(DO);
							case_code(SETI);
							case_code(RET);
							case_code(JUMP);
							case_code(JUMPI);
							case_code(CALL);
							case_code(CALLI);
							case_code(MOVE);
							case_code(MTFPR);
							case_code(MFFPR);
							case_code(CLR);
							case_code(PSPRMSGN0);
							case_code(PSPRMSGN1);
						}
						break;
				case 2: break; // do nothing
				case 3:	break; // do nothing
			}
			break;
#undef case_code
		default: sim_error_internal(); break;
	}

end:

	// switch to next stage
	ent -> stage += 1;

	// return last stage flag
	return (ent->stage == 4);
}



//
void prockern_haltdump () {

	#define fout (k128cp2_machine -> outstream)
	// dump registers
	if (k128cp2_machine -> opt_haltdump_regs) {
		prockern_dump_fpu     (fout);
		prockern_dump_gpr     (fout);
		prockern_dump_addrreg (fout);
		prockern_dump_ctrlreg (fout);
	}

/*
	// dump local memory
	if (k128cp2_machine -> opt_haltdump_lmem_sect0) prockern_dump_lmem (fout, 0);
	if (k128cp2_machine -> opt_haltdump_lmem_sect1) prockern_dump_lmem (fout, 1);
	if (k128cp2_machine -> opt_haltdump_lmem_sect2) prockern_dump_lmem (fout, 2);
	if (k128cp2_machine -> opt_haltdump_lmem_sect3) prockern_dump_lmem (fout, 3);
*/

  int i;
	uint64_t data;
	uint32_t datahi, datalo;
  bool_t s0 = k128cp2_machine -> opt_haltdump_lmem_sect0;
  bool_t s1 = k128cp2_machine -> opt_haltdump_lmem_sect1;
  bool_t s2 = k128cp2_machine -> opt_haltdump_lmem_sect2;
  bool_t s3 = k128cp2_machine -> opt_haltdump_lmem_sect3;

 	// print local memory of given section
        	if( s0 || s1 || s2 || s3 ) fprintf(fout, "Local memory dump, sections");
  if (s0) fprintf(fout, " 1");  if (s1) fprintf(fout, " 2");  if (s2) fprintf(fout, " 3");  if (s3) fprintf(fout, " 4");
        	if( s0 || s1 || s2 || s3 ) fprintf(fout, " :\n");
	for (i=0; i<LMEM_SIZE/sizeof(uint64_t); i++) {
        if( s0 || s1 || s2 || s3 )    fprintf(fout, "%04d: ", i);
		if (s0)
    {
  		data = rawread_lmem_ui64 (0,i);
	  	datalo = data; datahi = data >> 32;
      fprintf(fout, "%016" PRIx64 " (%10.5f, %-10.5f)|", data, hex2float(datahi), hex2float(datalo));
    };
    if (s1)
    {
  		data = rawread_lmem_ui64 (1,i);
  		datalo = data; datahi = data >> 32;
      fprintf(fout, "%016" PRIx64 " (%10.5f, %-10.5f)|", data, hex2float(datahi), hex2float(datalo));
    };
		if (s2)
    {
  		data = rawread_lmem_ui64 (2,i);
		  datalo = data; datahi = data >> 32;
      fprintf(fout, "%016" PRIx64 " (%10.5f, %-10.5f)|", data, hex2float(datahi), hex2float(datalo));
    };

		if (s3)
    {
  		data = rawread_lmem_ui64 (3,i);
	  	datalo = data; datahi = data >> 32;
      fprintf(fout, "%016" PRIx64 " (%10.5f, %-10.5f)|", data, hex2float(datahi), hex2float(datalo));
    };
    if( s0 || s1 || s2 || s3 ) fprintf(fout, "\n");
	}
	fflush(fout);


	#undef fout
}



// init k64fifo support
void k64fifo_init () {

	kern.k64fifo_start = 0;
	kern.k64fifo_size  = 0;
}

//
bool_t prockern_k64fifo_full () {
	return (kern.k64fifo_size >= K64FIFO_MAXSIZE);
}


// push one more entry into k64fifo
void prockern_k64fifo_push (uint64_t data, int ldc2flag) {

	int index;
	event_t ev;

	// fifo full?
	if (prockern_k64fifo_full ()) {
// alex
#if 0
		sim_error("trying to write into full k64fifo");
#else
		sim_printf("Error: trying to write into full k64fifo");
#endif
		return;
	}

	// write data
	index = (kern.k64fifo_start+kern.k64fifo_size) % K64FIFO_MAXSIZE;
	kern.k64fifo[index].data     = data;
	kern.k64fifo[index].ldc2flag = ldc2flag;
	kern.k64fifo_size += 1;

	// send event
	ev.src      = EVENT_SRC_FIFO;
	ev.type     = EVENT_TYPE_WRITE,
	ev.ldc2flag = ldc2flag;
	ev.data     = data;
	ev.sect     = 0;
	ev.addr     = 0;
	ev.mask     = 0;
	ev.clck     = 0;
	ev.mod      = 1;
	observer_newevent (&ev);
}


// pop one entry from k64fifo
void prockern_k64fifo_pop (k64fifo_t *data) {

	// fifo empty?
	if (kern.k64fifo_size <= 0) {
		sim_error("trying to read empty k64fifo");
		return;
	}

	// read data
	*data = kern.k64fifo[kern.k64fifo_start];
	kern.k64fifo_start = (kern.k64fifo_start + 1) % K64FIFO_MAXSIZE;
	kern.k64fifo_size -= 1;
}

//
void get_next_instr_from_k64fifo (instr_t *instr, int *ldc2flag) {

	k64fifo_t fifoitem;

	// read 64bit dword from iram
	prockern_k64fifo_pop (&fifoitem);

	// store it into vliw instr
	*instr     = fifoitem.data;
	*ldc2flag  = fifoitem.ldc2flag;
}



// save state common part
void prockern_save_state_common () {

	char filename[100];

	// compose file name
	snprintf (filename, 100, "%s%04d",
		k128cp2_machine -> opt_savestate_fileprefix, k128cp2_machine -> savestate_counter
	);

	// increment counter
	k128cp2_machine -> savestate_counter += 1;

	// optionally, print message
	if (k128cp2_machine -> opt_savestate_message) {
		sim_printf ("saving state to file %s", filename);
	}

	// save state
	prockern_save_state (filename);

}



// save state on executing 'run' instruction
void prockern_save_state_onrun () {

	// check options
	if ((k128cp2_machine -> opt_savestate_enable) &&
		(k128cp2_machine -> opt_savestate_onrun)
	) {
		// save state
		prockern_save_state_common ();
	}

}


// save state on executing 'stop' instruction
void prockern_save_state_onstop () {

	// check options
	if ((k128cp2_machine -> opt_savestate_enable) &&
		(k128cp2_machine -> opt_savestate_onstop)
	) {
		// save state
		prockern_save_state_common ();
	}

}


