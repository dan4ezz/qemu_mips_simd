/**
 * \file src/k128cp2elfun_sinc.c
 * \brief Вычисление кардинального значения синуса
 * \author Andrey Lesnykh <lesnykh@niisi.ras.ru>
 * \date 2007
 *
 * Вычисление значения функции
 * sinc(x) = sin(pi/2 x) / x
 */

#include <math.h>
#include <assert.h>
#include "local.h"
#include "softfloat.h"


// function prototypes
float k128cp2elfun_common (k128cp2elfun_params_t *func_params, float x);

// sin(pi/2 x) / x
// quadratic polinom coeffs table
static k128cp2elfun_table_t k128cp2elfun_sinc_coefstable[] = {
/*   0 */ { 0xfffffd6b, 0x00000000, 0x06487ed4 },
/*   1 */ { 0xfffffd6a, 0xfffffd6b, 0x0648747e },
/*   2 */ { 0xfffffd6b, 0xfffffad5, 0x0648557e },
/*   3 */ { 0xfffffd6b, 0xfffff840, 0x064821d1 },
/*   4 */ { 0xfffffd6b, 0xfffff5ab, 0x0647d97c },
/*   5 */ { 0xfffffd6b, 0xfffff316, 0x06477c7f },
/*   6 */ { 0xfffffd6c, 0xfffff081, 0x06470adc },
/*   7 */ { 0xfffffd6c, 0xffffeded, 0x06468496 },
/*   8 */ { 0xfffffd6d, 0xffffeb59, 0x0645e9b0 },
/*   9 */ { 0xfffffd6d, 0xffffe8c6, 0x06453a2d },
/*  10 */ { 0xfffffd6e, 0xffffe633, 0x06447612 },
/*  11 */ { 0xfffffd6f, 0xffffe3a1, 0x06439d62 },
/*  12 */ { 0xfffffd6e, 0xffffe110, 0x0642b025 },
/*  13 */ { 0xfffffd6f, 0xffffde7f, 0x0641ae5f },
/*  14 */ { 0xfffffd70, 0xffffdbef, 0x06409814 },
/*  15 */ { 0xfffffd73, 0xffffd95f, 0x063f6d4c },
/*  16 */ { 0xfffffd73, 0xffffd6d1, 0x063e2e0f },
/*  17 */ { 0xfffffd73, 0xffffd444, 0x063cda64 },
/*  18 */ { 0xfffffd76, 0xffffd1b7, 0x063b7250 },
/*  19 */ { 0xfffffd76, 0xffffcf2c, 0x0639f5e0 },
/*  20 */ { 0xfffffd77, 0xffffcca2, 0x06386519 },
/*  21 */ { 0xfffffd79, 0xffffca19, 0x0636c005 },
/*  22 */ { 0xfffffd7a, 0xffffc791, 0x063506b0 },
/*  23 */ { 0xfffffd7b, 0xffffc50b, 0x06333921 },
/*  24 */ { 0xfffffd7c, 0xffffc286, 0x06315765 },
/*  25 */ { 0xfffffd7e, 0xffffc002, 0x062f6187 },
/*  26 */ { 0xfffffd80, 0xffffbd80, 0x062d5790 },
/*  27 */ { 0xfffffd80, 0xffffbb00, 0x062b398f },
/*  28 */ { 0xfffffd82, 0xffffb881, 0x0629078f },
/*  29 */ { 0xfffffd85, 0xffffb603, 0x0626c19e },
/*  30 */ { 0xfffffd87, 0xffffb387, 0x062467c9 },
/*  31 */ { 0xfffffd87, 0xffffb10e, 0x0621fa1d },
/*  32 */ { 0xfffffd8b, 0xffffae95, 0x061f78aa },
/*  33 */ { 0xfffffd8c, 0xffffac1f, 0x061ce37e },
/*  34 */ { 0xfffffd8e, 0xffffa9ab, 0x061a3aa6 },
/*  35 */ { 0xfffffd8f, 0xffffa739, 0x06177e36 },
/*  36 */ { 0xfffffd91, 0xffffa4c9, 0x0614ae3b },
/*  37 */ { 0xfffffd95, 0xffffa25a, 0x0611cac7 },
/*  38 */ { 0xfffffd97, 0xffff9fee, 0x060ed3eb },
/*  39 */ { 0xfffffd98, 0xffff9d85, 0x060bc9b6 },
/*  40 */ { 0xfffffd9b, 0xffff9b1d, 0x0608ac3f },
/*  41 */ { 0xfffffd9d, 0xffff98b8, 0x06057b94 },
/*  42 */ { 0xfffffda0, 0xffff9655, 0x060237c9 },
/*  43 */ { 0xfffffda2, 0xffff93f5, 0x05fee0f1 },
/*  44 */ { 0xfffffda5, 0xffff9197, 0x05fb7720 },
/*  45 */ { 0xfffffda7, 0xffff8f3c, 0x05f7fa6a },
/*  46 */ { 0xfffffdaa, 0xffff8ce3, 0x05f46ae5 },
/*  47 */ { 0xfffffdac, 0xffff8a8d, 0x05f0c8a5 },
/*  48 */ { 0xfffffdb0, 0xffff8839, 0x05ed13be },
/*  49 */ { 0xfffffdb2, 0xffff85e9, 0x05e94c46 },
/*  50 */ { 0xfffffdb5, 0xffff839b, 0x05e57256 },
/*  51 */ { 0xfffffdb8, 0xffff8150, 0x05e18602 },
/*  52 */ { 0xfffffdbb, 0xffff7f08, 0x05dd8761 },
/*  53 */ { 0xfffffdbd, 0xffff7cc3, 0x05d9768d },
/*  54 */ { 0xfffffdc0, 0xffff7a81, 0x05d5539b },
/*  55 */ { 0xfffffdc3, 0xffff7842, 0x05d11ea4 },
/*  56 */ { 0xfffffdc7, 0xffff7606, 0x05ccd7c1 },
/*  57 */ { 0xfffffdca, 0xffff73cd, 0x05c87f0d },
/*  58 */ { 0xfffffdce, 0xffff7197, 0x05c4149f },
/*  59 */ { 0xfffffdd1, 0xffff6f65, 0x05bf9890 },
/*  60 */ { 0xfffffdd5, 0xffff6d36, 0x05bb0afc },
/*  61 */ { 0xfffffdd9, 0xffff6b0a, 0x05b66c00 },
/*  62 */ { 0xfffffddc, 0xffff68e2, 0x05b1bbb3 },
/*  63 */ { 0xfffffdde, 0xffff66be, 0x05acfa32 },
/*  64 */ { 0xfffffde3, 0xffff649c, 0x05a8279a },
/*  65 */ { 0xfffffde5, 0xffff627f, 0x05a34405 },
/*  66 */ { 0xfffffdea, 0xffff6064, 0x059e4f92 },
/*  67 */ { 0xfffffded, 0xffff5e4e, 0x05994a5c },
/*  68 */ { 0xfffffdf1, 0xffff5c3b, 0x05943482 },
/*  69 */ { 0xfffffdf5, 0xffff5a2c, 0x058f0e1f },
/*  70 */ { 0xfffffdf8, 0xffff5821, 0x0589d754 },
/*  71 */ { 0xfffffdfd, 0xffff5619, 0x0584903e },
/*  72 */ { 0xfffffe00, 0xffff5416, 0x057f38fc },
/*  73 */ { 0xfffffe05, 0xffff5216, 0x0579d1ad },
/*  74 */ { 0xfffffe08, 0xffff501b, 0x05745a70 },
/*  75 */ { 0xfffffe0c, 0xffff4e23, 0x056ed368 },
/*  76 */ { 0xfffffe11, 0xffff4c2f, 0x05693cb1 },
/*  77 */ { 0xfffffe14, 0xffff4a40, 0x0563966e },
/*  78 */ { 0xfffffe19, 0xffff4854, 0x055de0c0 },
/*  79 */ { 0xfffffe1d, 0xffff466d, 0x05581bc5 },
/*  80 */ { 0xfffffe21, 0xffff448a, 0x055247a3 },
/*  81 */ { 0xfffffe26, 0xffff42ab, 0x054c6478 },
/*  82 */ { 0xfffffe29, 0xffff40d1, 0x0546726a },
/*  83 */ { 0xfffffe2e, 0xffff3efb, 0x05407196 },
/*  84 */ { 0xfffffe32, 0xffff3d29, 0x053a6227 },
/*  85 */ { 0xfffffe36, 0xffff3b5c, 0x05344438 },
/*  86 */ { 0xfffffe3b, 0xffff3993, 0x052e17f0 },
/*  87 */ { 0xfffffe40, 0xffff37ce, 0x0527dd75 },
/*  88 */ { 0xfffffe45, 0xffff360e, 0x052194e7 },
/*  89 */ { 0xfffffe49, 0xffff3453, 0x051b3e6c },
/*  90 */ { 0xfffffe4e, 0xffff329c, 0x0514da29 },
/*  91 */ { 0xfffffe53, 0xffff30ea, 0x050e6841 },
/*  92 */ { 0xfffffe57, 0xffff2f3d, 0x0507e8dc },
/*  93 */ { 0xfffffe5c, 0xffff2d94, 0x05015c1f },
/*  94 */ { 0xfffffe61, 0xffff2bf0, 0x04fac22e },
/*  95 */ { 0xfffffe65, 0xffff2a51, 0x04f41b31 },
/*  96 */ { 0xfffffe6b, 0xffff28b6, 0x04ed674e },
/*  97 */ { 0xfffffe6f, 0xffff2721, 0x04e6a6a9 },
/*  98 */ { 0xfffffe74, 0xffff2590, 0x04dfd96d },
/*  99 */ { 0xfffffe79, 0xffff2404, 0x04d8ffbe },
/* 100 */ { 0xfffffe7f, 0xffff227d, 0x04d219c3 },
/* 101 */ { 0xfffffe84, 0xffff20fb, 0x04cb27a7 },
/* 102 */ { 0xfffffe89, 0xffff1f7e, 0x04c4298f },
/* 103 */ { 0xfffffe8c, 0xffff1e07, 0x04bd1fa4 },
/* 104 */ { 0xfffffe92, 0xffff1c94, 0x04b60a0d },
/* 105 */ { 0xfffffe97, 0xffff1b26, 0x04aee8f6 },
/* 106 */ { 0xfffffe9d, 0xffff19bd, 0x04a7bc85 },
/* 107 */ { 0xfffffea2, 0xffff185a, 0x04a084e1 },
/* 108 */ { 0xfffffea6, 0xffff16fc, 0x04994239 },
/* 109 */ { 0xfffffeab, 0xffff15a3, 0x0491f4b1 },
/* 110 */ { 0xfffffeb1, 0xffff144f, 0x048a9c75 },
/* 111 */ { 0xfffffeb7, 0xffff1300, 0x048339b0 },
/* 112 */ { 0xfffffebb, 0xffff11b7, 0x047bcc8b },
/* 113 */ { 0xfffffec2, 0xffff1072, 0x04745531 },
/* 114 */ { 0xfffffec6, 0xffff0f34, 0x046cd3c9 },
/* 115 */ { 0xfffffecd, 0xffff0dfa, 0x04654881 },
/* 116 */ { 0xfffffed2, 0xffff0cc6, 0x045db384 },
/* 117 */ { 0xfffffed6, 0xffff0b98, 0x045614fb },
/* 118 */ { 0xfffffedd, 0xffff0a6e, 0x044e6d14 },
/* 119 */ { 0xfffffee3, 0xffff094a, 0x0446bbf7 },
/* 120 */ { 0xfffffee7, 0xffff082c, 0x043f01d2 },
/* 121 */ { 0xfffffeed, 0xffff0713, 0x04373ecf },
/* 122 */ { 0xfffffef2, 0xffff0600, 0x042f731a },
/* 123 */ { 0xfffffef7, 0xffff04f2, 0x04279ee2 },
/* 124 */ { 0xfffffefe, 0xffff03e9, 0x041fc24f },
/* 125 */ { 0xffffff02, 0xffff02e7, 0x0417dd8f },
/* 126 */ { 0xffffff09, 0xffff01e9, 0x040ff0d0 },
/* 127 */ { 0xffffff0d, 0xffff00f2, 0x0407fc3b }
};

