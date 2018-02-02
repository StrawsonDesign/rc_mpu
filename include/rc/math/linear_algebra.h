/*******************************************************************************
* Linear Algebra
*
* @ int rc_matrix_times_col_vec(rc_matrix_t A, rc_vector_t v, rc_vector_t* c)
*
* Multiplies matrix A times column vector v and places the result in column
* vector c. Any existing data in c is freed if necessary and c is resized
* appropriately. Vectors v and c are interpreted as column vectors, but nowhere
* in their definitions are they actually specified as one or the other.
* Returns 0 on success and -1 on failure.
*
* @ int rc_row_vec_times_matrix(rc_vector_t v, rc_matrix_t A, rc_vector_t* c)
*
* Multiplies row vector v times matrix A and places the result in row
* vector c. Any existing data in c is freed if necessary and c is resized
* appropriately. Vectors v and c are interpreted as row vectors, but nowhere
* in their definitions are they actually specified as one or the other.
* Returns 0 on success and -1 on failure.
*
* @ float rc_matrix_determinant(rc_matrix_t A)
*
* Returns the determinant of square matrix A or -1.0f on failure.
*
* @ int rc_lup_decomp(rc_matrix_t A, rc_matrix_t* L, rc_matrix_t* U, rc_matrix_t* P)
*
* Performs LUP decomposition on matrix A with partial pivoting and places the
* result in matrices L,U,&P. Matrix A remains untouched and the original
* contents of LUP (if any) are freed and LUP are resized appropriately.
* Returns 0 on success or -1 on failure.
*
* @ int rc_qr_decomp(rc_matrix_t A, rc_matrix_t* Q, rc_matrix_t* R)
*
* Uses householder reflection method to find the QR decomposition of A.
* Returns 0 on success or -1 on failure.
*
* @ int rc_invert_matrix(rc_matrix_t A, rc_matrix_t* Ainv)
*
* Inverts Matrix A via LUP decomposition method and places the result in matrix
* Ainv. Any existing memory allocated for Ainv is freed if necessary and its
* contents are overwritten. Returns 0 on success or -1 on failure such as if
* matrix A is not invertible.
*
* @ int rc_invert_matrix_inplace(rc_matrix_t* A)
*
* Inverts Matrix A in place. The original contents of A are lost.
* Returns 0 on success or -1 on failure such as if A is not invertible.
*
* @ int rc_lin_system_solve(rc_matrix_t A, rc_vector_t b, rc_vector_t* x)
*
* Solves Ax=b for given matrix A and vector b. Places the result in vector x.
* existing contents of x are freed and new memory is allocated if necessary.
* Thank you to Henry Guennadi Levkin for open sourcing this routine, it's
* adapted here for RC use and includes better detection of unsolvable systems.
* Returns 0 on success or -1 on failure.
*
* @ int rc_lin_system_solve_qr(rc_matrix_t A, rc_vector_t b, rc_vector_t* x)
*
* Finds a least-squares solution to the system Ax=b for non-square A using QR
* decomposition method and places the solution in x.
* Returns 0 on success or -1 on failure.
*
* @ int rc_fit_ellipsoid(rc_matrix_t pts, rc_vector_t* ctr, rc_vector_t* lens)
*
* Fits an ellipsoid to a set of points in 3D space. The principle axes of the
* fitted ellipsoid align with the global coordinate system. Therefore there are
* 6 degrees of freedom defining the ellipsoid: the x,y,z coordinates of the
* centroid and the lengths from the centroid to the surface in each of the 3
* directions.
*
* rc_matrix_t 'pts' is a tall matrix with 3 columns and at least 6 rows.
* Each row must contain the x,y&z components of each individual point to be fit.
* If only 6 rows are provided, the resulting ellipsoid will be an exact fit.
* Otherwise the result is a least-squares fit to the over-defined dataset.
*
* The final x,y,z position of the centroid will be placed in vector 'ctr' and
* the lengths or radius from the centroid to the surface along each axis will
* be placed in the vector 'lens'
*
* Returns 0 on success or -1 on failure.
*******************************************************************************/
#ifndef RC_LINEAR_ALGEBRA_H
#define RC_LINEAR_ALGEBRA_H

#include <rc/math/matrix.h>

int   rc_matrix_times_col_vec(rc_matrix_t A, rc_vector_t v, rc_vector_t* c);
int   rc_row_vec_times_matrix(rc_vector_t v, rc_matrix_t A, rc_vector_t* c);
float rc_matrix_determinant(rc_matrix_t A);
int   rc_lup_decomp(rc_matrix_t A, rc_matrix_t* L, rc_matrix_t* U, rc_matrix_t* P);
int   rc_qr_decomp(rc_matrix_t A, rc_matrix_t* Q, rc_matrix_t* R);
int   rc_invert_matrix(rc_matrix_t A, rc_matrix_t* Ainv);
int   rc_invert_matrix_inplace(rc_matrix_t* A);
int   rc_lin_system_solve(rc_matrix_t A, rc_vector_t b, rc_vector_t* x);
int   rc_lin_system_solve_qr(rc_matrix_t A, rc_vector_t b, rc_vector_t* x);
int   rc_fit_ellipsoid(rc_matrix_t pts, rc_vector_t* ctr, rc_vector_t* lens);

#endif // RC_LINEAR_ALGEBRA_H
