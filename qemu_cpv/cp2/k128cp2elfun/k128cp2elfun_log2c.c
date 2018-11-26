/**
 * \file src/k128cp2elfun_log2c.c
 * \brief Вычисление кардинального значения логарифма
 * \author Andrey Lesnykh <lesnykh@niisi.ras.ru>
 * \date 2007
 *
 * Вычисление значения функции
 * log2c(x) = log2(x) / (x-1)
 */

#include <math.h>
#include <assert.h>
#include "local.h"

// Elementary function calculation: log2c
// log2 (x) / (x-1)

// function prototypes
float k128cp2elfun_common (k128cp2elfun_params_t *func_params, float x);

// (log2 x) / (x-1)
// quadratic polinom coeffs table
static k128cp2elfun_table_t k128cp2elfun_log2c_coefstable[] = {
	/*   0 */ { 0x000001e7, 0xffff4757, 0x05c551d9 },
	/*   1 */ { 0x000001df, 0xffff493f, 0x05bf942d },
	/*   2 */ { 0x000001d8, 0xffff4b1e, 0x05b9e5a2 },
	/*   3 */ { 0x000001cf, 0xffff4cf6, 0x05b445f1 },
	/*   4 */ { 0x000001c8, 0xffff4ec5, 0x05aeb4dd },
	/*   5 */ { 0x000001c0, 0xffff508d, 0x05a93224 },
	/*   6 */ { 0x000001b8, 0xffff524d, 0x05a3bd8a },
	/*   7 */ { 0x000001b1, 0xffff5405, 0x059e56d2 },
	/*   8 */ { 0x000001ab, 0xffff55b6, 0x0598fdbe },
	/*   9 */ { 0x000001a4, 0xffff5760, 0x0593b218 },
	/*  10 */ { 0x0000019d, 0xffff5903, 0x058e73a6 },
	/*  11 */ { 0x00000195, 0xffff5aa0, 0x05894231 },
	/*  12 */ { 0x0000018f, 0xffff5c35, 0x05841d85 },
	/*  13 */ { 0x00000189, 0xffff5dc4, 0x057f056a },
	/*  14 */ { 0x00000183, 0xffff5f4d, 0x0579f9ad },
	/*  15 */ { 0x0000017c, 0xffff60d0, 0x0574fa1f },
	/*  16 */ { 0x00000177, 0xffff624c, 0x0570068e },
	/*  17 */ { 0x00000170, 0xffff63c3, 0x056b1ec8 },
	/*  18 */ { 0x0000016c, 0xffff6533, 0x0566429f },
	/*  19 */ { 0x00000166, 0xffff669e, 0x056171e7 },
	/*  20 */ { 0x00000160, 0xffff6804, 0x055cac6f },
	/*  21 */ { 0x0000015b, 0xffff6964, 0x0557f20e },
	/*  22 */ { 0x00000155, 0xffff6abf, 0x05534299 },
	/*  23 */ { 0x00000151, 0xffff6c14, 0x054e9de6 },
	/*  24 */ { 0x0000014b, 0xffff6d65, 0x054a03c9 },
	/*  25 */ { 0x00000147, 0xffff6eb0, 0x0545741e },
	/*  26 */ { 0x00000142, 0xffff6ff7, 0x0540eeb8 },
	/*  27 */ { 0x0000013c, 0xffff7139, 0x053c7378 },
	/*  28 */ { 0x00000138, 0xffff7276, 0x05380231 },
	/*  29 */ { 0x00000135, 0xffff73ae, 0x05339ac1 },
	/*  30 */ { 0x00000130, 0xffff74e2, 0x052f3d05 },
	/*  31 */ { 0x0000012b, 0xffff7612, 0x052ae8d6 },
	/*  32 */ { 0x00000128, 0xffff773d, 0x05269e13 },
	/*  33 */ { 0x00000122, 0xffff7865, 0x05225c9a },
	/*  34 */ { 0x00000120, 0xffff7987, 0x051e244a },
	/*  35 */ { 0x0000011c, 0xffff7aa6, 0x0519f500 },
	/*  36 */ { 0x00000118, 0xffff7bc1, 0x0515ce9f },
	/*  37 */ { 0x00000114, 0xffff7cd8, 0x0511b105 },
	/*  38 */ { 0x0000010e, 0xffff7dec, 0x050d9c15 },
	/*  39 */ { 0x0000010c, 0xffff7efb, 0x05098fae },
	/*  40 */ { 0x00000108, 0xffff8007, 0x05058bb4 },
	/*  41 */ { 0x00000104, 0xffff810f, 0x0501900c },
	/*  42 */ { 0x00000102, 0xffff8213, 0x04fd9c95 },
	/*  43 */ { 0x000000fd, 0xffff8315, 0x04f9b134 },
	/*  44 */ { 0x000000fb, 0xffff8412, 0x04f5cdd0 },
	/*  45 */ { 0x000000f6, 0xffff850d, 0x04f1f24c },
	/*  46 */ { 0x000000f3, 0xffff8604, 0x04ee1e8e },
	/*  47 */ { 0x000000f0, 0xffff86f8, 0x04ea527a },
	/*  48 */ { 0x000000ee, 0xffff87e8, 0x04e68dfb },
	/*  49 */ { 0x000000ea, 0xffff88d6, 0x04e2d0f4 },
	/*  50 */ { 0x000000e9, 0xffff89c0, 0x04df1b4c },
	/*  51 */ { 0x000000e5, 0xffff8aa8, 0x04db6cee },
	/*  52 */ { 0x000000e1, 0xffff8b8d, 0x04d7c5c1 },
	/*  53 */ { 0x000000e0, 0xffff8c6e, 0x04d425ac },
	/*  54 */ { 0x000000dd, 0xffff8d4d, 0x04d08c9a },
	/*  55 */ { 0x000000da, 0xffff8e29, 0x04ccfa75 },
	/*  56 */ { 0x000000d6, 0xffff8f03, 0x04c96f24 },
	/*  57 */ { 0x000000d5, 0xffff8fd9, 0x04c5ea94 },
	/*  58 */ { 0x000000d2, 0xffff90ad, 0x04c26caf },
	/*  59 */ { 0x000000ce, 0xffff917f, 0x04bef560 },
	/*  60 */ { 0x000000cc, 0xffff924e, 0x04bb8490 },
	/*  61 */ { 0x000000ca, 0xffff931a, 0x04b81a2f },
	/*  62 */ { 0x000000c7, 0xffff93e4, 0x04b4b626 },
	/*  63 */ { 0x000000c6, 0xffff94ab, 0x04b15863 },
	/*  64 */ { 0x000000c4, 0xffff9570, 0x04ae00d1 },
	/*  65 */ { 0x000000c1, 0xffff9633, 0x04aaaf5f },
	/*  66 */ { 0x000000bd, 0xffff96f4, 0x04a763fb },
	/*  67 */ { 0x000000bb, 0xffff97b2, 0x04a41e91 },
	/*  68 */ { 0x000000b9, 0xffff986e, 0x04a0df0e },
	/*  69 */ { 0x000000b8, 0xffff9927, 0x049da563 },
	/*  70 */ { 0x000000b6, 0xffff99df, 0x049a717b },
	/*  71 */ { 0x000000b2, 0xffff9a95, 0x0497434a },
	/*  72 */ { 0x000000b1, 0xffff9b48, 0x04941abc },
	/*  73 */ { 0x000000b0, 0xffff9bf9, 0x0490f7bf },
	/*  74 */ { 0x000000ac, 0xffff9ca9, 0x048dda47 },
	/*  75 */ { 0x000000ab, 0xffff9d56, 0x048ac240 },
	/*  76 */ { 0x000000aa, 0xffff9e01, 0x0487af9c },
	/*  77 */ { 0x000000a7, 0xffff9eab, 0x0484a24b },
	/*  78 */ { 0x000000a6, 0xffff9f52, 0x04819a40 },
	/*  79 */ { 0x000000a4, 0xffff9ff8, 0x047e9767 },
	/*  80 */ { 0x000000a2, 0xffffa09c, 0x047b99b5 },
	/*  81 */ { 0x000000a0, 0xffffa13e, 0x0478a11c },
	/*  82 */ { 0x0000009f, 0xffffa1de, 0x0475ad8b },
	/*  83 */ { 0x0000009c, 0xffffa27d, 0x0472bef5 },
	/*  84 */ { 0x0000009c, 0xffffa319, 0x046fd54d },
	/*  85 */ { 0x0000009a, 0xffffa3b4, 0x046cf085 },
	/*  86 */ { 0x00000098, 0xffffa44e, 0x046a108d },
	/*  87 */ { 0x00000095, 0xffffa4e6, 0x0467355c },
	/*  88 */ { 0x00000094, 0xffffa57c, 0x04645ee1 },
	/*  89 */ { 0x00000094, 0xffffa610, 0x04618d11 },
	/*  90 */ { 0x00000092, 0xffffa6a3, 0x045ebfe0 },
	/*  91 */ { 0x0000008f, 0xffffa735, 0x045bf740 },
	/*  92 */ { 0x0000008e, 0xffffa7c5, 0x04593324 },
	/*  93 */ { 0x0000008d, 0xffffa853, 0x04567384 },
	/*  94 */ { 0x0000008b, 0xffffa8e0, 0x0453b850 },
	/*  95 */ { 0x0000008b, 0xffffa96b, 0x0451017c },
	/*  96 */ { 0x00000089, 0xffffa9f5, 0x044e4f00 },
	/*  97 */ { 0x00000087, 0xffffaa7e, 0x044ba0cd },
	/*  98 */ { 0x00000086, 0xffffab05, 0x0448f6da },
	/*  99 */ { 0x00000084, 0xffffab8b, 0x0446511b },
	/* 100 */ { 0x00000084, 0xffffac0f, 0x0443af84 },
	/* 101 */ { 0x00000081, 0xffffac93, 0x0441120b },
	/* 102 */ { 0x00000081, 0xffffad14, 0x043e78a7 },
	/* 103 */ { 0x0000007f, 0xffffad95, 0x043be34b },
	/* 104 */ { 0x0000007e, 0xffffae14, 0x043951ef },
	/* 105 */ { 0x0000007d, 0xffffae92, 0x0436c486 },
	/* 106 */ { 0x0000007b, 0xffffaf0f, 0x04343b09 },
	/* 107 */ { 0x0000007b, 0xffffaf8a, 0x0431b56b },
	/* 108 */ { 0x0000007a, 0xffffb004, 0x042f33a6 },
	/* 109 */ { 0x00000079, 0xffffb07d, 0x042cb5ad },
	/* 110 */ { 0x00000077, 0xffffb0f5, 0x042a3b79 },
	/* 111 */ { 0x00000076, 0xffffb16c, 0x0427c4fd },
	/* 112 */ { 0x00000074, 0xffffb1e2, 0x04255233 },
	/* 113 */ { 0x00000073, 0xffffb256, 0x0422e313 },
	/* 114 */ { 0x00000073, 0xffffb2c9, 0x04207790 },
	/* 115 */ { 0x00000070, 0xffffb33c, 0x041e0fa3 },
	/* 116 */ { 0x0000006f, 0xffffb3ad, 0x041bab44 },
	/* 117 */ { 0x0000006e, 0xffffb41d, 0x04194a6a },
	/* 118 */ { 0x0000006d, 0xffffb48c, 0x0416ed0b },
	/* 119 */ { 0x0000006c, 0xffffb4fa, 0x04149320 },
	/* 120 */ { 0x0000006b, 0xffffb567, 0x04123ca0 },
	/* 121 */ { 0x0000006c, 0xffffb5d2, 0x040fe984 },
	/* 122 */ { 0x0000006b, 0xffffb63d, 0x040d99c3 },
	/* 123 */ { 0x00000069, 0xffffb6a7, 0x040b4d56 },
	/* 124 */ { 0x00000068, 0xffffb710, 0x04090434 },
	/* 125 */ { 0x00000067, 0xffffb778, 0x0406be55 },
	/* 126 */ { 0x00000066, 0xffffb7df, 0x04047bb2 },
	/* 127 */ { 0x00000065, 0xffffb845, 0x04023c43 }
};

