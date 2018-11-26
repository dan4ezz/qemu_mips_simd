/*!
 * \file k128cp2.h
 * \brief Simulator API
 *
 * Заголовки интерфейсных функций.
 */

#ifndef __k128cp2_h__
#define __k128cp2_h__

#include "sys/cdefs.h"
#include "inttypes.h"

//! Type for address in CP2 local memory
typedef uint16_t k128cp2_cp2addr_t;

//! Dma context incomplete type def
struct k128dma_ctrl_state;

#ifdef __cplusplus
extern "C" {
#endif
//__BEGIN_DECLS


// ==========  Start of API Functions Declarations  ==========

// Управляющие функции.
int  libk128cp2_create          (void **cp2ptr);
int  libk128cp2_delete          (void *cp2ptr);

int  libk128cp2_init            (void *cp2ptr);
int  libk128cp2_set_output_file (void *cp2ptr, const char *filename);

void libk128cp2_load_state      (void *cp2ptr, char *state_filename);
void libk128cp2_save_state      (void *cp2ptr, char *state_filename);
void libk128cp2_load_options    (void *cp2ptr, char *config_filename);

void libk128cp2_load_option (void *cp2ptr, const char *opt_name, const char *newval);

//void libk128cp2_haltdump    (void *cp2ptr);

void libk128cp2_dump_lmem       (void *cp2ptr, unsigned output_type);

int  libk128cp2_iram_read       (void *cp2ptr, uint64_t *prg_ptr, uint64_t prg_len, uint64_t addr);

uint64_t k128cp2_debug_ctrlreg_read (void *cp2ptr, int regno);
uint64_t k128cp2_debug_gpr_read (void *cp2ptr, int regno);
uint64_t k128cp2_debug_areg_read (void *cp2ptr, int regno, int type);
uint64_t k128cp2_debug_lstack_read (void *cp2ptr, int num, int lalc);
uint64_t k128cp2_debug_pstack_read (void *cp2ptr, int num);
uint64_t k128cp2_debug_fpr_read (void *cp2ptr, int regno, int secno);
uint64_t k128cp2_debug_fpu_ctrlreg_read (void *cp2ptr, int regno, int secno);


// Функции, соответствующие интерфейсу самого симулируемого CP2.
void     libk128cp2_reset      (void *cp2ptr);

void     libk128cp2_clock      (void *cp2ptr, uint64_t k64clock);

uint64_t libk128cp2_reg_read   (void *cp2ptr, int regno);
void     libk128cp2_reg_write  (void *cp2ptr, int regno, uint64_t value, uint64_t mask, int ldc2flag);

int      libk128cp2_iram_write (void *cp2ptr, uint64_t *prg_ptr, uint64_t prg_len);
int      libk128cp2_lmem_read  (void *cp2ptr, uint64_t *data, k128cp2_cp2addr_t cp2addr, int sec, uint64_t data_size);
int      libk128cp2_lmem_write (void *cp2ptr, uint64_t *data, k128cp2_cp2addr_t cp2addr, int sec, uint64_t data_size);
int      libk128cp2_read64    (void *cp2ptr, uint64_t *data, k128cp2_cp2addr_t cp2dataptr, int sec);
int      libk128cp2_write64   (void *cp2ptr, uint64_t *data, k128cp2_cp2addr_t cp2dataptr, int sec);

int      libk128cp2_condcode   (void *cp2ptr, int cc);

void     libk128cp2_set_dma_context (void *cp2ptr, int  (*fptr) (struct k128dma_ctrl_state *), struct k128dma_ctrl_state *dma_ctx);
int      libk128cp2_start_dma_read (void *cp2ptr);
uint64_t libk128cp2_ireg_read (void *cp2ptr, int regno);

/* Required for QEMU */
void     libk128cp2_print(void *cp2ptr, const char *str);
int      libk128cp2_pending_work(void *cp2ptr);

// ==========  End of API Functions Declarations  ==========

//__END_DECLS
#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif

// register numbers
#define K128CP2_REG_FIFO       0
#define K128CP2_REG_COMM       1
#define K128CP2_REG_CONTROL    2
#define K128CP2_REG_STATUS     3
#define K128CP2_REG_STOPCODE   5
#define K128CP2_REG_CLOCKCOUNT 100

typedef enum{

	K128CP2_FPU_CTRL_FCCR = 0,
	K128CP2_FPU_CTRL_FCSR = 1

} k128cp2_fpu_ctrl;


typedef enum{

	K128CP2_TYPE_ADDRAN = 3,
	K128CP2_TYPE_ADDRNN = 4,
	K128CP2_TYPE_ADDRMN = 5

} k128cp2_type_addr;

typedef enum{

	K128CP2_TYPE_LC = 5,
	K128CP2_TYPE_LA = 6

} k128cp2_type_loop;

#endif // __k128cp2_h__
