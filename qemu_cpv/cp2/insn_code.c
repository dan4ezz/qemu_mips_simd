#include <stdio.h>
#include <string.h>
#include "insn_code.h"


// instr hi
insntab_t insntab_hi[]={
	{"nop"        ,   INSTR_OPCODE_NOP        , INSN_ARGS_NOARGS     },
	{"caddsub"    ,   INSTR_OPCODE_CADDSUB    , INSN_ARGS_FDFSFTFQ   },
	{"psaddsub"   ,   INSTR_OPCODE_PSADDSUB   , INSN_ARGS_FDFSFTFQ   },
	{"mtrans"     ,   INSTR_OPCODE_MTRANS     , INSN_ARGS_FDFSFTFQ   },
	{"mvmul"      ,   INSTR_OPCODE_MVMUL      , INSN_ARGS_FDFSFTFQ   },
	{"mtvmul"     ,   INSTR_OPCODE_MTVMUL     , INSN_ARGS_FDFSFTFQ   },
	{"mvmadd"     ,   INSTR_OPCODE_MVMADD     , INSN_ARGS_FDFSFT     },
	{"mvmsub"     ,   INSTR_OPCODE_MVMSUB     , INSN_ARGS_FDFSFT     },
	{"mtvmadd"    ,   INSTR_OPCODE_MTVMADD    , INSN_ARGS_FDFSFT     },
	{"mtvmsub"    ,   INSTR_OPCODE_MTVMSUB    , INSN_ARGS_FDFSFT     },
	{"cadd"       ,   INSTR_OPCODE_CADD       , INSN_ARGS_FDFSFT     },
	{"csub"       ,   INSTR_OPCODE_CSUB       , INSN_ARGS_FDFSFT     },
	{"cmul"       ,   INSTR_OPCODE_CMUL       , INSN_ARGS_FDFSFT     },
	{"cmadd"      ,   INSTR_OPCODE_CMADD      , INSN_ARGS_FDFSFT     },
	{"cmsub"      ,   INSTR_OPCODE_CMSUB      , INSN_ARGS_FDFSFT     },
	{"cmuli"      ,   INSTR_OPCODE_CMULI      , INSN_ARGS_FDFS       },
	{"cmulni"     ,   INSTR_OPCODE_CMULNI     , INSN_ARGS_FDFS       },
	{"cneg"       ,   INSTR_OPCODE_CNEG       , INSN_ARGS_FDFS       },
	{"cconj"      ,   INSTR_OPCODE_CCONJ      , INSN_ARGS_FDFS       },
	{"psadd"      ,   INSTR_OPCODE_PSADD      , INSN_ARGS_FDFSFT     },
	{"pssub"      ,   INSTR_OPCODE_PSSUB      , INSN_ARGS_FDFSFT     },
	{"psmadd"     ,   INSTR_OPCODE_PSMADD     , INSN_ARGS_FDFSFT     },
	{"psmul"      ,   INSTR_OPCODE_PSMUL      , INSN_ARGS_FDFSFT     },
	{"psneg"      ,   INSTR_OPCODE_PSNEG      , INSN_ARGS_FDFS       },
	{"clear"      ,   INSTR_OPCODE_CLEAR      , INSN_ARGS_FDFS       },
	{"section"    ,   INSTR_OPCODE_RDSEC      , INSN_ARGS_FD         },
	{"li0"        ,   INSTR_OPCODE_LI0        , INSN_ARGS_FDIMM16    },
	{"li1"        ,   INSTR_OPCODE_LI1        , INSN_ARGS_FDIMM16    },
	{"li2"        ,   INSTR_OPCODE_LI2        , INSN_ARGS_FDIMM16    },
	{"li3"        ,   INSTR_OPCODE_LI3        , INSN_ARGS_FDIMM16    },
	{"lshift"     ,   INSTR_OPCODE_LSHIFT     , INSN_ARGS_FDFSFT     },
	{"lshifti"    ,   INSTR_OPCODE_LSHIFTI    , INSN_ARGS_FDFSIMM6   },
	{"ashift"     ,   INSTR_OPCODE_ASHIFT     , INSN_ARGS_FDFSFT     },
	{"ashifti"    ,   INSTR_OPCODE_ASHIFTI    , INSN_ARGS_FDFSIMM6   },
	{"pscopysign" ,   INSTR_OPCODE_PSCOPYSIGN , INSN_ARGS_FDFSFT     },
	{"split8"     ,   INSTR_OPCODE_SPLIT8     , INSN_ARGS_FDFS       },
	{"split16"    ,   INSTR_OPCODE_SPLIT16    , INSN_ARGS_FDFS       },
	{"split32"    ,   INSTR_OPCODE_SPLIT32    , INSN_ARGS_FDFS       },
	{"join8"      ,   INSTR_OPCODE_JOIN8      , INSN_ARGS_FDFS       },
	{"join16"     ,   INSTR_OPCODE_JOIN16     , INSN_ARGS_FDFS       },
	{"join32"     ,   INSTR_OPCODE_JOIN32     , INSN_ARGS_FDFS       },
	{"extsign8w"  ,   INSTR_OPCODE_EXTSIGN8W  , INSN_ARGS_FD         },
	{"extsign8h"  ,   INSTR_OPCODE_EXTSIGN8H  , INSN_ARGS_FD         },
	{"extsign16w" ,   INSTR_OPCODE_EXTSIGN16W , INSN_ARGS_FD         },
	{"add"        ,   INSTR_OPCODE_ADD        , INSN_ARGS_FDFSFT     },
	{"abs"        ,   INSTR_OPCODE_ABS        , INSN_ARGS_FDFS       },
	{"psabs"      ,   INSTR_OPCODE_PSABS      , INSN_ARGS_FDFS       },
	{"sub"        ,   INSTR_OPCODE_SUB        , INSN_ARGS_FDFSFT     },
	{"and"        ,   INSTR_OPCODE_AND        , INSN_ARGS_FDFSFT     },
	{"or"         ,   INSTR_OPCODE_OR         , INSN_ARGS_FDFSFT     },
	{"xor"        ,   INSTR_OPCODE_XOR        , INSN_ARGS_FDFSFT     },
	{"not"        ,   INSTR_OPCODE_NOT        , INSN_ARGS_FDFS       },
	{"neg"        ,   INSTR_OPCODE_NEG        , INSN_ARGS_FDFS       },
	{"addi"       ,   INSTR_OPCODE_ADDI       , INSN_ARGS_FDFSIMM12  },
	{"subi"       ,   INSTR_OPCODE_SUBI       , INSN_ARGS_FDFSIMM12  },
	{"inci"       ,   INSTR_OPCODE_INCI       , INSN_ARGS_FDIMM18    },
	{"deci"       ,   INSTR_OPCODE_DECI       , INSN_ARGS_FDIMM18    },
	{"c"          ,   INSTR_OPCODE_CCOND      , INSN_ARGS_CCOND      },
	{"cbutterfly" ,   INSTR_OPCODE_CFLY       , INSN_ARGS_FDFS       },
	{"cbutterfly2",   INSTR_OPCODE_CFLY2      , INSN_ARGS_FDFSFT     },
	{"mfc"        ,   INSTR_OPCODE_MFC        , INSN_ARGS_MFCMTC     },
	{"mtc"        ,   INSTR_OPCODE_MTC        , INSN_ARGS_MFCMTC     },
	{"copy"       ,   INSTR_OPCODE_COPY       , INSN_ARGS_FDFS       },
	{"pwtops"     ,   INSTR_OPCODE_PWTOPS     , INSN_ARGS_FDFS       },
	{"pstopw"     ,   INSTR_OPCODE_PSTOPW     , INSN_ARGS_FDFS       },
	{"sinc"       ,   INSTR_OPCODE_SINC       , INSN_ARGS_FDFS       },
	{"rrsin"      ,   INSTR_OPCODE_RRSIN      , INSN_ARGS_FDFS       },
	{"rrcos"      ,   INSTR_OPCODE_RRCOS      , INSN_ARGS_FDFS       },
	{"rrcossin"   ,   INSTR_OPCODE_RRCOSSIN   , INSN_ARGS_FDFS       },
	{"rrlog2"     ,   INSTR_OPCODE_RRLOG2     , INSN_ARGS_FDFSFQ     },
	{"recip"      ,   INSTR_OPCODE_RECIP      , INSN_ARGS_FDFS       },
	{"exp2"       ,   INSTR_OPCODE_EXP2       , INSN_ARGS_FDFS       },
	{"log2c"      ,   INSTR_OPCODE_LOG2C      , INSN_ARGS_FDFS       },
	{"atanc"      ,   INSTR_OPCODE_ATANC      , INSN_ARGS_FDFS       },
	{"rsqrt"      ,   INSTR_OPCODE_RSQRT      , INSN_ARGS_FDFS       },
	{"swaphl"     ,   INSTR_OPCODE_SWAPHL     , INSN_ARGS_FDFS       },
	{"swap64"     ,   INSTR_OPCODE_SWAP64     , INSN_ARGS_FDFS       },
	{"psgetexp"   ,   INSTR_OPCODE_PSGETEXP   , INSN_ARGS_FDFS       },
	{"psgetman"   ,   INSTR_OPCODE_PSGETMAN   , INSN_ARGS_FDFS       },
	{"psscale"    ,   INSTR_OPCODE_PSSCALE    , INSN_ARGS_FDFSFT     },
	{"getcoeff"   ,   INSTR_OPCODE_GETCOEFF   , INSN_ARGS_FD         },
	{"chmul"      ,   INSTR_OPCODE_CHMUL      , INSN_ARGS_FDFSFT     },
	{"chmadd"     ,   INSTR_OPCODE_CHMADD     , INSN_ARGS_FDFSFT     },
	{"chmsub"     ,   INSTR_OPCODE_CHMSUB     , INSN_ARGS_FDFSFT     },
	{"qsdot"      ,   INSTR_OPCODE_QSDOT      , INSN_ARGS_QSDOT      },
	{"unpck16wstops", INSTR_OPCODE_UNPCK16WSTOPS, INSN_ARGS_FDFSFT   },
	{ NULL, 0, 0 }
};


