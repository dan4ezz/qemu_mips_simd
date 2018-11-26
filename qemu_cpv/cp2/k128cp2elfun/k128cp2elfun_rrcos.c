/**
 * \file src/k128cp2elfun_rrcos.c
 * \brief Приведение аргумента синуса
 * \author Andrey Lesnykh <lesnykh@niisi.ras.ru>
 * \date 2008
 *
 */

#include <math.h>
#include <assert.h>
#include <stdio.h>
#include "local.h"
#include "softfloat.h"

extern float_status f_status;

union f32 {
	float f;
	uint32_t u;
};

/**
 * \brief Вычисление целой и дробной частей
 * \param x      Значение аргумента
 * \param xint   Указатель для возвращения целой части x
 * \param xfrac  Указатель для возвращения дробной части x
 * \param xfrac1 Указатель для возвращения 1-дробной части x (xfrac1 = 1-xfrac)
 * \return 0
 *
 *
 * Вычисление целой и дробной частей с помощью Softfloat
 */
/*int intfrac (float x, int *xint, float *xfrac, float *xfrac1) {

	// FIXME implement with Softfloat (independent on hardware floating point support)
	return 0;
}*/


/**
 * \brief Приведение аргумента синуса
 * \param x Значение аргумента
 * \return Приведенное значение в диапазоне [-1,1]
 *
 * Значение функции
 * - rrcos(nan) = nan
 * - rrcos(+/- inf) = nan
 * - rrcos(+ 0) = +1
 * - rrcos(- 0) = +1
 * - rrcos(denorm) = +1
 * - rrcos(x) = sign(cos(pi/2 x)) * (1-frac(|x|)),     если x -- normalized, x\\in[2k,2k+1),   k-целое
 * - rrcos(x) = sign(cos(pi/2 x)) * frac(|x|),         если x -- normalized, x\\in[2k+1,2k+2), k-целое
 *
 * Заметим, что для x>=0
 * - sign(cos(pi/2 x)) = +1, если [x]\%4 = 0 или 3
 * - sign(cos(pi/2 x)) = -1, если [x]\%4 = 1 или 2
 */
float k128cp2elfun_rrcos (float x) {

	float rv;
	uint32_t xint;
	float xfrac;
	uint32_t rv32, x32, xfrac_sgn, xfrac_exp, xfrac32;
	int exp;
	union f32 xx, xxfrac, rrv;
	xx.f = x;
	x32 = xx.u;

	// switch
	switch (fpclassify (x)) {
		case FP_NAN       :  rv = x;   break;
		case FP_INFINITE  :  rv = NAN;   break;
		case FP_ZERO      :  rv = 1;     break;
		case FP_SUBNORMAL :  rv = 1;     break;
		case FP_NORMAL    :
			// get integer and fractional parts of absolute value
			// FIXME implement with intfrac (i.e. with Softfloat)
			exp = ((x32 & 0x7f800000) >> 23) - 127;

			if( exp < 0 ){
				rv32 = float32_sub(0x3f800000,x32 & 0x7fffffff, &f_status);
				rrv.u = rv32;
				rv = rrv.f;
				return rv;
			}
			else {
				if( exp > 24 )
					return 1.0;
				else {
					if( exp == 24 ){
						xint = (x32 & 0x007fffff) << 1;
						xfrac32 = 0;
					}
					else{
						xint = ((x32 & 0x007fffff) | 0x800000) >> (23 - exp);
						xfrac_sgn = ((x32 & 0x007fffff) << (exp)) & 0x007fffff;
						xfrac_exp = -1;

						if( xfrac_sgn != 0 ){
							while( (xfrac_sgn & 0x400000) == 0 ){
								xfrac_exp--;
								xfrac_sgn <<= 1;
							}

							xfrac_sgn = (xfrac_sgn<<1) & 0x007fffff;
							xfrac_exp = (xfrac_exp+127) << 23;

							xfrac32 = xfrac_exp | xfrac_sgn;
						}
						else
							xfrac32 = 0;
					}
				}
			}

			xxfrac.u = xfrac32;
			xfrac = xxfrac.f;

			// calc result
			switch (xint & 0x3) {
				case 0:  rv = + (1.0 - xfrac); break;
//				case 1:  if(xfrac!=0){rv = - (      xfrac);}else{rv = + (      xfrac);} break;
				case 1:  rv = - (      xfrac); break; // alex
				case 2:  rv = - (1.0 - xfrac); break;
				case 3:  rv = + (      xfrac); break;
				default: assert(0==1); break; // internal error
			}
			// don't take the argument sign into account
			//if (x < 0.0) rv = - rv;
			break;
		default: assert(0==1); break; // internal error
	}

	// return result
//	printf("rv = %f",rv);
	return rv;

}
