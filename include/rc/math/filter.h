/*******************************************************************************
* Discrete SISO Filters
*
* This is a collection of functions for generating and implementing discrete
* SISO filters for arbitrary transfer functions.
*
* @ int rc_alloc_filter(rc_filter_t* f, rc_vector_t num, rc_vector_t den, float dt)
*
* Allocate memory for a discrete-time filter & populates it with the transfer
* function coefficients provided in vectors num and den. The memory in num and
* den is duplicated so those vectors can be reused or freed after allocating a
* filter without fear of disturbing the function of the filter. Argument dt is
* the timestep in seconds at which the user expects to operate the filter.
* The length of demonimator den must be at least as large as numerator num to
* avoid describing an improper transfer function. If rc_filter_t pointer f
* points to an existing filter then the old filter's contents are freed safely
* to avoid memory leaks. We suggest initializing filter f with rc_empty_filter
* before calling this function if it is not a global variable to ensure it does
* not accidentally contain invlaid contents such as null pointers. The filter's
* order is derived from the length of the denominator polynomial.
* Returns 0 on success or -1 on failure.
*
* @ int rc_alloc_filter_from_arrays(rc_filter_t* f,float dt,float* num,
					int numlen,float* den,int denlen)
*
* Like rc_alloc_filter(), but takes arrays for the numerator and denominator
* coefficients instead of vectors. Arrays num and denmust be the same length
* (order+1) like a semi-proper transfer function. Proper transfer functions with
* relative degree >=1 can still be used but the numerator must be filled with
* leading zeros. This function will throw a segmentation fault if your arrays
* are not both of length order+1. It is safer to use the rc_alloc_filter.
* Returns 0 on success or -1 on failure.
*
* @ int rc_free_filter(rc_filter_t* f)
*
* Frees the memory allocated by a filter's buffers and coefficient vectors. Also
* resets all filter properties back to 0. Returns 0 on success or -1 on failure.
*
* @ rc_filter_t rc_empty_filter()
*
* This is a very important function. If your d_filter_t struct is not a global
* variable, then its initial contents cannot be guaranteed to be anything in
* particular. Therefore it could contain problematic contents which could
* interfere with functions in this library. Therefore, you should always
* initialize your filters with rc_empty_filter before using with any other
* function in this library such as rc_alloc_filter. This serves the same
* purpose as rc_empty_matrix, rc_empty_vector, and rc_empty_ringbuf.
*
* @ int rc_print_filter(rc_filter_t f)
*
* Prints the transfer function and other statistic of a filter to the screen.
* only works on filters up to order 9
*
* @ float rc_march_filter(rc_filter_t* f, float new_input)
*
* March a filter forward one step with new input provided as an argument.
* Returns the new output which could also be accessed with filter.newest_output
* If saturation or soft-start are enabled then the output will automatically be
* bound appropriately. The steps counter is incremented by one and internal
* ring buffers are updated accordingly. Once a filter is created, this is
* typically the only function required afterwards.
*
* @ int rc_reset_filter(rc_filter_t* f)
*
* Resets all previous inputs and outputs to 0 and resets the step counter
* and saturation flag. This is sufficient to start the filter again as if it
* were just created. Returns 0 on success or -1 on failure.
*
* @ int rc_enable_saturation(rc_filter_t* f, float min, float max)
*
* If saturation is enabled for a specified filter, the filter will automatically
* bound the output between min and max. You may ignore this function if you wish
* the filter to run unbounded.
*
* @ int rc_did_filter_saturate(rc_filter_t* f)
*
* Returns 1 if the filter saturated the last time step. Returns 0 otherwise.
* This information could also be retrieved by looking at the 'sat_flag' value
* in the filter struct.
*
* @ int enable_soft_start(rc_filter_t* filter, float seconds)
*
* Enables soft start functionality where the output bound is gradually opened
* linearly from 0 to the normal saturation range. This occurs over the time
* specified from argument 'seconds' from when the filter is first created or
* reset. Saturation must already be enabled for this to work. This assumes that
* the user does indeed call rc_march_filter at roughly the same time interval
* as the 'dt' variable in the filter struct which is set at creation time.
* The soft-start property is maintained through a call to rc_reset_filter
* so the filter will soft-start again after each reset. This feature should only
* really be used for feedback controllers to prevent jerky starts.
* The saturation flag will not be set during this period as the output is
* usually expected to be bounded and we don't want to falsely trigger alarms
* or saturation counters. Returns 0 on success or -1 on failure.
*
* @ float rc_previous_filter_input(rc_filter_t* f, int steps)
*
* Returns the input 'steps' back in time. Steps = 0 returns most recent input.
* 'steps' must be between 0 and order inclusively as those are the
* only steps retained in memory for normal filter operation. To record values
* further back in time we suggest creating your own rc_ringbuf_t ring buffer.
* Returns -1.0f and prints an error message if there is an issue.
*
* @ float rc_previous_filter_output(rc_filter_t* f, int steps)
*
* Returns the input 'steps' back in time. Steps = 0 returns most recent input.
* 'steps' must be between 0 and order inclusively as those are the
* only steps retained in memory for normal filter operation. To record values
* further back in time we suggest creating your own rc_ringbuf_t ring buffer.
* Returns -1.0f and prints an error message if there is an issue.
*
* @ float rc_newest_filter_output(rc_filter_t* f)
*
* Returns the most recent output from the filter. Alternatively the user could
* access the 'newest_output' component of the rc_filter_t struct. Returns -1.0f
* and prints an error message if there is an issue.
*
* @ float rc_newest_filter_input(rc_filter_t* f)
*
* Returns the most recent input to the filter. Alternatively the user could
* access the 'newest_input' component of the rc_filter_t struct. Returns -1.0f
* and prints an error message if there is an issue.
*
* @ int rc_prefill_filter_inputs(rc_filter_t* f, float in)
*
* Fills all previous inputs to the filter as if they had been equal to 'in'
* Most useful when starting high-pass filters to prevent unwanted jumps in the
* output when starting with non-zero input.
* Returns 0 on success or -1 on failure.
*
* @ int prefill_filter_outputs(rc_filter_t* f, float out)
*
* Fills all previous outputs of the filter as if they had been equal to 'out'
* Most useful when starting low-pass filters to prevent unwanted settling time
* when starting with non-zero input. Returns 0 on success or -1 on failure.
*
* @ int rc_multiply_filters(rc_filter_t f1, rc_filter_t f2, rc_filter_t* f3)
*
* Creates a new filter f3 by multiplying f1*f2. The contents of f3 are freed
* safely if necessary and new memory is allocated to avoid memory leaks.
* Returns 0 on success or -1 on failure.
*
* @ int rc_c2d_tustin(rc_filter_t* f,rc_vector_t num,rc_vector_t den,float dt,float w)
*
* Creates a discrete time filter with similar dynamics to a provided continuous
* time transfer function using tustin's approximation with prewarping about a
* frequency of interest 'w' in radians per second.
*
* arguments:
* rc_vector_t num: 	continuous time numerator coefficients
* rc_vector_t den: 	continuous time denominator coefficients
* float dt:			desired timestep of discrete filter
* float w:			prewarping frequency in rad/s
*
* Any existing memory allocated for f is freed is necessary to prevent memory
* leaks. Returns 0 on success or -1 on failure.
*
* @ int rc_first_order_lowpass(rc_filter_t* f, float dt, float time_constant)
*
* Creates a first order low pass filter. Any existing memory allocated for f is
* freed safely to avoid memory leaks and new memory is allocated for the new
* filter. dt is in units of seconds and time_constant is the number of seconds
* it takes to rise to 63.4% of a steady-state input. This can be used alongside
* rc_first_order_highpass to make a complementary filter pair.
* Returns 0 on success or -1 on failure.
*
* @ int rc_first_order_highpass(rc_filter_t* f, float dt, float time_constant)
*
* Creates a first order high pass filter. Any existing memory allocated for f is
* freed safely to avoid memory leaks and new memory is allocated for the new
* filter. dt is in units of seconds and time_constant is the number of seconds
* it takes to decay by 63.4% of a steady-state input. This can be used alongside
* rc_first_order_lowpass to make a complementary filter pair.
* Returns 0 on success or -1 on failure.
*
* @ int rc_butterworth_lowpass(rc_filter_t* f, int order, float dt, float wc)
*
* Creates a Butterworth low pass filter of specified order and cutoff frequency
* wc in rad/s. The user must also specify the discrete filter's timestep dt
* in seconds. Any existing memory allocated for f is freed safely to avoid
* memory leaks and new memory is allocated for the new filter.
* Returns 0 on success or -1 on failure.
*
* @ int rc_butterworth_highpass(rc_filter_t* f, int order, float dt, float wc)
*
* Creates a Butterworth high pass filter of specified order and cutoff frequency
* wc in rad/s. The user must also specify the discrete filter's timestep dt
* in seconds. Any existing memory allocated for f is freed safely to avoid
* memory leaks and new memory is allocated for the new filter.
* Returns 0 on success or -1 on failure.
*
* @ int rc_moving_average(rc_filter_t* f, int samples, int dt)
*
* Makes a FIR moving average filter that averages over 'samples' which must be
* greater than or equal to 2 otherwise no averaging would be performed. Any
* existing memory allocated for f is freed safely to avoid memory leaks and new
* memory is allocated for the new filter. Returns 0 on success or -1 on failure.
*
* @ int rc_integrator(rc_filter_t *f, float dt)
*
* Creates a first order integrator. Like most functions here, the dynamics are
* only accurate if the filter is called with a timestep corresponding to dt.
* Any existing memory allocated for f is freed safely to avoid memory leaks and
* new memory is allocated for the new filter.
* Returns 0 on success or -1 on failure.
*
* @ int rc_double_integrator(rc_filter_t* f, float dt)
*
* Creates a second order double integrator. Like most functions here, the
* dynamics are only accurate if the filter is called with a timestep
* corresponding to dt. Any existing memory allocated for f is freed safely to
* avoid memory leaks and new memory is allocated for the new filter.
* Returns 0 on success or -1 on failure.
*
* @ int rc_pid_filter(rc_filter_t* f,float kp,float ki,float kd,float Tf,float dt)
*
* Creates a discrete-time implementation of a parallel PID controller with
* high-frequency rolloff. This is equivalent to the Matlab function:
* C = pid(Kp,Ki,Kd,Tf,Ts)
*
* We cannot implement a pure differentiator with a discrete transfer function
* so this filter has high frequency rolloff with time constant Tf. Smaller Tf
* results in less rolloff, but Tf must be greater than dt/2 for stability.
* Returns 0 on success or -1 on failure.
*******************************************************************************/
#ifndef RC_FILTER_H
#define RC_FILTER_H