// instr lo
insntab_t insntab_lo[]={
	{"nop"        ,   INSTR_OPCODE2_NOP       ,   INSN_ARGS_NOARGS        },
	{"lwi"        ,   INSTR_OPCODE2_LWI       ,   INSN_ARGS_FT2IMM13      },
	{"ldi"        ,   INSTR_OPCODE2_LDI       ,   INSN_ARGS_FT2IMM13      },
	{"swi"        ,   INSTR_OPCODE2_SWI       ,   INSN_ARGS_FT2IMM13      },
	{"sdi"        ,   INSTR_OPCODE2_SDI       ,   INSN_ARGS_FT2IMM13      },
	{"lwnp"       ,   INSTR_OPCODE2_LWNP      ,   INSN_ARGS_FT2PRN        },
	{"ldnp"       ,   INSTR_OPCODE2_LDNP      ,   INSN_ARGS_FT2PRN        },
	{"swnp"       ,   INSTR_OPCODE2_SWNP      ,   INSN_ARGS_FT2PRN        },
	{"sdnp"       ,   INSTR_OPCODE2_SDNP      ,   INSN_ARGS_FT2PRN        },
	{"lwnm"       ,   INSTR_OPCODE2_LWNM      ,   INSN_ARGS_FT2MRN        },
	{"ldnm"       ,   INSTR_OPCODE2_LDNM      ,   INSN_ARGS_FT2MRN        },
	{"swnm"       ,   INSTR_OPCODE2_SWNM      ,   INSN_ARGS_FT2MRN        },
	{"sdnm"       ,   INSTR_OPCODE2_SDNM      ,   INSN_ARGS_FT2MRN        },
	{"lwo"        ,   INSTR_OPCODE2_LWO       ,   INSN_ARGS_FT2RNOFFS13   },
	{"ldo"        ,   INSTR_OPCODE2_LDO       ,   INSN_ARGS_FT2RNOFFS13   },
	{"swo"        ,   INSTR_OPCODE2_SWO       ,   INSN_ARGS_FT2RNOFFS13   },
	{"sdo"        ,   INSTR_OPCODE2_SDO       ,   INSN_ARGS_FT2RNOFFS13   },
	{"updaddr"    ,   INSTR_OPCODE2_UPDADDR   ,   INSN_ARGS_RNOFFS13      },
	{"updaddrnp"  ,   INSTR_OPCODE2_UPDADDRNP ,   INSN_ARGS_RN            },
	{"updaddrnm"  ,   INSTR_OPCODE2_UPDADDRNM ,   INSN_ARGS_RN            },
	{"sync"       ,   INSTR_OPCODE2_SYNC      ,   INSN_ARGS_NOARGS        },
	{"run"        ,   INSTR_OPCODE2_RUN       ,   INSN_ARGS_GS            },
	{"runi"       ,   INSTR_OPCODE2_RUNI      ,   INSN_ARGS_IMM13         },
	{"stop"       ,   INSTR_OPCODE2_STOP      ,   INSN_ARGS_GS            },
	{"stopi"      ,   INSTR_OPCODE2_STOPI     ,   INSN_ARGS_IMM13         },
	{"enddo"      ,   INSTR_OPCODE2_ENDDO     ,   INSN_ARGS_IMM13         },
	{"doi"        ,   INSTR_OPCODE2_DOI       ,   INSN_ARGS_CNT10IMM13    },
	{"do"         ,   INSTR_OPCODE2_DO        ,   INSN_ARGS_GSIMM13       },
	{"seti"       ,   INSTR_OPCODE2_SETI      ,   INSN_ARGS_SETI          },
	{"move"       ,   INSTR_OPCODE2_MOVE      ,   INSN_ARGS_MOVE          },
	{"mtfpr"      ,   INSTR_OPCODE2_MTFPR     ,   INSN_ARGS_MTFPR         },
	{"mffpr"      ,   INSTR_OPCODE2_MFFPR     ,   INSN_ARGS_MFFPR         },
	{"clr"        ,   INSTR_OPCODE2_CLR       ,   INSN_ARGS_RN            },
	{"jump"       ,   INSTR_OPCODE2_JUMP      ,   INSN_ARGS_RN            },
	{"jumpi"      ,   INSTR_OPCODE2_JUMPI     ,   INSN_ARGS_IMM13         },
	{"call"       ,   INSTR_OPCODE2_CALL      ,   INSN_ARGS_RN            },
	{"calli"      ,   INSTR_OPCODE2_CALLI     ,   INSN_ARGS_IMM13         },
	{"ret"        ,   INSTR_OPCODE2_RET       ,   INSN_ARGS_NOARGS        },
	{"start_dma"  ,   INSTR_OPCODE2_START_DMA ,   INSN_ARGS_NOARGS        },
	{"check_dma"  ,   INSTR_OPCODE2_CHECK_DMA ,   INSN_ARGS_GT            },
	{"psprmsgn0"  ,   INSTR_OPCODE2_PSPRMSGN0 ,   INSN_ARGS_PSPRMSGN0     },
	{"psprmsgn1"  ,   INSTR_OPCODE2_PSPRMSGN1 ,   INSN_ARGS_PSPRMSGN1     },
{ NULL, 0, 0}
};



