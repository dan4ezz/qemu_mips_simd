/**
 * \file src/k128cp2elfun_exp2.c
 * \brief Вычисление двоичной экспоненты
 * \author Andrey Lesnykh <lesnykh@niisi.ras.ru>
 * \date 2007
 *
 * Вычисление значения функции exp2(x) = 2^x
 */

#include <math.h>
#include <assert.h>
#include "local.h"
#include "softfloat.h"

extern float_status f_status;

// Elementary function calculation: exp2

// function prototypes
float k128cp2elfun_common (k128cp2elfun_params_t *func_params, float x);

// exp2
// quadratic polinom coeffs table
static k128cp2elfun_table_t k128cp2elfun_exp2_coefstable[] = {
	/*   0 */ { 0x000000f7, 0x0000b172, 0x03ffffff },
	/*   1 */ { 0x000000f7, 0x0000b269, 0x04058f6b },
	/*   2 */ { 0x000000f9, 0x0000b361, 0x040b268f },
	/*   3 */ { 0x000000fb, 0x0000b45a, 0x0410c579 },
	/*   4 */ { 0x000000fb, 0x0000b555, 0x04166c35 },
	/*   5 */ { 0x000000fd, 0x0000b651, 0x041c1aca },
	/*   6 */ { 0x000000ff, 0x0000b74e, 0x0421d146 },
	/*   7 */ { 0x00000100, 0x0000b84d, 0x04278fb3 },
	/*   8 */ { 0x00000102, 0x0000b94d, 0x042d561b },
	/*   9 */ { 0x00000102, 0x0000ba4f, 0x0433248c },
	/*  10 */ { 0x00000104, 0x0000bb52, 0x0438fb0c },
	/*  11 */ { 0x00000106, 0x0000bc56, 0x043ed9ac },
	/*  12 */ { 0x00000107, 0x0000bd5c, 0x0444c074 },
	/*  13 */ { 0x00000109, 0x0000be63, 0x044aaf70 },
	/*  14 */ { 0x0000010a, 0x0000bf6c, 0x0450a6ab },
	/*  15 */ { 0x0000010b, 0x0000c076, 0x0456a633 },
	/*  16 */ { 0x0000010e, 0x0000c181, 0x045cae0f },
	/*  17 */ { 0x0000010f, 0x0000c28e, 0x0462be4f },
	/*  18 */ { 0x00000110, 0x0000c39d, 0x0468d6fa },
	/*  19 */ { 0x00000111, 0x0000c4ad, 0x046ef821 },
	/*  20 */ { 0x00000113, 0x0000c5be, 0x047521cd },
	/*  21 */ { 0x00000114, 0x0000c6d1, 0x047b5409 },
	/*  22 */ { 0x00000117, 0x0000c7e5, 0x04818ee1 },
	/*  23 */ { 0x00000118, 0x0000c8fb, 0x0487d264 },
	/*  24 */ { 0x00000118, 0x0000ca13, 0x048e1e9b },
	/*  25 */ { 0x0000011b, 0x0000cb2b, 0x04947395 },
	/*  26 */ { 0x0000011c, 0x0000cc46, 0x049ad159 },
	/*  27 */ { 0x0000011d, 0x0000cd62, 0x04a137f9 },
	/*  28 */ { 0x00000120, 0x0000ce7f, 0x04a7a77d },
	/*  29 */ { 0x00000121, 0x0000cf9e, 0x04ae1ff5 },
	/*  30 */ { 0x00000122, 0x0000d0bf, 0x04b4a16a },
	/*  31 */ { 0x00000124, 0x0000d1e1, 0x04bb2beb },
	/*  32 */ { 0x00000125, 0x0000d305, 0x04c1bf83 },
	/*  33 */ { 0x00000127, 0x0000d42a, 0x04c85c40 },
	/*  34 */ { 0x00000129, 0x0000d551, 0x04cf022c },
	/*  35 */ { 0x00000129, 0x0000d67a, 0x04d5b159 },
	/*  36 */ { 0x0000012b, 0x0000d7a4, 0x04dc69ce },
	/*  37 */ { 0x0000012c, 0x0000d8d0, 0x04e32b9c },
	/*  38 */ { 0x0000012f, 0x0000d9fd, 0x04e9f6cd },
	/*  39 */ { 0x00000131, 0x0000db2c, 0x04f0cb70 },
	/*  40 */ { 0x00000132, 0x0000dc5d, 0x04f7a992 },
	/*  41 */ { 0x00000134, 0x0000dd8f, 0x04fe9141 },
	/*  42 */ { 0x00000135, 0x0000dec3, 0x05058289 },
	/*  43 */ { 0x00000136, 0x0000dff9, 0x050c7d77 },
	/*  44 */ { 0x00000139, 0x0000e130, 0x05138218 },
	/*  45 */ { 0x0000013b, 0x0000e269, 0x051a907a },
	/*  46 */ { 0x0000013c, 0x0000e3a4, 0x0521a8ad },
	/*  47 */ { 0x0000013e, 0x0000e4e0, 0x0528cabd },
	/*  48 */ { 0x00000140, 0x0000e61e, 0x052ff6b6 },
	/*  49 */ { 0x00000142, 0x0000e75e, 0x05372ca6 },
	/*  50 */ { 0x00000143, 0x0000e8a0, 0x053e6c9d },
	/*  51 */ { 0x00000145, 0x0000e9e3, 0x0545b6a9 },
	/*  52 */ { 0x00000147, 0x0000eb28, 0x054d0ad6 },
	/*  53 */ { 0x00000149, 0x0000ec6f, 0x05546932 },
	/*  54 */ { 0x0000014a, 0x0000edb8, 0x055bd1cd },
	/*  55 */ { 0x0000014d, 0x0000ef02, 0x056344b4 },
	/*  56 */ { 0x0000014f, 0x0000f04e, 0x056ac1f7 },
	/*  57 */ { 0x00000151, 0x0000f19c, 0x057249a2 },
	/*  58 */ { 0x00000152, 0x0000f2ec, 0x0579dbc6 },
	/*  59 */ { 0x00000153, 0x0000f43e, 0x0581786f },
	/*  60 */ { 0x00000156, 0x0000f591, 0x05891fad },
	/*  61 */ { 0x00000157, 0x0000f6e7, 0x0590d18d },
	/*  62 */ { 0x00000159, 0x0000f83e, 0x05988e21 },
	/*  63 */ { 0x0000015b, 0x0000f997, 0x05a05575 },
	/*  64 */ { 0x0000015d, 0x0000faf2, 0x05a8279a },
	/*  65 */ { 0x0000015e, 0x0000fc4f, 0x05b0049e },
	/*  66 */ { 0x00000160, 0x0000fdae, 0x05b7ec8f },
	/*  67 */ { 0x00000163, 0x0000ff0e, 0x05bfdf7f },
	/*  68 */ { 0x00000164, 0x00010071, 0x05c7dd7a },
	/*  69 */ { 0x00000167, 0x000101d5, 0x05cfe692 },
	/*  70 */ { 0x00000168, 0x0001033c, 0x05d7fad5 },
	/*  71 */ { 0x0000016a, 0x000104a4, 0x05e01a54 },
	/*  72 */ { 0x0000016d, 0x0001060e, 0x05e8451d },
	/*  73 */ { 0x0000016d, 0x0001077b, 0x05f07b41 },
	/*  74 */ { 0x00000170, 0x000108e9, 0x05f8bccd },
	/*  75 */ { 0x00000172, 0x00010a59, 0x060109d6 },
	/*  76 */ { 0x00000175, 0x00010bcb, 0x06096266 },
	/*  77 */ { 0x00000175, 0x00010d40, 0x0611c692 },
	/*  78 */ { 0x00000178, 0x00010eb6, 0x061a3666 },
	/*  79 */ { 0x0000017a, 0x0001102e, 0x0622b1f7 },
	/*  80 */ { 0x0000017d, 0x000111a8, 0x062b3951 },
	/*  81 */ { 0x0000017e, 0x00011325, 0x0633cc86 },
	/*  82 */ { 0x00000181, 0x000114a3, 0x063c6ba7 },
	/*  83 */ { 0x00000182, 0x00011624, 0x064516c4 },
	/*  84 */ { 0x00000186, 0x000117a6, 0x064dcdec },
	/*  85 */ { 0x00000187, 0x0001192b, 0x06569133 },
	/*  86 */ { 0x00000189, 0x00011ab2, 0x065f60a8 },
	/*  87 */ { 0x0000018b, 0x00011c3b, 0x06683c5c },
	/*  88 */ { 0x0000018d, 0x00011dc6, 0x06712462 },
	/*  89 */ { 0x00000190, 0x00011f53, 0x067a18c7 },
	/*  90 */ { 0x00000191, 0x000120e3, 0x0683199f },
	/*  91 */ { 0x00000194, 0x00012274, 0x068c26fc },
	/*  92 */ { 0x00000196, 0x00012408, 0x069540ed },
	/*  93 */ { 0x00000198, 0x0001259e, 0x069e6785 },
	/*  94 */ { 0x0000019b, 0x00012736, 0x06a79ad5 },
	/*  95 */ { 0x0000019e, 0x000128d0, 0x06b0daf0 },
	/*  96 */ { 0x0000019f, 0x00012a6d, 0x06ba27e6 },
	/*  97 */ { 0x000001a1, 0x00012c0c, 0x06c381ca },
	/*  98 */ { 0x000001a3, 0x00012dad, 0x06cce8af },
	/*  99 */ { 0x000001a6, 0x00012f50, 0x06d65ca4 },
	/* 100 */ { 0x000001a8, 0x000130f6, 0x06dfddbc },
	/* 101 */ { 0x000001aa, 0x0001329e, 0x06e96c0c },
	/* 102 */ { 0x000001ad, 0x00013448, 0x06f307a4 },
	/* 103 */ { 0x000001ae, 0x000135f5, 0x06fcb097 },
	/* 104 */ { 0x000001b2, 0x000137a3, 0x070666f8 },
	/* 105 */ { 0x000001b3, 0x00013955, 0x07102ad8 },
	/* 106 */ { 0x000001b6, 0x00013b08, 0x0719fc4c },
	/* 107 */ { 0x000001b9, 0x00013cbe, 0x0723db64 },
	/* 108 */ { 0x000001bb, 0x00013e76, 0x072dc838 },
	/* 109 */ { 0x000001bd, 0x00014031, 0x0737c2d6 },
	/* 110 */ { 0x000001c0, 0x000141ee, 0x0741cb52 },
	/* 111 */ { 0x000001c1, 0x000143ae, 0x074be1c2 },
	/* 112 */ { 0x000001c4, 0x00014570, 0x07560636 },
	/* 113 */ { 0x000001c7, 0x00014734, 0x076038c5 },
	/* 114 */ { 0x000001c9, 0x000148fb, 0x076a7981 },
	/* 115 */ { 0x000001cc, 0x00014ac4, 0x0774c87d },
	/* 116 */ { 0x000001ce, 0x00014c90, 0x077f25cd },
	/* 117 */ { 0x000001d1, 0x00014e5e, 0x07899186 },
	/* 118 */ { 0x000001d3, 0x0001502f, 0x07940bba },
	/* 119 */ { 0x000001d6, 0x00015202, 0x079e9480 },
	/* 120 */ { 0x000001d9, 0x000153d8, 0x07a92be8 },
	/* 121 */ { 0x000001da, 0x000155b1, 0x07b3d20b },
	/* 122 */ { 0x000001df, 0x0001578b, 0x07be86fb },
	/* 123 */ { 0x000001e0, 0x00015969, 0x07c94ace },
	/* 124 */ { 0x000001e3, 0x00015b49, 0x07d41d97 },
	/* 125 */ { 0x000001e5, 0x00015d2c, 0x07deff6c },
	/* 126 */ { 0x000001e9, 0x00015f11, 0x07e9f060 },
	/* 127 */ { 0x000001eb, 0x000160f9, 0x07f4f08b }
};

