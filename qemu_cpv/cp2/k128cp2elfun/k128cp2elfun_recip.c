/**
 * \file src/k128cp2elfun_recip.c
 * \brief Вычисление обратной величины 1/x
 * \author Andrey Lesnykh <lesnykh@niisi.ras.ru>
 * \date 2007
 *
 * Вычисление обратной величины 1/x
 */

#include <math.h>
#include <assert.h>
#include "local.h"
#include "softfloat.h"

extern float_status f_status;

// Elementary function calculation: 1/x (recip)

// function prototypes
float k128cp2elfun_common (k128cp2elfun_params_t *func_params, float x);

// 1 / x
// quadratic polinom coeffs table
static k128cp2elfun_table_t k128cp2elfun_recip_coefstable[] = {
/*   0 */ { 0x000003f5, 0xffff0002, 0x03fffffe },
/*   1 */ { 0x000003dd, 0xffff03f6, 0x03f80fe0 },
/*   2 */ { 0x000003c6, 0xffff07d3, 0x03f03f04 },
/*   3 */ { 0x000003b1, 0xffff0b99, 0x03e88cb3 },
/*   4 */ { 0x0000039b, 0xffff0f4a, 0x03e0f83c },
/*   5 */ { 0x00000386, 0xffff12e5, 0x03d980f6 },
/*   6 */ { 0x00000373, 0xffff166b, 0x03d22636 },
/*   7 */ { 0x0000035f, 0xffff19de, 0x03cae758 },
/*   8 */ { 0x0000034d, 0xffff1d3d, 0x03c3c3c2 },
/*   9 */ { 0x0000033b, 0xffff2089, 0x03bcbadc },
/*  10 */ { 0x00000329, 0xffff23c3, 0x03b5cc0f },
/*  11 */ { 0x00000317, 0xffff26ec, 0x03aef6c9 },
/*  12 */ { 0x00000306, 0xffff2a03, 0x03a83a83 },
/*  13 */ { 0x000002f6, 0xffff2d09, 0x03a196b2 },
/*  14 */ { 0x000002e6, 0xffff2fff, 0x039b0ad1 },
/*  15 */ { 0x000002d7, 0xffff32e5, 0x03949660 },
/*  16 */ { 0x000002c7, 0xffff35bc, 0x038e38e3 },
/*  17 */ { 0x000002b8, 0xffff3884, 0x0387f1e0 },
/*  18 */ { 0x000002aa, 0xffff3b3d, 0x0381c0e0 },
/*  19 */ { 0x0000029d, 0xffff3de8, 0x037ba570 },
/*  20 */ { 0x00000290, 0xffff4085, 0x03759f21 },
/*  21 */ { 0x00000282, 0xffff4315, 0x036fad87 },
/*  22 */ { 0x00000277, 0xffff4597, 0x0369d035 },
/*  23 */ { 0x0000026a, 0xffff480d, 0x036406c8 },
/*  24 */ { 0x0000025d, 0xffff4a77, 0x035e50d7 },
/*  25 */ { 0x00000252, 0xffff4cd4, 0x0358ae04 },
/*  26 */ { 0x00000246, 0xffff4f26, 0x03531dec },
/*  27 */ { 0x0000023c, 0xffff516c, 0x034da034 },
/*  28 */ { 0x00000231, 0xffff53a7, 0x03483484 },
/*  29 */ { 0x00000225, 0xffff55d8, 0x0342da7f },
/*  30 */ { 0x0000021c, 0xffff57fd, 0x033d91d3 },
/*  31 */ { 0x00000211, 0xffff5a19, 0x03385a29 },
/*  32 */ { 0x00000207, 0xffff5c2a, 0x03333333 },
/*  33 */ { 0x000001fe, 0xffff5e31, 0x032e1c9f },
/*  34 */ { 0x000001f5, 0xffff602f, 0x0329161e },
/*  35 */ { 0x000001ea, 0xffff6224, 0x03241f69 },
/*  36 */ { 0x000001e2, 0xffff640f, 0x031f3831 },
/*  37 */ { 0x000001da, 0xffff65f1, 0x031a6032 },
/*  38 */ { 0x000001d1, 0xffff67cb, 0x03159722 },
/*  39 */ { 0x000001c9, 0xffff699c, 0x0310dcbf },
/*  40 */ { 0x000001c1, 0xffff6b65, 0x030c30c3 },
/*  41 */ { 0x000001b9, 0xffff6d26, 0x030792ef },
/*  42 */ { 0x000001b1, 0xffff6edf, 0x03030303 },
/*  43 */ { 0x000001aa, 0xffff7090, 0x02fe80c0 },
/*  44 */ { 0x000001a2, 0xffff723a, 0x02fa0be8 },
/*  45 */ { 0x0000019c, 0xffff73dc, 0x02f5a441 },
/*  46 */ { 0x00000193, 0xffff7578, 0x02f14990 },
/*  47 */ { 0x0000018d, 0xffff770c, 0x02ecfb9c },
/*  48 */ { 0x00000186, 0xffff7899, 0x02e8ba2f },
/*  49 */ { 0x0000017f, 0xffff7a20, 0x02e4850f },
/*  50 */ { 0x00000179, 0xffff7ba0, 0x02e05c0a },
/*  51 */ { 0x00000173, 0xffff7d19, 0x02dc3eee },
/*  52 */ { 0x0000016e, 0xffff7e8c, 0x02d82d83 },
/*  53 */ { 0x00000166, 0xffff7ffa, 0x02d4279a },
/*  54 */ { 0x00000161, 0xffff8161, 0x02d02d02 },
/*  55 */ { 0x0000015c, 0xffff82c2, 0x02cc3d8c },
/*  56 */ { 0x00000155, 0xffff841e, 0x02c8590b },
/*  57 */ { 0x00000151, 0xffff8573, 0x02c47f50 },
/*  58 */ { 0x0000014b, 0xffff86c4, 0x02c0b02c },
/*  59 */ { 0x00000146, 0xffff880f, 0x02bceb76 },
/*  60 */ { 0x00000140, 0xffff8955, 0x02b93105 },
/*  61 */ { 0x0000013c, 0xffff8a95, 0x02b580ae },
/*  62 */ { 0x00000136, 0xffff8bd1, 0x02b1da47 },
/*  63 */ { 0x00000131, 0xffff8d08, 0x02ae3da7 },
/*  64 */ { 0x0000012e, 0xffff8e39, 0x02aaaaaa },
/*  65 */ { 0x00000129, 0xffff8f66, 0x02a7212a },
/*  66 */ { 0x00000123, 0xffff908f, 0x02a3a0fe },
/*  67 */ { 0x0000011f, 0xffff91b3, 0x02a02a02 },
/*  68 */ { 0x0000011b, 0xffff92d2, 0x029cbc15 },
/*  69 */ { 0x00000117, 0xffff93ed, 0x02995710 },
/*  70 */ { 0x00000112, 0xffff9504, 0x0295fad4 },
/*  71 */ { 0x0000010f, 0xffff9616, 0x0292a73d },
/*  72 */ { 0x0000010a, 0xffff9725, 0x028f5c29 },
/*  73 */ { 0x00000107, 0xffff982f, 0x028c1979 },
/*  74 */ { 0x00000102, 0xffff9936, 0x0288df0c },
/*  75 */ { 0x00000100, 0xffff9a38, 0x0285acc4 },
/*  76 */ { 0x000000fb, 0xffff9b37, 0x02828283 },
/*  77 */ { 0x000000f8, 0xffff9c32, 0x027f6028 },
/*  78 */ { 0x000000f3, 0xffff9d2a, 0x027c4598 },
/*  79 */ { 0x000000f0, 0xffff9e1e, 0x027932b4 },
/*  80 */ { 0x000000ed, 0xffff9f0e, 0x02762762 },
/*  81 */ { 0x000000e9, 0xffff9ffb, 0x02732386 },
/*  82 */ { 0x000000e7, 0xffffa0e4, 0x02702703 },
/*  83 */ { 0x000000e2, 0xffffa1cb, 0x026d31bf },
/*  84 */ { 0x000000df, 0xffffa2ae, 0x026a439f },
/*  85 */ { 0x000000de, 0xffffa38d, 0x02675c8b },
/*  86 */ { 0x000000da, 0xffffa46a, 0x02647c69 },
/*  87 */ { 0x000000d6, 0xffffa544, 0x0261a320 },
/*  88 */ { 0x000000d4, 0xffffa61a, 0x025ed098 },
/*  89 */ { 0x000000d0, 0xffffa6ee, 0x025c04b9 },
/*  90 */ { 0x000000cd, 0xffffa7bf, 0x02593f69 },
/*  91 */ { 0x000000cc, 0xffffa88c, 0x02568096 },
/*  92 */ { 0x000000c7, 0xffffa958, 0x0253c825 },
/*  93 */ { 0x000000c5, 0xffffaa20, 0x02511602 },
/*  94 */ { 0x000000c4, 0xffffaae5, 0x024e6a17 },
/*  95 */ { 0x000000c1, 0xffffaba8, 0x024bc44e },
/*  96 */ { 0x000000bd, 0xffffac69, 0x02492492 },
/*  97 */ { 0x000000ba, 0xffffad27, 0x02468acf },
/*  98 */ { 0x000000b8, 0xffffade2, 0x0243f6f1 },
/*  99 */ { 0x000000b6, 0xffffae9b, 0x024168e1 },
/* 100 */ { 0x000000b4, 0xffffaf51, 0x023ee090 },
/* 101 */ { 0x000000b2, 0xffffb005, 0x023c5de7 },
/* 102 */ { 0x000000af, 0xffffb0b7, 0x0239e0d5 },
/* 103 */ { 0x000000ad, 0xffffb166, 0x02376948 },
/* 104 */ { 0x000000ab, 0xffffb213, 0x0234f72c },
/* 105 */ { 0x000000a9, 0xffffb2be, 0x02328a6f },
/* 106 */ { 0x000000a6, 0xffffb367, 0x02302302 },
/* 107 */ { 0x000000a5, 0xffffb40d, 0x022dc0d1 },
/* 108 */ { 0x000000a1, 0xffffb4b2, 0x022b63cc },
/* 109 */ { 0x000000a0, 0xffffb554, 0x02290be2 },
/* 110 */ { 0x0000009f, 0xffffb5f4, 0x0226b902 },
/* 111 */ { 0x0000009b, 0xffffb693, 0x02246b1d },
/* 112 */ { 0x0000009a, 0xffffb72f, 0x02222222 },
/* 113 */ { 0x00000099, 0xffffb7c9, 0x021fde02 },
/* 114 */ { 0x00000096, 0xffffb862, 0x021d9ead },
/* 115 */ { 0x00000095, 0xffffb8f8, 0x021b6416 },
/* 116 */ { 0x00000093, 0xffffb98d, 0x02192e2a },
/* 117 */ { 0x00000091, 0xffffba20, 0x0216fcdd },
/* 118 */ { 0x0000008f, 0xffffbab1, 0x0214d022 },
/* 119 */ { 0x0000008f, 0xffffbb40, 0x0212a7e6 },
/* 120 */ { 0x0000008c, 0xffffbbce, 0x02108421 },
/* 121 */ { 0x0000008a, 0xffffbc5a, 0x020e64c2 },
/* 122 */ { 0x00000089, 0xffffbce4, 0x020c49bb },
/* 123 */ { 0x00000087, 0xffffbd6d, 0x020a32ff },
/* 124 */ { 0x00000085, 0xffffbdf4, 0x02082082 },
/* 125 */ { 0x00000084, 0xffffbe79, 0x02061237 },
/* 126 */ { 0x00000083, 0xffffbefd, 0x0204080f },
/* 127 */ { 0x00000082, 0xffffbf7f, 0x02020201 }
};