// find instruction opcode by its name
// returns: TRUE if ok, FALSE if error
int insntab_get_opcode_by_name (insntab_t *table, uint64_t *opcode, char *name) {

	int i;

	// get table index
	i = insntab_get_index_by_name (table, name);

	if (table[i].name != NULL) {
		// successfully found
		*opcode = table[i].opcode;
		return TRUE;
	} else {
		return FALSE;
	}

}



// returns index of insn table entry with given name,
// or index of last (NULL) entry if there is no such name
int    insntab_get_index_by_name (insntab_t *table, char *name) {

	int i;

	for (i=0; table[i].name != NULL; i++) {
		if (strcmp(table[i].name,name) == 0) break;
	}

	return i;
}



// returns index of insn table entry with given opcode
// or index of last (NULL) entry if there is no such opcode
int    insntab_get_index_by_opcode (insntab_t *table, uint64_t opcode) {

	int i;

	for (i=0; table[i].name != NULL; i++) {
		if (table[i].opcode == opcode) break;
	}

	return i;
}



// print disassembled instruction to string
void  disasm (char *str, instr_t instr)
{
	sprintf         (str, " ");
	disasm_cc       (str+strlen(str), instr);
	sprintf         (str+strlen(str), " ");
	disasm_subinstr (str+strlen(str), insntab_hi, instr, instr_opcode(instr));
	sprintf         (str+strlen(str), " ");
	disasm_subinstr (str+strlen(str), insntab_lo, instr, instr_opcode2(instr));
}


