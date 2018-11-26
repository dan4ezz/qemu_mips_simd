#include <math.h>
#include <assert.h>
#include "local.h"
#include "softfloat.h"

extern float_status f_status;

//
float k128cp2elfun_common (k128cp2elfun_params_t *func_params, float x) {

	k128cp2elfun_single_t arg, res;
	uint32_t x1;
	k128cp2elfun_table_t tbl_entry;
	int64_t x2, c0, c1, c2, a0, a1, a2, a;
	uint32_t m;
	int nlz, mshift;

	// check argument value
	arg.f = x;
	assert (isnormal (x));
	assert (x >= func_params->argmin.f);
   	assert (x <= func_params->argmax.f);
	assert (arg.fields.e >= func_params->emin);
	assert (arg.fields.e <= func_params->emax);

	// unpack float
	// align mantissa
	m = bit32(23) | arg.fields.m;
	mshift = 126 - arg.fields.e;
	if (mshift > 23) {
		m = 0;
	} else if (mshift >= 0) {
		m = m >> mshift;
	} else {
		// for 1/x on [1.0,2.0) only
		assert (mshift == (-1));
		m = m << 1;
	}

	// get x1 and x2
	x2 = getbits32(m, 16,  0);
	x1 = getbits32(m, 23, 17);
	if ((x1 == 0) && (x2 == 0)) {
		res.f = func_params->small_value.f;
		f_status.float_exception_flags |= float_flag_inexact;
		goto return_result;
	}

	// get coefs from the table
	assert (x1 < K128CP2ELFUN_TABLES_SIZE);
	tbl_entry = func_params->coefs_table[x1];
	c0 = tbl_entry.c0;
	c1 = tbl_entry.c1;
	c2 = tbl_entry.c2;

	// calc aligned terms
	a0 = (c0          ) << 32;
	a1 = (c1 * x2     ) << 18;
	a2 = (c2 * x2 * x2) <<  0;

	// addition
	a = a0 + a1 + a2;
	assert (a > 0);

	// pack into float
	// get number of leading zeros
	assert (a != 0);
	nlz = 0;
	while ((a & bit64(63)) == 0) {
		a = a << 1;
		nlz += 1;
	}
	// get mantissa (with removed leading "1") and biased exponent
	res.fields.s = 0;
	res.fields.m = getbits64(a, 62, 63-24+1);
	res.fields.e = 132 - nlz;
	if( getbits64(a, 63-24+1, 0) )
		f_status.float_exception_flags |= float_flag_inexact;


return_result:
	// return result
	return res.f;

}


