/*!
 * \file machine.c
 * \brief Simulator API
 *
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "system.h"
#include "machine.h"
#include "options.h"
#include "instr.h"
#include "insn_code.h"
#include "observer.h"
#include "prockern_access.h"


// include xml files in string format
#include "k128cp2_config.xmlc"
#include "k128cp2_resetstate.xmlc"


// global machine pointer
Machine_T *k128cp2_machine = NULL;

// set/release global pointer to cp2 structure
#define SET_GLOBAL_POINTER     \
	if (cp2ptr != NULL) { \
		k128cp2_machine=cp2ptr; \
	} else { \
		sim_error ("null cp2 pointer"); \
	}
#define RELEASE_GLOBAL_POINTER k128cp2_machine=NULL;


//! Creates CP2 and returns pointer to it
/*!
 * \brief
 * \param  *cp2ptrptr
 * \return
 *
 */
int     libk128cp2_create (void **cp2ptrptr) {

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptrptr=%p)", __FUNCTION__, cp2ptrptr);
	#endif

	*cp2ptrptr = NULL;
	*cp2ptrptr = (void*) malloc(sizeof(Machine_T));
	if (*cp2ptrptr != NULL) {
		return 0;
	} else {
		return 1;
	}
}


// delete struct with CP2 state
/*!
 * \brief
 * \param  *cp2ptr *cp2ptr
 * \return
 *
 */
int     libk128cp2_delete (void *cp2ptr) {

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p)", __FUNCTION__, cp2ptr);
	#endif

	// finalize
	machine_finalize ();

	RELEASE_GLOBAL_POINTER;

	// free
	free(cp2ptr);

	// return
	return 0;
}

void libk128cp2_print  (void *cp2ptr, const char *str)
{
    SET_GLOBAL_POINTER;

    sim_print_string(str);

    RELEASE_GLOBAL_POINTER;
}


/*!
 * \brief
 * \param  *cp2ptr *cp2ptr      Указатель на
 * \return       Ноль в случае успеха, не ноль в противном случае.
 *
 */
int libk128cp2_init (void *cp2ptr) {

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p)", __FUNCTION__, cp2ptr);
	#endif

	// flush clockcount
	reg_clockcount = 1;

	// flush save state counter
	mch -> savestate_counter = 0;

	// init observer
	observer_init ();

	// init kernel
	prockern_init ();

	// creates default output stream
	mch -> outstream = stdout;

	// load default options
	load_options_from_string (k128cp2_config_xmlfile_string);

	// reset cp2
	machine_reset ();

	RELEASE_GLOBAL_POINTER;

	// return
	return 0;
}

// reset cp2
// (load state from resetstate)
/*!
 * \brief
 * \param  *cp2ptr
 *
 */
void libk128cp2_reset (void *cp2ptr) {

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p)", __FUNCTION__, cp2ptr);
	#endif

	// reset machine
	machine_reset ();
	kern.control.ui32 = 0;

	RELEASE_GLOBAL_POINTER;
}


//
/*!
 * \brief
 * \param  *cp2ptr
 * \param  *state_filename
 *
 */
void libk128cp2_load_state (void *cp2ptr, char *state_filename) {

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p, state_filename=%s)", __FUNCTION__, cp2ptr, state_filename);
	#endif

	// load state from file
	machine_load_state_from_file (state_filename);

	RELEASE_GLOBAL_POINTER;
}


//
/*!
 * \brief
 * \param  *cp2ptr
 * \param  *state_filename
 *
 */
void libk128cp2_save_state (void *cp2ptr, char *state_filename) {

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p, state_filename=%s)", __FUNCTION__, cp2ptr, state_filename);
	#endif

	// save kernel state
	prockern_save_state (state_filename);

	RELEASE_GLOBAL_POINTER;
}


//
/*!
 * \brief
 * \param  *cp2ptr
 * \param  *config_filename
 *
 */
void libk128cp2_load_options (void *cp2ptr, char *config_filename) {

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p, config_filename=%s)", __FUNCTION__, cp2ptr, config_filename);
	#endif

	// load options
	load_options_from_xmlfile (config_filename);

	RELEASE_GLOBAL_POINTER;
}



