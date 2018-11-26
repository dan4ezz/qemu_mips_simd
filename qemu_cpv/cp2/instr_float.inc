#include "k128cp2elfun/include/k128cp2elfun.h"
#include "instr_float.h"































































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

void execute_instr_CFLY2  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int ftnum;


   // input acc regs
   //no regs

   // input/output regs
   int fdnum;
   int fsnum;



   // input/output acc regs
   //no regs

   // temporary regs

   
   fpr_t ft;

   
   fpr_t fd;
   fpr_t fs;


   
   fpr_t ftmp1;
   fpr_t ftmp2;
   fpr_t ftmp3;
   fpr_t ftmp4;
   fpr_t ftmp5;






   // get fpr numbers
   
   ftnum = instr_ft(instr);

   
   fdnum = instr_fd(instr);
   fsnum = instr_fs(instr);


   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     ft = read_fpr(reg_clockcount-1,sectnum,ftnum);

     
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
//		flush_fcsr_cause(sectnum,CFLY2);

	// perform operation
	exc_sec_acc = 0;
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

 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CFLY2);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);
   float_result_write(sectnum,fsnum,fs,BYPASS_NO);


     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_CMADD  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   fpr_t fs;
   fpr_t ft;


   
   fpr_t fd;

   
   fpr_t ftmp1;
   fpr_t ftmp2;



   // get fpr numbers
   
   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,CMADD);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CMADD);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_CMSUB  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   fpr_t fs;
   fpr_t ft;


   
   fpr_t fd;

   
   fpr_t ftmp1;
   fpr_t ftmp2;



   // get fpr numbers
   
   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,CMSUB);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CMSUB);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_CMUL  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   fpr_t ftmp1;


   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,CMUL);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CMUL);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_CADD  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,CADD);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CADD);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_PSADD  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,PSADD);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,PSADD);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_PSSUB  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,PSSUB);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,PSSUB);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_PSNEG  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,PSNEG);

	// perform operation
	exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 fd.ui32.hi = float_neg(fs.ui32.hi);
 fd.ui32.lo = float_neg(fs.ui32.lo);
 fd.ui32.lo = adjust_result(fd.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,PSNEG);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_CSUB  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,CSUB);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CSUB);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_PSADDSUB  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fqnum;
   int fdnum;



   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fq;
   fpr_t fd;


   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fqnum = instr_fq(instr);
   fdnum = instr_fd(instr);


   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,PSADDSUB);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,PSADDSUB);

     // write result
     float_result_write(sectnum,fqnum,fq,BYPASS_NO);
   float_result_write(sectnum,fdnum,fd,BYPASS_NO);


     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_MTRANS  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;
   int fqnum;



   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;
   fpr_t fq;


   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);
   fqnum = instr_fq(instr);


   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,MTRANS);

	// perform operation
	fd.ui32.hi = fs.ui32.hi;
 fd.ui32.lo = ft.ui32.hi;
 fq.ui32.hi = fs.ui32.lo;
 fq.ui32.lo = ft.ui32.lo;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,MTRANS);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);
   float_result_write(sectnum,fqnum,fq,BYPASS_NO);


     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_MVMUL  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fqnum;


   // input regs
   int fdnum;
   int fsnum;
   int ftnum;




   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fq;

   fpr_t fd;
   fpr_t fs;
   fpr_t ft;



   
   
   
   fpr_t ftmp1;


   // get fpr numbers
   fqnum = instr_fq(instr);

   fdnum = instr_fd(instr);
   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);



   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);
   fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);



     
     
     

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
//		flush_fcsr_cause(sectnum,MVMUL);

	// perform operation
	exc_sec_acc = 0;
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
//fq.ui32.lo = float_add(float_mul(fd.ui32.hi,ft.ui32.hi), float_mul(fd.ui32.lo,ft.ui32.lo));
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,MVMUL);

     // write result
     float_result_write(sectnum,fqnum,fq,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_MTVMUL  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fqnum;


   // input regs
   int fdnum;
   int fsnum;
   int ftnum;




   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fq;

   fpr_t fd;
   fpr_t fs;
   fpr_t ft;



   
   
   
   fpr_t ftmp1;


   // get fpr numbers
   fqnum = instr_fq(instr);

   fdnum = instr_fd(instr);
   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);



   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);
   fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);



     
     
     

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
//		flush_fcsr_cause(sectnum,MTVMUL);

	// perform operation
	exc_sec_acc = 0;
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
//fq.ui32.lo = float_add(float_mul(fs.ui32.lo,ft.ui32.hi), float_mul(fd.ui32.lo,ft.ui32.lo));
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,MTVMUL);

     // write result
     float_result_write(sectnum,fqnum,fq,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_CADDSUB  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fqnum;
   int fdnum;



   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fq;
   fpr_t fd;


   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fqnum = instr_fq(instr);
   fdnum = instr_fd(instr);


   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,CADDSUB);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CADDSUB);

     // write result
     float_result_write(sectnum,fqnum,fq,BYPASS_NO);
   float_result_write(sectnum,fdnum,fd,BYPASS_NO);


     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_CNEG  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,CNEG);

	// perform operation
	exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 fd.ui32.hi = float_neg(fs.ui32.hi);
 fd.ui32.lo = float_neg(fs.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CNEG);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_CMULI  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   fpr_t ftmp1;
   fpr_t ftmp2;



   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,CMULI);

	// perform operation
	ftmp2.ui32.hi = 0;
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
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CMULI);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_CMULNI  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   fpr_t ftmp1;
   fpr_t ftmp2;



   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,CMULNI);

	// perform operation
	ftmp2.ui32.hi = 0;
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
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CMULNI);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_CCONJ  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,CCONJ);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CCONJ);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_CLEAR  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;
   int fsnum;



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
   fpr_t fs;


   
   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);
   fsnum = instr_fs(instr);


   
   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

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
//		flush_fcsr_cause(sectnum,CLEAR);

	// perform operation
	fd.ui32.hi =  0;
 fd.ui32.lo =  0;
 fs.ui32.hi =  0;
 fs.ui32.lo =  0;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CLEAR);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);
   float_result_write(sectnum,fsnum,fs,BYPASS_NO);


     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_COPY  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
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

	// clear exception bits in fcsr