// print disassembled 'cc' field of instruction
void  disasm_cc (char *str, instr_t instr) {

	int cc;

	// get 'cc' field
	cc = instr_cc (instr);

	// print 'if(cc)' if nonzero
	if (cc != 0) {
		sprintf (str, "if(%d)", cc);
	} else {
		sprintf (str, "     ");
	}
}


// print disassembled subinstruction to string
void  disasm_subinstr (char *str, insntab_t table[], instr_t instr, uint64_t opcode) {

	int i;

	// get index in opcodes table
	i = insntab_get_index_by_opcode (table, opcode);
	if (table[i].name == NULL) {
		// unknown instruction
		sprintf(str," ???");
		return;
	}

	// print instruction name
	strcpy(str, table[i].name);

	// print arguments
	disasm_subinstr_args (str+strlen(str), instr, table[i].args);
}



// print arguments of float instruction to string
void disasm_subinstr_args (char *str, instr_t instr, insn_arg_t arg_type) {

	// for 'c.cond.fmt' instruction
	static const char *ccond_cond_str[] = {"t", "un", "lt", "le", "eq", "ne", "gt", "ge"};
	static const char *ccond_fmt_str[]  = {"s", "w", "ps", "pw"};
	// for 'move' instruction
	static const char *move_regtype_str[] = {"g", "?", "ctrl", "a", "n", "m", "i", "?" };
	// for 'seti' instruction
	static const char *seti_regtype_str[] = {"g", "a", "n", "m"};
	// for 'mfc','mtc' instructions
	static const char *mfcmtc_cntrlreg_str[] = {"fcsr", "fccr"};

#define print_reg(_c,_regno)  sprintf(str+strlen(str)," %c%02d",_c,(int)(_regno));
#define print_immed(_immed)   sprintf(str+strlen(str)," 0x%x", (int)(_immed));
#define print_comma           sprintf(str+strlen(str),",");
#define print_openpar         sprintf(str+strlen(str),"(");
#define print_closepar        sprintf(str+strlen(str),")");
	switch (arg_type) {
		case INSN_ARGS_NOARGS:
			break;
		case INSN_ARGS_FD:
			print_reg('f',instr_fd(instr));
			break;
		case INSN_ARGS_FDFS:
			print_reg('f',instr_fd(instr)); print_comma;
			print_reg('f',instr_fs(instr));
			break;
		case INSN_ARGS_FDFSFT:
			sprintf(str+strlen(str),
				" f%02d,f%02d,f%02d",
				(int) instr_fd(instr),
				(int) instr_fs(instr),
				(int) instr_ft(instr)
			);
			break;
		case INSN_ARGS_FDFSFQ:
			sprintf(str+strlen(str),
				" f%02d,f%02d,f%02d",
				(int) instr_fd(instr),
				(int) instr_fs(instr),
				(int) instr_fq(instr)
			);
			break;
		case INSN_ARGS_FDFSFTFQ:
			sprintf(str+strlen(str),
				" f%02d,f%02d,f%02d,f%02d",
				(int) instr_fd(instr),
				(int) instr_fs(instr),
				(int) instr_ft(instr),
				(int) instr_fq(instr)
			);
			break;
		case INSN_ARGS_FT2IMM13:
			print_reg('f',instr_ft2(instr));      print_comma;
			print_immed(instr_2imm13(instr));
			break;
		case INSN_ARGS_CNT10IMM13:
			print_immed(instr_cnt10(instr)); print_comma;
			print_immed(instr_2imm13(instr));
			break;
		case INSN_ARGS_GS:
			print_reg('g',instr_gs(instr));
			break;
		case INSN_ARGS_GSIMM13:
			print_reg('g',instr_gs(instr));     print_comma;
			print_immed(instr_2imm13(instr));
			break;
		case INSN_ARGS_IMM13:
			print_immed(instr_2imm13(instr));
			break;
		case INSN_ARGS_RN:
			print_reg('r',instr_rn(instr));
			break;
		case INSN_ARGS_FT2RNOFFS13:
			sprintf(str+strlen(str),
				" f%02d,(r%02d)%+d",
				(int) instr_ft2(instr),
				(int) instr_rn(instr),
				(int) sign_ext32(instr_offset13(instr),12)
			);
			break;
		case INSN_ARGS_RNOFFS13:
			sprintf(str+strlen(str),
				" (r%02d)%+d",
				(int) instr_rn(instr),
				(int) instr_offset13_sigext32(instr)
			);
			break;
		case INSN_ARGS_FDIMM16:
			sprintf(str+strlen(str),
				" f%02d, 0x%x",
				(int) instr_fd(instr),
				(int) instr_imm16(instr)
			);
			break;
		case INSN_ARGS_FDIMM18:
			sprintf(str+strlen(str),
				" f%02d, %d",
				(int) instr_fd(instr),
				(int) instr_imm18_sigext32(instr)
			);
			break;
		case INSN_ARGS_FDFSIMM6:
			sprintf(str+strlen(str),
				" f%02d, f%02d, %d",
				(int) instr_fd(instr),
				(int) instr_fs(instr),
				(int) instr_imm6_sigext32(instr)
			);
			break;
		case INSN_ARGS_FDFSIMM12:
			sprintf(str+strlen(str),
				" f%02d, f%02d, %d",
				(int) instr_fd(instr),
				(int) instr_fs(instr),
				(int) instr_imm12_sigext32(instr)
			);
			break;
		case INSN_ARGS_CCOND:
			sprintf(str+strlen(str),
				".%s.%s %d, f%02d, f%02d",
				ccond_cond_str[instr_cond(instr)],
				ccond_fmt_str[instr_fmt(instr)],
				(int) instr_cc1(instr),
				(int) instr_fs(instr),
				(int) instr_ft(instr)
			);
			break;
		case INSN_ARGS_SETI:
			sprintf(str+strlen(str),
				" %s%02d, 0x%04x",
				seti_regtype_str[instr_regtype(instr)],
				(int) instr_rn2(instr),
				(int) instr_2imm16_sigext64(instr)
			);
			break;
		case INSN_ARGS_MOVE:
			sprintf(str+strlen(str),
				" %s%02d, %s%02d, %d",
				move_regtype_str[instr_dtyp(instr)],
				(int) instr_dreg(instr),
				move_regtype_str[instr_styp(instr)],
				(int) instr_sreg(instr),
				(int) instr_secn(instr)
			);
			break;
		case INSN_ARGS_MFCMTC:
			sprintf(str+strlen(str),
				" f%02d, %s",
				(int) instr_fd(instr),
				mfcmtc_cntrlreg_str[instr_cntrlreg(instr)]
			);
			break;
		case INSN_ARGS_MTFPR:
// alex
			sprintf(str+strlen(str),
				" f%02d, g%02d, sec%d",
				(int) instr_2ft(instr),
				(int) instr_gr(instr),
				(int) instr_secn(instr)
			);
			break;
		case INSN_ARGS_MFFPR:
// alex
			sprintf(str+strlen(str),
				" g%02d, f%02d, sec%d",
				(int) instr_gr(instr),
				(int) instr_2ft(instr),
				(int) instr_secn(instr)
			);
			break;
		case INSN_ARGS_QSDOT:
			sprintf(str+strlen(str),
				" f%02d,f%02d,f%02d,%01d",
				(int) instr_fd(instr),
				(int) instr_fs(instr),
				(int) instr_ft(instr),
				(int) instr_mode(instr)
			);
			break;
		case INSN_ARGS_PSPRMSGN0:
			sprintf(str+strlen(str),
				" f%02d,f%02d,%1d,%1d,%1d,%1d,%1d,%1d,%1d,%1d",
				(int) instr_fd2(instr),
				(int) instr_ft2(instr),
				(int) instr_n4(instr),
				(int) instr_n3(instr),
				(int) instr_n2(instr),
				(int) instr_n1(instr),
				(int) instr_sel4(instr),
				(int) instr_sel3(instr),
				(int) instr_sel2(instr),
				(int) instr_sel1(instr)
			);
			break;
		case INSN_ARGS_PSPRMSGN1:
			sprintf(str+strlen(str),
				" f%02d,f%02d,%1d,%1d,%1d,%1d,%1d,%1d,%1d,%1d",
				(int) instr_fd2(instr)+32,
				(int) instr_ft2(instr),
				(int) instr_n4(instr),
				(int) instr_n3(instr),
				(int) instr_n2(instr),
				(int) instr_n1(instr),
				(int) instr_sel4(instr),
				(int) instr_sel3(instr),
				(int) instr_sel2(instr),
				(int) instr_sel1(instr)
			);
			break;
        case INSN_ARGS_FT2MRN:
        case INSN_ARGS_FT2PRN:
            sprintf(str+strlen(str),
                " f%02d, r%02d",
                (int) instr_ft2(instr),
                (int) instr_rn(instr)
            );
            break;
		case INSN_ARGS_GT:
            sprintf(str+strlen(str),
                " g%02d",
                (int)instr_gt(instr)
            );
            break;
		default:
			sprintf(str+strlen(str),"...");
			break;
	}

#undef print_reg
#undef print_immed
#undef print_comma
#undef print_openpar
#undef print_closepar
}




