#ifndef __prockern_h__
#define __prockern_h__

#include <libxml/xmlwriter.h>
//#include "config.h"
#include "sysinclude.h" // for HOST_BIG_ENDIAN
#include "common.h"
#include "observer.h"
#include "insn_code.h"

// alex add. default value 0
// If != 0, ignore lc/lsp/la/psp value, use only *_cur
#define CORRECT_READ_WRITE 1

#ifndef HOST_BIG_ENDIAN
	#error undefined HOST_BIG_ENDIAN
#endif

#if (HOST_BIG_ENDIAN)
typedef union {
	uint64_t ui64;
	struct {
		uint32_t hi;
		uint32_t lo;
	} ui32;
	struct {
		float hi;
		float lo;
	} f;
} fpr_t;
#else
typedef union {
	uint64_t ui64;
	struct {
		uint32_t lo;
		uint32_t hi;
	} ui32;
	struct {
		float lo;
		float hi;
	} f;
} fpr_t;
#endif
#define FVAL_ZERO ((fpr_t){.ui64 = 0})

// address register (an,nn,mn)
typedef uint16_t addrreg_t;

typedef uint64_t ireg_t;

// gpr register (gn)
typedef uint64_t gpr_t;

// fcsr register (fpu status)
typedef union {
	uint32_t ui32;
	struct {
		uint32_t reserved0 : 2;
		uint32_t flags     : 6;
		uint32_t reserved1 : 4;
		uint32_t cause     : 6;
		uint32_t reserved2 : 14;
	} fields;
	struct {
		uint32_t reserved0 : 2;
		uint32_t flags_I   : 1;
		uint32_t flags_U   : 1;
		uint32_t flags_O   : 1;
		uint32_t flags_Z   : 1;
		uint32_t flags_V   : 1;
		uint32_t flags_E   : 1;
		uint32_t reserved1 : 4;
		uint32_t cause_I   : 1;
		uint32_t cause_U   : 1;
		uint32_t cause_O   : 1;
		uint32_t cause_Z   : 1;
		uint32_t cause_V   : 1;
		uint32_t cause_E   : 1;
		uint32_t reserved2 : 14;
	} bits;
} fcsr_t;


// fccr register (condition codes)
typedef uint32_t fccr_t;

// control register
typedef uint64_t ctrlreg_t;

// cp2 local memory address type
typedef uint32_t cp2addr_t;

// register types
typedef enum {
	REGID_TYPE_FPR,
	REGID_TYPE_FCCR,
	REGID_TYPE_FCSR,
	REGID_TYPE_CTRLREG,
	REGID_TYPE_GPR,
	REGID_TYPE_ADDRAN,
	REGID_TYPE_ADDRNN,
	REGID_TYPE_ADDRMN,
	REGID_TYPE_IREG
} regid_type_t;

// fpr halves
typedef enum {
	REGID_HALF_HI,
	REGID_HALF_LO,
	REGID_HALF_BOTH,
	REGID_HALF_NONE
} regid_half_t;

// register id type
typedef struct {
	sectid_t      section;
	regid_type_t  type;
	unsigned      regno;
	regid_half_t  half; // for fpr pairs
} regid_t;
#define REGID(_sect,_type,_regno,_half) \
	((regid_t) {                        \
		.section = _sect,               \
		.type    = REGID_TYPE_##_type,  \
		.regno   = _regno,              \
		.half    = REGID_HALF_##_half   \
	})
#define REGNO_NONE (-1)

// read/write_ctrlreg interface
// (source id)
typedef enum {
	READWRITE_CTRLREG_SRCID_K64,
	READWRITE_CTRLREG_SRCID_CP2,
	READWRITE_CTRLREG_SRCID_NONE,
} readwrite_ctrlreg_srcid_t;


// register value type
typedef struct {
	union {
		gpr_t      gpr;
		fpr_t      fpr;
		fccr_t     fccr;
		fcsr_t     fcsr;
		addrreg_t  addran;
		addrreg_t  addrnn;
		addrreg_t  addrmn;
		ireg_t     ireg;
		ctrlreg_t  ctrlreg;
		uint64_t   ui64;
	} val;
	uint64_t mask;
	readwrite_ctrlreg_srcid_t srcid;
} regval_t;
#define REGVAL(_type,_data,_mask,_srcid) \
	((regval_t) {                 \
		.val._type = _data,       \
		.mask      = _mask,       \
		.srcid     = READWRITE_CTRLREG_SRCID_##_srcid \
	})

// bypass presence
enum {
	BYPASS_NO  = FALSE,
	BYPASS_YES = TRUE
};



// control register fields
#define REG_CONTROL_RM       bits32(1,0)