#include <rc/math/vector.h>
#include <rc/math/ring_buffer.h>

typedef struct rc_filter_t{
	// transfer function properties
	int order;		// transfer function order
	float dt;		// timestep in seconds
	float gain;		// gain usually 1.0
	rc_vector_t num;	// numerator coefficients
	rc_vector_t den;	// denominator coefficients
	// saturation settings
	int sat_en;		// set to 1 by enable_saturation()
	float sat_min;		// lower saturation limit
	float sat_max;		// upper saturation limit
	int sat_flag;		// 1 if saturated on the last step
	// soft start settings
	int ss_en;		// set to 1 by enbale_soft_start()
	float ss_steps;		// steps before full output allowed
	// dynamically allocated ring buffers
	rc_ringbuf_t in_buf;
	rc_ringbuf_t out_buf;
	// newest input and output for quick reference
	float newest_input;	// shortcut for the most recent input
	float newest_output;	// shortcut for the most recent output
	// other
	uint64_t step;		// steps since last reset
	int initialized;	// initialization flag
} rc_filter_t;

int   rc_alloc_filter(rc_filter_t* f, rc_vector_t num, rc_vector_t den, float dt);
int   rc_alloc_filter_from_arrays(rc_filter_t* f,float dt,float* num,int numlen,\
							float* den,int denlen);
