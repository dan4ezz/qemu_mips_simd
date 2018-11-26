#ifndef __machine_h__
#define __machine_h__

#include "k128cp2.h"
#include "common.h"
#include "prockern.h"
#include "options.h"

#define machine_halt() k128cp2_machine.halted = TRUE;

// machine access macroses
#define mch  (k128cp2_machine)
#define sc   (k128cp2_machine->sysctrl)
#define kern (k128cp2_machine->prockern)


// Machine
typedef struct {

	// processor kernel
	ProcKern_T prockern;

	// output stream
	FILE *outstream;

	//pointers to dma interface functions
	int  (*dma_ifptr_check_dma) (struct k128dma_ctrl_state *dma_ctx);

	//dma controller context
	struct k128dma_ctrl_state *dma_ctx;

	// options cache
	int   opt_dump_inst             ;
	int   opt_dump_inststop         ;
	int   opt_dump_fpu              ;
	int   opt_dump_fpu_float        ;
	int   opt_dump_gpr              ;
	int   opt_dump_addrreg          ;
	int   opt_dump_ctrlreg          ;
	int   opt_dump_lstack           ;
	int   opt_dump_pstack           ;
	int   opt_dump_event_enable     ;
	int   opt_dump_event_float      ;
	int   opt_dump_event_sect0      ;
	int   opt_dump_event_sect1      ;
	int   opt_dump_event_sect2      ;
	int   opt_dump_event_sect3      ;
	int   opt_dump_event_iram       ;
	int   opt_dump_event_k64        ;
	int   opt_dump_event_fcsr       ;
	int   opt_dump_event_ctrlreg    ;
	int   opt_haltdump_regs         ;
	int   opt_haltdump_lmem_sect0   ;
	int   opt_haltdump_lmem_sect1   ;
	int   opt_haltdump_lmem_sect2   ;
	int   opt_haltdump_lmem_sect3   ;
	int   opt_savestate_enable      ;
	int   opt_savestate_onrun       ;
	int   opt_savestate_onstop      ;
	int   opt_savestate_message     ;
	char *opt_savestate_fileprefix  ;

// savestate counter
	int savestate_counter;

} Machine_T;


// Functions
void print_word64                 (uint64_t data, unsigned output_type);
int  set_output_file              (const char *filename);
int  machine_finalize             ();
void machine_reset                ();

int machine_load_state_from_file   (char *state_filename);
int machine_load_state_from_string (char *str);

#endif // __machine_h__