//
static k128cp2elfun_params_t k128cp2elfun_log2c_params = {
	"log2c",                           // function name
	k128cp2elfun_log2c_coefstable,     // pointer to quadratic polinom coeffs table
	{ .f = 1.0           },            // minimum argument value
	{ .f = 2.0           },            // maximum argument value
	{ .f = (1.0 / M_LN2) },            // function value for small (i.e. x1=x2=0) arguments
	127,                               // minimum exponent value
	127                                // maximum exponent value
};


// compute log2c for x \in [1.0, 2.0)
float k128cp2elfun_core_log2c (float x) {

	k128cp2elfun_single_t arg, res;

	// check argument value
	arg.f = x;
	assert (arg.fields.s == 0);
	assert (arg.fields.e == 127);

	// calc quadratic polinom
	res.f = k128cp2elfun_common (&k128cp2elfun_log2c_params, x);

	// check return value
	assert (res.fields.s == 0);
	assert (res.fields.e == (127 + (+0))); // log2(x) / (x-1) \in (1.0,1.44] if x \in [1.0,2.0)

	// return result
	return res.f;
}


/**
 * \brief Вычисление кардинального значения логарифма
 * \param x Значение аргумента
 * \return log2c(x) = log2(x) / (x-1)
 */
float k128cp2elfun_log2c (float x) {

	k128cp2elfun_single_t arg, res;

	// return nan for negative values and -0
	if ((signbit(x) != 0) && (x!=0)) {
		res.f = NAN;
		goto return_result;
	}

	// switch for non-negative values
	switch (fpclassify (x)) {
		case FP_NAN       :  res.f = NAN;       break;
		case FP_INFINITE  :  res.f = 0.0;       break;
		case FP_ZERO      :  res.f = +INFINITY; break;
		case FP_SUBNORMAL :  res.f = +INFINITY; break;
		case FP_NORMAL    :
			arg.f = x;
			if (arg.fields.e == 127) {
				res.f = k128cp2elfun_core_log2c (arg.f);
			} else {
				res.f = NAN;
			}
			break;
		default: /* internal error */ assert(0==1); break;
	}

	// return result
return_result:
	return res.f;
}

/**
 * \brief Эталонное вычисление кардинального значения логарифма
 * \param x Значение аргумента
 * \return log2c(x) = log2(x) / (x-1)
 */
float k128cp2elfun_log2c_gold (float x) {

	k128cp2elfun_single_t arg, res;

	// return nan for negative values and -0
	if (signbit(x) != 0) {
		res.f = NAN;
		goto return_result;
	}

	// switch for non-negative values
	switch (fpclassify (x)) {
		case FP_NAN       :  res.f = NAN;       break;
		case FP_INFINITE  :  res.f = 0.0;       break;
		case FP_ZERO      :  res.f = +INFINITY; break;
		case FP_SUBNORMAL :  res.f = +INFINITY; break;
		case FP_NORMAL    :
			arg.f = x;
			if (arg.fields.e == 127) {
				if (x == 1.0) {
					res.f = 1.0 / M_LN2;
				} else {
					res.f = log2f (x) / (x - 1.0);
				}
			} else {
				res.f = NAN;
			}
			break;
		default: assert(0==1); break; // internal error
	}

	// return result
return_result:
	return res.f;
}

