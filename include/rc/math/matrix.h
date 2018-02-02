/*******************************************************************************
* Matrix
*
* @ int rc_alloc_matrix(rc_matrix_t* A, int rows, int cols)
*
* Allocates memory for matrix A to have new dimensions given by arguments rows
* and cols. If A is initially the right size, nothing is done and the data in A
* is preserved. If A is uninitialized or of the wrong size then any existing
* memory is freed and new memory is allocated, helping to prevent accidental
* memory leaks. The contents of the new matrix is not guaranteed to be anything
* in particular.
* Returns 0 on success, otherwise -1. Will only be unsuccessful if
* rows&cols are invalid or there is insufficient memory available.
*
* @ int rc_free_matrix(rc_matrix_t* A)
*
* Frees the memory allocated for a matrix A and importantly sets the dimensions
* and initialized flag of the rc_matrix_t struct to 0 to indicate to other
* functions that A no longer points to allocated memory and cannot be used until
* more memory is allocated such as with rc_alloc_matrix or rc_matrix_zeros.
* Returns 0 on success. Will only fail and return -1 if it is passed a NULL
* pointer.
*
* @ rc_matrix_t rc_empty_matrix()
*
* Returns an rc_matrix_t with no allocated memory and the initialized flag set
* to 0. This is useful for initializing rc_matrix_t structs when they are
* declared since local variables declared in a function without global variable
* scope in C are not guaranteed to be zeroed out which can lead to bad memory
* pointers and segfaults if not handled carefully. We recommend initializing all
* matrices with this before using rc_alloc_matrix or any other function.
*
* @ int rc_matrix_zeros(rc_matrix_t* A, int rows, int cols)
*
* Resizes matrix A and allocates memory for a matrix with specified rows &
* columns. The new memory is pre-filled with zeros. Any existing memory
* allocated for A is freed if necessary to avoid memory leaks.
* Returns 0 on success or -1 on error.
*
* @ int rc_identity_matrix(rc_matrix_t* A, int dim)
*
* Resizes A to be a square identity matrix with dimensions dim-by-dim. Any
* existing memory allocated for A is freed if necessary to avoid memory leaks.
* Returns 0 on success or -1 on failure.
*
* @ int rc_random_matrix(rc_matrix_t* A, int rows, int cols)
*
* Resizes A to be a matrix with the specified number of rows and columns and
* populates the new memory with random numbers evenly distributed between -1.0
* and 1.0. Any existing memory allocated for A is freed if necessary to avoid
* memory leaks. Returns 0 on success or -1 on failure.
*
* @ int rc_diag_matrix(rc_matrix_t* A, rc_vector_t v)
*
* Resizes A to be a square matrix with the same number of rows and columns as
* vector v's length. The diagonal entries of A are then populated with the
* contents of v and the off-diagonal entries are set to 0. The original contents
* of A are freed to avoid memory leaks.
* Returns 0 on success or -1 on failure.
*
* @ int rc_duplicate_matrix(rc_matrix_t A, rc_matrix_t* B)
*
* Makes a duplicate of the data from matrix A and places into matrix B. If B is
* already the right size then its contents are overwritten. If B is unallocated
* or is of the wrong size then the memory is freed if necessary and new memory
* is allocated to hold the duplicate of A.
* Returns 0 on success or -1 on error.
*
* @ int rc_set_matrix_entry(rc_matrix_t* A, int row, int col, float val)
*
* Sets the specified single entry of matrix A to 'val' where the position is
* zero-indexed at the top-left corner. In practice this is never used as it is
* much easier for the user to set values directly with this code:
*
* A.d[row][col]=val;
*
* However, we provide this function for completeness. It is not strictly
* necessary for A to be provided as a pointer since a copy of the struct A
* would also contain the correct pointer to the original matrix's allocated
* memory. However, in this library we use the convention of passing an
* rc_vector_t struct or rc_matrix_struct as a pointer when its data is to be
* modified by the function, and as a normal argument when it is only to be read
* by the function. Returns 0 on success or -1 on error.
*
* @ float rc_get_matrix_entry(rc_matrix_t A, int row, int col)
*
* Returns the specified single entry of matrix 'A' in position 'pos' where the
* position is zero-indexed. Returns -1.0f on failure and prints an error message
* to stderr. In practice this is never used as it is much easier for the user to
* read values directly with this code:
*
* val = A.d[row][col];
*
* However, we provide this function for completeness. It also provides sanity
* checks to avoid possible segfaults.
*
* @ int rc_print_matrix(rc_matrix_t A)
*
* Prints the contents of matrix A to stdout in decimal notation with 4 decimal
* places. Not recommended for very large matrices as rows will typically
* linewrap if the terminal window is not wide enough.
*
* @ void rc_print_matrix_sci(rc_matrix_t A)
*
* Prints the contents of matrix A to stdout in scientific notation with 4
* significant figures. Not recommended for very large matrices as rows will
* typically linewrap if the terminal window is not wide enough.
*
* @ int rc_matrix_times_scalar(rc_matrix_t* A, float s)
*
* Multiplies every entry in A by scalar value s. It is not strictly
* necessary for A to be provided as a pointer since a copy of the struct A
* would also contain the correct pointer to the original matrix's allocated
* memory. However, in this library we use the convention of passing an
* rc_vector_t struct or rc_matrix_struct as a pointer when its data is to be
* modified by the function, and as a normal argument when it is only to be read
* by the function. Returns 0 on success or -1 on failure.
*
* @ int rc_multiply_matrices(rc_matrix_t A, rc_matrix_t B, rc_matrix_t* C)
*
* Multiplies A*B=C. C is resized and its original contents are freed if
* necessary to avoid memory leaks. Returns 0 on success or -1 on failure.
*
* @ int rc_left_multiply_matrix_inplace(rc_matrix_t A, rc_matrix_t* B)
*
* Multiplies A*B and puts the result back in the place of B. B is resized and
* its original contents are freed if necessary to avoid memory leaks.
* Returns 0 on success or -1 on failure.
*
* @ int rc_right_multiply_matrix_inplace(rc_matrix_t* A, rc_matrix_t B)
*
* Multiplies A*B and puts the result back in the place of A. A is resized and
* its original contents are freed if necessary to avoid memory leaks.
* Returns 0 on success or -1 on failure.
*
* @ int rc_add_matrices(rc_matrix_t A, rc_matrix_t B, rc_matrix_t* C)
*
* Resizes matrix C and places the sum A+B in C. The original contents of C are
* safely freed if necessary to avoid memory leaks.
* Returns 0 on success or -1 on failure.
*
* @ int rc_add_matrices_inplace(rc_matrix_t* A, rc_matrix_t B)
*
* Adds matrix B to A and places the result in A so the original contents of A
* are lost. Use rc_add_matrices if you wish to keep the contents of both matrix
* A and B. Returns 0 on success or -1 on failure.
*
* @ int rc_matrix_transpose(rc_matrix_t A, rc_matrix_t* T)
*
* Resizes matrix T to hold the transposed contents of A and leaves A untouched.
* Returns 0 on success or -1 on failure.
*
* @ int rc_matrix_transpose_inplace(rc_matrix_t* A)
*
* Transposes matrix A in place. Use as an alternative to rc_matrix_transpose
* if you no longer have need for the original contents of matrix A.
* Returns 0 on success or -1 on failure.
*******************************************************************************/

