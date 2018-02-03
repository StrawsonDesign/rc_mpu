/**
 * @headerfile mpu.h s<rc/mpu.h>
 *
 * @brief      A C interface for the Linux gpio driver.
 *
 *             Developed and tested on the BeagleBone Black but should work fine
 *             on any Linux system.
 *
 * @author     James Strawson
 *
 * @date       1/19/2018
 */

/** @addtogroup Sensors */
/** @{ */

#ifndef RC_MPU_H
#define RC_MPU_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>


// defines for index location within TaitBryan and quaternion vectors
#define TB_PITCH_X	0
#define TB_ROLL_Y	1
#define TB_YAW_Z	2
#define QUAT_W		0
#define QUAT_X		1
#define QUAT_Y		2
#define QUAT_Z		3

#define DEG_TO_RAD	0.0174532925199
#define MS2_TO_G	0.10197162129

/**
 * @brief accelerometer full scale range options
 */
typedef enum rc_mpu_accel_fsr_t{
	ACCEL_FSR_2G,
	ACCEL_FSR_4G,
	ACCEL_FSR_8G,
	ACCEL_FSR_16G
} rc_mpu_accel_fsr_t;

/**
 * @brief gyroscope full scale range options
 */
typedef enum rc_mpu_gyro_fsr_t{
	GYRO_FSR_250DPS,
	GYRO_FSR_500DPS,
	GYRO_FSR_1000DPS,
	GYRO_FSR_2000DPS
} rc_mpu_gyro_fsr_t;

/**
 * @brief      accelerometer digital low-pass filter options
 *
 *             Number is cutoff frequency in hz.
 */
typedef enum rc_mpu_accel_dlpf_t{
	ACCEL_DLPF_OFF,
	ACCEL_DLPF_460,
	ACCEL_DLPF_184,
	ACCEL_DLPF_92,
	ACCEL_DLPF_41,
	ACCEL_DLPF_20,
	ACCEL_DLPF_10,
	ACCEL_DLPF_5
} rc_mpu_accel_dlpf_t;

/**
 * @brief      gyroscope digital low-pass filter options
 *
 *             Number is cutoff frequency in hz.
 */
typedef enum rc_mpu_gyro_dlpf_t{
	GYRO_DLPF_OFF,
	GYRO_DLPF_250,
	GYRO_DLPF_184,
	GYRO_DLPF_92,
	GYRO_DLPF_41,
	GYRO_DLPF_20,
	GYRO_DLPF_10,
	GYRO_DLPF_5
} rc_mpu_gyro_dlpf_t;


/**
 * @brief      Orientation of the sensor.
 *
 *             This is only applicable when operating in DMP mode. This is the
 *             orientation that the DMP consideres neutral, aka where
 *             roll/pitch/yaw are zero.
 */
typedef enum rc_mpu_orientation_t{
	ORIENTATION_Z_UP	= 136,
	ORIENTATION_Z_DOWN	= 396,
	ORIENTATION_X_UP	= 14,
	ORIENTATION_X_DOWN	= 266,
	ORIENTATION_Y_UP	= 112,
	ORIENTATION_Y_DOWN	= 336,
	ORIENTATION_X_FORWARD	= 133,
	ORIENTATION_X_BACK	= 161
} rc_mpu_orientation_t;

/**
 * @brief configuration of the mpu sensor
 */