//
/*!
 * \brief
 * \param  *cp2ptr
 * \param  k64clock
 *
 */
void libk128cp2_clock (void *cp2ptr, uint64_t k64clock) {

	SET_GLOBAL_POINTER;

	// debug message
	#if ((LIBK128CP2_DEBUG_PRINT_API_CALLS > 0) && (LIBK128CP2_DEBUG_PRINT_API_CALLS_CLOCK > 0))
	sim_printf ("%s (cp2ptr=%p, k64clock=%lld)", __FUNCTION__, cp2ptr, (uint64_t) k64clock);
	#endif

	// notify observer
	observer_step_start (reg_clockcount);

	// kernel step
	prockern_do_one_step (k64clock);

	// notify observer
	observer_step_finish (reg_clockcount);

	// increment clock count
	reg_clockcount += 1;

	RELEASE_GLOBAL_POINTER;
}



//
/*!
 * \brief
 * \param  *cp2ptr
 *
 */
/*
void libk128cp2_haltdump (void *cp2ptr) {

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p)", __FUNCTION__, cp2ptr);
	#endif

	// halt dump prockern
	prockern_haltdump ();

	RELEASE_GLOBAL_POINTER;
}
*/

// запись в регистр cp2 номер regno значения value с маской mask
// ldc2flag -- источник записи:
//   0 -- dmtc2
//   1 -- ldc2
/*!
 * \brief
 * \param  *cp2ptr
 * \param  regno
 * \param  value
 * \param  mask
 * \param  ldc2flag
 *
 */
void libk128cp2_reg_write (void *cp2ptr, int regno, uint64_t value, uint64_t mask, int ldc2flag) {

	event_t ev;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p, regno=%d, value=%016llx, mask=%016llx, ldc2flag=%d)",
		__FUNCTION__, cp2ptr, regno, value, mask, ldc2flag);
	#endif

	// write
#define case_regwrite(name) \
	case K128CP2_REG_##name : write_ctrlreg (reg_clockcount, CTRLREG_##name, value, mask, K64); break;
	switch(regno) {
//		case_regwrite (CLOCKCOUNT );
		case_regwrite (COMM       );
		case_regwrite (CONTROL    );
		case_regwrite (STATUS     );
		case K128CP2_REG_FIFO :  prockern_k64fifo_push(value, ldc2flag); break;
		default: sim_error("writing invalid register: regno=%d",regno); break;
	}
#undef case_regwrite

	// send event
    ev.src      = EVENT_SRC_K64;
    ev.type     = EVENT_TYPE_WRITE,
    ev.addr     = regno;
    ev.data     = value;
    ev.mask     = mask;
    ev.ldc2flag = ldc2flag;
    ev.clck     = 0; // fake
    ev.sect     = 0; // fake
    observer_newevent (&ev);

	RELEASE_GLOBAL_POINTER;
}


//
/*!
 * \brief
 * \param  *cp2ptr
 * \param  regno
 * \return
 *
 */
uint64_t libk128cp2_reg_read (void *cp2ptr, int regno) {

	event_t ev;
	uint64_t retval;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
		sim_printf ("%s (cp2ptr=%p, regno=%d)",
			__FUNCTION__, cp2ptr, regno);
	#endif

	retval = 0;

	// read
#define case_regread(name) \
	case K128CP2_REG_##name: retval = read_ctrlreg(reg_clockcount,CTRLREG_##name, K64); break;
	switch(regno) {
		case_regread (CLOCKCOUNT );
		case_regread (COMM       );
		case_regread (CONTROL    );
		case_regread (STATUS     );
		case_regread (STOPCODE   );
		default: sim_error("reading invalid register: regno=%d",regno); break;
	}
#undef case_regread

	// send event
    ev.src      = EVENT_SRC_K64;
    ev.type     = EVENT_TYPE_READ,
    ev.addr     = regno;
    ev.data     = retval;
    observer_newevent (&ev);

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
		sim_printf ("  val=0x%016llx", retval);
	#endif

	RELEASE_GLOBAL_POINTER;

	return retval;
}


