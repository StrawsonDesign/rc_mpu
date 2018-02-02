/*******************************************************************************
* Quaternion Math
*
* @ float rc_quaternion_norm(rc_vector_t q)
*
* Returns the length of a quaternion vector by finding its 2-norm.
* Prints an error message and returns -1.0f on error.
*
* @ float rc_quaternion_norm_array(float q[4])
*
* Returns the length of a quaternion vector by finding its 2-norm.
* Prints an error message and returns -1.0f on error.
*
* @ int rc_normalize_quaternion(rc_vector_t* q)
*
* Normalizes a quaternion in-place to have length 1.0. Returns 0 on success.
* Returns -1 if the quaternion is uninitialized or has 0 length.
*
* @ int rc_normalize_quaternion_array(float q[4])
*
* Same as normalize_quaternion but performs the action on an array instead of
* a rc_vector_t type.
*
* @ int rc_quaternion_to_tb(rc_vector_t q, rc_vector_t* tb)
*
* Populates vector tb with 321 Tait Bryan angles in array order XYZ with
* operation order 321(yaw-Z, pitch-Y, roll-x). If tb is already allocated and of
* length 3 then the new values are written in place, otherwise any existing
* memory is freed and a new vector of length 3 is allocated for tb.
* Returns 0 on success or -1 on failure.
*
* @ void rc_quaternion_to_tb_array(float q[4], float tb[3])
*
* Same as rc_quaternion_to_tb but takes arrays instead.
*
* @ int rc_tb_to_quaternion(rc_vector_t tb, rc_vector_t* q)
*
* Populates quaternion vector q with the quaternion corresponding to the
* tait-bryan pitch-roll-yaw values in vector tb. If q is already of length 4
* then old contents are simply overwritten. Otherwise q'd existing memory is
* freed and new memory is allocated to avoid memory leaks.
* Returns 0 on success or -1 on failure.
*
* @ void rc_tb_to_quaternion_array(float tb[3], float q[4])
*
* Like rc_tb_to_quaternion but takes arrays as arguments.
*
* @ int rc_quaternion_conjugate(rc_vector_t q, rc_vector_t* c)
*
* Populates quaternion vector c with the conjugate of quaternion q where the 3
* imaginary parts ijk are multiplied by -1. If c is already of length 4 then the
* old values are overwritten. Otherwise the old memory in c is freed and new
* memory is allocated to help prevent memory leaks.
* Returns 0 on success or -1 on failure.
*
* @ int rc_quaternion_conjugate_inplace(rc_vector_t* q)
*
* Conjugates quaternion q by multiplying the 3 imaginary parts ijk by -1.
* Returns 0 on success or -1 on failure.
*
* @ void rc_quaternion_conjugate_array(float q[4], float c[4])
*
* Populates quaternion vector c with the conjugate of quaternion q where the 3
* imaginary parts ijk are multiplied by -1.
* Returns 0 on success or -1 on failure.
*
* @ void rc_quaternion_conjugate_array_inplace(float q[4])
*
* Conjugates quaternion q by multiplying the 3 imaginary parts ijk by -1.
* Returns 0 on success or -1 on failure.
*
* @ int rc_quaternion_imaginary_part(rc_vector_t q, rc_vector_t* img)
*
* Populates vector i with the imaginary components ijk of of quaternion vector
* q. If img is already of length 3 then its original contents are overwritten.
* Otherwise the original allocated memory is freed and new memory is allocated.
* Returns 0 on success or -1 on failure.
*
* @ int rc_quaternion_multiply(rc_vector_t a, rc_vector_t b, rc_vector_t* c)
*
* Calculates the quaternion Hamilton product ab=c and places the result in
* vector argument c. If c is already of length 4 then the old values are
* overwritten. Otherwise the old memory in c is freed and new memory is
* allocated to help prevent memory leaks.
* Returns 0 on success or -1 on failure.
*
* @ void rc_quaternion_multiply_array(float a[4], float b[4], float c[4])
*
* Calculates the quaternion Hamilton product ab=c and places the result in c
*
* @ int rc_rotate_quaternion(rc_vector_t* p, rc_vector_t q)
*
* Rotates the quaternion p by quaternion q with the operation p'=qpq*
* Returns 0 on success or -1 on failure.
*
* @ void rc_rotate_quaternion_array(float p[4], float q[4])
*
* Rotates the quaternion p by quaternion q with the operation p'=qpq*
*
* @ int rc_quaternion_rotate_vector(rc_vector_t* v, rc_vector_t q)
*
* Rotate a 3D vector v in-place about the origin by quaternion q by converting
* v to a quaternion and performing the operation p'=qpq*
* Returns 0 on success or -1 on failure.
*
* @ void rc_quaternion_rotate_vector_array(float v[3], float q[4])
*
* Rotate a 3D vector v in-place about the origin by quaternion q by converting
* v to a quaternion and performing the operation p'=qpq*
*
* @ int rc_quaternion_to_rotation_matrix(rc_vector_t q, rc_matrix_t* m)
*
* Populates m with a 3x3 orthogonal rotation matrix, q must be normalized!
* The orthogonal matrix corresponds to a rotation by the unit quaternion q
* when post-multiplied with a column vector as such: v_rotated=mv
* If m is already
* 3x3 then its contents are overwritten, otherwise its existing memory is freed
* and new memory is allocated.
* Returns 0 on success or -1 on failure.
*******************************************************************************/
#ifndef RC_QUATERNION_H
#define RC_QUATERNION_H

#include <rc/math/vector.h>
#include <rc/math/matrix.h>

float rc_quaternion_norm(rc_vector_t q);
float rc_quaternion_norm_array(float q[4]);
int   rc_normalize_quaternion(rc_vector_t* q);
int   rc_normalize_quaternion_array(float q[4]);
int   rc_quaternion_to_tb(rc_vector_t q, rc_vector_t* tb);
void  rc_quaternion_to_tb_array(float q[4], float tb[3]);
int   rc_tb_to_quaternion(rc_vector_t tb, rc_vector_t* q);
void  rc_tb_to_quaternion_array(float tb[3], float q[4]);
int   rc_quaternion_conjugate(rc_vector_t q, rc_vector_t* c);
int   rc_quaternion_conjugate_inplace(rc_vector_t* q);
void  rc_quaternion_conjugate_array(float q[4], float c[4]);
void  rc_quaternion_conjugate_array_inplace(float q[4]);
int   rc_quaternion_imaginary_part(rc_vector_t q, rc_vector_t* img);
int   rc_quaternion_multiply(rc_vector_t a, rc_vector_t b, rc_vector_t* c);
void  rc_quaternion_multiply_array(float a[4], float b[4], float c[4]);
int   rc_rotate_quaternion(rc_vector_t* p, rc_vector_t q);
void  rc_rotate_quaternion_array(float p[4], float q[4]);
int   rc_quaternion_rotate_vector(rc_vector_t* v, rc_vector_t q);
void  rc_quaternion_rotate_vector_array(float v[3], float q[4]);
int   rc_quaternion_to_rotation_matrix(rc_vector_t q, rc_matrix_t* m);

#endif // RC_QUATERNION_H
