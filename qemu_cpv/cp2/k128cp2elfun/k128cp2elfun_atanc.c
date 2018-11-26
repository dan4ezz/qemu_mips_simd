/**
 * \file src/k128cp2elfun_atanc.c
 * \brief Вычисление кардинального значения арктангенса
 * \author Andrey Lesnykh <lesnykh@niisi.ras.ru>
 * \date 2007
 *
 * Вычисление значения функции
 * atanc(x) = 2/pi atan(x) / x
 */

#include <math.h>
#include <assert.h>
#include "local.h"

// Elementary function calculation: atanc
// 2/pi atan(x) / x

// function prototypes
float k128cp2elfun_common (k128cp2elfun_params_t *func_params, float x);

// 2/pi atan(x) / x
// quadratic polinom coeffs table
static k128cp2elfun_table_t k128cp2elfun_atanc_coefstable[] = {
/*   0 */ { 0xffffff27, 0x00000000, 0x028be60d },
/*   1 */ { 0xffffff26, 0xffffff27, 0x028be2a9 },
/*   2 */ { 0xffffff26, 0xfffffe4e, 0x028bd879 },
/*   3 */ { 0xffffff26, 0xfffffd75, 0x028bc782 },
/*   4 */ { 0xffffff27, 0xfffffc9c, 0x028bafc3 },
/*   5 */ { 0xffffff29, 0xfffffbc3, 0x028b9140 },
/*   6 */ { 0xffffff28, 0xfffffaec, 0x028b6bfb },
/*   7 */ { 0xffffff2a, 0xfffffa14, 0x028b3ffb },
/*   8 */ { 0xffffff29, 0xfffff93e, 0x028b0d44 },
/*   9 */ { 0xffffff2a, 0xfffff868, 0x028ad3da },
/*  10 */ { 0xffffff2b, 0xfffff793, 0x028a93c3 },
/*  11 */ { 0xffffff2c, 0xfffff6bf, 0x028a4d08 },
/*  12 */ { 0xffffff2d, 0xfffff5ec, 0x0289ffb1 },
/*  13 */ { 0xffffff2e, 0xfffff51a, 0x0289abc6 },
/*  14 */ { 0xffffff30, 0xfffff449, 0x02895150 },
/*  15 */ { 0xffffff33, 0xfffff379, 0x0288f057 },
/*  16 */ { 0xffffff34, 0xfffff2ab, 0x028888ea },
/*  17 */ { 0xffffff34, 0xfffff1df, 0x02881b10 },
/*  18 */ { 0xffffff37, 0xfffff113, 0x0287a6d9 },
/*  19 */ { 0xffffff38, 0xfffff04a, 0x02872c4d },
/*  20 */ { 0xffffff3a, 0xffffef82, 0x0286ab7c },
/*  21 */ { 0xffffff3b, 0xffffeebc, 0x02862474 },
/*  22 */ { 0xffffff3f, 0xffffedf7, 0x02859740 },
/*  23 */ { 0xffffff40, 0xffffed35, 0x028503f3 },
/*  24 */ { 0xffffff41, 0xffffec75, 0x02846a9a },
/*  25 */ { 0xffffff44, 0xffffebb6, 0x0283cb46 },
/*  26 */ { 0xffffff46, 0xffffeafa, 0x02832606 },
/*  27 */ { 0xffffff47, 0xffffea40, 0x02827aee },
/*  28 */ { 0xffffff4a, 0xffffe988, 0x0281ca0a },
/*  29 */ { 0xffffff4d, 0xffffe8d2, 0x02811371 },
/*  30 */ { 0xffffff4e, 0xffffe81f, 0x02805734 },
/*  31 */ { 0xffffff50, 0xffffe76e, 0x027f9566 },
/*  32 */ { 0xffffff54, 0xffffe6bf, 0x027ece16 },
/*  33 */ { 0xffffff56, 0xffffe613, 0x027e015d },
/*  34 */ { 0xffffff59, 0xffffe569, 0x027d2f4d },
/*  35 */ { 0xffffff5b, 0xffffe4c2, 0x027c57fa },
/*  36 */ { 0xffffff5f, 0xffffe41d, 0x027b7b77 },
/*  37 */ { 0xffffff62, 0xffffe37b, 0x027a99d9 },
/*  38 */ { 0xffffff64, 0xffffe2dc, 0x0279b338 },
/*  39 */ { 0xffffff65, 0xffffe240, 0x0278c7a7 },
/*  40 */ { 0xffffff68, 0xffffe1a6, 0x0277d73b },
/*  41 */ { 0xffffff6c, 0xffffe10e, 0x0276e20d },
/*  42 */ { 0xffffff6e, 0xffffe07a, 0x0275e82e },
/*  43 */ { 0xffffff72, 0xffffdfe8, 0x0274e9b7 },
/*  44 */ { 0xffffff75, 0xffffdf59, 0x0273e6be },
/*  45 */ { 0xffffff77, 0xffffdecd, 0x0272df5a },
/*  46 */ { 0xffffff7a, 0xffffde44, 0x0271d39f },
/*  47 */ { 0xffffff7c, 0xffffddbe, 0x0270c3a6 },
/*  48 */ { 0xffffff7f, 0xffffdd3a, 0x026faf87 },
/*  49 */ { 0xffffff83, 0xffffdcb9, 0x026e9754 },
/*  50 */ { 0xffffff86, 0xffffdc3b, 0x026d7b28 },
/*  51 */ { 0xffffff89, 0xffffdbc0, 0x026c5b17 },
/*  52 */ { 0xffffff8b, 0xffffdb48, 0x026b373a },
/*  53 */ { 0xffffff8d, 0xffffdad3, 0x026a0fa7 },
/*  54 */ { 0xffffff91, 0xffffda60, 0x0268e475 },
/*  55 */ { 0xffffff93, 0xffffd9f1, 0x0267b5b8 },
/*  56 */ { 0xffffff96, 0xffffd984, 0x0266838b },
/*  57 */ { 0xffffff99, 0xffffd91a, 0x02654e03 },
/*  58 */ { 0xffffff9b, 0xffffd8b3, 0x02641536 },
/*  59 */ { 0xffffff9f, 0xffffd84e, 0x0262d93b },
/*  60 */ { 0xffffffa1, 0xffffd7ed, 0x02619a26 },
/*  61 */ { 0xffffffa4, 0xffffd78e, 0x02605811 },
/*  62 */ { 0xffffffa6, 0xffffd732, 0x025f1311 },
/*  63 */ { 0xffffffaa, 0xffffd6d8, 0x025dcb3b },
/*  64 */ { 0xffffffac, 0xffffd682, 0x025c80a3 },
/*  65 */ { 0xffffffaf, 0xffffd62e, 0x025b3362 },
/*  66 */ { 0xffffffb1, 0xffffd5dd, 0x0259e38d },
/*  67 */ { 0xffffffb4, 0xffffd58e, 0x02589139 },
/*  68 */ { 0xffffffb7, 0xffffd542, 0x02573c79 },
/*  69 */ { 0xffffffb8, 0xffffd4f9, 0x0255e565 },
/*  70 */ { 0xffffffbb, 0xffffd4b2, 0x02548c0f },
/*  71 */ { 0xffffffbd, 0xffffd46e, 0x0253308c },
/*  72 */ { 0xffffffc0, 0xffffd42c, 0x0251d2f2 },
/*  73 */ { 0xffffffc2, 0xffffd3ed, 0x02507353 },
/*  74 */ { 0xffffffc5, 0xffffd3b0, 0x024f11c3 },
/*  75 */ { 0xffffffc9, 0xffffd375, 0x024dae57 },
/*  76 */ { 0xffffffcb, 0xffffd33d, 0x024c4923 },
/*  77 */ { 0xffffffcc, 0xffffd308, 0x024ae236 },
/*  78 */ { 0xffffffd0, 0xffffd2d4, 0x024979a6 },
/*  79 */ { 0xffffffd2, 0xffffd2a3, 0x02480f86 },
/*  80 */ { 0xffffffd3, 0xffffd275, 0x0246a3e5 },
/*  81 */ { 0xffffffd6, 0xffffd248, 0x024536d9 },
/*  82 */ { 0xffffffd7, 0xffffd21e, 0x0243c871 },
/*  83 */ { 0xffffffd9, 0xffffd1f6, 0x024258bf },
/*  84 */ { 0xffffffdb, 0xffffd1d0, 0x0240e7d4 },
/*  85 */ { 0xffffffde, 0xffffd1ac, 0x023f75c0 },
/*  86 */ { 0xffffffe0, 0xffffd18a, 0x023e0297 },
/*  87 */ { 0xffffffe2, 0xffffd16a, 0x023c8e67 },
/*  88 */ { 0xffffffe4, 0xffffd14c, 0x023b1940 },
/*  89 */ { 0xffffffe7, 0xffffd130, 0x0239a331 },
/*  90 */ { 0xffffffe8, 0xffffd116, 0x02382c4d },
/*  91 */ { 0xffffffea, 0xffffd0fe, 0x0236b49e },
/*  92 */ { 0xffffffeb, 0xffffd0e8, 0x02353c37 },
/*  93 */ { 0xffffffee, 0xffffd0d3, 0x0233c325 },
/*  94 */ { 0xffffffef, 0xffffd0c1, 0x02324975 },
/*  95 */ { 0xfffffff1, 0xffffd0b0, 0x0230cf38 },
/*  96 */ { 0xfffffff2, 0xffffd0a1, 0x022f547b },
/*  97 */ { 0xfffffff4, 0xffffd093, 0x022dd94c },
/*  98 */ { 0xfffffff6, 0xffffd087, 0x022c5db5 },
/*  99 */ { 0xfffffff7, 0xffffd07d, 0x022ae1c6 },
/* 100 */ { 0xfffffff9, 0xffffd074, 0x0229658c },
/* 101 */ { 0xfffffffa, 0xffffd06d, 0x0227e911 },
/* 102 */ { 0xfffffffd, 0xffffd067, 0x02266c62 },
/* 103 */ { 0xfffffffe, 0xffffd063, 0x0224ef8c },
/* 104 */ { 0xffffffff, 0xffffd060, 0x0223729c },
/* 105 */ { 0x00000000, 0xffffd05f, 0x0221f599 },
/* 106 */ { 0x00000001, 0xffffd05f, 0x02207892 },
/* 107 */ { 0x00000003, 0xffffd060, 0x021efb8f },
/* 108 */ { 0x00000004, 0xffffd063, 0x021d7e9c },
/* 109 */ { 0x00000005, 0xffffd067, 0x021c01c3 },
/* 110 */ { 0x00000006, 0xffffd06c, 0x021a8510 },
/* 111 */ { 0x00000008, 0xffffd072, 0x0219088a },
/* 112 */ { 0x00000009, 0xffffd07a, 0x02178c3a },
/* 113 */ { 0x00000009, 0xffffd083, 0x0216102e },
/* 114 */ { 0x0000000a, 0xffffd08d, 0x0214946a },
/* 115 */ { 0x0000000d, 0xffffd097, 0x021318fb },
/* 116 */ { 0x0000000e, 0xffffd0a3, 0x02119de7 },
/* 117 */ { 0x0000000f, 0xffffd0b0, 0x02102337 },
/* 118 */ { 0x0000000e, 0xffffd0bf, 0x020ea8f2 },
/* 119 */ { 0x0000000f, 0xffffd0ce, 0x020d2f22 },
/* 120 */ { 0x00000010, 0xffffd0de, 0x020bb5ce },
/* 121 */ { 0x00000013, 0xffffd0ee, 0x020a3cff },
/* 122 */ { 0x00000013, 0xffffd100, 0x0208c4ba },
/* 123 */ { 0x00000013, 0xffffd113, 0x02074d07 },
/* 124 */ { 0x00000015, 0xffffd126, 0x0205d5ed },
/* 125 */ { 0x00000015, 0xffffd13b, 0x02045f71 },
/* 126 */ { 0x00000016, 0xffffd150, 0x0202e99d },
/* 127 */ { 0x00000017, 0xffffd166, 0x02017475 }
};