// Print ui64 in specified format
// output_format values:
//   0 = 0xXX..X
//   1 = (float, float)
//   2 = 0xXX..X (float, float)
void print_word64 (uint64_t data, unsigned output_format)
{
	uint32_t datahi, datalo;

	datalo = data;
	datahi = data >> 32;

	switch(output_format)
	{
		case 0:
			fprintf(stderr, "%016" PRIx64, data);
		break;
		case 1:
			fprintf(stderr, "(%10.5f, %-10.5f)", hex2float(datahi), hex2float(datalo));
		break;
		case 2:
			fprintf(stderr, "%016" PRIx64 " (%10.5f, %-10.5f)", data, hex2float(datahi), hex2float(datalo));
		break;
	}
}

// Dump lmem
/*!
 * \brief
 * \param  *cp2ptr
 * \param  output_format
 *
 */
void libk128cp2_dump_lmem  (void *cp2ptr, unsigned output_format)
{
	int i;
	uint64_t data;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
		sim_printf ("%s (cp2ptr=%p, output_format=%d)",
			__FUNCTION__, cp2ptr, output_format);
	#endif

 	// print local memory of given section
	fprintf(stderr, "Local memory dump, sections:\n");
	for (i=0; i<LMEM_SIZE/sizeof(uint64_t); i++)
	{
		data = rawread_lmem_ui64 (0, i);
		fprintf(stderr, "%04d: ", i);	print_word64(data, output_format); fprintf(stderr, " | ");

		data = rawread_lmem_ui64 (1, i);
		fprintf(stderr, "%04d: ", i); print_word64(data, output_format); fprintf(stderr, " | ");

		data = rawread_lmem_ui64 (2, i);
		fprintf(stderr, "%04d: ", i);	print_word64(data, output_format); fprintf(stderr, " | ");

		data = rawread_lmem_ui64 (3, i);
		fprintf(stderr, "%04d: ", i); print_word64(data, output_format); fprintf(stderr, "\n");
	}

	fflush(stderr);

	RELEASE_GLOBAL_POINTER;
}


//! Read from cp2 local memory. cp2addr and data_size are given in 64bit words.
/*!
 * \brief
 * \param  *cp2ptr
 * \param  *dataptr
 * \param  cp2addr
 * \param  sec
 * \param  data_size
 * \return
 *
 */
int  libk128cp2_lmem_read (void *cp2ptr, uint64_t *dataptr, k128cp2_cp2addr_t cp2addr, int sec, uint64_t data_size) {

	int rv;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
		sim_printf ("%s (cp2ptr=%p, dataptr=%p, cp2addr=0x%08x, sec=%d, data_size=%lld)",
			__FUNCTION__, cp2ptr, dataptr, (int) cp2addr, sec, data_size);
	#endif

	// set initial values
	rv = 0;

	// write lmem
	memcpy ( dataptr, &((kern.exesect[sec].lmem)[cp2addr*2]), data_size*sizeof(uint64_t));

	// debug message
	// print data
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
		for (i=0; i<data_size; i++) {
			sim_printf ("  dataptr[%d]=0x%016llx", i, dataptr[i]);
		}
	#endif

	RELEASE_GLOBAL_POINTER;

	return rv;
}


/*!
 * \brief Write to cp2 local memory. cp2addr and data_size are given in 64bit words.
 * \param  *cp2ptr
 * \param  *dataptr
 * \param  cp2addr
 * \param  sec
 * \param  data_size
 * \return
 *
 */
int  libk128cp2_lmem_write (void *cp2ptr, uint64_t *dataptr, k128cp2_cp2addr_t cp2addr, int sec, uint64_t data_size) {

	int rv;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
		sim_printf ("%s (cp2ptr=%p, dataptr=%p, cp2addr=0x%08x, sec=%d, data_size=%lld)",
			__FUNCTION__, cp2ptr, dataptr, (int) cp2addr, sec, data_size);
	#endif

	// set initial values
	rv = 0;

	// write lmem
	memcpy ( &((kern.exesect[sec].lmem)[cp2addr*2]), dataptr, data_size*sizeof(uint64_t));

	// debug message
	// print data
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
		for (i=0; i<data_size; i++) {
			sim_printf ("  dataptr[%d]=0x%016llx", i, dataptr[i]);
		}
	#endif

	RELEASE_GLOBAL_POINTER;

	return rv;
}

