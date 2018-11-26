dnl instr_def(SET) {
dnl
dnl   int sectnum;
dnl   int idnum, isnum, itnum;
dnl
dnl   // get register numbers
dnl    idnum = instr_id(instr);
dnl    isnum = instr_is(instr);
dnl    itnum = instr_it(instr);
dnl
dnl    // loop for all sections
dnl    for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {
dnl     ind_t id, is, it;
dnl
dnl    // read arguments
dnl     is = reg_ind(sectnum,is);
dnl     it = reg_ind(sectnum,it);
dnl
dnl    // perform operation
dnl     id = is + it;
dnl
dnl    // write result
dnl     ind_result_write(sectnum,idnum,id,SET);
dnl   }
dnl }


define(`regs_num_def',`ifelse($#, 0,`', $#, 1, `int $1num;',
`int $1num;
   regs_num_def(shift($@))')'
)
define(`regs_ind_def', `ifelse($#, 0,`', $#, 1, `ind_t $1;',
`ind_t $1;
   regs_ind_def(shift($@))')'
)
define(`regs_indnum_get', `ifelse($#, 0,`', $#, 1, `$1num = instr_$1(instr);',
`$1num = instr_$1(instr);
   regs_indnum_get(shift($@))')'
)

define(`regs_readind',
`ifelse($#, 0,`', $#, 1, `read_ind(sectnum,$1num,$1);',
`read_ind(sectnum,$1num,$1);
   regs_readind(shift($@))')'
)
dnl ind_result_write(sectnum,id,idreg,SET);
define(`regs_result_write',
`ifelse($#, 0,`', $#, 1, `ind_result_write(sectnum,$1num,$1,name);',
`ind_result_write(sectnum,$1num,$1,name);
   regs_result_write(shift($@))')'
)

dnl not empty argument section

define(`common_def',
dnl Задаем int fdnum;
`define(`$1_finr_num_def', `regs_num_def(shift($@))')
dnl Задаем fpr_t fd;
define(`$1_finr_ind_def', `regs_ind_def(shift($@))')
dnl Задаем fdnum = instr_fd(instr);
define(`$1_finr_indnum_get', `regs_indnum_get(shift($@))')
')

define(`temp_def',
dnl Задаем int fdnum;
`define(`$1_finr_num_def', `regs_num_def(shift($@))')
dnl Задаем fpr_t fd;
define(`$1_finr_ind_def', `regs_ind_def(shift($@))')
')

define(`input_def',
dnl Задаем fs = read_fpr(sectnum,fsnum)
`define(`$1_finr_readind', `regs_readind(shift($@))')
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
define(`$1_finr_ind_def', `')
dnl Задаем fdnum = instr_fd(instr);
define(`$1_finr_indnum_get', `')
')

define(`temp_emptydef',
dnl Задаем int fdnum;
`define(`$1_finr_num_def', `//no regs')
dnl Задаем fpr_t fd;
define(`$1_finr_ind_def', `')
')


define(`input_emptydef',
dnl Задаем fs = read_fpr(sectnum,fsnum)
`define(`$1_finr_readind', `')
')

define(`output_emptydef',
dnl Задаем float_result_write(sectnum,fdnum,fd,ADD);
`define(`$1_finr_result_write', `')
')



define(`input_regs',  `ifelse($#, 0, `common_emptydef(`input')
input_emptydef(`input')',
`common_def(`input', $@)
input_def(`input', $@)'
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
define(`temp_regs',   `ifelse($#, 0, `temp_emptydef(`temp')', `temp_def(`temp', $@)')')
define(`insn_type', `$1 define(`name', `$1')')
define(`immed',`instr_immed(instr)')


define(`instr_def',

void execute_instr_$1(iinstr_t instr) {

   int sectnum;

   // output regs
   output_finr_num_def

   // input regs
   input_finr_num_def

   // input/output regs
   io_finr_num_def

   //temporary regs
   temp_finr_num_def



   output_finr_ind_def
   input_finr_ind_def
   io_finr_ind_def
   temp_finr_ind_def

   // get fpr numbers
   output_finr_indnum_get
   input_finr_indnum_get
   io_finr_indnum_get

   // loop for all sections
   for (sectnum=0; sectnum<NUMBER_OF_EXESECT; sectnum++) {

dnl     // clear exception bits in fcsr
dnl     flush_fcsr(sectnum,name);

     // read arguments
     input_finr_readind
     io_finr_readind

     // perform operation
     $6
dnl     // update fcsr
dnl     update_fcsr(sectnum,name);

     // write result
     output_finr_result_write
     io_finr_result_write
  }
}
)

dnl change comment delimiters
changecom(`/*', `*/')

/*
instr_def(
 insn_type(SET),
 output_regs(id),
 input_regs(it,is),
 io_regs,
 temp_regs,
 `id = is + it;'
)

instr_def(
 insn_type(SETI),
 output_regs(id),
 input_regs(is),
 io_regs,
 temp_regs,
 `id = is + immed;'
)

instr_def(
 insn_type(INC),
 output_regs,
 input_regs(it,is),
 io_regs(id),
 temp_regs,
 `id = is + it + id;'
)

instr_def(
 insn_type(INCI),
 output_regs,
 input_regs(is),
 io_regs(id),
 temp_regs,
 `id = is + id + immed;'
)


instr_def(
 insn_type(INCIS),
 output_regs,
 input_regs(is),
 io_regs(id),
 temp_regs,
 `id = id*sectnum + is + immed;'
)

instr_def(
 insn_type(CPR),
 output_regs,
 input_regs(is),
 io_regs(id),
 temp_regs,
 `if ((is + immed)<CONST_SIZE){
   read_const(sectnum, is+immed, id);
  }
  else{
  sim_error()
  }
'
)

*/
