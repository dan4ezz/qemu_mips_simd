// CP2 internals access functions headers
// (read/write registers, memory, etc.)

#ifndef __prockern_access_h__
#define __prockern_access_h__


// access macroses
#define reg_pc               (kern.pc)
#define reg_comm             (kern.comm)
#define reg_status           (kern.status)
#define reg_control          (kern.control)
#define reg_psp              (kern.psp)
#define reg_lc               (kern.lc)
#define reg_la               (kern.la)
#define reg_lsp              (kern.lsp)
#define reg_stopcode         (kern.stopcode)
#define reg_clockcount       (kern.clockcount)
#define reg_fccr(_sect)      (kern.exesect[(_sect)].fccr)
#define reg_fcsr(_sect)      (kern.exesect[(_sect)].fcsr)
#define reg_fpr(_sect,_num)  (kern.exesect[(_sect)].fpr[(_num)])
#define reg_gpr(_num)        (kern.gpr[(_num)])
#define reg_addran(_num)     (kern.addran[(_num)])
#define reg_addrnn(_num)     (kern.addrnn[(_num)])
#define reg_addrmn(_num)     (kern.addrmn[(_num)])
#define reg_ireg(_num)       (kern.ireg[(_num)])
#define iram_ui64(_addr)       ((kern.iram)[(_addr)])
#define lmem_ui32(_sect,_addr) ((kern.exesect[(_sect)].lmem)[(_addr)]) /* address in 32bit blocks */
#define lmem_ui64(_sect,_addr) \
	( (((uint64_t) (lmem_ui32(_sect, (_addr)*2   ))) << 32) | \
	  ( (uint64_t) (lmem_ui32(_sect,((_addr)*2+1))))) /* address in 64bit blocks */

// basic reg read/write functions
regval_t reg_read_raw  (regid_t id);
regval_t reg_read      (regid_t id, uint64_t clck, readwrite_ctrlreg_srcid_t srcid);
regval_t reg_read_ext (regid_t id, uint64_t clck, readwrite_ctrlreg_srcid_t srcid);
regval_t reg_read_on_bypass (regid_t id, uint64_t clck, readwrite_ctrlreg_srcid_t srcid);
void     reg_write_raw (regid_t id, regval_t data);
void     reg_write     (regid_t id, regval_t data, uint64_t clck, readwrite_ctrlreg_srcid_t srcid);

#define SNONE         REGID_SECT_NONE
#define SRCID(_srcid) READWRITE_CTRLREG_SRCID_##_srcid

// raw read
// (without notifying observer)
#define rawread_fccr(_sect)      reg_read_raw ( REGID(_sect, FCCR   , REGNO_NONE, NONE) ) .val.fccr
#define rawread_fcsr(_sect)      reg_read_raw ( REGID(_sect, FCSR   , REGNO_NONE, NONE) ) .val.fcsr
#define rawread_fpr(_sect,_num)  reg_read_raw ( REGID(_sect, FPR    ,       _num, BOTH) ) .val.fpr
#define rawread_gpr(_num)        reg_read_raw ( REGID(SNONE, GPR    ,       _num, NONE) ) .val.gpr
#define rawread_ctrlreg(_num)    reg_read_raw ( REGID(SNONE, CTRLREG,       _num, NONE) ) .val.ctrlreg
#define rawread_addran(_num)     reg_read_raw ( REGID(SNONE, ADDRAN ,       _num, NONE) ) .val.addran
#define rawread_addrnn(_num)     reg_read_raw ( REGID(SNONE, ADDRNN ,       _num, NONE) ) .val.addrnn
#define rawread_addrmn(_num)     reg_read_raw ( REGID(SNONE, ADDRMN ,       _num, NONE) ) .val.addrmn
#define rawread_ir(_num)         reg_read_raw ( REGID(SNONE, IREG,       _num, NONE) ) .val.ireg

// read
// (with notifying observer)
#define read_fccr(_sect,_clck)           reg_read ( REGID(_sect, FCCR   , REGNO_NONE, NONE), _clck, SRCID(  NONE) ) .val.fccr
#define read_fcsr(_sect,_clck)           reg_read ( REGID(_sect, FCSR   , REGNO_NONE, NONE), _clck, SRCID(  NONE) ) .val.fcsr
#define read_fpr(_clck,_sect,_num)       reg_read ( REGID(_sect, FPR    ,       _num, BOTH), _clck, SRCID(  NONE) ) .val.fpr
#define read_gpr(_clck,_num)             reg_read ( REGID(SNONE, GPR    ,       _num, NONE), _clck, SRCID(  NONE) ) .val.gpr
#define read_ctrlreg(_clck,_num,_srcid)  reg_read ( REGID(SNONE, CTRLREG,       _num, NONE), _clck, SRCID(_srcid) ) .val.ctrlreg
#define read_addran(_clck,_num)          reg_read ( REGID(SNONE, ADDRAN ,       _num, NONE), _clck, SRCID(  NONE) ) .val.addran
#define read_addrnn(_clck,_num)          reg_read ( REGID(SNONE, ADDRNN ,       _num, NONE), _clck, SRCID(  NONE) ) .val.addrnn
#define read_addrmn(_clck,_num)          reg_read ( REGID(SNONE, ADDRMN ,       _num, NONE), _clck, SRCID(  NONE) ) .val.addrmn
#define read_ireg(_clck,_num)            reg_read ( REGID(SNONE, IREG   ,       _num, NONE), _clck, SRCID(  NONE) ) .val.ireg