//
static k128cp2elfun_params_t k128cp2elfun_atanc_params = {
	"atanc",                        // function name
	k128cp2elfun_atanc_coefstable,  // pointer to quadratic polinom coeffs table
	{ .f = -1.0    },               // minimum argument value
	{ .f =  1.0    },               // maximum argument value
	{ .f = M_2_PI },                // function value for small (i.e. x1=x2=0) arguments
	1,                              // minimum exponent value
	126                             // maximum exponent value
};


/**
 * \brief Вычисление кардинального значения арктангенса
 * \param x Значение аргумента
 * \return atanc(x) = 2/pi atan(x) / x
 */
float k128cp2elfun_atanc (float x) {

	float res;
	k128cp2elfun_single_t arg;

	switch (fpclassify (x)) {
		case FP_NAN       :  res = NAN;    break;
		case FP_INFINITE  :  res = 0.0;    break;
		case FP_ZERO      :  res = M_2_PI; break;
		case FP_SUBNORMAL :  res = M_2_PI; break;
		case FP_NORMAL    :
			arg.f = x;
			if ((1 <=arg.fields.e) && (arg.fields.e <= 126)) {
				// approximate with quadratic polynom if arg \in (-1, 1)
				res = k128cp2elfun_common (&k128cp2elfun_atanc_params, arg.f);
			} else if ((arg.fields.e == 127) && (arg.fields.m == 0)) {
				// atan (+/- 1.0) = 0.5
				res = 0.5;
			} else {
				// return NAN otherwise
				res = NAN;
			}
			break;
		default: /* internal error */ assert(0==1); break;
	}

	return res;
}


/**
 * \brief Эталонное вычисление кардинального значения арктангенса
 * \param x Значение аргумента
 * \return atanc(x) = 2/pi atan(x) / x
 */
float k128cp2elfun_atanc_gold (float x) {

	float res;

	switch (fpclassify (x)) {
		case FP_NAN       :  res = NAN;    break;
		case FP_INFINITE  :  res = 0.0;    break;
		case FP_ZERO      :  res = M_2_PI; break;
		case FP_SUBNORMAL :  res = M_2_PI; break;
		case FP_NORMAL    :
			if ((-1.0 <= x) && (x <= 1.0)) {
				// return atanc for arg \in [-1, 1]
				res = M_2_PI * atanf(x) / x;
			} else {
				// return NAN otherwise
				res = NAN;
			}
			break;
		default: assert(0==1); break; // internal error
	}

	return res;
}

