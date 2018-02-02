/*******************************************************************************
* rc_algebra_common.h
*
* all things shared between rc_vector.c, rc_matrix.c, and rc_linear_algebra.c
*******************************************************************************/

#ifndef RC_ALGEBRA_COMMON_H
#define RC_ALGEBRA_COMMON_H

#include "rc/preprocessor_macros.h"

#define ZERO_TOLERANCE 1e-8 // consider v to be zero if fabs(v)<ZERO_TOLERANCE

/*******************************************************************************
* float rc_mult_accumulate(float * __restrict__ a, float * __restrict__ b, int n)
*
* Performs a vector dot product on the contents of a and b over n values.
* This is a dangerous function that could segfault if not used properly. Hence
* it is only for internal use in the RC library. the 'restrict' attributes tell
* the C compiler that the pointers are not aliased which helps the vectorization
* process for optimization with the NEON FPU.
*******************************************************************************/
float rc_mult_accumulate(float * __restrict__ a, float * __restrict__ b, int n);

#endif // RC_ALGEBRA_COMMON_H