//
static k128cp2elfun_params_t k128cp2elfun_exp2_params = {
	"exp2",                            // function name
	k128cp2elfun_exp2_coefstable,      // pointer to quadratic polinom coeffs table
	{ .f = 0.0 },                      // minimum argument value
	{ .f = 1.0 },                      // maximum argument value
	{ .f = 1.0 },                      // function value for small (i.e. x1=x2=0) arguments
	127 + (-126),                      // minimum exponent value
	127 + (-1)                         // maximum exponent value
};


//
float _old_k128cp2elfun_core_exp2 (float x) {

	k128cp2elfun_single_t arg, rarg, res;
	uint32_t mtmp;
	int mshift;

	// check argument value
	arg.f = x;
	assert (arg.fields.s == 0);
	assert (arg.fields.e < 134);

	// shift mantissa
	mtmp  = arg.fields.m;
	mtmp |= bit32(23);
	mshift = arg.fields.e - 127;
	if (mshift > 0) {
		mtmp <<=  mshift;
	} else {
		mtmp = ((-mshift < 32) ? (mtmp >> -mshift) : 0);
	}

	// prepare reduced argument in interval [0.0, 1.0)
	rarg.fields.s = 0;
	rarg.fields.e = 127 + (+0);
	rarg.fields.m = getbits32(mtmp, 22, 0);
	rarg.f -= 1.0;

	// call common function for quadratic approximation
	if (rarg.f != 0.0) {
		res.f = k128cp2elfun_common (&k128cp2elfun_exp2_params, rarg.f);
	} else {
		res.f = 1.0;
	}

	// check return value
	assert (res.fields.s == 0);
	assert (res.fields.e == (127 + (+0))); // exp2(x) \in [1.0,2.0) if x \in [0.0,1.0)

	// save result exponent = mtmp[23+5:23]
	res.fields.e = 127 + getbits32(mtmp, 23+6, 23);

	// return result
	return res.f;
}