// control register RM field values
#define REG_CONTROL_RM_RN 0x00
#define REG_CONTROL_RM_RZ 0x01
#define REG_CONTROL_RM_RP 0x02
#define REG_CONTROL_RM_RM 0x03
#define REG_CONTROL_RS bit32(31)
#define REG_CONTROL_ST bit32(30)

// local memory operations pipe entry type
// ring buffer, index 0 -- oldest operation
// each tick some operation (read memory, write fpr, etc)
// is performed on each entry according to its stage number
// and code
// local memory instruction types
typedef enum {
	LMEM_INSTRTYPE_LOAD,
	LMEM_INSTRTYPE_STORE,
	LMEM_INSTRTYPE_UPDADDR,
	LMEM_INSTRTYPE_OTHER
} lmem_instrtype_t;

// local memory address types
typedef enum {
	LMEM_ADDRTYPE_IMM,
	LMEM_ADDRTYPE_NP,
	LMEM_ADDRTYPE_NM,
	LMEM_ADDRTYPE_OFFS
} lmem_addrtype_t;

// local memory operations data size
typedef enum {
	LMEM_DATASIZE_WORD,
	LMEM_DATASIZE_DWORD,
} lmem_datasize_t;

// local memory instruction info
typedef struct {
	char*              name;      // instruction name
	uint32_t           opcode2;   // instruction opcode (ld,sd,...)
	lmem_instrtype_t   instrtype; // instruction type (load/store/updaddr)
	lmem_addrtype_t    addrtype;  // address type (imm, rn+offs, ...)
	lmem_datasize_t    datasize;  // word/dword
	bool_t             updatean;  // whether to update AN register
} lmem_instr_params_t;

typedef struct {
	int tkt;
	const lmem_instr_params_t  *params; // instruction parameters (pointer to constant table)
	int      stage;      // pipe stage number
	uint32_t   addr;       // address
	int      fpr_num;    // fpr number
	int      rn;         // address register number
	uint32_t   annewval;   // new value of AN register
	uint32_t   offset;   // new value of AN register
	struct {
		uint64_t data0;    // data
		uint64_t data1;    // data (for ldd, sdd)
	} data[NUMBER_OF_EXESECT];
	instr_t instr;
	bool_t nlf;
} lmem_pipe_entry_t;

typedef struct {
	int tkt;
	int      stage;      // pipe stage number
	instr_t  instr;
	int      opcode;
	bool_t nlf;
} cal_pipe_entry_t;

// execution section definition
typedef struct {

	// floating registers
	fpr_t fpr[FPR_SIZE];

	// fcsr register
	fcsr_t fcsr;

	// fccr register
	fccr_t fccr;

	// local memory
	uint32_t lmem[LMEM_SIZE/sizeof(uint32_t)];
}
ExeSect_T;

// k64 fifo
typedef struct {
	uint64_t data;
	int      ldc2flag;
} k64fifo_t;


// PC register type
typedef uint32_t reg_pc_t;

// Status register type
typedef union {
	uint32_t ui32;
	struct {
		uint32_t reserved  : 16;
		uint32_t fpe_I     : 1;
		uint32_t fpe_U     : 1;
		uint32_t fpe_O     : 1;
		uint32_t fpe_Z     : 1;
		uint32_t fpe_V     : 1;
		uint32_t fpe_E     : 1;
		uint32_t reserved2 : 10;
	} bits;
	struct {
		uint32_t run       : 1;
		uint32_t ff        : 1;
		uint32_t fe        : 1;
		uint32_t commcp2   : 1;
		uint32_t commk64   : 1;
		uint32_t reserved0 : 10;
		uint32_t dep       : 1;
		uint32_t fpe       : 6;
		uint32_t reserved1 : 1;
		uint32_t poe       : 1;
		uint32_t pue       : 1;
		uint32_t loe       : 1;
		uint32_t lue       : 1;
		uint32_t reserved2 : 5;
	} fields;
} reg_status_t;
#define REG_STATUS_RUN     ((reg_status_t) { .fields = { .run       = 1    }})
#define REG_STATUS_FF      ((reg_status_t) { .fields = { .ff        = 1    }})
#define REG_STATUS_FE      ((reg_status_t) { .fields = { .fe        = 1    }})
#define REG_STATUS_COMMCP2 ((reg_status_t) { .fields = { .commcp2   = 1    }})
#define REG_STATUS_COMMK64 ((reg_status_t) { .fields = { .commk64   = 1    }})
#define REG_STATUS_DEP     ((reg_status_t) { .fields = { .dep       = 1    }})
#define REG_STATUS_FPE     ((reg_status_t) { .fields = { .fpe       = 0x3f }})
#define REG_STATUS_POE     ((reg_status_t) { .fields = { .poe       = 1    }})
#define REG_STATUS_PUE     ((reg_status_t) { .fields = { .pue       = 1    }})
#define REG_STATUS_LOE     ((reg_status_t) { .fields = { .loe       = 1    }})
#define REG_STATUS_LUE     ((reg_status_t) { .fields = { .lue       = 1    }})