#ifndef RC_MATRIX_H
#define RC_MATRIX_H

#include <rc/math/vector.h>

// matrix type
typedef struct rc_matrix_t{
	int rows;
	int cols;
	float** d;
	int initialized;
} rc_matrix_t;

int   rc_alloc_matrix(rc_matrix_t* A, int rows, int cols);
int   rc_free_matrix(rc_matrix_t* A);
rc_matrix_t rc_empty_matrix();
int   rc_matrix_zeros(rc_matrix_t* A, int rows, int cols);
int   rc_identity_matrix(rc_matrix_t* A, int dim);
int   rc_random_matrix(rc_matrix_t* A, int rows, int cols);
int   rc_diag_matrix(rc_matrix_t* A, rc_vector_t v);
int   rc_duplicate_matrix(rc_matrix_t A, rc_matrix_t* B);
int   rc_set_matrix_entry(rc_matrix_t* A, int row, int col, float val);
float rc_get_matrix_entry(rc_matrix_t A, int row, int col);
int   rc_print_matrix(rc_matrix_t A);
void  rc_print_matrix_sci(rc_matrix_t A);
int   rc_matrix_times_scalar(rc_matrix_t* A, float s);
int   rc_multiply_matrices(rc_matrix_t A, rc_matrix_t B, rc_matrix_t* C);
int   rc_left_multiply_matrix_inplace(rc_matrix_t A, rc_matrix_t* B);
int   rc_right_multiply_matrix_inplace(rc_matrix_t* A, rc_matrix_t B);
int   rc_add_matrices(rc_matrix_t A, rc_matrix_t B, rc_matrix_t* C);
int   rc_add_matrices_inplace(rc_matrix_t* A, rc_matrix_t B);
int   rc_matrix_transpose(rc_matrix_t A, rc_matrix_t* T);
int   rc_matrix_transpose_inplace(rc_matrix_t* A);
int   rc_vector_outer_product(rc_vector_t v1, rc_vector_t v2, rc_matrix_t* A);

#endif // RC_MATRIX_H