//	if(if_update_fcsr(instr) == 1)
//		flush_fcsr_cause(sectnum,COPY);

	// perform operation
	fd.ui32.hi = fs.ui32.hi;
 fd.ui32.lo = fs.ui32.lo;
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,COPY);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_SWAP64  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   //no regs

   // input acc regs
   //no regs

   // input/output regs
   int fsnum;
   int fdnum;



   // input/output acc regs
   //no regs

   // temporary regs

   
   
   
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

	// clear exception bits in fcsr
//	if(if_update_fcsr(instr) == 1)
//		flush_fcsr_cause(sectnum,SWAP64);

	// perform operation
	ftmp = fd;
 fd = fs;
 fs = ftmp;
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,SWAP64);

     // write result
     
     float_result_write(sectnum,fsnum,fs,BYPASS_NO);
   float_result_write(sectnum,fdnum,fd,BYPASS_NO);


     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_SWAPHL  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,SWAPHL);

	// perform operation
	fd.ui32.hi = fs.ui32.lo;
 fd.ui32.lo = fs.ui32.hi;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,SWAPHL);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_PSMADD  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   fpr_t fs;
   fpr_t ft;


   
   fpr_t fd;

   
   fpr_t ftmp;


   // get fpr numbers
   
   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,PSMADD);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,PSMADD);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_PSMSUB  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   fpr_t fs;
   fpr_t ft;


   
   fpr_t fd;

   
   fpr_t ftmp;


   // get fpr numbers
   
   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,PSMSUB);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,PSMSUB);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_PSMUL  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,PSMUL);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,PSMUL);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_PSABS  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,PSABS);

	// perform operation
	exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fs = adjust_operand(fs);
 fd.ui32.hi = float_abs(fs.ui32.hi);
 fd.ui32.lo = float_abs(fs.ui32.lo);
 exc_sec_acc |= f_status.float_exception_flags;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,PSABS);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}