//
float k128cp2elfun_core_exp2 (float x) {

	k128cp2elfun_single_t res;
	int xint;
	float xfrac;

	// check argument value
	assert (fpclassify (x) == FP_NORMAL);
	assert (x >= -126.);
	assert (x <   128.);

	// get integer and fractional parts
	xint  = floorf (x);
	xfrac = x - xint;
	assert (xint <= x);
	assert (xfrac >= 0.0);
	assert (xfrac <= 1.0); // xfrac could be 1.0 for small x

	// calc 2^xfrac with table
	if (xfrac == 0.0) {
		res.f = 1.0;
	} else if (xfrac == 1.0) {
		res.f = 2.0;
		f_status.float_exception_flags |= float_flag_inexact;
	} else {
		res.f = k128cp2elfun_common (&k128cp2elfun_exp2_params, xfrac);
	}

	// check value of 2^xfrac
	// 2^xfrac \in [1.0, 2.0] if x \in [0.0, 1.0]
	assert (res.f >= 1.0);
	assert (res.f <= 2.0);

	// multiply on 2^xint
	// (adjust exponent)
	res.fields.e += xint;

	// return result
	return res.f;
}



/**
 * \brief Вычисление экспоненты
 * \param x Значение аргумента
 * \return exp2(x) = 2^x
 */