int   rc_free_filter(rc_filter_t* f);
rc_filter_t rc_empty_filter();
int   rc_print_filter(rc_filter_t f);
float rc_march_filter(rc_filter_t* f, float new_input);
int   rc_reset_filter(rc_filter_t* f);
int   rc_enable_saturation(rc_filter_t* f, float min, float max);
int   rc_did_filter_saturate(rc_filter_t* f);
int   rc_enable_soft_start(rc_filter_t* f, float seconds);
float rc_previous_filter_input(rc_filter_t* f, int steps);
float rc_previous_filter_output(rc_filter_t* f, int steps);
float rc_newest_filter_output(rc_filter_t* f);
float rc_newest_filter_input(rc_filter_t* f);
int   rc_prefill_filter_inputs(rc_filter_t* f, float in);
int   rc_prefill_filter_outputs(rc_filter_t* f, float out);
int   rc_multiply_filters(rc_filter_t f1, rc_filter_t f2, rc_filter_t* f3);
int   rc_c2d_tustin(rc_filter_t* f,rc_vector_t num,rc_vector_t den,float dt,float w);
int   rc_first_order_lowpass(rc_filter_t* f, float dt, float time_constant);
int   rc_first_order_highpass(rc_filter_t* f, float dt, float time_constant);
int   rc_butterworth_lowpass(rc_filter_t* f, int order, float dt, float wc);
int   rc_butterworth_highpass(rc_filter_t* f, int order, float dt, float wc);
int   rc_moving_average(rc_filter_t* f, int samples, int dt);
int   rc_integrator(rc_filter_t *f, float dt);
int   rc_double_integrator(rc_filter_t* f, float dt);
int   rc_pid_filter(rc_filter_t* f,float kp,float ki,float kd,float Tf,float dt);

#endif // RC_FILTER_H
