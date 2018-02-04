/**
 * @headerfile other.h <rc/math/other.h>
 *
 * @brief      general low-level math functions that don't fit elsewhere
 *
 * @author     James Strawson
 * @date       2016
 *
 * @addtogroup other_math
 * @ingroup math
 * @{
 */


#ifndef RC_MATH_OTHER_H
#define RC_MATH_OTHER_H

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * @brief      Returns a random floating point number between -1 and 1.
 *
 *             Uses standard C rand function and bitwise operations which is
 *             much faster than doing floating point arithmetic.
 *
 * @return     random floating point number between -1 and 1
 */
float rc_get_random_float();

/**
 * @brief      Returns a random double-precision floating point number between -1 and 1.
 *
 *             Uses standard C rand function and bitwise operations which is
 *             much faster than doing floating point arithmetic.
 *
 * @return     random double-precision floating point number between -1 and 1
 */
double rc_get_random_double();

/**
 * @brief      Modifies val to be bounded between between min and max.
 *
 * @param      val   The value to be checked and possibly modified
 * @param[in]  min   The lower bound
 * @param[in]  max   The upper bound
 *
 * @return     Returns 1 if saturation occurred, 0 if val was already in bound,
 *             and -1 if min was falsely larger than max.
 */
int rc_saturate_float(float* val, float min, float max);

/**
 * @brief      Modifies val to be bounded between between min and max.
 *
 * @param      val   The value to be checked and possibly modified
 * @param[in]  min   The lower bound
 * @param[in]  max   The upper bound
 *
 * @return     Returns 1 if saturation occurred, 0 if val was already in bound,
 *             and -1 if min was falsely larger than max.
 */
int rc_saturate_double(double* val, double min, double max);


#ifdef  __cplusplus
}
#endif

#endif // RC_MATH_OTHER_H

/** @}  end group math*/