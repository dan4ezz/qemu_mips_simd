#ifndef __common_h__
#define __common_h__

#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include "inttypes.h"
#include "softfloat.h"

typedef int bool_t;

// ==========  Debug parameters  ==========

// Print messages on api function calls
// Values:
//   0 = off;
//   1 = on.
#ifndef LIBK128CP2_DEBUG_PRINT_API_CALLS
#define LIBK128CP2_DEBUG_PRINT_API_CALLS 0
#endif


// Print message on calling libk128cp2_clock() api function.
// Disabled by default to avoid log flooding.
// Works together with LIBK128CP2_DEBUG_PRINT_API_CALLS macros.
// Values:
//   0 = off;
//   1 = on.
#ifndef LIBK128CP2_DEBUG_PRINT_API_CALLS_CLOCK
#define LIBK128CP2_DEBUG_PRINT_API_CALLS_CLOCK 0
#endif




// ==========  CP2 parameters  ==========

#define NUMBER_OF_EXESECT 4
#define FPR_SIZE      64
#define GPR_SIZE      16
#define ADDRREG_SIZE  16
#define IREG_SIZE     16
#define IRAM_SIZE     (64 * 1024) /* in 8bit bytes */
#define LMEM_SIZE     (64 * 1024) /* in 8bit bytes */

// bits in address registers
#define ADDRREG_BITS 13

// instruction word size
// used in pc=pc+INSTR_SIZE
// =1 because PC counts 64bit dwords
//#define INSTR_SIZE (sizeof(vliwinstr_t))
#define INSTR_SIZE 1

// instruction delays
// 0 means its result can be read on next tick
#define INSTR_DELAY_FPU     7
#define INSTR_DELAY_FCSR    6
#define INSTR_DELAY_MOVE    1
#define INSTR_DELAY_MOVE_CTRL 0
#define INSTR_DELAY_MTFPR   2
#define INSTR_DELAY_MFFPR   1
#define INSTR_DELAY_SETI    1
#define INSTR_DELAY_ADDRREG 1
#define INSTR_DELAY_MFC     7
#define INSTR_DELAY_MTC     6
#define INSTR_DELAY_COND    6
#define INSTR_DELAY_RUNSTOP 2
#define INSTR_DELAY_CHECK_DMA 1
#define INSTR_DELAY_PSPRMSGN 2

// maximum loop depth
#define LOOP_MAX_DEPTH 16

// maximum call depth
#define CALL_MAX_DEPTH 16

// local memory pipe size
#define LMEM_PIPE_SIZE 4
#define CAL_PIPE_SIZE 9

// k64 interface fifo maximum size
#define K64FIFO_MAXSIZE 4


// ==========  Types  ==========
typedef union {
	uint32_t ui32;
	float    f;
} ui32f_t;

// ==========  Auxiliary macroses  ==========
#define TRUE  (0==0)
#define FALSE (0!=0)
#define bits32(up,lo)  ( (((uint32_t) 0xffffffffUL)          >> (31-((up)-(lo))) ) << (lo) )
#define bits64(up,lo)  ( (((uint64_t) 0xffffffffffffffffULL) >> (63-((up)-(lo))) ) << (lo) )
#define bit32(b)     (((uint32_t) 1UL)  << (b))
#define bit64(b)     (((uint64_t) 1ULL) << (b))
// sign_ext32: записать в биты, старше n, значение бита n (расширение знака)
#define sign_ext32(a,n) (((((uint32_t)(a))&bit32(n)) == 0) ? ((a)&~bits32(31,(n)+1)) : ((a)|bits32(31,(n)+1)))
//#define sign_ext32(a,n) (((((uint32_t)(a))&bit32(n)) == 0) ? (a) : ((a)|bits32(31,(n)+1)))
#define sign_ext64(a,n) (((((uint64_t)(a))&bit64(n)) == 0) ? (a) : ((a)|bits64(63,(n)+1)))

#define NOMASK64     ((uint64_t) 0xffffffffffffffffULL)
#define ADDRREGMASK  ((uint64_t) 0x0000000000001fffULL)
#define RSTEPMASK    ((uint64_t) 0x000000000000ffffULL)
#define RINDMASK     ((uint64_t) 0x000000000000ffffULL)
#define RMASKMASK     ((uint64_t) 0x000000000000ffffULL)
#define FCCRMASK     ((uint64_t) 0x00000000000000ffULL)
#define FCSRMASK     ((uint64_t) 0x000000000003f0fcULL)

// convert ui64 to float type
//#define hex2float(a) (*((float*)(&(a))))
#define hex2float(_a) ((ui32f_t){.ui32=(_a)}).f

// checkpoint
#define checkpoint() fprintf (stderr,"\t*** libk128cp2: checkpoint %s:%d ",__FILE__,__LINE__);

// simulator internal error
#define sim_error_internal() sim_error ("internal error at %s:%d", __FILE__,__LINE__); exit(1);

// ========== Float operations (links to softfloat)  ==========
extern float_status f_status;

#define float_add(a,b) float32_add(a,b,&f_status)
#define float_mul(a,b) float32_mul(a,b,&f_status)
#define float_sub(a,b) float32_sub(a,b,&f_status)
#define float_div(a,b) float32_div(a,b,&f_status)
#define float_sqrt(a)   float32_sqrt(a,&f_status)
#define float_neg(a)    ((a)^((uint32_t)0x80000000))
#define float_abs(a)    ((a)&((uint32_t)0x7FFFFFFF))


#endif // __common_h__