//
static k128cp2elfun_params_t k128cp2elfun_recip_params = {
	"1 / x",                           // function name
	k128cp2elfun_recip_coefstable,     // pointer to quadratic polinom coeffs table
	{ .f = 1.0 },                      // minimum argument value
	{ .f = 2.0 },                      // maximum argument value
	{ .f = 1.0 },                      // function value for small (i.e. x1=x2=0) arguments
	127,                               // minimum exponent value
	127                                // maximum exponent value
};


//
float k128cp2elfun_core_recip (float x) {

	k128cp2elfun_single_t arg, rarg, res;
	int etmp;

	if(x == 1.0)
		return 1.0;

	// prepare for quadratic approximation
	// reduce argument to interval [1.0, 2.0)
	rarg.f = x;
	rarg.fields.s = 0;
	rarg.fields.e = 127;

	// call common function for quadratic approximation
	res.f = k128cp2elfun_common (&k128cp2elfun_recip_params, rarg.f);

	// restore sign and exponent
	arg.f = x;
	res.fields.s = arg.fields.s;
	etmp = 127 + (res.fields.e - 127) - (arg.fields.e - 127);
	assert (etmp > 0);
	assert (etmp < 255);
	res.fields.e = etmp;

	// return result
	return res.f;
}

/**
 * \brief Вычисление обратной величины 1/x
 * \param x Значение аргумента
 * \return Обратная величина
 */
