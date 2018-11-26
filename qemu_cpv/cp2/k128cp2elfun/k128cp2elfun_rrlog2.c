/**
 * \file src/k128cp2elfun_rrlog2.c
 * \brief Приведение аргумента синуса
 * \author Nikita Panyunin <panyunin@niisi.msk.ru>
 * \date 2008
 *
 */

#include <math.h>
#include <assert.h>
#include "local.h"
#include "softfloat.h"

union f32 {
	float f;
	uint32_t u;
};

float k128cp2elfun_rrlog2m (float x) {

	k128cp2elfun_single_t arg, res;

	// switch
	switch (fpclassify (x)) {
		case FP_NAN       :  res.f = NAN;      break;
		case FP_INFINITE  :  res.f = isinf(x)*INFINITY; break;
		case FP_ZERO      :  res.f = x;     break;
		case FP_SUBNORMAL :  res.f = x;     break;
		case FP_NORMAL    :
				arg.f = x;
				res.fields.s = arg.fields.s;
				res.fields.e = 127;
				res.fields.m = arg.fields.m;
				break;
		default: assert(0==1); break; // internal error
	}

	// return result
	return res.f;

}

float k128cp2elfun_rrlog2e (float x) {

	k128cp2elfun_single_t arg, res;

	// switch
	switch (fpclassify (x)) {
		case FP_NAN       :  res.f = 0;     break;
		case FP_INFINITE  :  res.f = 0; 	break;
		case FP_ZERO      :  res.f = 0;     break;
		case FP_SUBNORMAL :  res.f = 0;     break;
		case FP_NORMAL    :
				arg.f = x;
				res.f = arg.fields.e - 127;
				break;
		default: assert(0==1); break; // internal error
	}

	// return result
	return res.f;

}
