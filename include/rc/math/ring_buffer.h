/*******************************************************************************
* Ring Buffer
*
* Ring buffers are FIFO (first in first out) buffers of fixed length which
* efficiently boot out the oldest value when full. They are particularly well
* suited for storing the last n values in a discrete time filter.
*
* The user creates their own instance of a buffer and passes a pointer to the
* these ring_buf functions to perform normal operations.
*
* @ int rc_alloc_ringbuf(rc_ringbuf_t* buf, int size)
*
* Allocates memory for a ring buffer and initializes an rc_ringbuf_t struct.
* If ring buffer b is already the right size then it is left untouched.
* Otherwise any existing memory allocated for buf is free'd to avoid memory
* leaks and new memory is allocated. Returns 0 on success or -1 on failure.
*
* @ rc_ringbuf_t rc_empty_ringbuf()
*
* Returns an rc_ringbuf_t struct which is completely zero'd out with no memory
* allocated for it. This is useful for declaring new ring buffers since structs
* declared inside of functions are not necessarily zero'd out which can cause
* the struct to contain problematic contents leading to segfaults. New ring
* buffers should be initialized with this before calling rc_alloc_ringbuf.
*
* @ int rc_free_ringbuf(rc_ringbuf_t* buf)
*
* Frees the memory allocated for buffer buf. Also set the initialized flag to 0
* so other functions don't try to access unallocated memory.
* Returns 0 on success or -1 on failure.
*
* @ int rc_reset_ringbuf(rc_ringbuf_t* buf)
*
* Sets all values in the buffer to 0 and sets the buffer index back to 0.
* Returns 0 on success or -1 on failure.
*
* @ int rc_insert_new_ringbuf_value(rc_ringbuf_t* buf, float val)
*
* Puts a new float into the ring buffer and updates the index accordingly.
* If the buffer was full then the oldest value in the buffer is automatically
* removed. Returns 0 on success or -1 on failure.
*
* @ float rc_get_ringbuf_value(rc_ringbuf_t* buf, int pos)
*
* Returns the float which is 'pos' steps behind the last value added to the
* buffer. If 'position' is given as 0 then the most recent value is returned.
* Position 'pos' obviously can't be larger than the buffer size minus 1.
* Prints an error message and return -1.0f on error.
*
* @ float rc_std_dev_ringbuf(rc_ringbuf_t buf)
*
* Returns the standard deviation of the values in the ring buffer.
*******************************************************************************/
#ifndef RC_RING_BUFFER_H
#define RC_RING_BUFFER_H

typedef struct rc_ringbuf_t {
	float* d;
	int size;
	int index;
	int initialized;
} rc_ringbuf_t;

int   rc_alloc_ringbuf(rc_ringbuf_t* buf, int size);
rc_ringbuf_t rc_empty_ringbuf();
int   rc_reset_ringbuf(rc_ringbuf_t* buf);
int   rc_free_ringbuf(rc_ringbuf_t* buf);
int   rc_insert_new_ringbuf_value(rc_ringbuf_t* buf, float val);
float rc_get_ringbuf_value(rc_ringbuf_t* buf, int position);
float rc_std_dev_ringbuf(rc_ringbuf_t buf);

#endif // RC_RING_BUFFER_H
