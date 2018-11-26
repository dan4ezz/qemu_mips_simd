#include <math.h>

/**
 * \file include/k128cp2elfun.h
 * \brief Прикладной интерфейс библиотеки k128cp2elfun
 * \author Andrey Lesnykh <lesnykh@niisi.ras.ru>
 * \date 2007
 *
 * Application interface for k128cp2elfun library
 */

#ifndef __k128cp2elfun_h__
#define __k128cp2elfun_h__

/**
 *  \defgroup RRfun Функции приведения аргумента
 *
 *  Функции приведения аргумента,
 *  вычислямые с помощью алгоритма,
 *  который используется в k128cp2
 *
 *  \{
 */
float    k128cp2elfun_rrsin   (float x);
float    k128cp2elfun_rrcos   (float x);
float    k128cp2elfun_rrlog2m (float x);
float    k128cp2elfun_rrlog2e (float x);
/** \} */

/**
 *  \defgroup Elfun Элементарные функции
 *
 *  Элементарные функции, вычислямые с помощью
 *  алгоритма, который используется в k128cp2
 *
 *  \{
 */
float    k128cp2elfun_recip   (float x);
float    k128cp2elfun_sqrt    (float x);
float    k128cp2elfun_log2c   (float x);
float    k128cp2elfun_exp2    (float x);
float    k128cp2elfun_sinc    (float x);
float    k128cp2elfun_atanc   (float x);
/** \} */


/**
 *  \defgroup ElfunG Эталонные элементарные функции
 *
 *  Эталонные элементарные функции,
 *  вычислямые с помощью standard C math library
 *
 *  \{
 */
float    k128cp2elfun_recip_gold   (float x);
float    k128cp2elfun_sqrt_gold    (float x);
float    k128cp2elfun_log2c_gold   (float x);
float    k128cp2elfun_exp2_gold    (float x);
float    k128cp2elfun_sinc_gold    (float x);
float    k128cp2elfun_atanc_gold   (float x);
/** \} */

#endif // __k128cp2elfun_h__
