/*******************************************************************************
* polynomial Manipulation
*
* We represent polynomials as a vector of coefficients with the highest power
* term on the left at vector index 0. The following polynomial manipulation
* functions are designed to behave like their counterparts in the Numerical
* Renaissance codebase.
*
* @ int rc_print_poly(rc_vector_t v)
*
* Like rc_print_vector, but assumes the contents represent a polynomial and
* prints the coefficients with trailing powers of x for easier reading. This
* relies on your terminal supporting unicode UTF-8.
*
* @ int rc_poly_conv(rc_vector_t a, rc_vector_t b, rc_vector_t* c)
*
* Convolutes the polynomials a&b and places the result in vector c. This finds
* the coefficients of the polynomials resulting from multiply a*b. The original
* contents of c are freed and new memory is allocated if necessary.
* returns 0 on success or -1 on failure.
*
* @ int rc_poly_power(rc_vector_t a, int n, rc_vector_t* b)
*
* Raises a polynomial a to itself n times where n is greater than or equal to 0.
* Places the result in vector b, any existing memory allocated for b is freed
* and its contents are lost. Returns 0 on success and -1 on failure.
*
* @ int rc_poly_add(rc_vector_t a, rc_vector_t b, rc_vector_t* c)
*
* Add two polynomials a&b with right justification and place the result in c.
* Any existing memory allocated for c is freed and its contents are lost.
* Returns 0 on success and -1 on failure.
*
* @ int rc_poly_add_inplace(rc_vector_t* a, rc_vector_t b)
*
* Adds polynomials b&a with right justification. The result is placed in vector
* a and a's original contents are lost. More memory is allocated for a if
* necessary. Returns 0 on success or -1 on failure.
*
* @ int rc_poly_subtract(rc_vector_t a, rc_vector_t b, rc_vector_t* c)
*
* Subtracts two polynomials a-b with right justification and place the result in
* c. Any existing memory allocated for c is freed and its contents are lost.
* Returns 0 on success and -1 on failure.
*
* @ int rc_poly_subtract_inplace(rc_vector_t* a, rc_vector_t b)
*
* Subtracts b from a with right justification. a stays in place and new memory
* is allocated only if b is longer than a.
*
* @ int rc_poly_differentiate(rc_vector_t a, int d, rc_vector_t* b)
*
* Calculates the dth derivative of the polynomial a and places the result in
* vector b. Returns 0 on success or -1 on failure.
*
* @ int rc_poly_divide(rc_vector_t n, rc_vector_t d, rc_vector_t* div, rc_vector_t* rem)
*
* Divides denominator d into numerator n. The remainder is placed into vector
* rem and the divisor is placed into vector div. Returns 0 on success or -1
* on failure.
*
* @ int rc_poly_butter(int N, float wc, rc_vector_t* b)
*
* Calculates vector of coefficients for continuous-time Butterworth polynomial
* of order N and cutoff wc (rad/s) and places them in vector b.
* Returns 0 on success or -1 on failure.
*******************************************************************************/
#ifndef RC_POLYNOMIAL_H
#define RC_POLYNOMIAL_H

#include <rc/math/vector.h>

int rc_print_poly(rc_vector_t v);
int rc_poly_conv(rc_vector_t a, rc_vector_t b, rc_vector_t* c);
int rc_poly_power(rc_vector_t a, int n, rc_vector_t* b);
int rc_poly_add(rc_vector_t a, rc_vector_t b, rc_vector_t* c);
int rc_poly_add_inplace(rc_vector_t* a, rc_vector_t b);
int rc_poly_subtract(rc_vector_t a, rc_vector_t b, rc_vector_t* c);
int rc_poly_subtract_inplace(rc_vector_t* a, rc_vector_t b);
int rc_poly_differentiate(rc_vector_t a, int d, rc_vector_t* b);
int rc_poly_divide(rc_vector_t n, rc_vector_t d, rc_vector_t* div, rc_vector_t* rem);
int rc_poly_butter(int N, float wc, rc_vector_t* b);

#endif // RC_POLYNOMIAL_H
