#include "k128cp2elfun/include/k128cp2elfun.h"
#include "instr_float.h"

dnl usage:
dnl instr_def(ADD,
dnl   output_regs(fd),     # output regs
dnl   input_regs(fs,ft),   # input  regs
dnl   io_regs(),           # input/output regs
dnl   temp_regs(),         # temporary regs
dnl   `fd.ui32.hi = float_add(fs.ui32.hi, ft.ui32.hi);
dnl    fd.ui32.lo = float_add(fs.ui32.lo, ft.ui32.lo);'
dnl )
dnl
dnl expanded to
dnl
dnl void execute_instr_ADD(finstr_t instr) {
dnl
dnl   int sectnum;
dnl   int fsnum;
dnl   int ftnum;
dnl   int fdnum;
dnl
dnl   fpr_t fs;
dnl   fpr_t ft;
dnl   fpr_t fd;
dnl
dnl   // get fpr numbers
dnl   fdnum = instr_fd(instr);
dnl   fsnum = instr_fs(instr);
dnl   ftnum = instr_ft(instr);
dnl
dnl   // loop for all sections
dnl   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {
dnl
dnl     // clear exception bits in fcsr
dnl     flush_fcsr_cause(sectnum,ADD);
dnl
dnl     // read arguments
dnl     fs = read_fpr(sectnum,fsnum);
dnl     ft = read_fpr(sectnum,ftnum);
dnl
dnl     // perform operation
dnl     fdreg.ui32.hi = float_add(fsreg.ui32.hi, ftreg.ui32.hi);
dnl     fdreg.ui32.lo = float_add(fsreg.ui32.lo, ftreg.ui32.lo);
dnl
dnl     // update fcsr
dnl     update_fcsr(sectnum,ADD);
dnl
dnl     // write result
dnl     float_result_write(sectnum,fdnum,fd,ADD);
dnl   }
dnl
dnl
dnl
dnl define(`submdef',
dnl   `
dnl   define(`submacro1',`$1 $2')
dnl   define(`submacro2',`$2 $1')
dnl   '
dnl )
dnl
dnl
dnl define(`main',
dnl
dnl   submacro1
dnl
dnl   submacro2
dnl
dnl )
dnl
dnl main(ADD,submdef(a,b))
dnl
dnl

define(`regs_num_def',`ifelse($#, 0,`', $#, 1, `int $1num;',
`int $1num;
   regs_num_def(shift($@))')'
)
define(`regs_fpr_def', `ifelse($#, 0,`', $#, 1, `fpr_t $1;',
`fpr_t $1;
   regs_fpr_def(shift($@))')'
)
define(`regs_fprnum_get', `ifelse($#, 0,`', $#, 1, `$1num = instr_$1(instr);',
`$1num = instr_$1(instr);
   regs_fprnum_get(shift($@))')'
)

define(`regs_readfpr',
`ifelse($#, 0,`', $#, 1, `$1 = read_fpr(reg_clockcount-1,sectnum,$1num);',
`$1 = read_fpr(reg_clockcount-1,sectnum,$1num);
   regs_readfpr(shift($@))')'
)

define(`regs_acc_readfpr',
`ifelse($#, 0,`', $#, 1, `read_fpr_fprt_acc(reg_clockcount-1,sectnum,$1num,$1);',
`read_fpr_fprt_acc(sectnum,$1num,$1);
   regs_readfpr(shift($@))')'
)

define(`regs_result_write',
`ifelse($#, 0,`', $#, 1, `float_result_write(sectnum,$1num,$1,BPASS);',
`float_result_write(sectnum,$1num,$1,BYPASS_NO);
   regs_result_write(shift($@))')'
)
dnl define(`reverse', `ifelse($#, 0, , $#, 1, `$1',`reverse(shift($@)), $1')')


dnl not empty argument section

define(`common_def',
dnl Задаем int fdnum;
`define(`$1_finr_num_def', `regs_num_def(shift($@))')
dnl Задаем fpr_t fd;
define(`$1_finr_fpr_def', `regs_fpr_def(shift($@))')
dnl Задаем fdnum = instr_fd(instr);
define(`$1_finr_fprnum_get', `regs_fprnum_get(shift($@))')
')

define(`temp_def',
dnl Задаем int fdnum;
`define(`$1_finr_num_def', `regs_num_def(shift($@))')
dnl Задаем fpr_t fd;
define(`$1_finr_fpr_def', `regs_fpr_def(shift($@))')
')

define(`input_def',
dnl Задаем fs = read_fpr(sectnum,fsnum)
`define(`$1_finr_readfpr', `regs_readfpr(shift($@))')
')
define(`input_acc_def',
dnl Задаем fs = read_fpr(sectnum,fsnum)
`define(`$1_finr_readfpr', `regs_acc_readfpr(shift($@))')
')
define(`output_def',
dnl float_result_write(sectnum,fdnum,fd,ADD);
`define(`$1_finr_result_write', `regs_result_write(shift($@))')
')

dnl empty section

define(`common_emptydef',
dnl Задаем int fdnum;
`define(`$1_finr_num_def', `//no regs')
dnl Задаем fpr_t fd;
define(`$1_finr_fpr_def', `')
dnl Задаем fdnum = instr_fd(instr);
define(`$1_finr_fprnum_get', `')
')

define(`temp_emptydef',
dnl Задаем int fdnum;
`define(`$1_finr_num_def', `//no regs')
dnl Задаем fpr_t fd;
define(`$1_finr_fpr_def', `')
')


define(`input_emptydef',
dnl Задаем fs = read_fpr(sectnum,fsnum)
`define(`$1_finr_readfpr', `')
')

define(`input_acc_emptydef',
dnl Задаем fs = read_fpr(sectnum,fsnum)
`define(`$1_finr_readfpr', `')
')

define(`output_emptydef',
dnl Задаем float_result_write(sectnum,fdnum,fd,ADD);
`define(`$1_finr_result_write', `')
')



define(`input_regs_ext',  `ifelse($#, 0, `common_emptydef(`input')
input_emptydef(`input')',
`common_def(`input', $@)
input_def(`input', $@)'
)')

define(`input_regs',  `ifelse($#, 0, `common_emptydef(`input')
input_emptydef(`input')',
`common_def(`input', $@)
input_def(`input', $@)'
)')

define(`input_acc_regs',  `ifelse($#, 0, `common_emptydef(`input_acc')
input_acc_emptydef(`input_acc')',
`common_def(`input_acc', $@)
input_acc_def(`input_acc', $@)'
)')

define(`output_regs', `ifelse($#, 0, `common_emptydef(`output')
output_emptydef(`output')', `common_def(`output', $@)
output_def(`output', $@)')')

define(`io_regs',     `ifelse($#, 0, `common_emptydef(`io')
input_emptydef(`io')
output_emptydef(`io')',
`common_def(`io',$@)
input_def(`io',$@)
output_def(`io', $@)')')

define(`io_acc_regs',     `ifelse($#, 0, `common_emptydef(`io_acc')
input_acc_emptydef(`io_acc')
output_emptydef(`io_acc')',
`common_def(`io_acc',$@)
input_acc_def(`io_acc',$@)
output_def(`io_acc', $@)')')


define(`temp_regs',   `ifelse($#, 0, `temp_emptydef(`temp')', `temp_def(`temp', $@)')')
define(`insn_name', `$1 empty_all_regdefs define(`name', `$1')')
define(`BPS_type', `define(`BPASS',`BYPASS_$1')')

define(`empty_all_regdefs',
` output_regs
 input_regs
 input_acc_regs
 io_regs
 io_acc_regs
 temp_regs'
)

define(`instr_def',

void execute_instr_$1(instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   output_finr_num_def

   // input regs
   input_finr_num_def

   // input acc regs
   input_acc_finr_num_def

   // input/output regs
   io_finr_num_def

   // input/output acc regs
   io_acc_finr_num_def

   // temporary regs
dnl   temp_finr_num_def

   output_finr_fpr_def
   input_finr_fpr_def
   input_acc_finr_fpr_def
   io_finr_fpr_def
   io_acc_finr_fpr_def
   temp_finr_fpr_def

   // get fpr numbers
   output_finr_fprnum_get
   input_finr_fprnum_get
   input_acc_finr_fprnum_get
   io_finr_fprnum_get
   io_acc_finr_fprnum_get

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     input_finr_readfpr
     input_acc_finr_readfpr
     io_finr_readfpr
     io_acc_finr_readfpr

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
//		flush_fcsr_cause(sectnum,name);

	// perform operation
	$2

	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,name);

     // write result
     output_finr_result_write
     io_finr_result_write
     io_acc_finr_result_write
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}
)



dnl change comment delimiters
changecom(`/*', `*/')



dnl ============ Instructions ===========
/*
instr_def(
 insn_name(GETCOEFF),
`regid_t  regid;
 regval_t regval;
 reg_rind_t rind;
 union f32 { float f; uint32_t u; };
 union f32 sin_a32, cos_a32;
 rind = kern.rind;
 regval = reg_read_on_bypass(REGID(SNONE, CTRLREG, CTRLREG_RIND, NONE), reg_clockcount-1, READWRITE_CTRLREG_SRCID_CP2);
 rind.ui32 = regval.val.ctrlreg;
 sim_printf("myout rind = %x", rind.ui32);
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
 if(sectnum==3){
  rind.ui32 = ((rind.ui32 + kern.rstep)&kern.rmask) | (rind.ui32 & ~kern.rmask);
  regid  = REGID (SNONE, CTRLREG, CTRLREG_RIND, NONE);
  regval = REGVAL(ctrlreg, rind.ui32 , NOMASK64, NONE);
  delayed_reg_write (&regid, &regval, INSTR_DELAY_MOVE_CTRL, BYPASS_YES);
 }
'
 output_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(CFLY),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 regid_t  regid;
 regval_t regval;
 reg_rind_t rind;
 reg_rstep_t rstep;
 reg_rmask_t rmask;
 union f32 { float f; uint32_t u; };
 union f32 sin_a32, cos_a32;
 if(sectnum==0){
 regval = reg_read_on_bypass(REGID(SNONE, CTRLREG, CTRLREG_RIND, NONE), reg_clockcount-1, READWRITE_CTRLREG_SRCID_CP2);
 rind.ui32 = regval.val.ctrlreg;
/////// rind.ui32 = kern.rind.ui32;
// rind.ui32 = read_ctrlreg(reg_clockcount, 8, CP2);
 }
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

 if(sectnum==3){
  rstep = read_ctrlreg (reg_clockcount-1, CTRLREG_RSTEP, CP2);
  rmask = read_ctrlreg (reg_clockcount-1, CTRLREG_RMASK, CP2);
  rind.ui32 = ((rind.ui32 + rstep)&rmask) | (rind.ui32 & ~rmask);
  regid  = REGID (SNONE, CTRLREG, CTRLREG_RIND, NONE);
  regval = REGVAL(ctrlreg, rind.ui32, NOMASK64, NONE);
  delayed_reg_write (&regid, &regval, INSTR_DELAY_MOVE_CTRL, BYPASS_YES);
//  write_ctrlreg (reg_clockcount, CTRLREG_RIND, rind.ui32, NOMASK64, CP2);
 }
 fd = ftmp4;
 fs = ftmp5;
'
 io_regs(fd,fs),
 temp_regs(ftt,ftmp1,ftmp2,ftmp3,ftmp4,ftmp5),
 BPS_type(NO)
)
*/

instr_def(
 insn_name(CFLY2),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 fd = adjust_operand(fd);
// ftmp1.ui32.hi = float_add(fd.ui32.hi,float_sub(float_mul(fs.ui32.hi, ft.ui32.hi),float_mul(fs.ui32.lo,ft.ui32.lo)));

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp2.ui32.hi = float_mul(fs.ui32.lo,ft.ui32.lo);
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


// ftmp1.ui32.lo = float_add(fd.ui32.lo,float_add(float_mul(fs.ui32.hi, ft.ui32.lo),float_mul(fs.ui32.lo,ft.ui32.hi)));

 ftmp1.ui32.lo = float_mul(fs.ui32.hi, ft.ui32.lo);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp2.ui32.lo = float_mul(fs.ui32.lo,ft.ui32.hi);
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


// ftmp2.ui32.hi = float_sub(fd.ui32.hi,float_sub(float_mul(fs.ui32.hi, ft.ui32.hi),float_mul(fs.ui32.lo,ft.ui32.lo)));

 ftmp5.ui32.hi = float_sub(fd.ui32.hi, ftmp3.ui32.hi);
 ftmp5.ui32.hi = adjust_result(ftmp5.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;


// ftmp2.ui32.lo = float_sub(fd.ui32.lo,float_add(float_mul(fs.ui32.hi, ft.ui32.lo),float_mul(fs.ui32.lo,ft.ui32.hi)));

 ftmp5.ui32.lo = float_sub(fd.ui32.lo, ftmp3.ui32.lo);
 ftmp5.ui32.lo = adjust_result(ftmp5.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;

 fd = ftmp4;
 fs = ftmp5;
'
 io_regs(fd,fs),
 input_regs(ft),
 temp_regs(ftmp1,ftmp2,ftmp3,ftmp4,ftmp5),
 BPS_type(NO)
)

instr_def(
 insn_name(CMADD),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 fd = adjust_operand(fd);
 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.lo);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp2.ui32.hi = float_sub(ftmp1.ui32.hi,ftmp1.ui32.lo);
 ftmp2.ui32.hi = adjust_result(ftmp2.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.hi = float_add(fd.ui32.hi, ftmp2.ui32.hi);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;


 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.hi);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.lo);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp2.ui32.lo = float_add(ftmp1.ui32.hi,ftmp1.ui32.lo);
 ftmp2.ui32.lo = adjust_result(ftmp2.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_add(fd.ui32.lo, ftmp2.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 io_regs(fd),
 temp_regs(ftmp1,ftmp2),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(CMSUB),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 fd = adjust_operand(fd);
 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.lo);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp2.ui32.hi = float_sub(ftmp1.ui32.hi,ftmp1.ui32.lo);
 ftmp2.ui32.hi = adjust_result(ftmp2.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.hi = float_sub(fd.ui32.hi, ftmp2.ui32.hi);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;


 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.hi);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.lo);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp2.ui32.lo = float_add(ftmp1.ui32.hi,ftmp1.ui32.lo);
 ftmp2.ui32.lo = adjust_result(ftmp2.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_sub(fd.ui32.lo, ftmp2.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 io_regs(fd),
 temp_regs(ftmp1,ftmp2),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(CMUL),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.lo);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.hi = float_sub(ftmp1.ui32.hi,ftmp1.ui32.lo);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.hi);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.lo);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_add(ftmp1.ui32.hi,ftmp1.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 input_regs(fs,ft),
 temp_regs(ftmp1),
 BPS_type(NO)
)

instr_def(
 insn_name(CADD),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 fd.ui32.hi = float_add(fs.ui32.hi, ft.ui32.hi);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_add(fs.ui32.lo, ft.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(PSADD),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 fd.ui32.hi = float_add(fs.ui32.hi, ft.ui32.hi);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_add(fs.ui32.lo, ft.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(PSSUB),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 fd.ui32.hi = float_sub(fs.ui32.hi, ft.ui32.hi);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_sub(fs.ui32.lo, ft.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(PSNEG),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 fd.ui32.hi = float_neg(fs.ui32.hi);
 fd.ui32.lo = float_neg(fs.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(CSUB),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 fd.ui32.hi = float_sub(fs.ui32.hi, ft.ui32.hi);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_sub(fs.ui32.lo, ft.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(PSADDSUB),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 fd.ui32.hi = float_add(fs.ui32.hi, ft.ui32.hi);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_add(fs.ui32.lo, ft.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;


 fq.ui32.hi = float_sub(fs.ui32.hi, ft.ui32.hi);
 fq.ui32.hi = adjust_result(fq.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fq.ui32.lo = float_sub(fs.ui32.lo, ft.ui32.lo);
 fq.ui32.lo = adjust_result(fq.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fq,fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(MTRANS),
`fd.ui32.hi = fs.ui32.hi;
 fd.ui32.lo = ft.ui32.hi;
 fq.ui32.hi = fs.ui32.lo;
 fq.ui32.lo = ft.ui32.lo;'
 output_regs(fd,fq),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(MVMUL),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 fd = adjust_operand(fd);
 ft = adjust_operand(ft);

 ftmp1.ui32.hi = float_mul(fs.ui32.hi,ft.ui32.hi);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.lo = float_mul(fs.ui32.lo,ft.ui32.lo);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fq.ui32.hi = float_add(ftmp1.ui32.lo,ftmp1.ui32.hi);
 fq.ui32.hi = adjust_result(fq.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fd.ui32.hi,ft.ui32.hi);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.lo = float_mul(fd.ui32.lo,ft.ui32.lo);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fq.ui32.lo = float_add(ftmp1.ui32.lo,ftmp1.ui32.hi);
 fq.ui32.lo = adjust_result(fq.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;
//fq.ui32.hi = float_add(float_mul(fs.ui32.hi,ft.ui32.hi), float_mul(fs.ui32.lo,ft.ui32.lo));
//fq.ui32.lo = float_add(float_mul(fd.ui32.hi,ft.ui32.hi), float_mul(fd.ui32.lo,ft.ui32.lo));'
 output_regs(fq),
 input_regs(fd,fs,ft),
 temp_regs(ftmp1),
 BPS_type(NO)
)

instr_def(
 insn_name(MTVMUL),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 fd = adjust_operand(fd);
 ft = adjust_operand(ft);

 ftmp1.ui32.hi = float_mul(fs.ui32.hi,ft.ui32.hi);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.lo = float_mul(fd.ui32.hi,ft.ui32.lo);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fq.ui32.hi = float_add(ftmp1.ui32.lo,ftmp1.ui32.hi);
 fq.ui32.hi = adjust_result(fq.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.lo,ft.ui32.hi);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.lo = float_mul(fd.ui32.lo,ft.ui32.lo);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fq.ui32.lo = float_add(ftmp1.ui32.lo,ftmp1.ui32.hi);
 fq.ui32.lo = adjust_result(fq.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;
//fq.ui32.hi = float_add(float_mul(fs.ui32.hi,ft.ui32.hi), float_mul(fd.ui32.hi,ft.ui32.lo));
//fq.ui32.lo = float_add(float_mul(fs.ui32.lo,ft.ui32.hi), float_mul(fd.ui32.lo,ft.ui32.lo));'
 output_regs(fq),
 input_regs(fd,fs,ft),
 temp_regs(ftmp1),
 BPS_type(NO)
)

instr_def(
 insn_name(CADDSUB),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 fd.ui32.hi = float_add(fs.ui32.hi, ft.ui32.hi);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_add(fs.ui32.lo, ft.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;


 fq.ui32.hi = float_sub(fs.ui32.hi, ft.ui32.hi);
 fq.ui32.hi = adjust_result(fq.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fq.ui32.lo = float_sub(fs.ui32.lo, ft.ui32.lo);
 fq.ui32.lo = adjust_result(fq.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fq,fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(CNEG),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 fd.ui32.hi = float_neg(fs.ui32.hi);
 fd.ui32.lo = float_neg(fs.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(CMULI),
`ftmp2.ui32.hi = 0;
 ftmp2.ui32.lo = 0x3f800000;
 exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ftmp2 = adjust_operand(ftmp2);
 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ftmp2.ui32.lo);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ftmp2.ui32.hi);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.hi = float_sub(ftmp1.ui32.hi,ftmp1.ui32.lo);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ftmp2.ui32.hi);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ftmp2.ui32.lo);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_add(ftmp1.ui32.hi,ftmp1.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 temp_regs(ftmp1,ftmp2),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(CMULNI),
`ftmp2.ui32.hi = 0;
 ftmp2.ui32.lo = 0xbf800000;
 exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ftmp2 = adjust_operand(ftmp2);
 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ftmp2.ui32.lo);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ftmp2.ui32.hi);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.hi = float_sub(ftmp1.ui32.hi,ftmp1.ui32.lo);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ftmp2.ui32.hi);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ftmp2.ui32.lo);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_add(ftmp1.ui32.hi,ftmp1.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 temp_regs(ftmp1,ftmp2),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(CCONJ),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 fd.ui32.hi =  fs.ui32.hi;
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_neg(fs.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(CLEAR),
`fd.ui32.hi =  0;
 fd.ui32.lo =  0;
 fs.ui32.hi =  0;
 fs.ui32.lo =  0;'
 output_regs(fd,fs)
)

instr_def(
 insn_name(COPY),
`fd.ui32.hi = fs.ui32.hi;
 fd.ui32.lo = fs.ui32.lo;'
 io_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(SWAP64),
`ftmp = fd;
 fd = fs;
 fs = ftmp;'
 io_regs(fs,fd),
 temp_regs(ftmp),
 BPS_type(NO)
)

instr_def(
 insn_name(SWAPHL),
`fd.ui32.hi = fs.ui32.lo;
 fd.ui32.lo = fs.ui32.hi;'
 input_regs(fs),
 output_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(PSMADD),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 fd = adjust_operand(fd);
 ftmp.ui32.hi = float_mul (fs.ui32.hi, ft.ui32.hi);
 ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.hi = float_add(fd.ui32.hi, ftmp.ui32.hi);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp.ui32.lo = float_mul (fs.ui32.lo, ft.ui32.lo);
 ftmp.ui32.lo = adjust_result(ftmp.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_add(fd.ui32.lo, ftmp.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 io_regs(fd),
 temp_regs(ftmp),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(PSMSUB),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 fd = adjust_operand(fd);
 ftmp.ui32.hi = float_mul (fs.ui32.hi, ft.ui32.hi);
 ftmp.ui32.hi = adjust_result(ftmp.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.hi = float_sub(fd.ui32.hi, ftmp.ui32.hi);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp.ui32.lo = float_mul (fs.ui32.lo, ft.ui32.lo);
 ftmp.ui32.lo = adjust_result(ftmp.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_sub(fd.ui32.lo, ftmp.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 io_regs(fd),
 temp_regs(ftmp),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(PSMUL),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 fd.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(PSABS),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 fd.ui32.hi = float_abs(fs.ui32.hi);
 fd.ui32.lo = float_abs(fs.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)


instr_def(
 insn_name(LI0),
`fd.ui32.lo = (fd.ui32.lo & ~bits32(15,0)) | (instr_imm16(instr));'
 io_regs(fd),
 BPS_type(NO)
)


instr_def(
 insn_name(LI1),
`fd.ui32.lo = (fd.ui32.lo & ~bits32(31,16)) | ((instr_imm16(instr) << 16));'
 io_regs(fd),
 BPS_type(NO)
)


instr_def(
 insn_name(LI2),
`fd.ui32.hi = (fd.ui32.hi & ~bits32(15,0)) | (instr_imm16(instr));'
 io_regs(fd),
 BPS_type(NO)
)


instr_def(
 insn_name(LI3),
`fd.ui32.hi = (fd.ui32.hi & ~bits32(31,16)) | ((instr_imm16(instr) << 16));'
 io_regs(fd),
 BPS_type(NO)
)


instr_def(
 insn_name(ASHIFT),
`#define sig(_a) ((int32_t)(_a))
 ft.ui32.lo &= 0x3f;
 ft.ui32.lo = ((ft.ui32.lo & 0x20) != 0) ? (ft.ui32.lo | 0xffffffc0) : (ft.ui32.lo & 0x3f);
 fd.ui32.hi = (sig(ft.ui32.lo) < 0) ? (sig(fs.ui32.hi) >> (-sig(ft.ui32.lo))) : (sig(fs.ui32.hi) << (sig(ft.ui32.lo)));
 fd.ui32.lo = (sig(ft.ui32.lo) < 0) ? (sig(fs.ui32.lo) >> (-sig(ft.ui32.lo))) : (sig(fs.ui32.lo) << (sig(ft.ui32.lo)));
 if(-sig(ft.ui32.lo) == 32){
  fd.ui32.hi = (sig(fs.ui32.hi) < 0) ? 0xffffffff : 0;
  fd.ui32.lo = (sig(fs.ui32.lo) < 0) ? 0xffffffff : 0;
 }'
 input_regs(fs,ft),
 output_regs(fd),
 BPS_type(NO)
)


instr_def(
 insn_name(ASHIFTI),
`#define simm (instr_imm6_sigext32(instr))
 fd.ui32.hi = ((signed)simm < 0) ? (((int32_t)(fs.ui32.hi)) >> (-simm)) : (((int32_t)(fs.ui32.hi)) << simm);
 fd.ui32.lo = ((signed)simm < 0) ? (((int32_t)(fs.ui32.lo)) >> (-simm)) : (((int32_t)(fs.ui32.lo)) << simm);
 if(-(signed)simm == 32){
  fd.ui32.hi = (sig(fs.ui32.hi) < 0) ? 0xffffffff : 0;
  fd.ui32.lo = (sig(fs.ui32.lo) < 0) ? 0xffffffff : 0;
 }
 #undef sig
 #undef simm'
 input_regs(fs),
 output_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(RDSEC),
`fd.ui32.lo = sectnum;
 fd.ui32.hi = 0;'
 output_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(LSHIFT),
`#define sig(_a) ((int32_t)(_a))
 ft.ui32.lo &= 0x3f;
 ft.ui32.lo = (((ft.ui32.lo & 0x20) != 0) && ((ft.ui32.lo & 0x1f) != 0)) ? (ft.ui32.lo | 0xffffffc0) : (ft.ui32.lo & 0x3f);
 fd.ui32.hi = (sig(ft.ui32.lo) < 0) ? ((fs.ui32.hi) >> (-sig(ft.ui32.lo))) : ((fs.ui32.hi) << ((ft.ui32.lo)));
 fd.ui32.lo = (sig(ft.ui32.lo) < 0) ? ((fs.ui32.lo) >> (-sig(ft.ui32.lo))) : ((fs.ui32.lo) << ((ft.ui32.lo)));
if(sig(ft.ui32.lo) == 32){
  fd.ui32.hi = 0;
  fd.ui32.lo = 0;
 }
/* if(-sig(ft.ui32.lo) == 32){
  fd.ui32.hi = (sig(fs.ui32.hi) < 0) ? 0xffffffff : 0;
  fd.ui32.lo = (sig(fs.ui32.lo) < 0) ? 0xffffffff : 0;
 }*/'
 input_regs(fs,ft),
 output_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(LSHIFTI),
`#define simm (instr_imm6_sigext32(instr))
 fd.ui32.hi = ((signed)simm < 0) ? (fs.ui32.hi >> (-simm)) : (fs.ui32.hi << simm);
 fd.ui32.lo = ((signed)simm < 0) ? (fs.ui32.lo >> (-simm)) : (fs.ui32.lo << simm);
if(-(signed)simm == 32){
  fd.ui32.hi = 0;
  fd.ui32.lo = 0;
 }
 #undef sig
 #undef simm'
 input_regs(fs),
 output_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(ABS),
`if(fs.ui32.hi & 0x80000000)
  fd.ui32.hi = - fs.ui32.hi;
 else
  fd.ui32.hi = fs.ui32.hi;
 if(fs.ui32.lo & 0x80000000)
  fd.ui32.lo = - fs.ui32.lo;
 else
  fd.ui32.lo = fs.ui32.lo;
 if(fs.ui32.hi == 0x80000000)
  fd.ui32.hi = 0x7fffffff;
 if(fs.ui32.lo == 0x80000000)
  fd.ui32.lo = 0x7fffffff;
'
 input_regs(fs),
 output_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(ADD),
`fd.ui32.hi = fs.ui32.hi + ft.ui32.hi;
 fd.ui32.lo = fs.ui32.lo + ft.ui32.lo;'
 input_regs(fs,ft),
 output_regs(fd),
 BPS_type(NO)
)


instr_def(
 insn_name(ADDI),
`fd.ui32.hi = fs.ui32.hi + instr_imm12_sigext32(instr);
 fd.ui32.lo = fs.ui32.lo + instr_imm12_sigext32(instr);'
 input_regs(fs),
 output_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(NEG),
`fd.ui32.hi = -fs.ui32.hi;
 fd.ui32.lo = -fs.ui32.lo;'
 if(fs.ui32.hi == 0x80000000)
  fd.ui32.hi = 0x7fffffff;
 if(fs.ui32.lo == 0x80000000)
  fd.ui32.lo = 0x7fffffff;
 input_regs(fs),
 output_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(SUB),
`fd.ui32.hi = fs.ui32.hi - ft.ui32.hi;
 fd.ui32.lo = fs.ui32.lo - ft.ui32.lo;'
 input_regs(fs,ft),
 output_regs(fd),
 BPS_type(NO)
)


instr_def(
 insn_name(SUBI),
`fd.ui32.hi = fs.ui32.hi - instr_imm12_sigext32(instr);
 fd.ui32.lo = fs.ui32.lo - instr_imm12_sigext32(instr);'
 input_regs(fs),
 output_regs(fd),
 BPS_type(NO)
)



instr_def(
 insn_name(INCI),
`fd.ui32.hi = fd.ui32.hi + instr_imm18_sigext32(instr);
 fd.ui32.lo = fd.ui32.lo + instr_imm18_sigext32(instr);'
 io_regs(fd),
 BPS_type(NO)
)


instr_def(
 insn_name(DECI),
`fd.ui32.hi = fd.ui32.hi - instr_imm18_sigext32(instr);
 fd.ui32.lo = fd.ui32.lo - instr_imm18_sigext32(instr);'
 io_regs(fd),
 BPS_type(NO)
)


instr_def(
 insn_name(AND),
`fd.ui32.hi = fs.ui32.hi & ft.ui32.hi;
 fd.ui32.lo = fs.ui32.lo & ft.ui32.lo;'
 input_regs(fs,ft),
 output_regs(fd),
 BPS_type(NO)
)


instr_def(
 insn_name(OR),
`fd.ui32.hi = fs.ui32.hi | ft.ui32.hi;
 fd.ui32.lo = fs.ui32.lo | ft.ui32.lo;'
 input_regs(fs,ft),
 output_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(NOT),
`fd.ui32.hi = ~fs.ui32.hi;
 fd.ui32.lo = ~fs.ui32.lo;'
 input_regs(fs),
 output_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(XOR),
`fd.ui32.hi = fs.ui32.hi ^ ft.ui32.hi;
 fd.ui32.lo = fs.ui32.lo ^ ft.ui32.lo;'
 input_regs(fs,ft),
 output_regs(fd),
 BPS_type(NO)
)



instr_def(
 insn_name(PSCOPYSIGN),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 if( !float32_is_nan(ft.ui32.hi) )
 fd.ui32.hi = (fs.ui32.hi & bits32(30,0)) | (ft.ui32.hi & bit32(31));
 else
 fd.ui32.hi = 0x7fffffff;
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 if( !float32_is_nan(ft.ui32.lo) )
 fd.ui32.lo = (fs.ui32.lo & bits32(30,0)) | (ft.ui32.lo & bit32(31));
 else
 fd.ui32.lo = 0x7fffffff;
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 input_regs(fs,ft),
 output_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(PWTOPS),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fd.ui32.hi = int32_to_float32(fs.ui32.hi, &f_status);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 fd.ui32.lo = int32_to_float32(fs.ui32.lo, &f_status);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 input_regs(fs),
 output_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(PSTOPW),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.hi = float32_to_int32(fs.ui32.hi, &f_status);
 if( f_status.float_exception_flags & float_flag_invalid )
  fd.ui32.hi = 0x7fffffff;
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float32_to_int32(fs.ui32.lo, &f_status);
 if( f_status.float_exception_flags & float_flag_invalid )
  fd.ui32.lo = 0x7fffffff;
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 input_regs(fs),
 output_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(SPLIT8),
// fd.ui32.hi = (sig(ft.ui32.lo) < 0) ? (sig(fs.ui32.hi) >> (-sig(ft.ui32.lo))) : (sig(fs.ui32.hi) << (sig(ft.ui32.lo)));
`fd.ui32.hi = sign_ext32((fs.ui32.hi >> 8),7);
 fd.ui32.lo = sign_ext32((fs.ui32.lo >> 8),7);'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(SPLIT16),
`fd.ui32.hi = sign_ext32((fs.ui32.hi >> 16),15);
 fd.ui32.lo = sign_ext32((fs.ui32.lo >> 16),15);'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(SPLIT32),
`fd.ui32.hi = 0;
 fd.ui32.lo = fs.ui32.hi;'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(PSGETEXP),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 if( (fs.ui32.hi & 0x7f800000) != 0x7f800000  ){
  tmp.ui32.hi = (fs.ui32.hi & 0x7f800000) >> 23;
   fd.ui32.hi = tmp.ui32.hi - 127;
 }
 else{
  fd.ui32.hi = 0x7fffffff;
  f_status.float_exception_flags |= float_flag_invalid;
 }
 if( (fs.ui32.lo & 0x7f800000) != 0x7f800000  ){
  tmp.ui32.lo = (fs.ui32.lo & 0x7f800000) >> 23;
   fd.ui32.lo = tmp.ui32.lo - 127;
 }
 else{
  fd.ui32.lo = 0x7fffffff;
  f_status.float_exception_flags |= float_flag_invalid;
 }
 exc_sec_acc |= f_status.float_exception_flags;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 input_regs(fs),
 temp_regs(tmp),
 BPS_type(NO)
)

instr_def(
 insn_name(PSGETMAN),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 if( (fs.ui32.hi & 0x7f800000) != 0x7f800000  ){
  if( (fs.ui32.hi & 0x7fffffff) != 0 )
   fd.ui32.hi = (fs.ui32.hi & 0x807fffff) | 0x3f800000;
  else
   fd.ui32.hi = fs.ui32.hi;
 }
 else{
  fd.ui32.hi = 0x7fffffff;
  f_status.float_exception_flags |= float_flag_invalid;
 }
 if( (fs.ui32.lo & 0x7f800000) != 0x7f800000  ){
  if( (fs.ui32.lo & 0x7fffffff) != 0 )
   fd.ui32.lo = (fs.ui32.lo & 0x807fffff) | 0x3f800000;
  else
   fd.ui32.lo = fs.ui32.lo;
 }
 else{
  fd.ui32.lo = 0x7fffffff;
  f_status.float_exception_flags |= float_flag_invalid;
 }
 exc_sec_acc |= f_status.float_exception_flags;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(PSSCALE),
`#define sig(_a) ((int32_t)(_a))
 exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);

 ft.ui32.hi &= 0x1ff;
 ft.ui32.lo &= 0x1ff;

 if((ft.ui32.hi & 0x100) != 0)
    ft.ui32.hi |= 0xffffff00;
 if((ft.ui32.lo & 0x100) != 0)
    ft.ui32.lo |= 0xffffff00;

// sim_printf("myout fs = %08x %08x",fs.ui32.hi, fs.ui32.lo);

 if( (fs.ui32.hi == 0) || ((fs.ui32.hi & ~0x80000000) == 0) )
  fd.ui32.hi = fs.ui32.hi;
 else
 if( fs.ui32.hi == 0x7f800000 )
  fd.ui32.hi = 0x7f800000;
 else if( fs.ui32.hi == 0xff800000 )
  fd.ui32.hi = 0xff800000;
 else
 if( !float32_is_nan(fs.ui32.hi) ){
  ftmp.ui32.hi = (((fs.ui32.hi & 0x7f800000) >> 23) + (sig(ft.ui32.hi)));
  if( (sig(ftmp.ui32.hi) < 1) && !((fs.ui32.hi == 0) && (sig(ft.ui32.hi) == 0)) ){
   f_status.float_exception_flags |= float_flag_underflow | float_flag_inexact;
   fd.ui32.hi = fs.ui32.hi & 0x80000000;
  }
  else
  if( sig(ftmp.ui32.hi) > 254 ){
   f_status.float_exception_flags |= float_flag_overflow | float_flag_inexact;
   fd.ui32.hi = fs.ui32.hi & 0x80000000;
  }
  else{
   ftmp.ui32.hi = ftmp.ui32.hi << 23;
   fd.ui32.hi = (fs.ui32.hi & 0x807fffff) | ftmp.ui32.hi;
  }
 }
 else{
  fd.ui32.hi = 0x7fffffff;
  f_status.float_exception_flags |= float_flag_invalid;
 }
 fd.ui32.hi = adjust_result(fd.ui32.hi);


 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 if( (fs.ui32.lo == 0) || ((fs.ui32.lo & ~0x80000000) == 0) )
  fd.ui32.lo = fs.ui32.lo;
 else
 if( fs.ui32.lo == 0x7f800000 )
  fd.ui32.lo = 0x7f800000;
 else if( fs.ui32.lo == 0xff800000 )
  fd.ui32.lo = 0xff800000;
 else
 if( !float32_is_nan(fs.ui32.lo) ){
  ftmp.ui32.lo = (((fs.ui32.lo & 0x7f800000) >> 23) + (sig(ft.ui32.lo)));
  if( (sig(ftmp.ui32.lo) < 1) && !((fs.ui32.lo == 0) && (sig(ft.ui32.lo) == 0)) ){
   f_status.float_exception_flags |= float_flag_underflow | float_flag_inexact;
   fd.ui32.lo = fs.ui32.lo & 0x80000000;
//   sim_printf("myout 1 fd.lo = %08x", fs.ui32.lo);
  }
  else
  if( sig(ftmp.ui32.lo) > 254 ){
   f_status.float_exception_flags |= float_flag_overflow | float_flag_inexact;
   fd.ui32.lo = fs.ui32.lo & 0x80000000;
  }
  else{
   ftmp.ui32.lo = ftmp.ui32.lo << 23;
   fd.ui32.lo = (fs.ui32.lo & 0x807fffff) | ftmp.ui32.lo;
//   sim_printf("myout 2 fd.lo = %08x", fs.ui32.lo);
  }
 }
 else{
  fd.ui32.lo = 0x7fffffff;
  f_status.float_exception_flags |= float_flag_invalid;
 }
 fd.ui32.lo = adjust_result(fd.ui32.lo);

 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;
 #undef sig'
 output_regs(fd),
 temp_regs(ftmp),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(EXTSIGN8H),
`fd.ui32.hi = (sign_ext32(fd.ui32.hi >> 16,7) << 16) |  (sign_ext32(fd.ui32.hi,7) & 0x0000ffff);
 fd.ui32.lo = (sign_ext32(fd.ui32.lo >> 16,7) << 16) |  (sign_ext32(fd.ui32.lo,7) & 0x0000ffff);'
 io_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(EXTSIGN8W),
`fd.ui32.hi = sign_ext32(fd.ui32.hi,7);
 fd.ui32.lo = sign_ext32(fd.ui32.lo,7);'
 io_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(EXTSIGN16W),
`fd.ui32.hi = sign_ext32(fd.ui32.hi,15);
 fd.ui32.lo = sign_ext32(fd.ui32.lo,15);'
 io_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(JOIN8),
`fd.ui32.hi = ((fd.ui32.hi & 0x000000ff) | (fs.ui32.hi << 8)) & 0x0000ffff;
 fd.ui32.lo = ((fd.ui32.lo & 0x000000ff) | (fs.ui32.lo << 8)) & 0x0000ffff;'
 io_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(JOIN16),
`fd.ui32.hi = (fd.ui32.hi & 0x0000ffff) | (fs.ui32.hi << 16);
 fd.ui32.lo = (fd.ui32.lo & 0x0000ffff) | (fs.ui32.lo << 16);'
 io_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(JOIN32),
`fd.ui32.hi = fs.ui32.lo;'
 io_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

/*instr_def(
 insn_name(RECIP),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 switch(instr_elf_cond(instr)){
 case 0: fd.f.lo = k128cp2elfun_recip(fs.f.lo); fd.ui32.lo = adjust_result(fd.ui32.lo); break;
 case 1: fd.f.lo = k128cp2elfun_recip(fs.f.hi); fd.ui32.lo = adjust_result(fd.ui32.lo); break;
 case 2: fd.f.hi = k128cp2elfun_recip(fs.f.lo); fd.ui32.hi = adjust_result(fd.ui32.hi); break;
 case 3: fd.f.hi = k128cp2elfun_recip(fs.f.hi); fd.ui32.hi = adjust_result(fd.ui32.hi); break;
 }
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 input_regs(fs),
 io_regs(fd),
 BPS_type(NO)
)*/

/*instr_def(
 insn_name(EXP2),
`exc_sec_acc = 0;
 ftmp.ui32.hi = 0;
 ftmp.ui32.lo = 0;
 f_status.float_exception_flags = 0;
 switch(instr_elf_cond(instr)){
 case 0: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_exp2(fs.f.lo); fd.ui32.lo = adjust_result(fd.ui32.lo); break;
 case 1: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_exp2(fs.f.hi); fd.ui32.lo = adjust_result(fd.ui32.lo); break;
 case 2: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_exp2(fs.f.lo); fd.ui32.hi = adjust_result(fd.ui32.hi); break;
 case 3: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_exp2(fs.f.hi); fd.ui32.hi = adjust_result(fd.ui32.hi); break;
 }
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 input_regs(fs),
 temp_regs(ftmp),
 io_regs(fd),
 BPS_type(NO)
)*/

/*instr_def(
 insn_name(LOG2C),
`exc_sec_acc = 0;
 ftmp.ui32.hi = 0;
 ftmp.ui32.lo = 0;
 f_status.float_exception_flags = 0;
 switch(instr_elf_cond(instr)){
 case 0: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_log2c(fs.f.lo); fd.ui32.lo = adjust_result(fd.ui32.lo); break;
 case 1: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_log2c(fs.f.hi); fd.ui32.lo = adjust_result(fd.ui32.lo); break;
 case 2: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_log2c(fs.f.lo); fd.ui32.hi = adjust_result(fd.ui32.hi); break;
 case 3: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_log2c(fs.f.hi); fd.ui32.hi = adjust_result(fd.ui32.hi); break;
 }
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 input_regs(fs),
 temp_regs(ftmp),
 io_regs(fd),
 BPS_type(NO)
)*/

/*instr_def(
 insn_name(RSQRT),
`exc_sec_acc = 0;
 ftmp.ui32.hi = 0;
 ftmp.ui32.lo = 0;
 f_status.float_exception_flags = 0;
 switch(instr_elf_cond(instr)){
 case 0: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_recip(k128cp2elfun_sqrt(fs.f.lo)); fd.ui32.lo = adjust_result(fd.ui32.lo); break;
 case 1: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_recip(k128cp2elfun_sqrt(fs.f.hi)); fd.ui32.lo = adjust_result(fd.ui32.lo); break;
 case 2: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_recip(k128cp2elfun_sqrt(fs.f.lo)); fd.ui32.hi = adjust_result(fd.ui32.hi); break;
 case 3: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_recip(k128cp2elfun_sqrt(fs.f.hi)); fd.ui32.hi = adjust_result(fd.ui32.hi); break;
 }
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 input_regs(fs),
 temp_regs(ftmp),
 io_regs(fd),
 BPS_type(NO)
)*/

/*instr_def(
 insn_name(SINC),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 switch(instr_elf_cond(instr)){
 case 0: fd.f.lo = k128cp2elfun_sinc(fs.f.lo); fd.ui32.lo = adjust_result(fd.ui32.lo); break;
 case 1: fd.f.lo = k128cp2elfun_sinc(fs.f.hi); fd.ui32.lo = adjust_result(fd.ui32.lo); break;
 case 2: fd.f.hi = k128cp2elfun_sinc(fs.f.lo); fd.ui32.hi = adjust_result(fd.ui32.hi); break;
 case 3: fd.f.hi = k128cp2elfun_sinc(fs.f.hi); fd.ui32.hi = adjust_result(fd.ui32.hi); break;
 }
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 input_regs(fs),
 io_regs(fd),
 BPS_type(NO)
)*/

/*instr_def(
 insn_name(ATANC),
`exc_sec_acc = 0;
 ftmp.ui32.hi = 0;
 ftmp.ui32.lo = 0;
 f_status.float_exception_flags = 0;
 switch(instr_elf_cond(instr)){
 case 0: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_atanc(fs.f.lo); fd.ui32.lo = adjust_result(fd.ui32.lo); break;
 case 1: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.lo = k128cp2elfun_atanc(fs.f.hi); fd.ui32.lo = adjust_result(fd.ui32.lo); break;
 case 2: ftmp.f.lo = fs.f.lo; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_atanc(fs.f.lo); fd.ui32.hi = adjust_result(fd.ui32.hi); break;
 case 3: ftmp.f.hi = fs.f.hi; ftmp = adjust_operand(ftmp); fs = ftmp; fd.f.hi = k128cp2elfun_atanc(fs.f.hi); fd.ui32.hi = adjust_result(fd.ui32.hi); break;
 }
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 input_regs(fs),
 temp_regs(ftmp),
 io_regs(fd),
 BPS_type(NO)
)*/

instr_def(
 insn_name(RRSIN),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 fd.f.lo = k128cp2elfun_rrsin(fs.f.lo);
 fd.ui32.lo = adjust_result_rs(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.f.hi = k128cp2elfun_rrsin(fs.f.hi);
 fd.ui32.hi = adjust_result_rs(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 input_regs(fs),
 io_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(RRCOS),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 fd.f.lo = k128cp2elfun_rrcos(fs.f.lo);
 fd.ui32.lo = adjust_result_rs(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.f.hi = k128cp2elfun_rrcos(fs.f.hi);
 fd.ui32.hi = adjust_result_rs(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 input_regs(fs),
 io_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(RRCOSSIN),
`exc_sec_acc = 0;
 ftmp.f.hi = 0;
 ftmp.f.lo = fs.f.lo;
 f_status.float_exception_flags = 0;
 ftmp = adjust_operand(ftmp);
 fs = ftmp;
 fd.f.lo = k128cp2elfun_rrsin(fs.f.lo);
 fd.ui32.lo = adjust_result_rs(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.f.hi = k128cp2elfun_rrcos(fs.f.lo);
 fd.ui32.hi = adjust_result_rs(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 input_regs(fs),
 temp_regs(ftmp),
 io_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(RRLOG2),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 fd.f.lo = k128cp2elfun_rrlog2m(fs.f.lo);
 fd.ui32.lo = adjust_result_rs(fd.ui32.lo);
 fq.f.lo = k128cp2elfun_rrlog2e(fs.f.lo);
 fq.ui32.lo = adjust_result_rs(fq.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.f.hi = k128cp2elfun_rrlog2m(fs.f.hi);
 fd.ui32.hi = adjust_result_rs(fd.ui32.hi);
 fq.f.hi = k128cp2elfun_rrlog2e(fs.f.hi);
 fq.ui32.hi = adjust_result_rs(fq.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 input_regs(fs),
 io_regs(fd,fq),
 BPS_type(NO)
)

instr_def(
 insn_name(CHMUL),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 ft.f.lo = -ft.f.lo;
 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.lo);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.hi = float_sub(ftmp1.ui32.hi,ftmp1.ui32.lo);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.hi);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.lo);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_add(ftmp1.ui32.hi,ftmp1.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 output_regs(fd),
 input_regs(fs,ft),
 temp_regs(ftmp1),
 BPS_type(NO)
)

instr_def(
 insn_name(CHMADD),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 fd = adjust_operand(fd);
 ft.f.lo = -ft.f.lo;
 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.lo);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp2.ui32.hi = float_sub(ftmp1.ui32.hi,ftmp1.ui32.lo);
 ftmp2.ui32.hi = adjust_result(ftmp2.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.hi = float_add(fd.ui32.hi, ftmp2.ui32.hi);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;


 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.hi);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.lo);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp2.ui32.lo = float_add(ftmp1.ui32.hi,ftmp1.ui32.lo);
 ftmp2.ui32.lo = adjust_result(ftmp2.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_add(fd.ui32.lo, ftmp2.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 io_regs(fd),
 temp_regs(ftmp1,ftmp2),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(CHMSUB),
`exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 ft = adjust_operand(ft);
 fd = adjust_operand(fd);
 ft.f.lo = -ft.f.lo;
 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.lo);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp2.ui32.hi = float_sub(ftmp1.ui32.hi,ftmp1.ui32.lo);
 ftmp2.ui32.hi = adjust_result(ftmp2.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.hi = float_sub(fd.ui32.hi, ftmp2.ui32.hi);
 fd.ui32.hi = adjust_result(fd.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;


 ftmp1.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.hi);
 ftmp1.ui32.lo = adjust_result(ftmp1.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp1.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.lo);
 ftmp1.ui32.hi = adjust_result(ftmp1.ui32.hi);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 ftmp2.ui32.lo = float_add(ftmp1.ui32.hi,ftmp1.ui32.lo);
 ftmp2.ui32.lo = adjust_result(ftmp2.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 fd.ui32.lo = float_sub(fd.ui32.lo, ftmp2.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;

 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;'
 io_regs(fd),
 temp_regs(ftmp1,ftmp2),
 input_regs(fs,ft),
 BPS_type(NO)
)

dnl ====================================
dnl ДАЛЬШЕ ЗАКОММЕНТАРЕНО
/*
instr_def(
 insn_name(MADDSUB),
`ftmp1.ui32.hi = float_add(fd.ui32.hi,float_sub(float_mul(fs.ui32.hi, ft.ui32.hi),float_mul(fs.ui32.lo,ft.ui32.lo)));
 ftmp1.ui32.lo = float_add(fd.ui32.lo,float_add(float_mul(fs.ui32.hi, ft.ui32.lo),float_mul(fs.ui32.lo,ft.ui32.hi)));
 ftmp2.ui32.hi = float_sub(fd.ui32.hi,float_sub(float_mul(fs.ui32.hi, ft.ui32.hi),float_mul(fs.ui32.lo,ft.ui32.lo)));
 ftmp2.ui32.lo = float_sub(fd.ui32.lo,float_add(float_mul(fs.ui32.hi, ft.ui32.lo),float_mul(fs.ui32.lo,ft.ui32.hi)));
 fd = ftmp1;
 fs = ftmp2;
'
 io_regs(fd),
 io_regs(fs),
 input_regs(ft),
 temp_regs(ftmp1,ftmp2),
 BPS_type(NO)
)

instr_def(
 insn_name(MUL),
`fd.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
 fd.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.lo);'
 output_regs(fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(MULN1),
`fd.ui32.hi = float_neg(fs.ui32.hi);
 fd.ui32.lo = float_neg(fs.ui32.lo);'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(MULI),
`fd.ui32.hi = float_neg(fs.ui32.lo);
 fd.ui32.lo =  fs.ui32.hi;'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(MULNI),
`fd.ui32.hi =  fs.ui32.lo;
 fd.ui32.lo = float_neg(fs.ui32.hi);'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(CONJ),
`fd.ui32.hi =  fs.ui32.hi;
 fd.ui32.lo = float_neg(fs.ui32.lo);'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(ADD),
`fd.ui32.hi = float_add(fs.ui32.hi, ft.ui32.hi);
 fd.ui32.lo = float_add(fs.ui32.lo, ft.ui32.lo);
'
 output_regs(fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(SUB),
`fd.ui32.hi = float_sub(fs.ui32.hi, ft.ui32.hi);
 fd.ui32.lo = float_sub(fs.ui32.lo, ft.ui32.lo);'
 output_regs(fd),
 input_regs(fs,ft),
 BPS_type(NO)
)


instr_def(
 insn_name(ADDSUB),
`fd.ui32.hi = float_add(fs.ui32.hi, ft.ui32.hi);
 fd.ui32.lo = float_add(fs.ui32.lo, ft.ui32.lo);
 fr.ui32.hi = float_sub(fs.ui32.hi, ft.ui32.hi);
 fr.ui32.lo = float_sub(fs.ui32.lo, ft.ui32.lo);'
 output_regs(fd,fr),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(CLEAR2),
`if ( (fdnum%2) != 0 ) return;
 fd.ui32.hi =  0;
 fd.ui32.lo =  0;
 ftmpnum=fdnum+1;
 ftmp.ui32.lo = 0;
 ftmp.ui32.hi = 0;
 regs_result_write(ftmp);'
 output_regs(fd),
 temp_regs(ftmp),
)

instr_def(
 insn_name(CLEAR),
 `fd.ui32.hi =  0;
 fd.ui32.lo =  0;'
 output_regs(fd),
)

instr_def(
 insn_name(FMULMV),
  `ftmp.ui32.hi = float_add(float_add(float_mul(fs.ui32.hi, ft.ui32.hi),float_mul(fr.ui32.hi,ft.ui32.lo)),fd.ui32.hi);
   ftmp.ui32.lo = float_add(float_add(float_mul(fs.ui32.lo, ft.ui32.hi),float_mul(fr.ui32.lo,ft.ui32.lo)),fd.ui32.lo);
   fd = ftmp;
'
 input_regs(fr,fs,ft),
 io_regs(fd),
 temp_regs(ftmp),
 BPS_type(NO)
)

instr_def(
 insn_name(FMADD),
 `fr.ui32.hi = float_add(float_mul(fs.ui32.hi, ft.ui32.hi),fd.ui32.hi);
   fr.ui32.lo = float_add(float_mul(fs.ui32.lo, ft.ui32.lo),fd.ui32.lo);'
 output_regs(fr),
 input_regs(fs,ft),
 input_acc_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(FMSUB),
 `fr.ui32.hi = float_sub(float_mul(fs.ui32.hi, ft.ui32.hi),fd.ui32.hi);
   fr.ui32.lo = float_sub(float_mul(fs.ui32.lo, ft.ui32.lo),fd.ui32.lo);'
 output_regs(fr),
 input_regs(fs,ft),
 input_acc_regs(fd),
 BPS_type(NO)
)

instr_def(
 insn_name(FMUL),
 `fd.ui32.hi = float_mul(fs.ui32.hi, ft.ui32.hi);
   fd.ui32.lo = float_mul(fs.ui32.lo, ft.ui32.lo);'
 output_regs(fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(FABS),
 `fd.ui32.hi = float_abs(fs.ui32.hi);
   fd.ui32.lo = float_abs(fs.ui32.lo);'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)


instr_def(
 insn_name(FDIV),
 `fd.ui32.lo = float_div(fs.ui32.lo, ft.ui32.lo);'
 output_regs(fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

dnl 0x3F800000 - 1 в формате float_32
instr_def(
 insn_name(RECIP),
 `fd.ui32.lo = float_div(0x3F800000, fs.ui32.lo);'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(RSQRT),
 `fd.ui32.lo = float_div(0x3F800000, float_sqrt(fs.ui32.lo));'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(FSQRT),
 `fd.ui32.lo = float_sqrt(fs.ui32.lo);'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)
instr_def(
 insn_name(FPLL),
 `fd.ui32.hi = fs.ui32.lo;
   fd.ui32.lo = ft.ui32.lo;'
 output_regs(fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(FPLH),
 `fd.ui32.hi = fs.ui32.lo;
   fd.ui32.lo = ft.ui32.hi;'
 output_regs(fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(FPHL),
 `fd.ui32.hi = fs.ui32.hi;
   fd.ui32.lo = ft.ui32.lo;'
 output_regs(fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(FPHH),
 `fd.ui32.hi = fs.ui32.hi;
   fd.ui32.lo = ft.ui32.hi;'
 output_regs(fd),
 input_regs(fs,ft),
 BPS_type(NO)
)

instr_def(
 insn_name(MOV),
`fd.ui32.hi = fs.ui32.hi;
 fd.ui32.lo = fs.ui32.lo;'
 output_regs(fd),
 input_regs(fs),
 BPS_type(NO)
)

instr_def(
 insn_name(MOVZ),
`if ((ft.ui32.hi&((uint32_t)0x7FFFFFFF)) == ((uint32_t)0x0)){
 fd.ui32.hi = fs.ui32.hi;
 }
 if ((ft.ui32.lo&((uint32_t)0x7FFFFFFF)) == ((uint32_t)0x0)){
 fd.ui32.lo = fs.ui32.lo;
 }'
 output_regs(fd),
 input_regs(fs, ft),
 BPS_type(NO)
)

instr_def(
 insn_name(MOVNZ),
`if ((ft.ui32.hi&((uint32_t)0x7FFFFFFF)) != ((uint32_t)0x0)){
 fd.ui32.hi = fs.ui32.hi;
 }
 if ((ft.ui32.lo&((uint32_t)0x7FFFFFFF)) != ((uint32_t)0x0)){
 fd.ui32.lo = fs.ui32.lo;
 }'
 output_regs(fd),
 input_regs(fs, ft),
 BPS_type(NO)
)

instr_def(
 insn_name(MOVP),
`if (((ft.ui32.hi&((uint32_t)0x80000000)) == ((uint32_t)0x0))&&((ft.ui32.hi&&((uint32_t)0x7FFFFFFF))!=((uint32_t)0x0))){
 fd.ui32.hi = fs.ui32.hi;
 }
 if (((ft.ui32.lo&((uint32_t)0x80000000)) == ((uint32_t)0x0))&&((ft.ui32.lo&&((uint32_t)0x7FFFFFFF))!=((uint32_t)0x0))){
 fd.ui32.lo = fs.ui32.lo;
 }'
 output_regs(fd),
 input_regs(fs, ft),
 BPS_type(NO)
)

instr_def(
 insn_name(MOVNP),
`if ((ft.ui32.hi&((uint32_t)0x80000000)) != ((uint32_t)0x0)){
 fd.ui32.hi = fs.ui32.hi;
 }
 if ((ft.ui32.lo&((uint32_t)0x80000000)) != ((uint32_t)0x0)){
 fd.ui32.lo = fs.ui32.lo;
 }'
 output_regs(fd),
 input_regs(fs, ft),
 BPS_type(NO)
)

*/