//
/*!
 * \brief
 * \param  *cp2ptr
 * \param  *dataptr
 * \param  cp2addr
 * \param  sec
 * \return
 *
 */
int libk128cp2_read64  (void *cp2ptr, uint64_t *dataptr, k128cp2_cp2addr_t cp2addr, int sec)
{
	int rv;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
		sim_printf ("%s (cp2ptr=%p, dataptr=%p, cp2addr=0x%08x, sec=%d)",
			__FUNCTION__, cp2ptr, dataptr, (int) cp2addr, sec);
	#endif

	rv = 0;

	dataptr[0] = rawread_lmem_ui64 (sec, cp2addr);

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
		sim_printf ("  dataptr[0]=0x%016llx",
			dataptr[0]
		);
	#endif

	RELEASE_GLOBAL_POINTER;

	return rv;
}

//
/*!
 * \brief
 * \param  *cp2ptr
 * \param  *dataptr
 * \param  cp2addr
 * \param  sec
 * \return
 *
 */
int libk128cp2_write64  (void *cp2ptr, uint64_t *dataptr, k128cp2_cp2addr_t cp2addr, int sec)
{
	int rv;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p, dataptr=%p, cp2addr=0x%08x, sec=%d)",
		__FUNCTION__, cp2ptr, dataptr, (int) cp2addr, sec);
	#endif

	rv = 0;

	rawwrite_lmem_ui64 (sec, cp2addr  , dataptr[0]);

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
		sim_printf ("  dataptr[0]=0x%016llx",
			dataptr[0]
		);
	#endif

	RELEASE_GLOBAL_POINTER;

	return rv;
}

int libk128cp2_pending_work  (void *cp2ptr) {
    int retval;

    SET_GLOBAL_POINTER;

    /* Check if there is pending instructions */
    retval = kern.sync_pending || kern.stop_pending;

    if (!kern.run_flag) {
        /* Check if the fifo is non-empty */
        retval |= !prockern_k64fifo_empty();
    }

    RELEASE_GLOBAL_POINTER;

    return retval;
}

//
/*!
 * \brief
 * \param  *cp2ptr
 * \param  cc
 * \return
 *
 */
int libk128cp2_condcode  (void *cp2ptr, int cc) {

	int retval;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
		sim_printf ("%s (cp2ptr=%p, cc=%d)",
			__FUNCTION__, cp2ptr, cc);
	#endif

	// consistency check
	// cc=0 supported only
	if (cc != 0) {
		sim_error("invalid condition code: cc=%d", cc);
	}

	if (prockern_k64fifo_full()) {
		retval = 1;
	} else {
		retval = 0;
	}

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
		sim_printf ("  val=%d", retval);
	#endif

	RELEASE_GLOBAL_POINTER;

	return retval;
}

// Read iram starting from addr.
/*!
 * \brief
 * \param  *cp2ptr
 * \param  *prg_ptr
 * \param  prg_len
 * \param  addr
 * \return
 *
 */
int  libk128cp2_iram_read (void *cp2ptr, uint64_t *prg_ptr, uint64_t prg_len, uint64_t addr) {

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p, prg_ptr=%p, prg_len=%lld)",
		__FUNCTION__, cp2ptr, prg_ptr, prg_len);
	#endif

	rawread_iram (prg_ptr, prg_len, addr);

	RELEASE_GLOBAL_POINTER;

	// return
	return 0;
}

// Write iram starting from current pc. Then shift pc to the end of program.
// This is used with target linux and simulates loop with ldc2 instruction.
/*!
 * \brief
 * \param  *cp2ptr
 * \param  *prg_ptr
 * \param  prg_len
 * \return
 *
 */
