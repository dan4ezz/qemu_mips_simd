/**
 * \file src/k128cp2elfun_rrsin.c
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
 * - rrsin(nan) = nan
 * - rrsin(+/- inf) = +/- 0
 * - rrsin(+/- BIG (exp>23)) = +/- 0
 * - rrsin(+ 0) = +0
 * - rrsin(- 0) = -0
 * - rrsin(denorm) = x
 * - rrsin(x) = sign(sin(pi/2 x)) * frac(|x|),     если x -- normalized, x\\in[2k,2k+1),   k-целое
 * - rrsin(x) = sign(sin(pi/2 x)) * (1-frac(|x|)), если x -- normalized, x\\in[2k+1,2k+2), k-целое
 *
 * Заметим, что для x>=0
 * - sign(sin(pi/2 x)) = +1, если [x]\%4 = 0 или 1
 * - sign(sin(pi/2 x)) = -1, если [x]\%4 = 2 или 3
 */
int RRSIN_WORD_NUM;
float k128cp2elfun_rrsin (float x) {

	float rv;
	uint32_t xint;
	k128cp2elfun_single_t xfrac;
	uint32_t xfrac_sgn, xfrac_exp;

	k128cp2elfun_single_t arg;
	arg.f=x;

	// switch
	switch (fpclassify (x)) {
		case FP_NAN       :  rv = x;   break;
		case FP_INFINITE  :  rv = NAN; break;
		case FP_ZERO      :  rv=x;     break;
		case FP_SUBNORMAL :  rv = x;   break;
		case FP_NORMAL    :
		{
			int exp;
			// get integer and fractional parts of absolute value
			// FIXME implement with intfrac (i.e. with Softfloat)
			exp = arg.fields.e - 127;

			if( exp < 0 )    return x;
			if( exp > 24 )   return (x<0.0)?-0.0:0.0; // RTL behavior
			// FIXME it's code emutate BUG from RTL model
//			if( exp == 24 )  return (x<0.0)?-0.0:0.0; alex // RTL behavior
			/////////////////////////////////////////////////
			if( exp == 24 ){ // NOTE don't del it code. If need, del code above.
				xint = arg.fields.m << 1;
				xfrac.f = 0;
			}else{
				xint      = (arg.fields.m | 0x800000) >> (23 - exp);
				xfrac_sgn = (arg.fields.m << exp) & 0x007fffff;
				xfrac_exp = -1;

				if( xfrac_sgn != 0 ){
					while( (xfrac_sgn & 0x400000) == 0 ){
						xfrac_exp--;
						xfrac_sgn <<= 1;
					}

					xfrac_sgn = (xfrac_sgn<<1) & 0x007fffff;
					xfrac_exp = (xfrac_exp+127) << 23;

					xfrac.i = xfrac_exp | xfrac_sgn;
				}
				else
					xfrac.i = 0;
			}

			// calc result
			switch (xint & 0x3) {
				case 0:  rv = + (      xfrac.f); break;
				case 1:  rv = + (1.0 - xfrac.f); break;
				case 2:  rv = - (xfrac.f); break;
				case 3:  rv = - (1.0 - xfrac.f); break;
				default: assert(0==1); break; // internal error
			}
			// take the argument sign into account
			if (x < 0.0) rv = - rv;
			arg.f=rv;
			break;
		}
		default: assert(0==1); break; // internal error
	}
	return rv;

}

