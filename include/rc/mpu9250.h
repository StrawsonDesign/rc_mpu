/******************************************************************************
* 9-AXIS IMU
*
* The Robotics Cape features an Invensense MPU9250 9-axis IMU. This API allows
* the user to configure this IMU in two modes: RANDOM and DMP
*
* RANDOM: The accelerometer, gyroscope, magnetometer, and thermometer can be
* read directly at any time. To use this mode, call rc_initialize_imu() with your
* imu_config and imu_data structs as arguments as defined below. You can then
* call rc_read_accel_data, rc_read_gyro_data, rc_read_mag_data, or rc_read_imu_temp
* at any time to get the latest sensor values.
*
* DMP: Stands for Digital Motion Processor which is a feature of the MPU9250.
* in this mode, the DMP will sample the sensors internally and fill a FIFO
* buffer with the data at a fixed rate. Furthermore, the DMP will also calculate
* a filtered orientation quaternion which is placed in the same buffer. When
* new data is ready in the buffer, the IMU sends an interrupt to the BeagleBone
* triggering the buffer read followed by the execution of a function of your
* choosing set with the rc_set_imu_interrupt_func() function.
*
* @ enum rc_accel_fsr_t rc_gyro_fsr_t
*
* The user may choose from 4 full scale ranges of the accelerometer and
* gyroscope. They have units of gravity (G) and degrees per second (DPS)
* The defaults values are A_FSR_4G and G_FSR_1000DPS respectively.
*
* enum rc_accel_dlpf_t rc_gyro_dlpf_t
*
* The user may choose from 7 digital low pass filter constants for the
* accelerometer and gyroscope. The filter runs at 1kz and helps to reduce sensor
* noise when sampling more slowly. The default values are ACCEL_DLPF_184
* GYRO_DLPF_250. Lower cut-off frequencies incur phase-loss in measurements.
*
* @ struct rc_imu_config_t
*
* Configuration struct passed to rc_initialize_imu and rc_initialize_imu_dmp. It is
* best to get the default config with rc_default_imu_config() function and
* modify from there.
*
* @ struct rc_imu_data_t
*
* This is the container for holding the sensor data from the IMU.
* The user is intended to make their own instance of this struct and pass
* its pointer to imu read functions.
*
* @ rc_imu_config_t rc_default_imu_config()
*
* Returns an rc_imu_config_t struct with default settings. Use this as a starting
* point and modify as you wish.
*
* @ int rc_initialize_imu(rc_imu_data_t *data, rc_imu_config_t conf)
*
* Sets up the IMU in random-read mode. First create an instance of the imu_data
* struct to point to as rc_initialize_imu will put useful data in it.
* rc_initialize_imu only reads from the config struct. After this, you may read
* sensor data.
*
* @ int rc_read_accel_data(rc_imu_data_t *data)
* @ int rc_read_gyro_data(rc_imu_data_t *data)
* @ int rc_read_mag_data(rc_imu_data_t *data)
* @ int rc_read_imu_temp(rc_imu_data_t* data)
*
* These are the functions for random sensor sampling at any time. Note that
* if you wish to read the magnetometer then it must be enabled in the
* configuration struct. Since the magnetometer requires additional setup and
* is slower to read, it is disabled by default.
*
******************************************************************************/
#ifndef RC_MPU9250_H
#define RC_MPU9250_H

#include <stdint.h>
#include "preprocessor_macros.h"

// defines for index location within TaitBryan and quaternion vectors
#define TB_PITCH_X	0
#define TB_ROLL_Y	1
#define TB_YAW_Z	2
#define QUAT_W		0
#define QUAT_X		1
#define QUAT_Y		2
#define QUAT_Z		3

typedef enum rc_accel_fsr_t{
	ACCEL_FSR_2G,
	ACCEL_FSR_4G,
	ACCEL_FSR_8G,
	ACCEL_FSR_16G
} rc_accel_fsr_t;

typedef enum rc_gyro_fsr_t{
	GYRO_FSR_250DPS,
	GYRO_FSR_500DPS,
	GYRO_FSR_1000DPS,
	GYRO_FSR_2000DPS
} rc_gyro_fsr_t;

typedef enum rc_accel_dlpf_t{
	ACCEL_DLPF_OFF,
	ACCEL_DLPF_460,
	ACCEL_DLPF_184,
	ACCEL_DLPF_92,
	ACCEL_DLPF_41,
	ACCEL_DLPF_20,
	ACCEL_DLPF_10,
	ACCEL_DLPF_5
} rc_accel_dlpf_t;

typedef enum rc_gyro_dlpf_t{
	GYRO_DLPF_OFF,
	GYRO_DLPF_250,
	GYRO_DLPF_184,
	GYRO_DLPF_92,
	GYRO_DLPF_41,
	GYRO_DLPF_20,
	GYRO_DLPF_10,
	GYRO_DLPF_5
} rc_gyro_dlpf_t;