int  libk128cp2_iram_write (void *cp2ptr, uint64_t *prg_ptr, uint64_t prg_len) {

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p, prg_ptr=%p, prg_len=%lld)",
		__FUNCTION__, cp2ptr, prg_ptr, prg_len);
	#endif

	rawwrite_iram (prg_ptr, prg_len);

	RELEASE_GLOBAL_POINTER;

	// return
	return 0;
}


//! Set output stream
/*!
 * \brief
 * \param  *cp2ptr
 * \param  *filename
 * \return
 *
 */
int libk128cp2_set_output_file (void *cp2ptr, const char *filename) {

	int rv;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p, filename=%s)",
		__FUNCTION__, cp2ptr, filename);
	#endif

	// call further
	rv = set_output_file (filename);

	RELEASE_GLOBAL_POINTER;

	// return
	return rv;
}

//
void libk128cp2_set_dma_context (void *cp2ptr,
	int  (*fptr) (struct k128dma_ctrl_state *),
	struct k128dma_ctrl_state *dma_ctx) {

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p, fptr=%p)",
		__FUNCTION__, cp2ptr, fptr);
	#endif

	// set function ptr
	mch->dma_ifptr_check_dma = fptr;
	mch->dma_ctx = dma_ctx;

	RELEASE_GLOBAL_POINTER;
}

//
int libk128cp2_start_dma_read (void *cp2ptr) {

	int start_dma;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p, fptr=%p)",
		__FUNCTION__, cp2ptr, fptr);
	#endif

	// read & consume start_dma bit
	start_dma = start_dma_read();

	RELEASE_GLOBAL_POINTER;

	return start_dma;
}


uint64_t libk128cp2_ireg_read (void *cp2ptr, int regno) {

	uint64_t ret_val;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p)",
		__FUNCTION__, cp2ptr);
	#endif

	ret_val = rawread_ir (regno);

	RELEASE_GLOBAL_POINTER;

	return ret_val;

}

//
uint64_t k128cp2_debug_ctrlreg_read (void *cp2ptr, int regno) {

	uint64_t ret_val;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p)",
		__FUNCTION__, cp2ptr);
	#endif

	ret_val = 0;

	switch (regno) {
		case CTRLREG_PC        : ret_val = rawread_ctrlreg (CTRLREG_PC); break;
		case CTRLREG_COMM      : ret_val = rawread_ctrlreg (CTRLREG_COMM      ); break;
		case CTRLREG_CONTROL   : ret_val = rawread_ctrlreg (CTRLREG_CONTROL   ); break;
		case CTRLREG_STATUS    : ret_val = rawread_ctrlreg (CTRLREG_STATUS    ); break;
		case CTRLREG_PSP       : ret_val = rawread_ctrlreg (CTRLREG_PSP       ); break;
		case CTRLREG_LC        : ret_val = rawread_ctrlreg (CTRLREG_LC        ); break;
		case CTRLREG_LA        : ret_val = rawread_ctrlreg (CTRLREG_LA        ); break;
		case CTRLREG_LSP       : ret_val = rawread_ctrlreg (CTRLREG_LSP       ); break;
		case CTRLREG_RIND      : ret_val = rawread_ctrlreg (CTRLREG_RIND      ); break;
		case CTRLREG_RSTEP     : ret_val = rawread_ctrlreg (CTRLREG_RSTEP     ); break;
		case CTRLREG_RMASK     : ret_val = rawread_ctrlreg (CTRLREG_RMASK     ); break;
		case CTRLREG_CLOCKCOUNT: ret_val = rawread_ctrlreg (CTRLREG_CLOCKCOUNT); break;
		case CTRLREG_STOPCODE  : ret_val = rawread_ctrlreg (CTRLREG_STOPCODE  ); break;
		default: sim_error ("invalid register: regno=%d",regno); break;
	}

	RELEASE_GLOBAL_POINTER;

	return ret_val;

}