void execute_instr_LI0  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   //no regs

   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   
   
   fpr_t fd;

   
   

   // get fpr numbers
   
   
   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     
     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,LI0);

	// perform operation
	fd.ui32.lo = (fd.ui32.lo & ~bits32(15,0)) | (instr_imm16(instr));
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,LI0);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}



void execute_instr_LI1  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   //no regs

   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   
   
   fpr_t fd;

   
   

   // get fpr numbers
   
   
   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     
     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,LI1);

	// perform operation
	fd.ui32.lo = (fd.ui32.lo & ~bits32(31,16)) | ((instr_imm16(instr) << 16));
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,LI1);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}



void execute_instr_LI2  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   //no regs

   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   
   
   fpr_t fd;

   
   

   // get fpr numbers
   
   
   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     
     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,LI2);

	// perform operation
	fd.ui32.hi = (fd.ui32.hi & ~bits32(15,0)) | (instr_imm16(instr));
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,LI2);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}



void execute_instr_LI3  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   //no regs

   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   
   
   fpr_t fd;

   
   

   // get fpr numbers
   
   
   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     
     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,LI3);

	// perform operation
	fd.ui32.hi = (fd.ui32.hi & ~bits32(31,16)) | ((instr_imm16(instr) << 16));
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,LI3);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}



void execute_instr_ASHIFT  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,ASHIFT);

	// perform operation
	#define sig(_a) ((int32_t)(_a))
 ft.ui32.lo &= 0x3f;
 ft.ui32.lo = ((ft.ui32.lo & 0x20) != 0) ? (ft.ui32.lo | 0xffffffc0) : (ft.ui32.lo & 0x3f);
 fd.ui32.hi = (sig(ft.ui32.lo) < 0) ? (sig(fs.ui32.hi) >> (-sig(ft.ui32.lo))) : (sig(fs.ui32.hi) << (sig(ft.ui32.lo)));
 fd.ui32.lo = (sig(ft.ui32.lo) < 0) ? (sig(fs.ui32.lo) >> (-sig(ft.ui32.lo))) : (sig(fs.ui32.lo) << (sig(ft.ui32.lo)));
 if(-sig(ft.ui32.lo) == 32){
  fd.ui32.hi = (sig(fs.ui32.hi) < 0) ? 0xffffffff : 0;
  fd.ui32.lo = (sig(fs.ui32.lo) < 0) ? 0xffffffff : 0;
 }
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,ASHIFT);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}



void execute_instr_ASHIFTI  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,ASHIFTI);

	// perform operation
	#define simm (instr_imm6_sigext32(instr))
 fd.ui32.hi = ((signed)simm < 0) ? (((int32_t)(fs.ui32.hi)) >> (-simm)) : (((int32_t)(fs.ui32.hi)) << simm);
 fd.ui32.lo = ((signed)simm < 0) ? (((int32_t)(fs.ui32.lo)) >> (-simm)) : (((int32_t)(fs.ui32.lo)) << simm);
 if(-(signed)simm == 32){
  fd.ui32.hi = (sig(fs.ui32.hi) < 0) ? 0xffffffff : 0;
  fd.ui32.lo = (sig(fs.ui32.lo) < 0) ? 0xffffffff : 0;
 }
 #undef sig
 #undef simm
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,ASHIFTI);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_RDSEC  





 





 





 







 







 


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
//		flush_fcsr_cause(sectnum,RDSEC);

	// perform operation
	fd.ui32.lo = sectnum;
 fd.ui32.hi = 0;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,RDSEC);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_LSHIFT  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,LSHIFT);

	// perform operation
	#define sig(_a) ((int32_t)(_a))
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
 }*/
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,LSHIFT);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_LSHIFTI  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,LSHIFTI);

	// perform operation
	#define simm (instr_imm6_sigext32(instr))
 fd.ui32.hi = ((signed)simm < 0) ? (fs.ui32.hi >> (-simm)) : (fs.ui32.hi << simm);
 fd.ui32.lo = ((signed)simm < 0) ? (fs.ui32.lo >> (-simm)) : (fs.ui32.lo << simm);
