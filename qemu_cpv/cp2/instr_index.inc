








































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