typedef struct rc_mpu_config_t{
	/** @name physical connection configuration */
	///@{
	int gpio_interrupt_pin;		///< gpio pin, default 117 on Robotics Cape and BB Blue
	int i2c_bus;			///< which bus to use, default 2 on Robotics Cape and BB Blue
	uint8_t i2c_addr;		///< default is 0x68, pull pin ad0 high to make it 0x69
	int show_warnings;		///< set to 1 to print i2c_bus warnings for debug
	///@}

	/** @name accelerometer, gyroscope, and magnetometer configuration */
	///@{
	rc_mpu_accel_fsr_t accel_fsr;	///< accelerometer full scale range, default ACCEL_FSR_2G
	rc_mpu_gyro_fsr_t gyro_fsr;	///< gyroscope full scale range, default GYRO_FSR_2000DPS
	rc_mpu_accel_dlpf_t accel_dlpf;	///< internal low pass filter cutoff, default ACCEL_DLPF_184
	rc_mpu_gyro_dlpf_t gyro_dlpf;	///< internal low pass filter cutoff, default GYRO_DLPF_184
	int enable_magnetometer;	///< magnetometer use is optional, set to 1 to enable, default 0 (off)
	///@}

	/** @name DMP settings, only used with DMP mode */
	///@{
	int dmp_sample_rate;		///< sample rate in hertz, 200,100,50,40,25,20,10,8,5,4
	int dmp_fetch_accel_gyro;	///< set to 1 to optionally raw accel/gyro when reading DMP quaternion, default: 0 (off)
	int dmp_auto_calibrate_gyro;	///< set to 1 to let DMP auto calibrate the gyro while in use, default: 0 (off)
	rc_mpu_orientation_t orient;	///< DMP orientation matrix, see rc_mpu_orientation_t
	float compass_time_constant;	///< time constant (seconds) for filtering compass with gyroscope yaw value, default 25
	int dmp_interrupt_priority;	///< scheduler priority for handling DMP interrupt and user's callback
	int read_mag_after_callback;	///< reads magnetometer after DMP callback function to improve latency, default 1 (true)
	int mag_sample_rate_div;	///< magnetometer_sample_rate = dmp_sample_rate/mag_sample_rate_div, default: 4
	int tap_threshold;		///< threshold impulse for triggering a tap in units of mg/ms
	///@}

} rc_mpu_config_t;

/**
 * @brief data struct populated with new sensor data
 */
typedef struct rc_mpu_data_t{
	/** @name base sensor readings in real units */
	///@{
	float accel[3];		///< accelerometer (XYZ) in units of m/s^2
	float gyro[3];		///< gyroscope (XYZ) in units of degrees/s
	float mag[3];		///< magnetometer (XYZ) in units of uT
	float temp;		///< thermometer, in units of degrees Celsius
	///@}

	/** @name 16 bit raw adc readings and conversion rates*/
	///@{
	int16_t raw_gyro[3];	///< raw gyroscope (XYZ)from 16-bit ADC
	int16_t raw_accel[3];	///< raw accelerometer (XYZ) from 16-bit ADC
	float accel_to_ms2;	///< conversion rate from raw accelerometer to m/s^2
	float gyro_to_degs;	///< conversion rate from raw gyroscope to degrees/s
	///@}

	/** @name DMP data */
	///@{
	float dmp_quat[4];	///< normalized quaternion from DMP based on ONLY Accel/Gyro
	float dmp_TaitBryan[3];	///< Tait-Bryan angles (roll pitch yaw) in radians from DMP based on ONLY Accel/Gyro
	int tap_detected;	///< set to 1 if there was a tap detect on the last dmp sample period
	int last_tap_direction;	///< direction of last tap, 1-6 corresponding to X+ X- Y+ Y- Z+ Z-
	///@}

	/** @name fused DMP data filtered with magnetometer */
	///@{
	float fused_quat[4];		///< fused and normalized quaternion
	float fused_TaitBryan[3];	///< fused Tait-Bryan angles (roll pitch yaw) in radians
	float compass_heading;		///< fused heading filtered with gyro and accel data, same as Tait-Bryan yaw
	float compass_heading_raw;	///< unfiltered heading from magnetometer
	///@}
} rc_mpu_data_t;

// General functions
rc_mpu_config_t rc_mpu_default_config();
int rc_mpu_set_config_to_default(rc_mpu_config_t* conf);
int rc_mpu_power_off();

// one-shot sampling mode functions
int rc_mpu_initialize(rc_mpu_data_t* data, rc_mpu_config_t conf);
int rc_mpu_read_accel(rc_mpu_data_t* data);
int rc_mpu_read_gyro(rc_mpu_data_t* data);
int rc_mpu_read_mag(rc_mpu_data_t* data);
int rc_mpu_read_temp(rc_mpu_data_t* data);