// Rind register type
typedef union {
	uint32_t ui32;
	struct {
		uint32_t addr      : 13;
		uint32_t b13       : 1;
		uint32_t b14       : 1;
		uint32_t b15       : 1;
	} fields;
} reg_rind_t;

// Rstep register type
typedef uint32_t reg_rstep_t;

// Rmask register type
typedef uint32_t reg_rmask_t;

// Status register type
typedef union {
	uint32_t ui32;
	struct {
		uint32_t addr      : 2;
		uint32_t reserved0 : 5;
		uint32_t enables   : 5;
		uint32_t reserved1 : 3;
		uint32_t i         : 1;
		uint32_t reserved2 : 14;
		uint32_t St        : 1;
		uint32_t Rs        : 1;
	} fields;
} reg_control_t;

// LA register type
typedef union {
	struct {
		uint32_t lae       : 13;
		uint32_t reserved0 : 3 ;
		uint32_t las       : 13;
		uint32_t reserved1 : 3 ;
	} fields;
	uint32_t ui32;
} reg_la_t;

// LC register type
typedef uint32_t reg_lc_t;

// =====  Processor kernel definition  =====
typedef struct {

	// clock count
	uint64_t clockcount;

	// k64 clock count
	// updated in LIBPREFIX(clock)
	uint64_t k64clock;

	// Control registers
	reg_pc_t     pc      ;
	uint64_t     comm    ;
	reg_control_t control;
	reg_status_t status  ;
	uint32_t     psp     ;
	reg_lc_t     lc      ;
	reg_la_t     la      ;
	uint32_t     lsp     ;
	reg_rind_t   rind    ;
	reg_rstep_t  rstep   ;
	reg_rmask_t  rmask   ;


	// k64 interface fifo
	// circular buffer
	k64fifo_t k64fifo[K64FIFO_MAXSIZE];
//	uint64_t k64fifo_ldc2bit; // additional bit: fifo written by ldc2/dmtc2
//	uint64_t k64fifo_full;    // fifo full flag: full => flag=1
	int k64fifo_start;  // start index
	int k64fifo_size;   // current number of elements in fifo

	// stop code register
	uint64_t stopcode;

	//loop registers
//	uint32_t la;

	// dma interface registers
	ireg_t ireg[IREG_SIZE];

	// gpr registers
	gpr_t gpr[GPR_SIZE];

	// address registers
	addrreg_t addran[ADDRREG_SIZE]; // address
	addrreg_t addrnn[ADDRREG_SIZE]; // autoincrement (offset)
	addrreg_t addrmn[ADDRREG_SIZE]; // increment mode

	// instruction ram
	uint64_t iram[IRAM_SIZE/sizeof(uint64_t)];

	// execution sections
	ExeSect_T exesect[NUMBER_OF_EXESECT];

	// gmem vector operations length
//	uint32_t gmem_len;

	// 'sync' instruction support
	// sync pending flag
	bool_t sync_pending;
	int sync_pending_counter;

	// 'stop' instruction support
	// stop pending flag
	bool_t stop_pending;
	int stop_pending_counter;

	// delayed register writing queue
	// ring buffer, being shifted each tick
	// index ..._start: entry will be written between current and next ticks,
	// so that the value can be read by next instruction
	// each entry -- array of slots (register id,value)
#define DELAYED_REG_WRITE_QUEUE_SIZE        20
#define DELAYED_REG_WRITE_QUEUE_ENTRY_SIZE  20
	struct {
		struct {
			regid_t  regid;
			regval_t regval;
			uint64_t insn_start_clk; // when the instruction was started (U stage)
			bool_t   bypass;         // whether the result of the instruction can be read via bypass
		} entry[DELAYED_REG_WRITE_QUEUE_ENTRY_SIZE];
		int entry_size; // number of written values in this entry
	} delayed_reg_write_queue[DELAYED_REG_WRITE_QUEUE_SIZE];
	int delayed_reg_write_queue_start;      // start index
	int delayed_reg_write_queue_numentries; // total number of written values in all entries

	// jump support (for 'loop' and 'call')
	int      jump_flag;
	uint32_t jump_newpc;
	uint32_t newpc;
	int      delay_slot_flag;

	// stop support
	int run_flag;

	//loop support
	int loop_jump;
	int loop_last_it;
	int lsp_cur;
	reg_la_t la_cur;
	reg_lc_t lc_cur;
	bool_t nulify;

	//call support
	uint32_t     psp_cur;
	int          call_jump;

	// lstack
	struct {
		reg_la_t la;
		reg_lc_t lc;
	} lstack[LOOP_MAX_DEPTH];

	// pstack: subprogram calls
	// each entry -- return address
	struct {
		reg_pc_t ret_pc;      // pc of the 1st instruction AFTER 'call' instruction
	} pstack[CALL_MAX_DEPTH];

	// local memory operations support
	// ring buffer, index 0 -- oldest operation
	// each tick some operation (read memory, write fpr, etc)
	// is performed on each entry according to its stage number
	// and code
	lmem_pipe_entry_t lmem_pipe[LMEM_PIPE_SIZE];
	int lmem_pipe_start;      // start index
	int lmem_pipe_numentries; // total number of entries

	cal_pipe_entry_t cal_pipe[CAL_PIPE_SIZE];
	int cal_pipe_start;      // start index
	int cal_pipe_numentries; // total number of entries

	// describe registers written on any clock
	#define WRITTEN_REGS_NUM 200
	struct {
		regid_t  regid;
		regval_t regval;
	} written_regs[WRITTEN_REGS_NUM];

	int written_regs_num;


	// vliw instruction fetch from fifo flag
	bool_t instr_from_k64fifo;

	//dma support
	uint32_t start_dma;

}
ProcKern_T;