//
static k128cp2elfun_params_t k128cp2elfun_sinc_params = {
	"sinc",                            // function name
	k128cp2elfun_sinc_coefstable,      // pointer to quadratic polinom coeffs table
	{ .f = 0.0    },                   // minimum argument value
	{ .f = 1.0    },                   // maximum argument value
	{ .f = M_PI_2 },                   // function value for small (i.e. x1=x2=0) arguments
	1,                                 // minimum exponent value
	126                                // maximum exponent value
};


//
float k128cp2elfun_core_sinc (float x) {
	return k128cp2elfun_common (&k128cp2elfun_sinc_params, x);
}


/**
 * \brief Вычисление кардинального значения синуса
 * \param x Значение аргумента
 * \return sinc(x) = sin (pi/2 x) / x
 */
float k128cp2elfun_sinc (float x) {

	k128cp2elfun_single_t arg, res;

	// switch
	switch (fpclassify (x)) {
		case FP_NAN       :  res.f = NAN;       break;
		case FP_INFINITE  :  res.f = 0;         break;
		case FP_ZERO      :  res.f = M_PI_2;    break;
		case FP_SUBNORMAL :  res.f = M_PI_2;    break;
		case FP_NORMAL    :
			arg.f = x;
			if (arg.fields.e >= 128) {
				res.f = NAN;
			} else if (arg.fields.e == 127) {
				if (arg.fields.m == 0) {
					res.f = 1.0; // sinc(+-1.0) = 1.0
				} else {
					res.f = NAN;
				}
			} else {
				arg.fields.s = 0;
				res.f = k128cp2elfun_core_sinc (arg.f);
			}
			break;
		default: /* internal error */ assert(0==1); break;
	}

	// return result
	return res.f;

}


/**
 * \brief Эталонное вычисление кардинального значения синуса
 * \param x Значение аргумента
 * \return sinc(x) = sin (pi/2 x) / x
 */
float k128cp2elfun_sinc_gold (float x) {

	k128cp2elfun_single_t arg, res;

	// switch
	switch (fpclassify (x)) {
		case FP_NAN       :  res.f = NAN;       break;
		case FP_INFINITE  :  res.f = +INFINITY; break;
		case FP_ZERO      :  res.f = M_PI_2;    break;
		case FP_SUBNORMAL :  res.f = M_PI_2;    break;
		case FP_NORMAL    :
			arg.f = x;
			if (arg.fields.e >= 127) {
				res.f = NAN;
			} else {
				res.f = sinf (M_PI_2 * x) / x;
			}
			break;
		default: /* internal error */ assert(0==1); break;
	}

	// return result
	return res.f;

}


