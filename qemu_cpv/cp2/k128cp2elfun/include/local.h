/**
 * \file include/local.h
 * \brief Library internal macroses and typedefs
 * \author Andrey Lesnykh <lesnykh@niisi.ras.ru>
 * \date 2007
 *
 * Library internal macroses and typedefs
 */

#ifndef __local_h__
#define __local_h__

#include <stdbool.h>
#include <inttypes.h>

// auxiliary macroses
#define bits32(u,l) ( (0xffffffffUL          >> (31-((u)-(l))) ) << (l) )
#define bits64(u,l) ( (0xffffffffffffffffULL >> (63-((u)-(l))) ) << (l) )
#define bit32(b)    (((uint32_t)(1)) << (b))
#define bit64(b)    (((uint64_t)(1)) << (b))
#define getbits32(_x, _up, _lo) ((((uint32_t)(_x)) & bits32((_up),(_lo))) >> (_lo))
#define getbits64(_x, _up, _lo) ((((uint64_t)(_x)) & bits64((_up),(_lo))) >> (_lo))

// types
// single floating point
typedef union {
	float    f;
	uint32_t i;
	struct {
		uint32_t m  : 23;
		uint32_t e  :  8;
		uint32_t s  :  1;
	} fields;
} k128cp2elfun_single_t;

// quadratic polinom coeffs table
typedef struct {
	int32_t c2;
	int32_t c1;
	int32_t c0;
} k128cp2elfun_table_t;

// quadratic polinom coeffs table size
#define K128CP2ELFUN_TABLES_SIZE 128

// elementaru function params
typedef struct {
	char                   *name;        // function name
	k128cp2elfun_table_t   *coefs_table; // pointer to quadratic polinom coeffs table
	k128cp2elfun_single_t  argmin;       // minimum argument value
	k128cp2elfun_single_t  argmax;       // maximum argument value
	k128cp2elfun_single_t  small_value;  // function value for small (i.e. x1=x2=0) arguments
	uint32_t               emin;         // minimum exponent value
	uint32_t               emax;         // maximum exponent value
} k128cp2elfun_params_t;

#endif // __local_h__