float k128cp2elfun_recip (float x) {

	k128cp2elfun_single_t arg, res;

	switch (fpclassify (x)) {
		case FP_NAN       :  res.f = NAN;      break;
		case FP_INFINITE  :  res.f = 0.0;      break;
		case FP_ZERO      :  res.f = INFINITY; f_status.float_exception_flags |= float_flag_divbyzero; break;
		case FP_SUBNORMAL :  res.f = INFINITY; f_status.float_exception_flags |= float_flag_divbyzero; break;
		case FP_NORMAL    :
			arg.f = x;
			if ((1 <= arg.fields.e) && (arg.fields.e <= 252)) {
				// call core function
				res.f = k128cp2elfun_core_recip (x);
			} else {
				res.f = 0.0;
				f_status.float_exception_flags |= float_flag_underflow | float_flag_inexact;
			}
			break;
		default: /* internal error */ assert(0==1); break;
	}

	// restore sign
	arg.f = x;
	res.fields.s = arg.fields.s;

	return res.f;
}


/**
 * \brief Вычисление эталонной обратной величины 1/x
 * \param x Значение аргумента
 * \return Обратная величина 1/x,
 *   вычисленная с помощью standard C math library.
 */
float k128cp2elfun_recip_gold (float x) {

	k128cp2elfun_single_t arg, res;

	switch (fpclassify (x)) {
		case FP_NAN       :  res.f = NAN;      break;
		case FP_INFINITE  :  res.f = 0.0;      break;
		case FP_ZERO      :  res.f = INFINITY; break;
		case FP_SUBNORMAL :  res.f = INFINITY; break;
		case FP_NORMAL    :
			arg.f = x;
			if ((1 <= arg.fields.e) && (arg.fields.e <= 252)) {
				res.f = 1.0 / x;
			} else {
				res.f = 0.0;
			}
			break;
		default: /* internal error */ assert(0==1); break;
	}

	// restore sign
	arg.f = x;
	res.fields.s = arg.fields.s;

	return res.f;
}