// =====  Functions  =====

int adjust_addr(int addr,int offs, int mode);

int bit_rev(int num, int order);

uint32_t swap_word  (uint32_t w);
uint64_t swap_dword (uint64_t w);

void execute_instr_vliw (instr_t instr);

//
int  prockern_load_state_from_file   (char *state_filename);
int  prockern_load_state_from_string (char *str);
void prockern_load_state             ();
void xmldoc_load_iram                (uint64_t *ram,  int size, char *filename);
void xmldoc_load_lmem                (uint32_t *lmem, int size, char *filename);
void xmldoc_load_regs                ();

void   prockern_haltdump     ();
void   prockern_k64fifo_push (uint64_t data, int ldc2flag);
bool_t prockern_k64fifo_full ();
bool_t prockern_k64fifo_empty ();

void prockern_call_ret ();

void execute_instr_hi      (instr_t instr);
void execute_instr_lo      (instr_t instr);

//
void     prockern_init               ();
void     prockern_do_one_step        ();
void     get_next_instr_from_k64fifo (instr_t *instr, int *ldc2flag);
instr_t  get_next_instr_iram         ();
void     prockern_delta_stage        ();
void     set_next_pc                 ();
void     execute_k64ldc2             (instr_t instr);

// dump
void  prockern_dump_fpu     (FILE *f);
void  prockern_dump_gpr     (FILE *f);
void  prockern_dump_addrreg (FILE *f);
void  prockern_dump_ctrlreg (FILE *f);
void  prockern_dump_pstack  ();
void  prockern_dump_lstack  ();

// delayed register writing stuff
void delayed_reg_write             (regid_t *regid_ptr, regval_t *regval_ptr, int delay, bool_t bypass);
void delayed_reg_write_init        ();
void delayed_reg_write_float       (int sect, int regid_num, int regid_half, fpr_t regvalue, int delay);
void delayed_reg_write_queue_shift ();

// 'loop', 'jump' & 'call' instructions support
//void     loop_init                    ();
reg_pc_t set_next_pc_process_loop_end ();
void     prockern_loop_new            (uint64_t nloops, uint32_t loopend_offset);
void     prockern_loop_end            ();
void     call_init                    ();
void     prockern_call_new            (uint32_t target_pc);
void     prockern_call_end            ();
void     prockern_jump                (uint32_t newpc);


// local memory instructions support
void  lmem_init               ();
void  cal_init               ();
void  prockern_lmem_newop     (instr_t instr);
void  prockern_cal_newop     (instr_t instr);
void  lmem_pipe_new_entry     (instr_t instr);
void  cal_pipe_new_entry     (instr_t instr);
void  lmem_pipe_shift         ();
void  cal_pipe_shift         ();
int   lmem_pipe_execute_stage (lmem_pipe_entry_t *ent);
int   cal_pipe_execute_stage (cal_pipe_entry_t *ent);

// global memory instructions support
//void  prockern_gmem_init ();
//void  prockern_gmem_newop (gmem_request_t *request_ptr);

// k64fifo support
void k64fifo_init ();

// save state support
void prockern_save_state        (char *state_filename);
void prockern_save_state_onrun  ();
void prockern_save_state_onstop ();
void save_state_iram         (xmlTextWriterPtr xmlwriter);
void save_state_regs         (xmlTextWriterPtr xmlwriter);
void save_state_onereg       (xmlTextWriterPtr xmlwriter, char *regname, uint64_t regval);
void save_state_section      (xmlTextWriterPtr xmlwriter, int sec);
void save_state_section_lmem (xmlTextWriterPtr xmlwriter, int sec);


#endif // __prockern_h__