//
uint64_t k128cp2_debug_fpu_ctrlreg_read (void *cp2ptr, int regno, int secno){

	uint32_t ret_val;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p)",
		__FUNCTION__, cp2ptr);
	#endif

	ret_val = 0;

	switch (regno) {
		case K128CP2_FPU_CTRL_FCCR:  ret_val = reg_fccr (secno);         break;
		case K128CP2_FPU_CTRL_FCSR:  ret_val = reg_fcsr (secno).ui32;    break;
		default: sim_error ("invalid register: regno=%d",regno); break;
	}

	RELEASE_GLOBAL_POINTER;

	return ret_val;

}


//
uint64_t k128cp2_debug_greg_read (void *cp2ptr, int regno){

	uint64_t ret_val;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p)",
		__FUNCTION__, cp2ptr);
	#endif

	ret_val = 0;

	ret_val = rawread_gpr (regno);

	RELEASE_GLOBAL_POINTER;

	return ret_val;

}

//
uint64_t k128cp2_debug_freg_read (void *cp2ptr, int regno, int secno){

	uint64_t ret_val;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p)",
		__FUNCTION__, cp2ptr);
	#endif

	ret_val = 0;

	ret_val = rawread_fpr (secno, regno).ui64;

	RELEASE_GLOBAL_POINTER;

	return ret_val;

}

//
uint64_t k128cp2_debug_lstack_read (void *cp2ptr, int num, int lalc){

	uint64_t ret_val;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p)",
		__FUNCTION__, cp2ptr);
	#endif

	ret_val = 0;

	switch(lalc) {
		case K128CP2_TYPE_LA : ret_val = kern.lstack[num].la.ui32; break;
		case K128CP2_TYPE_LC : ret_val = kern.lstack[num].lc; break;
	}

	RELEASE_GLOBAL_POINTER;

	return ret_val;

}

//
uint64_t k128cp2_debug_pstack_read (void *cp2ptr, int num){

	uint64_t ret_val;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p)",
		__FUNCTION__, cp2ptr);
	#endif

	ret_val = 0;

	ret_val = kern.pstack[num].ret_pc;

	RELEASE_GLOBAL_POINTER;

	return ret_val;

}

//
uint64_t k128cp2_debug_areg_read (void *cp2ptr, int regno, int type){

	uint64_t ret_val;

	SET_GLOBAL_POINTER;

	// debug message
	#if LIBK128CP2_DEBUG_PRINT_API_CALLS > 0
	sim_printf ("%s (cp2ptr=%p)",
		__FUNCTION__, cp2ptr);
	#endif

	ret_val = 0;

	switch(type) {
		case K128CP2_TYPE_ADDRAN :   ret_val = rawread_addran (regno);  break;
		case K128CP2_TYPE_ADDRNN :   ret_val = rawread_addrnn (regno);  break;
		case K128CP2_TYPE_ADDRMN :   ret_val = rawread_addrmn (regno);  break;
	}

	RELEASE_GLOBAL_POINTER;

	return ret_val;

}

//! Direct output stream to a given file.
// If filename is NULL, then use stdout.
int set_output_file (const char *filename) {

	FILE *f;

	// close previous file if any
	if (mch -> outstream != stdout) {
		fclose (mch -> outstream);
	}

	// check for NULL filename
	if (filename == NULL) {
		// use stdout
		f = stdout;
	} else {
		// open file
		f = fopen (filename, "w");
		if (!f) {
			sim_error ("cannot open file %s: %s\n", filename, strerror(errno));
			return 1;
		}
	}

	// set output stream
	mch -> outstream = f;

	// return
	return 0;
}


//! Stop working with simulator
int machine_finalize () {

	// close output file if any
	if (mch -> outstream != stdout) {
		fclose (mch -> outstream);
	}

	// return
	return 0;
}


//! Reset cp2 (load reset state)
void machine_reset () {

	// load reset state
	machine_load_state_from_string (k128cp2_resetstate_xmlfile_string);
}


//! Load cp2 state from xml file.
int machine_load_state_from_file (char *state_filename)
{
	// load kernel state
	prockern_load_state_from_file (state_filename);

	// return
	return 0;
}

//! Load cp2 state from string.
int machine_load_state_from_string (char *str)
{
	// load kernel state
	prockern_load_state_from_string (str);

	// return
	return 0;
}