if(-(signed)simm == 32){
  fd.ui32.hi = 0;
  fd.ui32.lo = 0;
 }
 #undef sig
 #undef simm
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,LSHIFTI);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_ABS  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,ABS);

	// perform operation
	if(fs.ui32.hi & 0x80000000)
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

 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,ABS);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_ADD  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,ADD);

	// perform operation
	fd.ui32.hi = fs.ui32.hi + ft.ui32.hi;
 fd.ui32.lo = fs.ui32.lo + ft.ui32.lo;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,ADD);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}



void execute_instr_ADDI  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,ADDI);

	// perform operation
	fd.ui32.hi = fs.ui32.hi + instr_imm12_sigext32(instr);
 fd.ui32.lo = fs.ui32.lo + instr_imm12_sigext32(instr);
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,ADDI);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_NEG  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,NEG);

	// perform operation
	fd.ui32.hi = -fs.ui32.hi;
 fd.ui32.lo = -fs.ui32.lo;
 if(fs.ui32.hi == 0x80000000)
  fd.ui32.hi = 0x7fffffff;
 if(fs.ui32.lo == 0x80000000)
  fd.ui32.lo = 0x7fffffff;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,NEG);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_SUB  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,SUB);

	// perform operation
	fd.ui32.hi = fs.ui32.hi - ft.ui32.hi;
 fd.ui32.lo = fs.ui32.lo - ft.ui32.lo;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,SUB);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}



void execute_instr_SUBI  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,SUBI);

	// perform operation
	fd.ui32.hi = fs.ui32.hi - instr_imm12_sigext32(instr);
 fd.ui32.lo = fs.ui32.lo - instr_imm12_sigext32(instr);
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,SUBI);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}




void execute_instr_INCI  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   //no regs

   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   
   
   fpr_t fd;

   
   

   // get fpr numbers
   
   
   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     
     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,INCI);

	// perform operation
	fd.ui32.hi = fd.ui32.hi + instr_imm18_sigext32(instr);
 fd.ui32.lo = fd.ui32.lo + instr_imm18_sigext32(instr);
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,INCI);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}



void execute_instr_DECI  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   //no regs

   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   
   
   fpr_t fd;

   
   

   // get fpr numbers
   
   
   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     
     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,DECI);

	// perform operation
	fd.ui32.hi = fd.ui32.hi - instr_imm18_sigext32(instr);
 fd.ui32.lo = fd.ui32.lo - instr_imm18_sigext32(instr);
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,DECI);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}



void execute_instr_AND  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,AND);

	// perform operation
	fd.ui32.hi = fs.ui32.hi & ft.ui32.hi;
 fd.ui32.lo = fs.ui32.lo & ft.ui32.lo;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,AND);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}



void execute_instr_OR  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,OR);

	// perform operation
	fd.ui32.hi = fs.ui32.hi | ft.ui32.hi;
 fd.ui32.lo = fs.ui32.lo | ft.ui32.lo;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,OR);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_NOT  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,NOT);

	// perform operation
	fd.ui32.hi = ~fs.ui32.hi;
 fd.ui32.lo = ~fs.ui32.lo;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,NOT);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_XOR  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,XOR);

	// perform operation
	fd.ui32.hi = fs.ui32.hi ^ ft.ui32.hi;
 fd.ui32.lo = fs.ui32.lo ^ ft.ui32.lo;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,XOR);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}