// raw write
// (without notifying observer)
#define rawwrite_fccr(_sect,_data, _mask)   reg_write_raw ( REGID(_sect, FCCR   , REGNO_NONE, NONE), REGVAL(fccr   , _data, _mask   , NONE) )
#define rawwrite_fcsr(_sect,_data, _mask)   reg_write_raw ( REGID(_sect, FCSR   , REGNO_NONE, NONE), REGVAL(fcsr   , _data, _mask   , NONE) )
#define rawwrite_fpr(_sect,_num, _data)     reg_write_raw ( REGID(_sect, FPR    ,       _num, BOTH), REGVAL(fpr    , _data, NOMASK64, NONE) )
#define rawwrite_gpr(_num,_data)            reg_write_raw ( REGID(SNONE, GPR    ,       _num, NONE), REGVAL(gpr    , _data, NOMASK64, NONE) )
#define rawwrite_ctrlreg(_num,_data,_mask)  reg_write_raw ( REGID(SNONE, CTRLREG,       _num, NONE), REGVAL(ctrlreg, _data, _mask   , NONE) )
#define rawwrite_addran(_num,_data)         reg_write_raw ( REGID(SNONE, ADDRAN ,       _num, NONE), REGVAL(addran, _data, ADDRREGMASK, NONE) )
#define rawwrite_addrnn(_num,_data)         reg_write_raw ( REGID(SNONE, ADDRNN ,       _num, NONE), REGVAL(addrnn, _data, ADDRREGMASK, NONE) )
#define rawwrite_addrmn(_num,_data)         reg_write_raw ( REGID(SNONE, ADDRMN ,       _num, NONE), REGVAL(addrmn, _data, ADDRREGMASK, NONE) )

// write
// (with notifying observer)
#define write_fccr(_clck, _sect, _data, _mask)            reg_write ( REGID(_sect, FCCR   , REGNO_NONE, NONE), REGVAL(fccr       , _data,    _mask, NONE), _clck, SRCID(  NONE) )
#define write_fcsr(_clck, _sect, _data, _mask)            reg_write ( REGID(_sect, FCSR   , REGNO_NONE, NONE), REGVAL(fcsr       , _data,    _mask, NONE), _clck, SRCID(  NONE) )
#define write_fpr(_clck, _sect, _num, _data)              reg_write ( REGID(_sect, FPR    ,       _num, BOTH), REGVAL(fpr        , _data, NOMASK64, NONE), _clck, SRCID(  NONE) )
#define write_fpr_ui32_hi(_clck, _sect, _num, _data)      reg_write ( REGID(_sect, FPR    ,       _num,   HI), REGVAL(fpr.ui32.hi, _data, NOMASK64, NONE), _clck, SRCID(  NONE) )
#define write_fpr_ui32_lo(_clck, _sect, _num, _data)      reg_write ( REGID(_sect, FPR    ,       _num,   LO), REGVAL(fpr.ui32.lo, _data, NOMASK64, NONE), _clck, SRCID(  NONE) )
#define write_gpr(_clck, _num, _data)                     reg_write ( REGID(SNONE, GPR    ,       _num, NONE), REGVAL(gpr        , _data, NOMASK64, NONE), _clck, SRCID(  NONE) )
#define write_ctrlreg(_clck, _num, _data, _mask, _srcid)  reg_write ( REGID(SNONE, CTRLREG,       _num, NONE), REGVAL(ctrlreg    , _data,    _mask, NONE), _clck, SRCID(_srcid) )
#define write_addran(_clck, _num, _data)                  reg_write ( REGID(SNONE, ADDRAN ,       _num, NONE), REGVAL(addran     , _data, ADDRREGMASK, NONE), _clck, SRCID(  NONE) )
#define write_addrnn(_clck, _num, _data)                  reg_write ( REGID(SNONE, ADDRNN ,       _num, NONE), REGVAL(addrnn     , _data, ADDRREGMASK, NONE), _clck, SRCID(  NONE) )
#define write_addrmn(_clck, _num, _data)                  reg_write ( REGID(SNONE, ADDRMN ,       _num, NONE), REGVAL(addrmn     , _data, ADDRREGMASK, NONE), _clck, SRCID(  NONE) )
// cp2 lmem access
void     check_sect_cp2addr (               sectid_t sect, cp2addr_t addr);
uint64_t rawread_lmem_ui64  (               sectid_t sect, cp2addr_t addr);
void     rawwrite_lmem_ui64 (               sectid_t sect, cp2addr_t addr, uint64_t data64);
uint64_t read_lmem_ui64     (uint64_t clck, sectid_t sect, cp2addr_t addr);
void     write_lmem_ui64    (uint64_t clck, sectid_t sect, cp2addr_t addr, uint64_t data64);

// cp2 instruction memory access
void     rawwrite_iram      (uint64_t *prg_ptr, uint64_t prg_len);
void     rawread_iram       (uint64_t *prg_ptr, uint64_t prg_len, uint64_t addr);
uint64_t rawread_iram_ui64  (               cp2addr_t addr);
void     rawwrite_iram_ui64 (               cp2addr_t addr, uint64_t data64);
uint64_t read_iram_ui64     (uint64_t clck, cp2addr_t addr);
void     write_iram_ui64    (uint64_t clck, cp2addr_t addr, uint64_t data64);

// cp2 start_dma bit access
int      start_dma_read();


#endif // __prockern_access_h__

