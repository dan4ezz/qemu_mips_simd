#ifndef __instr_h__
#define __instr_h__

#include "common.h"

#endif // __instr_h__


void softfloat_prepare ();
void softfloat_clear   ();

void execute_instr_NOP     (instr_t instr);
void execute_instr_CHECK_DMA (instr_t instr);
void execute_instr_MVMADD  (instr_t instr);
void execute_instr_MVMSUB  (instr_t instr);
void execute_instr_MTVMADD (instr_t instr);
void execute_instr_MTVMSUB (instr_t instr);
void execute_instr_CCOND   (instr_t instr);
void execute_instr_MFC     (instr_t instr);
void execute_instr_MTC     (instr_t instr);
void execute_instr_DOI     (instr_t instr);
void execute_instr_DO      (instr_t instr);
void execute_instr_JUMP    (instr_t instr);
void execute_instr_JUMPI   (instr_t instr);
void execute_instr_ENDDO   (instr_t instr);
void execute_instr_SYNC    (instr_t instr);
void execute_instr_RUN     (instr_t instr);
void execute_instr_RUNI    (instr_t instr);
void execute_instr_STOP    (instr_t instr);
void execute_instr_STOPI   (instr_t instr);
void execute_instr_SETI    (instr_t instr);
void execute_instr_MOVE    (instr_t instr);
void execute_instr_MTFPR   (instr_t instr);
void execute_instr_MFFPR   (instr_t instr);
void execute_instr_CLR     (instr_t instr);
void execute_instr_CALL    (instr_t instr);
void execute_instr_CALLI   (instr_t instr);
void execute_instr_RET     (instr_t instr);
void execute_instr_UNPCK16WSTOPS (instr_t instr);
void execute_instr_PSPRMSGN0 (instr_t instr);
void execute_instr_PSPRMSGN1 (instr_t instr);