void execute_instr_PSCOPYSIGN  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,PSCOPYSIGN);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,PSCOPYSIGN);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_PWTOPS  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,PWTOPS);

	// perform operation
	exc_sec_acc = 0;
 f_status.float_exception_flags = 0;
 fd.ui32.hi = int32_to_float32(fs.ui32.hi, &f_status);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 fd.ui32.lo = int32_to_float32(fs.ui32.lo, &f_status);
 exc_sec_acc |= f_status.float_exception_flags;
 f_status.float_exception_flags = 0;
 exc_sec_acc = adjust_exception_flags(exc_sec_acc);
 f_status.float_exception_flags = exc_sec_acc;
 exc_acc |= exc_sec_acc;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,PWTOPS);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_PSTOPW  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,PSTOPW);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,PSTOPW);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_SPLIT8  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,SPLIT8);

	// perform operation
	// fd.ui32.hi = (sig(ft.ui32.lo) < 0) ? (sig(fs.ui32.hi) >> (-sig(ft.ui32.lo))) : (sig(fs.ui32.hi) << (sig(ft.ui32.lo)));
fd.ui32.hi = sign_ext32((fs.ui32.hi >> 8),7);
 fd.ui32.lo = sign_ext32((fs.ui32.lo >> 8),7);
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,SPLIT8);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_SPLIT16  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,SPLIT16);

	// perform operation
	fd.ui32.hi = sign_ext32((fs.ui32.hi >> 16),15);
 fd.ui32.lo = sign_ext32((fs.ui32.lo >> 16),15);
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,SPLIT16);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_SPLIT32  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,SPLIT32);

	// perform operation
	fd.ui32.hi = 0;
 fd.ui32.lo = fs.ui32.hi;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,SPLIT32);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_PSGETEXP  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   fpr_t tmp;


   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,PSGETEXP);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,PSGETEXP);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_PSGETMAN  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;

   
   
   
   

   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);

   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
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
//		flush_fcsr_cause(sectnum,PSGETMAN);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,PSGETMAN);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_PSSCALE  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   fpr_t ftmp;


   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,PSSCALE);

	// perform operation
	#define sig(_a) ((int32_t)(_a))
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
 #undef sig
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,PSSCALE);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_EXTSIGN8H  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   //no regs

   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   
   
   fpr_t fd;

   
   

   // get fpr numbers
   
   
   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     
     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,EXTSIGN8H);

	// perform operation
	fd.ui32.hi = (sign_ext32(fd.ui32.hi >> 16,7) << 16) |  (sign_ext32(fd.ui32.hi,7) & 0x0000ffff);
 fd.ui32.lo = (sign_ext32(fd.ui32.lo >> 16,7) << 16) |  (sign_ext32(fd.ui32.lo,7) & 0x0000ffff);
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,EXTSIGN8H);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_EXTSIGN8W  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   //no regs

   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   
   
   fpr_t fd;

   
   

   // get fpr numbers
   
   
   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     
     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,EXTSIGN8W);

	// perform operation
	fd.ui32.hi = sign_ext32(fd.ui32.hi,7);
 fd.ui32.lo = sign_ext32(fd.ui32.lo,7);
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,EXTSIGN8W);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_EXTSIGN16W  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   //no regs

   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   
   
   fpr_t fd;

   
   

   // get fpr numbers
   
   
   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     
     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,EXTSIGN16W);

	// perform operation
	fd.ui32.hi = sign_ext32(fd.ui32.hi,15);
 fd.ui32.lo = sign_ext32(fd.ui32.lo,15);
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,EXTSIGN16W);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_JOIN8  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
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

	// clear exception bits in fcsr
//	if(if_update_fcsr(instr) == 1)
//		flush_fcsr_cause(sectnum,JOIN8);

	// perform operation
	fd.ui32.hi = ((fd.ui32.hi & 0x000000ff) | (fs.ui32.hi << 8)) & 0x0000ffff;
 fd.ui32.lo = ((fd.ui32.lo & 0x000000ff) | (fs.ui32.lo << 8)) & 0x0000ffff;
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,JOIN8);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_JOIN16  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
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

	// clear exception bits in fcsr
//	if(if_update_fcsr(instr) == 1)
//		flush_fcsr_cause(sectnum,JOIN16);

	// perform operation
	fd.ui32.hi = (fd.ui32.hi & 0x0000ffff) | (fs.ui32.hi << 16);
 fd.ui32.lo = (fd.ui32.lo & 0x0000ffff) | (fs.ui32.lo << 16);
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,JOIN16);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_JOIN32  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
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

	// clear exception bits in fcsr