// interrupt-driven sampling mode functions
int rc_mpu_initialize_dmp(rc_mpu_data_t* data, rc_mpu_config_t conf);
int rc_mpu_set_dmp_callback(void (*func)(void));
int rc_mpu_block_until_dmp_data();
int rc_mpu_was_last_dmp_read_successful();
uint64_t rc_mpu_nanos_since_last_dmp_interrupt();
int rc_mpu_set_tap_callback(void (*func)(int direction));
int rc_mpu_block_until_tap();
int rc_mpu_nanos_since_last_tap();

// other
int rc_mpu_calibrate_gyro_routine();
int rc_mpu_calibrate_mag_routine();
int rc_mpu_is_gyro_calibrated();
int rc_mpu_is_mag_calibrated();


#ifdef  __cplusplus
}
#endif

#endif // RC_MPU_H

/** @}  end group Sensor*/

/******************************************************************************
* 9-AXIS IMU
*
* The Robotics Cape features an Invensense MPU9250 9-axis IMU. This API allows
* the user to configure this IMU in two modes: RANDOM and DMP
*
* RANDOM: The accelerometer, gyroscope, magnetometer, and thermometer can be
* read directly at any time. To use this mode, call rc_mpu_initialize() with your
* imu_config and imu_data structs as arguments as defined below. You can then
* call rc_mpu_read_accel, rc_mpu_read_gyro, rc_mpu_read_mag, or rc_mpu_read_temp
* at any time to get the latest sensor values.
*
* DMP: Stands for Digital Motion Processor which is a feature of the MPU9250.
* in this mode, the DMP will sample the sensors internally and fill a FIFO
* buffer with the data at a fixed rate. Furthermore, the DMP will also calculate
* a filtered orientation quaternion which is placed in the same buffer. When
* new data is ready in the buffer, the IMU sends an interrupt to the BeagleBone
* triggering the buffer read followed by the execution of a function of your
* choosing set with the rc_mpu_set_dmp_callback() function.
*
* @ enum rc_mpu_accel_fsr_t rc_mpu_gyro_fsr_t
*
* The user may choose from 4 full scale ranges of the accelerometer and
* gyroscope. They have units of gravity (G) and degrees per second (DPS)
* The defaults values are A_FSR_4G and G_FSR_1000DPS respectively.
*
* enum rc_mpu_accel_dlpf_t rc_mpu_gyro_dlpf_t
*
* The user may choose from 7 digital low pass filter constants for the
* accelerometer and gyroscope. The filter runs at 1kz and helps to reduce sensor
* noise when sampling more slowly. The default values are ACCEL_DLPF_184
* GYRO_DLPF_250. Lower cut-off frequencies incur phase-loss in measurements.
*
* @ struct rc_mpu_config_t
*
* Configuration struct passed to rc_mpu_initialize and rc_mpu_initialize_dmp. It is
* best to get the default config with rc_mpu_default_config() function and
* modify from there.
*
* @ struct rc_mpu_data_t
*
* This is the container for holding the sensor data from the IMU.
* The user is intended to make their own instance of this struct and pass
* its pointer to imu read functions.
*
* @ rc_mpu_config_t rc_mpu_default_config()
*
* Returns an rc_mpu_config_t struct with default settings. Use this as a starting
* point and modify as you wish.
*
* @ int rc_mpu_initialize(rc_mpu_data_t *data, rc_mpu_config_t conf)
*
* Sets up the IMU in random-read mode. First create an instance of the imu_data
* struct to point to as rc_mpu_initialize will put useful data in it.
* rc_mpu_initialize only reads from the config struct. After this, you may read
* sensor data.
*
* @ int rc_mpu_read_accel(rc_mpu_data_t *data)
* @ int rc_mpu_read_gyro(rc_mpu_data_t *data)
* @ int rc_mpu_read_mag(rc_mpu_data_t *data)
* @ int rc_mpu_read_temp(rc_mpu_data_t* data)
*
* These are the functions for random sensor sampling at any time. Note that
* if you wish to read the magnetometer then it must be enabled in the
* configuration struct. Since the magnetometer requires additional setup and
* is slower to read, it is disabled by default.
*
******************************************************************************/