float k128cp2elfun_exp2 (float x) {

	k128cp2elfun_single_t res;

	// switch for number class
	switch (fpclassify (x)) {
		case FP_NAN       :  res.f = NAN;       break;
		case FP_INFINITE  :
			if (signbit(x) == 0) {
				res.f = +INFINITY; // exp2(+inf) = +inf
			} else {
				res.f = 0.0;       // exp2(-inf) = 0
			}
			break;
		case FP_ZERO      :  res.f = 1.0;       break;
		case FP_SUBNORMAL :  res.f = 1.0;       break;
		case FP_NORMAL    :
			if (x >= 128.) {
				res.f = +INFINITY;                  // exp2(x) = +inf for x >= 2^7
				f_status.float_exception_flags |= float_flag_overflow | float_flag_inexact;
			} else if (x >= -126.) {
				res.f = k128cp2elfun_core_exp2 (x); // exp2(x) = 2^x  for x \in [-126, 2^7)
			} else {
				res.f = 0.0;                        // exp2(x) = 0    for x < -126
				f_status.float_exception_flags |= float_flag_underflow | float_flag_inexact;
			}
			break;
		default: /* internal error */ assert(0==1); break;
	}

	// return result
	return res.f;
}


/**
 * \brief Эталонное вычисление экспоненты
 * \param x Значение аргумента
 * \return exp2(x) = 2^x
 */
float k128cp2elfun_exp2_gold (float x) {
	float res;
	if (fpclassify(x) == FP_SUBNORMAL) {
		res = 1.0;
	} else {
		res = exp2f (x);
	}
	return res;
}


