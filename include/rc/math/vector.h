/*******************************************************************************
* Vectors
*
* The Vector and Matrix types here are used throughout the rest of the Robotics
* Cape library behind the scenes, but are also available to the user along with
* a collection of hardware-accelerated linear algebra functions. The premise
* of both types is that a small rc_vector_t and rc_matrix_t struct contains
* information about type's size and a pointer to where dynamically allocated
* memory exists that stores the actual data for the vector or matrix. Use
* rc_alloc_vector and rc_alloc_matrix to dynamically allocate memory for each
* new vector or matrix. Then use rc_free_vector and rc_free_matrix to free the
* memory when you are done using it. See the remaining vector, matrix, and
* linear algebra functions for more details.
*
* @ int rc_alloc_vector(rc_vector_t* v, int length)
*
* Allocates memory for vector v to have specified length. If v is initially the
* right length then nothing is done and the data in v is preserved. If v is
* uninitialized or of the wrong length then any existing memory is freed and new
* memory is allocated, helping to prevent accidental memory leaks. The contents
* of the new vector is not guaranteed to be anything in particular.
* Returns 0 if successful, otherwise returns -1. Will only be unsuccessful if
* length is invalid or there is insufficient memory available.
*
* @ int rc_free_vector(rc_vector_t* v)
*
* Frees the memory allocated for vector v and importantly sets the length and
* initialized flag of the rc_vector_t struct to 0 to indicate to other functions
* that v no longer points to allocated memory and cannot be used until more
* memory is allocated such as with rc_alloc_vector or rc_vector_zeros.
* Returns 0 on success. Will only fail and return -1 if it is passed a NULL
* pointer.
*
* @ rc_vector_t rc_empty_vector()
*
* Returns an rc_vector_t with no allocated memory and the initialized flag set
* to 0. This is useful for initializing vectors when they are declared since
* local variables declared in a function without global variable scope in C are
* not guaranteed to be zeroed out which can lead to bad memory pointers and
* segfaults if not handled carefully. We recommend initializing all
* vectors with this function before using rc_alloc_matrix or any other function.
*
* @ int rc_vector_zeros(rc_vector_t* v, int length)
*
* Resizes vector v and allocates memory for a vector with specified length.
* The new memory is pre-filled with zeros. Any existing memory allocated for v
* is freed if necessary to avoid memory leaks.
* Returns 0 on success or -1 on error.
*
* @ int rc_vector_ones(rc_vector_t* v, int length)
*
* Resizes vector v and allocates memory for a vector with specified length.
* The new memory is pre-filled with floating-point ones. Any existing memory
* allocated for v is freed if necessary to avoid memory leaks.
* Returns 0 on success or -1 on error.
*
* @ int rc_random_vector(rc_vector_t* v, int length)
*
* Resizes vector v and allocates memory for a vector with specified length.
* The new memory is pre-filled with random floating-point values between -1.0f
* and 1.0f. Any existing memory allocated for v is freed if necessary to avoid
* memory leaks.
* Returns 0 on success or -1 on error.
*
* @ int rc_vector_fibonnaci(rc_vector_t* v, int length)
*
* Creates a vector of specified length populated with the fibonnaci sequence.
* Returns 0 on success or -1 on error.
*
* @ int rc_vector_from_array(rc_vector_t* v, float* ptr, int length)
*
* Sometimes you will have a normal C-array of floats and wish to convert to
* rc_vector_t format for use with the other linear algebra functions.
* This function duplicates the contents of an array of floats into vector v and
* ensures v is sized correctly. Existing data in v (if any) is freed and lost.
* Returns 0 on success or -1 on failure.
*
* @ int rc_duplicate_vector(rc_vector_t a, rc_vector_t* b)
*
* Allocates memory for a duplicate of vector a and copies the contents into
* the new vector b. Simply making a copy of the rc_vector_t struct is not
* sufficient as the rc_vector_t struct simply contains a pointer to the memory
* allocated to contain the contents of the vector. rc_duplicate_vector sets b
* to be a new rc_vector_t with a pointer to freshly-allocated memory.
* Returns 0 on success or -1 on error.
*
* @ int rc_set_vector_entry(rc_vector_t* v, int pos, float val)
*
* Sets the entry of vector 'v' in position 'pos' to 'val' where the position is
* zero-indexed. In practice this is never used as it is much easier for the user
* to set values directly with this code:
*
* v.d[pos]=val;
*
* However, we provide this function for completeness. It is not strictly
* necessary for v to be provided as a pointer as a copy of the struct v
* would also contain the correct pointer to the original vector's allocated
* memory. However, in this library we use the convention of passing an
* rc_vector_t struct or rc_matrix_struct as a pointer when its data is to be
* modified by the function, and as a normal argument when it is only to be read
* by the function.
* Returns 0 on success or -1 on error.
*
* @ float rc_get_vector_entry(rc_vector_t v, int pos)
*
* Returns the entry of vector 'v' in position 'pos' where the position is
* zero-indexed. Returns -1.0f on failure and prints an error message to stderr.
* In practice this is never used as it is much easier for the user to read
* values directly with this code:
*
* val = v.d[pos];
*
* However, we provide this function for completeness. It also provides sanity
* checks to avoid possible segfaults.
*
* @ int rc_print_vector(rc_vector_t v)
*
* Prints to stdout the contents of vector v in one line. This is not advisable
* for extremely long vectors but serves for quickly debugging or printing
* results. It prints 4 decimal places with padding for a sign. We recommend
* rc_print_vector_sci() for very small or very large numbers where scientific
* notation would be more appropriate. Returns 0 on success or -1 on failure.
*
* @ int rc_print_vector_sci(rc_vector_t v)
*
* Prints to stdout the contents of vector v in one line. This is not advisable
* for extremely long vectors but serves for quickly debugging or printing
*
* @ int rc_vector_times_scalar(rc_vector_t* v, float s)
*
* Multiplies every entry in vector v by scalar s. It is not strictly
* necessary for v to be provided as a pointer since a copy of the struct v
* would also contain the correct pointer to the original vector's allocated
* memory. However, in this library we use the convention of passing an
* rc_vector_t struct or rc_matrix_struct as a pointer when its data is to be
* modified by the function, and as a normal argument when it is only to be read
* by the function.
* Returns 0 on success or -1 on failure.
*
* @ float rc_vector_norm(rc_vector_t v, float p)
*
* Just like the matlab norm(v,p) function, returns the vector norm defined by
* sum(abs(v)^p)^(1/p), where p is any positive real value. Most common norms
* are the 1 norm which gives the sum of absolute values of the vector and the
* 2-norm which is the square root of sum of squares.
* for infinity and -infinity norms see vector_max and vector_min
*
* @ int rc_vector_max(rc_vector_t v)
*
* Returns the index of the maximum value in v or -1 on failure. The value
* contained in the returned index is the equivalent to the infinity norm. If the
* max value occurs multiple times then the first instance is returned.
*
* @ int rc_vector_min(rc_vector_t v)
*
* Returns the index of the minimum value in v or -1 on failure. The value
* contained in the returned index is the equivalent to the minus-infinity norm.
* If the min value occurs multiple times then the first instance is returned.
*
* @ float rc_std_dev(rc_vector_t v)
*
* Returns the standard deviation of the values in a vector or -1.0f on failure.
*
* @ float rc_vector_mean(rc_vector_t v)
*
* Returns the mean (average) of all values in vector v or -1.0f on error.
*
* @ int rc_vector_projection(rc_vector_t v, rc_vector_t e, rc_vector_t* p)
*
* Populates vector p with the projection of vector v onto e.
* Returns 0 on success, otherwise -1.
*
* @ float rc_vector_dot_product(rc_vector_t v1, rc_vector_t v2)
*
* Returns the dot product of two equal-length vectors or floating-point -1.0f
* on error.
*
* @ int rc_vector_outer_product(rc_vector_t v1, rc_vector_t v2, rc_matrix_t* A)
*
* Computes v1 times v2 where v1 is a column vector and v2 is a row vector.
* Output is a matrix with same rows as v1 and same columns as v2.
* Returns 0 on success, otherwise -1.
*
* @ int rc_vector_cross_product(rc_vector_t v1, rc_vector_t v2, rc_vector_t* p)
*
* Computes the cross-product of two vectors, each of length 3. The result is
* placed in vector p and and existing memory used by p is freed and lost.
* Returns 0 on success, otherwise -1.
*
* @ int rc_vector_sum(rc_vector_t v1, rc_vector_t v2, rc_vector_t* s)
*
* Populates vector s with the sum of vectors v1 and v2. Any existing memory
* allocated for s is freed and lost, new memory is allocated if necessary.
* Returns 0 on success, otherwise -1.
*
* @ int rc_vector_sum_inplace(rc_vector_t* v1, rc_vector_t v2)
*
* Adds vector v2 to v1 and leaves the result in v1. The original contents of v1
* are lost and v2 is left untouched.
* Returns 0 on success, otherwise -1.
*******************************************************************************/
#ifndef RC_VECTOR_H
#define RC_VECTOR_H