typedef enum rc_imu_orientation_t{
	ORIENTATION_Z_UP	= 136,
	ORIENTATION_Z_DOWN	= 396,
	ORIENTATION_X_UP	= 14,
	ORIENTATION_X_DOWN	= 266,
	ORIENTATION_Y_UP	= 112,
	ORIENTATION_Y_DOWN	= 336,
	ORIENTATION_X_FORWARD	= 133,
	ORIENTATION_X_BACK	= 161
} rc_imu_orientation_t;

typedef struct rc_imu_config_t{
	int gpio_interrupt_pin;		// gpio pin, default 117 on Robotics Cape and BB Blue
	int i2c_bus;			// which bus to use, default 2 on Robotics Cape and BB Blue
	uint8_t i2c_addr;		// default is 0x68, pull pin ad0 high to make it 0x69
	int show_warnings;		// set to 1 to print i2c_bus warnings for debug

	// full scale ranges for sensors
	rc_accel_fsr_t accel_fsr;	// default: ACCEL_FSR_2G
	rc_gyro_fsr_t gyro_fsr;		// default: GYRO_FSR_2000DPS

	// internal low pass filter constants
	rc_accel_dlpf_t accel_dlpf;	// default ACCEL_DLPF_184
	rc_gyro_dlpf_t gyro_dlpf;	// default GYRO_DLPF_184

	// magnetometer use is optional
	int enable_magnetometer;	// 0 or 1

	// everything below here are DMP settings, only used with DMP interrupt
	int dmp_sample_rate;		// hertz, 200,100,50,40,25,20,10,8,5,4
	int dmp_fetch_accel_gyro;	// also get raw accel/gyro when reading dmp quaternion
	rc_imu_orientation_t orientation;// orientation matrix
	float compass_time_constant;	// time constant for filtering fused yaw
	int dmp_interrupt_priority;	// scheduler priority for handler
	// reading the magnetometer during DMP operation can add extra latency
	// To help this, by default the magnetometer will be read after the
	// user's interrupt service routine to reduce latency as much as possible.
	// set his flag to 0 to read the magnetometer before the ISR is called
	int read_mag_after_interrupt;	// default 1 (true)
	// the magnetometer only updates at 100hz, and sampling that fast generally
	// isn't necessary. Reduce the rate with this divider
	// sample rate = dmp_sample_rate/mag_sample_rate_div
	int mag_sample_rate_div; // default 4
	// connectivity options

} rc_imu_config_t;

typedef struct rc_imu_data_t{
	// last read sensor values in real units
	float accel[3];	// units of m/s^2
	float gyro[3];	// units of degrees/s
	float mag[3];	// units of uT
	float temp;		// units of degrees Celsius

	// 16 bit raw adc readings from each sensor
	int16_t raw_gyro[3];
	int16_t raw_accel[3];

	// FSR-derived conversion ratios from raw to real units
	float accel_to_ms2;	// to m/s^2
	float gyro_to_degs;	// to degrees/s

	// everything below this line is available in DMP mode only
	// quaternion and TaitBryan angles from DMP based on ONLY Accel/Gyro
	float dmp_quat[4];		// normalized quaternion
	float dmp_TaitBryan[3];	// radians pitch/roll/yaw X/Y/Z

	// If magnetometer is enabled in DMP mode, the following quaternion and
	// TaitBryan angles will be available which add magnetometer data to filter
	float fused_quat[4];		// normalized quaternion
	float fused_TaitBryan[3];	// radians pitch/roll/yaw X/Y/Z
	float compass_heading;		// heading filtered with gyro and accel data
	float compass_heading_raw;	// heading in radians from magnetometer
} rc_imu_data_t;

// General functions
rc_imu_config_t rc_default_imu_config();
int rc_set_imu_config_to_defaults(rc_imu_config_t* conf);
int rc_power_off_imu();

// one-shot sampling mode functions
int rc_initialize_imu(rc_imu_data_t* data, rc_imu_config_t conf);
int rc_read_accel_data(rc_imu_data_t* data);
int rc_read_gyro_data(rc_imu_data_t* data);
int rc_read_mag_data(rc_imu_data_t* data);
int rc_read_imu_temp(rc_imu_data_t* data);

// interrupt-driven sampling mode functions
int rc_initialize_imu_dmp(rc_imu_data_t* data, rc_imu_config_t conf);
int rc_set_imu_interrupt_func(void (*func)(void));
int rc_stop_imu_interrupt_func();
int rc_was_last_imu_read_successful();
uint64_t rc_nanos_since_last_imu_interrupt();

// other
int rc_calibrate_gyro_routine();
int rc_calibrate_mag_routine();
int rc_is_gyro_calibrated();
int rc_is_mag_calibrated();

#endif // RC_MPU9250_H