//	if(if_update_fcsr(instr) == 1)
//		flush_fcsr_cause(sectnum,JOIN32);

	// perform operation
	fd.ui32.hi = fs.ui32.lo;
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,JOIN32);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


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

void execute_instr_RRSIN  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
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

	// clear exception bits in fcsr
//	if(if_update_fcsr(instr) == 1)
//		flush_fcsr_cause(sectnum,RRSIN);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,RRSIN);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_RRCOS  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
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

	// clear exception bits in fcsr
//	if(if_update_fcsr(instr) == 1)
//		flush_fcsr_cause(sectnum,RRCOS);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,RRCOS);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_RRCOSSIN  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
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

	// clear exception bits in fcsr
//	if(if_update_fcsr(instr) == 1)
//		flush_fcsr_cause(sectnum,RRCOSSIN);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,RRCOSSIN);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_RRLOG2  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int fsnum;


   // input acc regs
   //no regs

   // input/output regs
   int fdnum;
   int fqnum;



   // input/output acc regs
   //no regs

   // temporary regs

   
   fpr_t fs;

   
   fpr_t fd;
   fpr_t fq;


   
   

   // get fpr numbers
   
   fsnum = instr_fs(instr);

   
   fdnum = instr_fd(instr);
   fqnum = instr_fq(instr);


   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);

     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);
   fq = read_fpr(reg_clockcount-1,sectnum,fqnum);


     

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
//		flush_fcsr_cause(sectnum,RRLOG2);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 







	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,RRLOG2);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);
   float_result_write(sectnum,fqnum,fq,BYPASS_NO);


     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_CHMUL  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   int fdnum;


   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   //no regs

   // input/output acc regs
   //no regs

   // temporary regs

   fpr_t fd;

   fpr_t fs;
   fpr_t ft;


   
   
   
   fpr_t ftmp1;


   // get fpr numbers
   fdnum = instr_fd(instr);

   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   
   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     
     

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
//		flush_fcsr_cause(sectnum,CHMUL);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 






	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CHMUL);

     // write result
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_CHMADD  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   fpr_t fs;
   fpr_t ft;


   
   fpr_t fd;

   
   fpr_t ftmp1;
   fpr_t ftmp2;



   // get fpr numbers
   
   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,CHMADD);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CHMADD);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


void execute_instr_CHMSUB  





 





 





 







 







 


 (instr_t instr) {

//	float tmp;

   int sectnum;
   uint32_t data32;

   // output regs
   //no regs

   // input regs
   int fsnum;
   int ftnum;



   // input acc regs
   //no regs

   // input/output regs
   int fdnum;


   // input/output acc regs
   //no regs

   // temporary regs

   
   fpr_t fs;
   fpr_t ft;


   
   fpr_t fd;

   
   fpr_t ftmp1;
   fpr_t ftmp2;



   // get fpr numbers
   
   fsnum = instr_fs(instr);
   ftnum = instr_ft(instr);


   
   fdnum = instr_fd(instr);

   

   uint32_t exc_sec_acc = 0;
   uint32_t exc_acc = 0;
   exc_sec_acc = exc_sec_acc;

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

     // read arguments
     fs = read_fpr(reg_clockcount-1,sectnum,fsnum);
   ft = read_fpr(reg_clockcount-1,sectnum,ftnum);


     
     fd = read_fpr(reg_clockcount-1,sectnum,fdnum);

     

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
//		flush_fcsr_cause(sectnum,CHMSUB);

	// perform operation
	exc_sec_acc = 0;
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
 exc_acc |= exc_sec_acc;
 








	//adjust exception flags
//	adjust_exception_flags();

	// update fcsr
	if(if_update_fcsr(instr) == 1)
		update_fcsr(sectnum,CHMSUB);

     // write result
     
     float_result_write(sectnum,fdnum,fd,BYPASS_NO);

     
  }

	// update FPE field of Status register
	update_reg_status_fpe (exc_acc);
}


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