// vector type
typedef struct rc_vector_t{
	int len;
	float* d;
	int initialized;
} rc_vector_t;

int   rc_alloc_vector(rc_vector_t* v, int length);
int   rc_free_vector(rc_vector_t* v);
rc_vector_t rc_empty_vector();
int   rc_vector_zeros(rc_vector_t* v, int length);
int   rc_vector_ones(rc_vector_t* v, int length);
int   rc_random_vector(rc_vector_t* v, int length);
int   rc_vector_fibonnaci(rc_vector_t* v, int length);
int   rc_vector_from_array(rc_vector_t* v, float* ptr, int length);
int   rc_duplicate_vector(rc_vector_t a, rc_vector_t* b);
int   rc_set_vector_entry(rc_vector_t* v, int pos, float val);
float rc_get_vector_entry(rc_vector_t v, int pos);
int   rc_print_vector(rc_vector_t v);
int   rc_print_vector_sci(rc_vector_t v);
int   rc_vector_times_scalar(rc_vector_t* v, float s);
float rc_vector_norm(rc_vector_t v, float p);
int   rc_vector_max(rc_vector_t v);
int   rc_vector_min(rc_vector_t v);
float rc_std_dev(rc_vector_t v);
float rc_vector_mean(rc_vector_t v);
int   rc_vector_projection(rc_vector_t v, rc_vector_t e, rc_vector_t* p);
float rc_vector_dot_product(rc_vector_t v1, rc_vector_t v2);
int   rc_vector_cross_product(rc_vector_t v1, rc_vector_t v2, rc_vector_t* p);
int   rc_vector_sum(rc_vector_t v1, rc_vector_t v2, rc_vector_t* s);
int   rc_vector_sum_inplace(rc_vector_t* v1, rc_vector_t v2);

#endif // RC_VECTOR_H
