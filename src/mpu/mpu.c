/**
 * @file mpu.c
 *
 * @author James Strawson
 * @date 2/1/2018
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>

#include <rc/mpu.h>
#include <rc/math/vector.h>
#include <rc/math/matrix.h>
#include <rc/math/quaternion.h>
#include <rc/math/filter.h>
#include <rc/math/algebra.h>
#include <rc/time.h>
#include <rc/gpio.h>
#include <rc/i2c.h>

#include "mpu_defs.h"
#include "dmp_firmware.h"
#include "dmpKey.h"
#include "dmpmap.h"

// macros
#define ARRAY_SIZE(array) sizeof(array)/sizeof(array[0])
#define min(a, b)	((a < b) ? a : b)
#define unlikely(x)	__builtin_expect (!!(x), 0)
#define __unused	__attribute__ ((unused))

#define DEG_TO_RAD	0.0174532925199
#define RAD_TO_DEG	57.295779513
#define PI		M_PI
#define TWO_PI		(2.0 * M_PI)

// there should be 28 or 35 bytes in the FIFO if the magnetometer is disabled
// or enabled.
#define FIFO_LEN_QUAT_TAP 20 // 16 for quat, 4 for tap
#define FIFO_LEN_QUAT_ACCEL_GYRO_TAP 32 // 16 quat, 6 accel, 6 gyro, 4 tap
#define MAX_FIFO_BUFFER	(FIFO_LEN_QUAT_ACCEL_GYRO_TAP*5)


// error threshold checks
#define QUAT_ERROR_THRESH	(1L<<16) // very precise threshold
#define QUAT_MAG_SQ_NORMALIZED	(1L<<28)
#define QUAT_MAG_SQ_MIN		(QUAT_MAG_SQ_NORMALIZED - QUAT_ERROR_THRESH)
#define QUAT_MAG_SQ_MAX		(QUAT_MAG_SQ_NORMALIZED + QUAT_ERROR_THRESH)
#define GYRO_CAL_THRESH		50
#define GYRO_OFFSET_THRESH	500

// Thread control
static pthread_mutex_t read_mutex	= PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  read_condition	= PTHREAD_COND_INITIALIZER;
static pthread_mutex_t tap_mutex	= PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  tap_condition	= PTHREAD_COND_INITIALIZER;

/*******************************************************************************
*	Local variables
*******************************************************************************/
static rc_mpu_config_t config;
static int bypass_en;
static int dmp_en;
static int packet_len;
static pthread_t imu_interrupt_thread;
static int thread_running_flag;
static pthread_attr_t pthread_attr;
static struct sched_param fifo_param;
static void (*dmp_callback_func)()=NULL;
static void (*tap_callback_func)(int dir)=NULL;
static float mag_factory_adjust[3];
static float mag_offsets[3];
static float mag_scales[3];
static int last_read_successful;
static uint64_t last_interrupt_timestamp_nanos;
static uint64_t last_tap_timestamp_nanos;
static rc_mpu_data_t* data_ptr;
static int imu_shutdown_flag = 0;
static rc_filter_t low_pass, high_pass; // for magnetometer Yaw filtering

/*******************************************************************************
* functions for internal use only
*******************************************************************************/
static int __reset_mpu9250();
static int __check_who_am_i();
static int __set_gyro_fsr(rc_mpu_gyro_fsr_t fsr, rc_mpu_data_t* data);
static int __set_accel_fsr(rc_mpu_accel_fsr_t, rc_mpu_data_t* data);
static int __set_gyro_dlpf(rc_mpu_gyro_dlpf_t dlpf);
static int __set_accel_dlpf(rc_mpu_accel_dlpf_t dlpf);
static int __init_magnetometer();
static int __power_off_magnetometer();
static int __mpu_set_bypass(unsigned char bypass_on);
static int __mpu_write_mem(unsigned short mem_addr, unsigned short length, unsigned char *data);
static int __mpu_read_mem(unsigned short mem_addr, unsigned short length, unsigned char *data);
static int __dmp_load_motion_driver_firmware();
static int __dmp_set_orientation(unsigned short orient);
static int __dmp_enable_gyro_cal(unsigned char enable);
static int __dmp_enable_lp_quat(unsigned char enable);
static int __dmp_enable_6x_lp_quat(unsigned char enable);
static int __mpu_reset_fifo(void);
static int __mpu_set_sample_rate(int rate);
static int __dmp_set_fifo_rate(unsigned short rate);
static int __dmp_enable_feature(unsigned short mask);
static int __mpu_set_dmp_state(unsigned char enable);
static int __set_int_enable(unsigned char enable);
static int __dmp_set_interrupt_mode(unsigned char mode);
static int __load_gyro_offets();
static int __load_mag_calibration();
static int __write_mag_cal_to_disk(float offsets[3], float scale[3]);
static void* __imu_interrupt_handler(void* ptr);
static int __read_dmp_fifo(rc_mpu_data_t* data);
static int __data_fusion(rc_mpu_data_t* data);

/*******************************************************************************
* rc_mpu_config_t rc_mpu_default_config()
*
* returns reasonable default configuration values
*******************************************************************************/
rc_mpu_config_t rc_mpu_default_config()
{
	rc_mpu_config_t conf;

	// connectivity
	conf.gpio_interrupt_pin = RC_IMU_INTERRUPT_PIN;
	conf.i2c_bus = RC_IMU_BUS;
	conf.i2c_addr = RC_MPU_DEFAULT_I2C_ADDR;
	conf.show_warnings = 0;

	// general stuff
	conf.accel_fsr	= ACCEL_FSR_2G;
	conf.gyro_fsr	= GYRO_FSR_2000DPS;
	conf.accel_dlpf	= ACCEL_DLPF_184;
	conf.gyro_dlpf	= GYRO_DLPF_184;
	conf.enable_magnetometer = 0;

	// DMP stuff
	conf.dmp_sample_rate = 100;
	conf.dmp_fetch_accel_gyro = 0;
	conf.dmp_auto_calibrate_gyro = 0;
	conf.orient = ORIENTATION_Z_UP;
	conf.compass_time_constant = 20.0;
	conf.dmp_interrupt_priority = sched_get_priority_max(SCHED_FIFO)-1;
	conf.read_mag_after_callback = 1;
	conf.mag_sample_rate_div = 4;
	conf.tap_threshold=150;

	return conf;
}

/*******************************************************************************
* int rc_mpu_set_config_to_default(*rc_mpu_config_t);
*
* resets an rc_mpu_config_t struct to default values
*******************************************************************************/
int rc_mpu_set_config_to_default(rc_mpu_config_t *conf)
{
	*conf = rc_mpu_default_config();
	return 0;
}

/*******************************************************************************
* int rc_mpu_initialize(rc_mpu_config_t conf)
*
* Set up the imu for one-shot sampling of sensor data by user
*******************************************************************************/
int rc_mpu_initialize(rc_mpu_data_t *data, rc_mpu_config_t conf)
{
	// update local copy of config struct with new values
	config=conf;

	// make sure the bus is not currently in use by another thread
	// do not proceed to prevent interfering with that process
	if(rc_i2c_get_lock(config.i2c_bus)){
		printf("i2c bus claimed by another process\n");
		printf("Continuing with rc_mpu_initialize() anyway.\n");
	}

	// if it is not claimed, start the i2c bus
	if(rc_i2c_init(config.i2c_bus, config.i2c_addr)<0){
		fprintf(stderr,"failed to initialize i2c bus\n");
		return -1;
	}
	// claiming the bus does no guarantee other code will not interfere
	// with this process, but best to claim it so other code can check
	// like we did above
	rc_i2c_lock_bus(config.i2c_bus);

	// restart the device so we start with clean registers
	if(__reset_mpu9250()<0){
		fprintf(stderr,"ERROR: failed to reset_mpu9250\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}
	if(__check_who_am_i()){
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// load in gyro calibration offsets from disk
	if(__load_gyro_offets()<0){
		fprintf(stderr,"ERROR: failed to load gyro calibration offsets\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// Set sample rate = 1000/(1 + SMPLRT_DIV)
	// here we use a divider of 0 for 1khz sample
	if(rc_i2c_write_byte(config.i2c_bus, SMPLRT_DIV, 0x00)){
		fprintf(stderr,"I2C bus write error\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// set full scale ranges and filter constants
	if(__set_gyro_fsr(conf.gyro_fsr, data)){
		fprintf(stderr,"failed to set gyro fsr\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}
	if(__set_accel_fsr(conf.accel_fsr, data)){
		fprintf(stderr,"failed to set accel fsr\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}
	if(__set_gyro_dlpf(conf.gyro_dlpf)){
		fprintf(stderr,"failed to set gyro dlpf\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}
	if(__set_accel_dlpf(conf.accel_dlpf)){
		fprintf(stderr,"failed to set accel_dlpf\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// initialize the magnetometer too if requested in config
	if(conf.enable_magnetometer){
		if(__init_magnetometer()){
			fprintf(stderr,"failed to initialize magnetometer\n");
			rc_i2c_unlock_bus(config.i2c_bus);
			return -1;
		}
	}
	else __power_off_magnetometer();

	// all done!!
	rc_i2c_unlock_bus(config.i2c_bus);
	return 0;
}

/*******************************************************************************
* int rc_mpu_read_accel(rc_mpu_data_t* data)
*
* Always reads in latest accelerometer values. The sensor
* self-samples at 1khz and this retrieves the latest data.
*******************************************************************************/
int rc_mpu_read_accel(rc_mpu_data_t *data)
{
	// new register data stored here
	uint8_t raw[6];
	// set the device address
	rc_i2c_set_device_address(config.i2c_bus, config.i2c_addr);
	 // Read the six raw data registers into data array
	if(rc_i2c_read_bytes(config.i2c_bus, ACCEL_XOUT_H, 6, &raw[0])<0){
		return -1;
	}
	// Turn the MSB and LSB into a signed 16-bit value
	data->raw_accel[0] = (int16_t)(((uint16_t)raw[0]<<8)|raw[1]);
	data->raw_accel[1] = (int16_t)(((uint16_t)raw[2]<<8)|raw[3]);
	data->raw_accel[2] = (int16_t)(((uint16_t)raw[4]<<8)|raw[5]);
	// Fill in real unit values
	data->accel[0] = data->raw_accel[0] * data->accel_to_ms2;
	data->accel[1] = data->raw_accel[1] * data->accel_to_ms2;
	data->accel[2] = data->raw_accel[2] * data->accel_to_ms2;
	return 0;
}

/*******************************************************************************
* int rc_mpu_read_gyro(rc_mpu_data_t* data)
*
* Always reads in latest gyroscope values. The sensor self-samples
* at 1khz and this retrieves the latest data.
*******************************************************************************/
int rc_mpu_read_gyro(rc_mpu_data_t *data)
{
	// new register data stored here
	uint8_t raw[6];
	// set the device address
	rc_i2c_set_device_address(config.i2c_bus, config.i2c_addr);
	// Read the six raw data registers into data array
	if(rc_i2c_read_bytes(config.i2c_bus, GYRO_XOUT_H, 6, &raw[0])<0){
		return -1;
	}
	// Turn the MSB and LSB into a signed 16-bit value
	data->raw_gyro[0] = (int16_t)(((int16_t)raw[0]<<8)|raw[1]);
	data->raw_gyro[1] = (int16_t)(((int16_t)raw[2]<<8)|raw[3]);
	data->raw_gyro[2] = (int16_t)(((int16_t)raw[4]<<8)|raw[5]);
	// Fill in real unit values
	data->gyro[0] = data->raw_gyro[0] * data->gyro_to_degs;
	data->gyro[1] = data->raw_gyro[1] * data->gyro_to_degs;
	data->gyro[2] = data->raw_gyro[2] * data->gyro_to_degs;
	return 0;
}

/*******************************************************************************
* int rc_mpu_read_mag(rc_mpu_data_t* data)
*
* Checks if there is new magnetometer data and reads it in if true.
* Magnetometer only updates at 100hz, if there is no new data then
* the values in rc_mpu_data_t struct are left alone.
*******************************************************************************/
int rc_mpu_read_mag(rc_mpu_data_t* data)
{
	uint8_t raw[7];
	int16_t adc[3];
	float factory_cal_data[3];
	if(!config.enable_magnetometer){
		fprintf(stderr,"ERROR: can't read magnetometer unless it is enabled in \n");
		fprintf(stderr,"rc_mpu_config_t struct before calling rc_mpu_initialize\n");
		return -1;
	}
	// magnetometer is actually a separate device with its
	// own address inside the mpu9250
	// MPU9250 was put into passthrough mode
	if(unlikely(rc_i2c_set_device_address(config.i2c_bus, AK8963_ADDR))){
		fprintf(stderr,"ERROR: in rc_mpu_read_mag, failed to set i2c address\n");
		return -1;
	}
	// don't worry about checking data ready bit, not worth thet time
	// read the data ready bit to see if there is new data
	uint8_t st1;
	if(unlikely(rc_i2c_read_byte(config.i2c_bus, AK8963_ST1, &st1)<0)){
		fprintf(stderr,"ERROR reading Magnetometer, i2c_bypass is probably not set\n");
		return -1;
	}
	#ifdef DEBUG
	printf("st1: %d", st1);
	#endif
	if(!(st1&MAG_DATA_READY)){
		if(config.show_warnings){
			printf("no new magnetometer data ready, skipping read\n");
		}
		return 0;
	}
	// Read the six raw data regs into data array
	if(unlikely(rc_i2c_read_bytes(config.i2c_bus,AK8963_XOUT_L,7,&raw[0])<0)){
		fprintf(stderr,"ERROR: rc_mpu_read_mag failed to read data register\n");
		return -1;
	}
	// check if the readings saturated such as because
	// of a local field source, discard data if so
	if(raw[6]&MAGNETOMETER_SATURATION){
		if(config.show_warnings){
			printf("WARNING: magnetometer saturated, discarding data\n");
		}
		return -1;
	}
	// Turn the MSB and LSB into a signed 16-bit value
	// Data stored as little Endian
	adc[0] = (int16_t)(((int16_t)raw[1]<<8) | raw[0]);
	adc[1] = (int16_t)(((int16_t)raw[3]<<8) | raw[2]);
	adc[2] = (int16_t)(((int16_t)raw[5]<<8) | raw[4]);
	#ifdef DEBUG
	printf("raw mag:%d %d %d\n", adc[0], adc[1], adc[2]);
	#endif

	// multiply by the sensitivity adjustment and convert to units of uT micro
	// Teslas. Also correct the coordinate system as someone in invensense
	// thought it would be bright idea to have the magnetometer coordiate
	// system aligned differently than the accelerometer and gyro.... -__-
	factory_cal_data[0] = adc[1] * mag_factory_adjust[1] * MAG_RAW_TO_uT;
	factory_cal_data[1] = adc[0] * mag_factory_adjust[0] * MAG_RAW_TO_uT;
	factory_cal_data[2] = -adc[2] * mag_factory_adjust[2] * MAG_RAW_TO_uT;

	// now apply out own calibration, but first make sure we don't accidentally
	// multiply by zero in case of uninitialized scale factors
	if(mag_scales[0]==0.0) mag_scales[0]=1.0;
	if(mag_scales[1]==0.0) mag_scales[1]=1.0;
	if(mag_scales[2]==0.0) mag_scales[2]=1.0;
	data->mag[0] = (factory_cal_data[0]-mag_offsets[0])*mag_scales[0];
	data->mag[1] = (factory_cal_data[1]-mag_offsets[1])*mag_scales[1];
	data->mag[2] = (factory_cal_data[2]-mag_offsets[2])*mag_scales[2];

	return 0;
}

/*******************************************************************************
* int rc_mpu_read_temp(rc_mpu_data_t* data)
*
* reads the latest temperature of the imu.
*******************************************************************************/
int rc_mpu_read_temp(rc_mpu_data_t* data)
{
	uint16_t adc;
	// set device address
	rc_i2c_set_device_address(config.i2c_bus, config.i2c_addr);
	// Read the two raw data registers
	if(rc_i2c_read_word(config.i2c_bus, TEMP_OUT_H, &adc)<0){
		fprintf(stderr,"failed to read IMU temperature registers\n");
		return -1;
	}
	// convert to real units
	data->temp = 21.0 + adc/TEMP_SENSITIVITY;
	return 0;
}

/*******************************************************************************
* int __reset_mpu9250()
*
* sets the reset bit in the power management register which restores
* the device to defualt settings. a 0.1 second wait is also included
* to let the device compelete the reset process.
*******************************************************************************/
int __reset_mpu9250()
{
	// disable the interrupt to prevent it from doing things while we reset
	imu_shutdown_flag = 1;
	// set the device address
	rc_i2c_set_device_address(config.i2c_bus, config.i2c_addr);
	// write the reset bit
	if(rc_i2c_write_byte(config.i2c_bus, PWR_MGMT_1, H_RESET)){
		// wait and try again
		rc_usleep(10000);
			if(rc_i2c_write_byte(config.i2c_bus, PWR_MGMT_1, H_RESET)){
				fprintf(stderr,"I2C write to MPU9250 Failed\n");
			return -1;
		}
	}
	// make sure all other power management features are off
	if(rc_i2c_write_byte(config.i2c_bus, PWR_MGMT_1, 0)){
		// wait and try again
		rc_usleep(10000);
		if(rc_i2c_write_byte(config.i2c_bus, PWR_MGMT_1, 0)){
			fprintf(stderr,"I2C write to MPU9250 Failed\n");
		return -1;
		}
	}
	rc_usleep(100000);
	return 0;
}

/*******************************************************************************
* int __check_who_am_i()
*******************************************************************************/
int __check_who_am_i(){
	uint8_t c;
	//check the who am i register to make sure the chip is alive
	if(rc_i2c_read_byte(config.i2c_bus, WHO_AM_I_MPU9250, &c)<0){
		fprintf(stderr,"i2c_read_byte failed reading who_am_i register\n");
		return -1;
	}
	// check which chip we are looking at
	// 0x71 for mpu9250, 0x75 for mpu9255, or 0x68 for mpu9150
	// 0x70 for mpu6500,  0x68 or 0x69 for mpu6050
	if(c!=0x68 && c!=0x69 && c!=0x70 && c!=0x71 && c!=75){
		fprintf(stderr,"invalid who_am_i register: 0x%x\n", c);
		fprintf(stderr,"expected 0x68 or 0x69 for mpu6050/9150, 0x70 for mpu6500, 0x71 for mpu9250, 0x75 for mpu9255,\n");
		return -1;
	}
	return 0;
}

/*******************************************************************************
* int __set_accel_fsr(rc_mpu_accel_fsr_t fsr, rc_mpu_data_t* data)
*
* set accelerometer full scale range and update conversion ratio
*******************************************************************************/
int __set_accel_fsr(rc_mpu_accel_fsr_t fsr, rc_mpu_data_t* data)
{
	uint8_t c;
	switch(fsr){
	case ACCEL_FSR_2G:
		c = ACCEL_FSR_CFG_2G;
		data->accel_to_ms2 = 9.80665*2.0/32768.0;
		break;
	case ACCEL_FSR_4G:
		c = ACCEL_FSR_CFG_4G;
		data->accel_to_ms2 = 9.80665*4.0/32768.0;
		break;
	case ACCEL_FSR_8G:
		c = ACCEL_FSR_CFG_8G;
		data->accel_to_ms2 = 9.80665*8.0/32768.0;
		break;
	case ACCEL_FSR_16G:
		c = ACCEL_FSR_CFG_16G;
		data->accel_to_ms2 = 9.80665*16.0/32768.0;
		break;
	default:
		fprintf(stderr,"invalid accel fsr\n");
		return -1;
	}
	return rc_i2c_write_byte(config.i2c_bus, ACCEL_CONFIG, c);
}


/*******************************************************************************
* int __set_gyro_fsr(rc_mpu_gyro_fsr_t fsr, rc_mpu_data_t* data)
*
* set gyro full scale range and update conversion ratio
*******************************************************************************/
int __set_gyro_fsr(rc_mpu_gyro_fsr_t fsr, rc_mpu_data_t* data)
{
	uint8_t c;
	switch(fsr){
	case GYRO_FSR_250DPS:
		c = GYRO_FSR_CFG_250 | FCHOICE_B_DLPF_EN;
		data->gyro_to_degs = 250.0/32768.0;
		break;
	case GYRO_FSR_500DPS:
		c = GYRO_FSR_CFG_500 | FCHOICE_B_DLPF_EN;
		data->gyro_to_degs = 500.0/32768.0;
		break;
	case GYRO_FSR_1000DPS:
		c = GYRO_FSR_CFG_1000 | FCHOICE_B_DLPF_EN;
		data->gyro_to_degs = 1000.0/32768.0;
		break;
	case GYRO_FSR_2000DPS:
		c = GYRO_FSR_CFG_2000 | FCHOICE_B_DLPF_EN;
		data->gyro_to_degs = 2000.0/32768.0;
		break;
	default:
		fprintf(stderr,"invalid gyro fsr\n");
		return -1;
	}
	return rc_i2c_write_byte(config.i2c_bus, GYRO_CONFIG, c);
}

/*******************************************************************************
* int __set_accel_dlpf(rc_mpu_accel_dlpf_t dlpf)
*
* Set accel low pass filter constants. This is the same register as
* the sample rate. We set it at 1khz as 4khz is unnecessary.
*******************************************************************************/
int __set_accel_dlpf(rc_mpu_accel_dlpf_t dlpf)
{
	uint8_t c = ACCEL_FCHOICE_1KHZ | BIT_FIFO_SIZE_1024;
	switch(dlpf){
	case ACCEL_DLPF_OFF:
		c = ACCEL_FCHOICE_4KHZ | BIT_FIFO_SIZE_1024;
		break;
	case ACCEL_DLPF_460:
		c |= 0;
		break;
	case ACCEL_DLPF_184:
		c |= 1;
		break;
	case ACCEL_DLPF_92:
		c |= 2;
		break;
	case ACCEL_DLPF_41:
		c |= 3;
		break;
	case ACCEL_DLPF_20:
		c |= 4;
		break;
	case ACCEL_DLPF_10:
		c |= 5;
		break;
	case ACCEL_DLPF_5:
		c |= 6;
		break;
	default:
		fprintf(stderr,"invalid config.accel_dlpf\n");
		return -1;
	}
	return rc_i2c_write_byte(config.i2c_bus, ACCEL_CONFIG_2, c);
}

/*******************************************************************************
* int __set_gyro_dlpf(rc_mpu_gyro_dlpf_t dlpf)
*
* Set GYRO low pass filter constants. This is the same register as
* the fifo overflow mode so we set it to keep the newest data too.
*******************************************************************************/
int __set_gyro_dlpf(rc_mpu_gyro_dlpf_t dlpf)
{
	uint8_t c = FIFO_MODE_REPLACE_OLD;
	switch(dlpf){
	case GYRO_DLPF_OFF:
		c |= 7; // not really off, but 3600Hz bandwith
		break;
	case GYRO_DLPF_250:
		c |= 0;
		break;
	case GYRO_DLPF_184:
		c |= 1;
		break;
	case GYRO_DLPF_92:
		c |= 2;
		break;
	case GYRO_DLPF_41:
		c |= 3;
		break;
	case GYRO_DLPF_20:
		c |= 4;
		break;
	case GYRO_DLPF_10:
		c |= 5;
		break;
	case GYRO_DLPF_5:
		c |= 6;
		break;
	default:
		fprintf(stderr,"invalid gyro_dlpf\n");
		return -1;
	}
	return rc_i2c_write_byte(config.i2c_bus, CONFIG, c);
}

/*******************************************************************************
* int __init_magnetometer()
*
* configure the magnetometer for 100hz reads, also reads in the factory
* sensitivity values into the global variables;
*******************************************************************************/
int __init_magnetometer()
{
	uint8_t raw[3];	// calibration data stored here

	// Enable i2c bypass to allow talking to magnetometer
	if(__mpu_set_bypass(1)){
		fprintf(stderr,"failed to set mpu9250 into bypass i2c mode\n");
		return -1;
	}
	// magnetometer is actually a separate device with its
	// own address inside the mpu9250
	rc_i2c_set_device_address(config.i2c_bus, AK8963_ADDR);
	// Power down magnetometer
	rc_i2c_write_byte(config.i2c_bus, AK8963_CNTL, MAG_POWER_DN);
	rc_usleep(1000);
	// Enter Fuse ROM access mode
	rc_i2c_write_byte(config.i2c_bus, AK8963_CNTL, MAG_FUSE_ROM);
	rc_usleep(1000);
	// Read the xyz sensitivity adjustment values
	if(rc_i2c_read_bytes(config.i2c_bus, AK8963_ASAX, 3, &raw[0])<0){
		fprintf(stderr,"failed to read magnetometer adjustment register\n");
		rc_i2c_set_device_address(config.i2c_bus, config.i2c_addr);
		__mpu_set_bypass(0);
		return -1;
	}
	// Return sensitivity adjustment values
	mag_factory_adjust[0] = (raw[0]-128)/256.0 + 1.0;
	mag_factory_adjust[1] = (raw[1]-128)/256.0 + 1.0;
	mag_factory_adjust[2] = (raw[2]-128)/256.0 + 1.0;
	// Power down magnetometer again
	rc_i2c_write_byte(config.i2c_bus, AK8963_CNTL, MAG_POWER_DN);
	rc_usleep(100);
	// Configure the magnetometer for 16 bit resolution
	// and continuous sampling mode 2 (100hz)
	uint8_t c = MSCALE_16|MAG_CONT_MES_2;
	rc_i2c_write_byte(config.i2c_bus, AK8963_CNTL, c);
	rc_usleep(100);
	// go back to configuring the IMU, leave bypass on
	rc_i2c_set_device_address(config.i2c_bus,config.i2c_addr);
	// load in magnetometer calibration
	__load_mag_calibration();
	return 0;
}

/*******************************************************************************
* int __power_off_magnetometer()
*
* Make sure the magnetometer is off.
*******************************************************************************/
int __power_off_magnetometer()
{
	rc_i2c_set_device_address(config.i2c_bus, config.i2c_addr);
	// Enable i2c bypass to allow talking to magnetometer
	if(__mpu_set_bypass(1)){
		fprintf(stderr,"failed to set mpu9250 into bypass i2c mode\n");
		return -1;
	}
	// magnetometer is actually a separate device with its
	// own address inside the mpu9250
	rc_i2c_set_device_address(config.i2c_bus, AK8963_ADDR);
	// Power down magnetometer
	if(rc_i2c_write_byte(config.i2c_bus, AK8963_CNTL, MAG_POWER_DN)<0){
		fprintf(stderr,"failed to write to magnetometer\n");
		return -1;
	}
	rc_i2c_set_device_address(config.i2c_bus, config.i2c_addr);
	// // Enable i2c bypass to allow talking to magnetometer
	// if(__mpu_set_bypass(0)){
	// 	fprintf(stderr,"failed to set mpu9250 into bypass i2c mode\n");
	// 	return -1;
	// }
	return 0;
}

/*******************************************************************************
* Power down the IMU
*******************************************************************************/
int rc_mpu_power_off()
{
	imu_shutdown_flag = 1;
	// set the device address
	rc_i2c_set_device_address(config.i2c_bus, config.i2c_addr);
	// write the reset bit
	if(rc_i2c_write_byte(config.i2c_bus, PWR_MGMT_1, H_RESET)){
		//wait and try again
		rc_usleep(1000);
		if(rc_i2c_write_byte(config.i2c_bus, PWR_MGMT_1, H_RESET)){
			fprintf(stderr,"I2C write to MPU9250 Failed\n");
			return -1;
		}
	}
	// write the sleep bit
	if(rc_i2c_write_byte(config.i2c_bus, PWR_MGMT_1, MPU_SLEEP)){
		//wait and try again
		rc_usleep(1000);
		if(rc_i2c_write_byte(config.i2c_bus, PWR_MGMT_1, MPU_SLEEP)){
			fprintf(stderr,"I2C write to MPU9250 Failed\n");
			return -1;
		}
	}
	// wait for the interrupt thread to exit if it hasn't already
	//allow up to 1 second for thread cleanup
	if(thread_running_flag){
		struct timespec thread_timeout;
		clock_gettime(CLOCK_REALTIME, &thread_timeout);
		thread_timeout.tv_sec += 1;
		int thread_err = 0;
		thread_err = pthread_timedjoin_np(imu_interrupt_thread, NULL, \
							&thread_timeout);
		if(thread_err == ETIMEDOUT){
			fprintf(stderr,"WARNING: imu_interrupt_thread exit timeout\n");
		}
		// cleanup mutexes
		pthread_cond_destroy(&read_condition);
		pthread_mutex_destroy(&read_mutex);
		pthread_cond_destroy(&tap_condition);
		pthread_mutex_destroy(&tap_mutex);
	}
	return 0;
}

/*******************************************************************************
* Set up the IMU for DMP accelerated filtering and interrupts
*******************************************************************************/
int rc_mpu_initialize_dmp(rc_mpu_data_t *data, rc_mpu_config_t conf)
{
	uint8_t tmp;
	// range check
	if(conf.dmp_sample_rate>DMP_MAX_RATE || conf.dmp_sample_rate<DMP_MIN_RATE){
		fprintf(stderr,"ERROR:dmp_sample_rate must be between %d & %d\n", \
						DMP_MIN_RATE, DMP_MAX_RATE);
		return -1;
	}
	// make sure the sample rate is a divisor so we can find a neat rate divider
	if(DMP_MAX_RATE%conf.dmp_sample_rate != 0){
		fprintf(stderr,"DMP sample rate must be a divisor of 200\n");
		fprintf(stderr,"acceptable values: 200,100,50,40,25,20,10,8,5,4 (HZ)\n");
		return -1;
	}
	// make sure the compass filter time constant is valid
	if(conf.enable_magnetometer && conf.compass_time_constant<=0.1){
		fprintf(stderr,"ERROR: compass time constant must be greater than 0.1\n");
		return -1;
	}
	const int max_pri = sched_get_priority_max(SCHED_FIFO);
	const int min_pri = sched_get_priority_min(SCHED_FIFO);
	if(conf.dmp_interrupt_priority>max_pri || conf.dmp_interrupt_priority<min_pri){
		printf("dmp priority must be between %d & %d\n",min_pri,max_pri);
		return -1;
	}
	// check dlpf
	if(conf.gyro_dlpf==GYRO_DLPF_OFF || conf.gyro_dlpf==GYRO_DLPF_250){
		fprintf(stderr,"WARNING, gyro dlpf bandwidth must be <= 184hz in DMP mode\n");
		fprintf(stderr,"setting to 184hz automatically\n");
		conf.gyro_dlpf	= GYRO_DLPF_184;
	}
	if(conf.accel_dlpf==ACCEL_DLPF_OFF || conf.accel_dlpf==ACCEL_DLPF_460){
		fprintf(stderr,"WARNING, accel dlpf bandwidth must be <= 184hz in DMP mode\n");
		fprintf(stderr,"setting to 184hz automatically\n");
		conf.accel_dlpf	= ACCEL_DLPF_184;
	}
	// check FSR
	if(conf.gyro_fsr!=GYRO_FSR_2000DPS){
		fprintf(stderr,"WARNING, gyro FSR must be GYRO_FSR_2000DPS in DMP mode\n");
		fprintf(stderr,"setting to 2000DPS automatically\n");
		conf.gyro_fsr = GYRO_FSR_2000DPS;
	}
	if(conf.accel_fsr!=ACCEL_FSR_2G){
		fprintf(stderr,"WARNING, accel FSR must be ACCEL_FSR_2G in DMP mode\n");
		fprintf(stderr,"setting to ACCEL_FSR_2G automatically\n");
		conf.accel_fsr = ACCEL_FSR_2G;
	}
	// update local copy of config and data struct with new values
	config = conf;
	data_ptr = data;

	// start the i2c bus
	if(rc_i2c_init(config.i2c_bus, config.i2c_addr)){
		fprintf(stderr,"rc_mpu_initialize_dmp failed at rc_i2c_init\n");
		return -1;
	}
	// configure the gpio interrupt pin
	if(rc_gpio_export(config.gpio_interrupt_pin)<0){
		fprintf(stderr,"ERROR: failed to export GPIO %d\n", config.gpio_interrupt_pin);
		fprintf(stderr,"probably insufficient privileges\n");
		return -1;
	}
	if(rc_gpio_set_dir(config.gpio_interrupt_pin, GPIO_INPUT_PIN)<0){
		fprintf(stderr,"ERROR: failed to configure GPIO %d", config.gpio_interrupt_pin);
		return -1;
	}
	if(rc_gpio_set_edge(config.gpio_interrupt_pin, GPIO_EDGE_FALLING)<0){
		fprintf(stderr,"ERROR: failed to configure GPIO %d", config.gpio_interrupt_pin);
		return -1;
	}
	// claiming the bus does no guarantee other code will not interfere
	// with this process, but best to claim it so other code can check
	rc_i2c_lock_bus(config.i2c_bus);
	// restart the device so we start with clean registers
	if(__reset_mpu9250()<0){
		fprintf(stderr,"failed to __reset_mpu9250()\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}
	if(__check_who_am_i()){
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}
	// MPU6500 shares 4kB of memory between the DMP and the FIFO. Since the
	//first 3kB are needed by the DMP, we'll use the last 1kB for the FIFO.
	// this is also set in set_accel_dlpf but we set here early on
	tmp = BIT_FIFO_SIZE_1024 | 0x8;
	if(rc_i2c_write_byte(config.i2c_bus, ACCEL_CONFIG_2, tmp)){
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}
	// load in gyro calibration offsets from disk
	if(__load_gyro_offets()<0){
		fprintf(stderr,"ERROR: failed to load gyro calibration offsets\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// set full scale ranges. It seems the DMP only scales the gyro properly
	// at 2000DPS. I'll assume the same is true for accel and use 2G like their
	// example
	__set_gyro_fsr(GYRO_FSR_2000DPS, data_ptr);
	__set_accel_fsr(ACCEL_FSR_2G, data_ptr);

	// set dlpf, these values already checked for bounds above
	if(__set_gyro_dlpf(conf.gyro_dlpf)){
		fprintf(stderr,"failed to set gyro dlpf\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}
	if(__set_accel_dlpf(conf.accel_dlpf)){
		fprintf(stderr,"failed to set accel_dlpf\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// This actually sets the rate of accel/gyro sampling which should always be
	// 200 as the dmp filters at that rate
	if(__mpu_set_sample_rate(200)<0){
	//if(__mpu_set_sample_rate(config.dmp_sample_rate)<0){
		fprintf(stderr,"ERROR: setting IMU sample rate\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// enable bypass, more importantly this also configures the interrupt pin behavior
	if(__mpu_set_bypass(1)){
		fprintf(stderr, "failed to run __mpu_set_bypass\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// initialize the magnetometer too if requested in config
	if(conf.enable_magnetometer){
		if(__init_magnetometer()){
			fprintf(stderr,"ERROR: failed to initialize_magnetometer\n");
			rc_i2c_unlock_bus(config.i2c_bus);
			return -1;
		}
	}
	else __power_off_magnetometer();


	// set up the DMP, order is important, from motiondrive_tutorial.pdf:
	// 1) load firmware
	// 2) set orientation matrix
	// 3) enable callbacks (we don't do this here)
	// 4) enable features
	// 5) set fifo rate
	// 6) set any feature-specific control functions
	// 7) turn dmp on
	dmp_en = 1; // log locally that the dmp will be running
	if(__dmp_load_motion_driver_firmware()<0){
		fprintf(stderr,"failed to load DMP motion driver\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// set the orientation of dmp quaternion
	if(__dmp_set_orientation((unsigned short)conf.orient)<0){
		fprintf(stderr,"ERROR: failed to set dmp orientation\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	/// enbale quaternion feature and accel/gyro if requested
	// due to a known bug in the DMP, the tap feature must be enabled to
	// get interrupts slower than 200hz
	unsigned short feature_mask = DMP_FEATURE_6X_LP_QUAT|DMP_FEATURE_TAP;

	// enable gyro calibration is requested
	if(config.dmp_auto_calibrate_gyro){
		feature_mask|=DMP_FEATURE_GYRO_CAL;
	}
	// enable reading accel/gyro is requested
	if(config.dmp_fetch_accel_gyro){
		feature_mask|=DMP_FEATURE_SEND_RAW_ACCEL|DMP_FEATURE_SEND_ANY_GYRO;
	}
	if(__dmp_enable_feature(feature_mask)<0){
		fprintf(stderr,"ERROR: failed to enable DMP features\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// this changes the rate new dmp data is put in the fifo
	// fixing at 200 causes gyro scaling issues at lower mpu sample rates
	if(__dmp_set_fifo_rate(config.dmp_sample_rate)<0){
		fprintf(stderr,"ERROR: failed to set DMP fifo rate\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// turn the dmp on
	if(__mpu_set_dmp_state(1)<0) {
		fprintf(stderr,"ERROR: __mpu_set_dmp_state(1) failed\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// set interrupt mode to continuous as opposed to GESTURE
	if(__dmp_set_interrupt_mode(DMP_INT_CONTINUOUS)<0){
		fprintf(stderr,"ERROR: failed to set DMP interrupt mode to continuous\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// done writing to bus for now
	rc_i2c_unlock_bus(config.i2c_bus);

	// get ready to start the interrupt handler thread
	data_ptr->tap_detected=0;
	imu_shutdown_flag = 0;
	dmp_callback_func=NULL;
	tap_callback_func=NULL;

	// now start the thread with specified priority
	pthread_attr_init(&pthread_attr);
	pthread_attr_setinheritsched(&pthread_attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&pthread_attr, SCHED_FIFO);
	fifo_param.sched_priority = config.dmp_interrupt_priority;
	pthread_attr_setschedparam(&pthread_attr, &fifo_param);
	pthread_create(&imu_interrupt_thread, &pthread_attr, __imu_interrupt_handler, (void*) NULL);

	// thread is running, set the flag
	thread_running_flag = 1;
	// sleep for a ms so the thread can start predictably
	rc_usleep(1000);
	#ifdef DEBUG
	int policy;
	struct sched_param params_tmp;
	pthread_getschedparam(imu_interrupt_thread, &policy, &params_tmp);
	printf("new policy: %d, fifo: %d, prio: %d\n", policy, SCHED_FIFO, params_tmp.sched_priority);
	#endif
	return 0;
}

/*******************************************************************************
 *  @brief      Write to the DMP memory.
 *  This function prevents I2C writes past the bank boundaries. The DMP memory
 *  is only accessible when the chip is awake.
 *  @param[in]  mem_addr    Memory location (bank << 8 | start address)
 *  @param[in]  length      Number of bytes to write.
 *  @param[in]  data        Bytes to write to memory.
 *  @return     0 if successful.
*******************************************************************************/
int __mpu_write_mem(unsigned short mem_addr, unsigned short length,\
							unsigned char *data)
{
	unsigned char tmp[2];
	if (!data){
		fprintf(stderr,"ERROR: in mpu_write_mem, NULL pointer\n");
		return -1;
	}
	tmp[0] = (unsigned char)(mem_addr >> 8);
	tmp[1] = (unsigned char)(mem_addr & 0xFF);
	// Check bank boundaries.
	if (tmp[1] + length > MPU6500_BANK_SIZE){
		fprintf(stderr,"mpu_write_mem exceeds bank size\n");
		return -1;
	}
	if (rc_i2c_write_bytes(config.i2c_bus,MPU6500_BANK_SEL, 2, tmp))
		return -1;
	if (rc_i2c_write_bytes(config.i2c_bus,MPU6500_MEM_R_W, length, data))
		return -1;
	return 0;
}

/*******************************************************************************
 *  @brief      Read from the DMP memory.
 *  This function prevents I2C reads past the bank boundaries. The DMP memory
 *  is only accessible when the chip is awake.
 *  @param[in]  mem_addr    Memory location (bank << 8 | start address)
 *  @param[in]  length      Number of bytes to read.
 *  @param[out] data        Bytes read from memory.
 *  @return     0 if successful.
*******************************************************************************/
int __mpu_read_mem(unsigned short mem_addr, unsigned short length,\
							unsigned char *data)
{
	unsigned char tmp[2];
	if (!data){
		fprintf(stderr,"ERROR: in mpu_write_mem, NULL pointer\n");
		return -1;
	}
	tmp[0] = (unsigned char)(mem_addr >> 8);
	tmp[1] = (unsigned char)(mem_addr & 0xFF);
	// Check bank boundaries.
	if (tmp[1] + length > MPU6500_BANK_SIZE){
		printf("mpu_read_mem exceeds bank size\n");
		return -1;
	}
	if (rc_i2c_write_bytes(config.i2c_bus,MPU6500_BANK_SEL, 2, tmp))
		return -1;
	if (rc_i2c_read_bytes(config.i2c_bus,MPU6500_MEM_R_W, length, data)!=length)
		return -1;
	return 0;
}

/*******************************************************************************
* int __dmp_load_motion_driver_firmware()
*
* loads pre-compiled firmware binary from invensense onto dmp
*******************************************************************************/
int __dmp_load_motion_driver_firmware()
{
	unsigned short ii;
	unsigned short this_write;
	// Must divide evenly into st.hw->bank_size to avoid bank crossings.
	unsigned char cur[DMP_LOAD_CHUNK], tmp[2];
	// make sure the address is set correctly
	rc_i2c_set_device_address(config.i2c_bus, config.i2c_addr);
	// loop through 16 bytes at a time and check each write for corruption
	for (ii=0; ii<DMP_CODE_SIZE; ii+=this_write) {
		this_write = min(DMP_LOAD_CHUNK, DMP_CODE_SIZE - ii);
		if (__mpu_write_mem(ii, this_write, (uint8_t*)&dmp_firmware[ii])){
			fprintf(stderr,"dmp firmware write failed\n");
			return -1;
		}
		if (__mpu_read_mem(ii, this_write, cur)){
			fprintf(stderr,"dmp firmware read failed\n");
			return -1;
		}
		if (memcmp(dmp_firmware+ii, cur, this_write)){
			fprintf(stderr,"dmp firmware write corrupted\n");
			return -2;
		}
	}
	// Set program start address.
	tmp[0] = dmp_start_addr >> 8;
	tmp[1] = dmp_start_addr & 0xFF;
	if (rc_i2c_write_bytes(config.i2c_bus, MPU6500_PRGM_START_H, 2, tmp)){
		fprintf(stderr,"ERROR writing to MPU6500_PRGM_START register\n");
		return -1;
	}
	return 0;
}

/*******************************************************************************
 *  @brief      Push gyro and accel orientation to the DMP.
 *  The orientation is represented here as the output of
 *  @e inv_orientation_matrix_to_scalar.
 *  @param[in]  orient  Gyro and accel orientation in body frame.
 *  @return     0 if successful.
*******************************************************************************/
int __dmp_set_orientation(unsigned short orient)
{
	unsigned char gyro_regs[3], accel_regs[3];
	const unsigned char gyro_axes[3] = {DINA4C, DINACD, DINA6C};
	const unsigned char accel_axes[3] = {DINA0C, DINAC9, DINA2C};
	const unsigned char gyro_sign[3] = {DINA36, DINA56, DINA76};
	const unsigned char accel_sign[3] = {DINA26, DINA46, DINA66};
	// populate fata to be written
	gyro_regs[0] = gyro_axes[orient & 3];
	gyro_regs[1] = gyro_axes[(orient >> 3) & 3];
	gyro_regs[2] = gyro_axes[(orient >> 6) & 3];
	accel_regs[0] = accel_axes[orient & 3];
	accel_regs[1] = accel_axes[(orient >> 3) & 3];
	accel_regs[2] = accel_axes[(orient >> 6) & 3];
	// Chip-to-body, axes only.
	if (__mpu_write_mem(FCFG_1, 3, gyro_regs)){
		fprintf(stderr, "ERROR: in dmp_set_orientation, failed to write dmp mem\n");
		return -1;
	}
	if (__mpu_write_mem(FCFG_2, 3, accel_regs)){
		fprintf(stderr, "ERROR: in dmp_set_orientation, failed to write dmp mem\n");
		return -1;
	}
	memcpy(gyro_regs, gyro_sign, 3);
	memcpy(accel_regs, accel_sign, 3);
	if (orient & 4) {
		gyro_regs[0] |= 1;
		accel_regs[0] |= 1;
	}
	if (orient & 0x20) {
		gyro_regs[1] |= 1;
		accel_regs[1] |= 1;
	}
	if (orient & 0x100) {
		gyro_regs[2] |= 1;
		accel_regs[2] |= 1;
	}
	// Chip-to-body, sign only.
	if(__mpu_write_mem(FCFG_3, 3, gyro_regs)){
		fprintf(stderr, "ERROR: in dmp_set_orientation, failed to write dmp mem\n");
		return -1;
	}
	if(__mpu_write_mem(FCFG_7, 3, accel_regs)){
		fprintf(stderr, "ERROR: in dmp_set_orientation, failed to write dmp mem\n");
		return -1;
	}
	return 0;
}

/*******************************************************************************
 *  @brief      Set DMP output rate.
 *  Only used when DMP is on.
 *  @param[in]  rate    Desired fifo rate (Hz).
 *  @return     0 if successful.
*******************************************************************************/
int __dmp_set_fifo_rate(unsigned short rate)
{
	const unsigned char regs_end[12] = {DINAFE, DINAF2, DINAAB,
		0xc4, DINAAA, DINAF1, DINADF, DINADF, 0xBB, 0xAF, DINADF, DINADF};
	unsigned short div;
	unsigned char tmp[8];
	if (rate > DMP_MAX_RATE){
		return -1;
	}
	// set the DMP scaling factors
	div = DMP_MAX_RATE / rate - 1;
	tmp[0] = (unsigned char)((div >> 8) & 0xFF);
	tmp[1] = (unsigned char)(div & 0xFF);
	if (__mpu_write_mem(D_0_22, 2, tmp)){
		fprintf(stderr,"ERROR: writing dmp sample rate reg");
		return -1;
	}
	if (__mpu_write_mem(CFG_6, 12, (unsigned char*)regs_end)){
		fprintf(stderr,"ERROR: writing dmp regs_end");
		return -1;
	}
	return 0;
}

/*******************************************************************************
* int __mpu_set_bypass(unsigned char bypass_on)
*
* configures the USER_CTRL and INT_PIN_CFG registers to turn on and off the
* i2c bypass mode for talking to the magnetometer. In random read mode this
* is used to turn on the bypass and left as is. In DMP mode bypass is turned
* off after configuration and the MPU fetches magnetometer data automatically.
* USER_CTRL - based on global variable dsp_en
* INT_PIN_CFG based on requested bypass state
*******************************************************************************/
int __mpu_set_bypass(uint8_t bypass_on)
{
	uint8_t tmp = 0;
	rc_i2c_set_device_address(config.i2c_bus, config.i2c_addr);
	// set up USER_CTRL first
	// DONT USE FIFO_EN_BIT in DMP mode, or the MPU will generate lots of
	// unwanted interruptss
	if(dmp_en){
		tmp |= FIFO_EN_BIT; // enable fifo for dsp mode
	}
	if(!bypass_on){
		tmp |= I2C_MST_EN; // i2c master mode when not in bypass
	}
	if (rc_i2c_write_byte(config.i2c_bus, USER_CTRL, tmp)){
		fprintf(stderr,"ERROR in mpu_set_bypass, failed to write USER_CTRL register\n");
		return -1;
	}
	rc_usleep(3000);
	// INT_PIN_CFG settings
	tmp = LATCH_INT_EN | INT_ANYRD_CLEAR | ACTL_ACTIVE_LOW; // latching
	//tmp =  ACTL_ACTIVE_LOW;	// non-latching
	if(bypass_on)
		tmp |= BYPASS_EN;
	if (rc_i2c_write_byte(config.i2c_bus, INT_PIN_CFG, tmp)){
		fprintf(stderr,"ERROR in mpu_set_bypass, failed to write INT_PIN_CFG register\n");
		return -1;
	}
	if(bypass_on){
		bypass_en = 1;
	}
	else{
		bypass_en = 0;
	}
	return 0;
}

/*******************************************************************************
* int __dmp_enable_gyro_cal(unsigned char enable)
*
* Taken straight from the Invensense DMP code. This enabled the automatic gyro
* calibration feature in the DMP. This this feature is fine for cell phones
* but annoying in control systems we do not use it here and instead ask users
* to run our own gyro_calibration routine.
*******************************************************************************/
int __dmp_enable_gyro_cal(unsigned char enable)
{
	if(enable){
		unsigned char regs[9] = {0xb8, 0xaa, 0xb3, 0x8d, 0xb4, 0x98, 0x0d, 0x35, 0x5d};
		return __mpu_write_mem(CFG_MOTION_BIAS, 9, regs);
	}
	else{
		unsigned char regs[9] = {0xb8, 0xaa, 0xaa, 0xaa, 0xb0, 0x88, 0xc3, 0xc5, 0xc7};
		return __mpu_write_mem(CFG_MOTION_BIAS, 9, regs);
	}
}

/*******************************************************************************
* int __dmp_enable_6x_lp_quat(unsigned char enable)
*
* Taken straight from the Invensense DMP code. This enabled quaternion filtering
* with accelerometer and gyro filtering.
*******************************************************************************/
int __dmp_enable_6x_lp_quat(unsigned char enable)
{
	unsigned char regs[4];
	if(enable){
		regs[0] = DINA20;
		regs[1] = DINA28;
		regs[2] = DINA30;
		regs[3] = DINA38;
	}
	else{
		memset(regs, 0xA3, 4);
	}
	__mpu_write_mem(CFG_8, 4, regs);
	return 0;
}

/*******************************************************************************
* int __dmp_enable_lp_quat(unsigned char enable)
*
* sets the DMP to do gyro-only quaternion filtering. This is not actually used
* here but remains as a vestige of the Invensense DMP code.
*******************************************************************************/
int __dmp_enable_lp_quat(unsigned char enable)
{
	unsigned char regs[4];
	if(enable){
		regs[0] = DINBC0;
		regs[1] = DINBC2;
		regs[2] = DINBC4;
		regs[3] = DINBC6;
	}
	else{
		memset(regs, 0x8B, 4);
	}
	__mpu_write_mem(CFG_LP_QUAT, 4, regs);
	return 0;
}

/*******************************************************************************
* int __mpu_reset_fifo()
*
* This is mostly from the Invensense open source codebase but modified to also
* allow magnetometer data to come in through the FIFO. This just turns off the
* interrupt, resets fifo and DMP, then starts them again. Used once while
* initializing (probably no necessary) then again if the fifo gets too full.
*******************************************************************************/
int __mpu_reset_fifo(void)
{
	uint8_t data;
	// make sure the i2c address is set correctly.
	// this shouldn't take any time at all if already set
	rc_i2c_set_device_address(config.i2c_bus, config.i2c_addr);
	// turn off interrupts, fifo, and usr_ctrl which is where the dmp fifo is enabled
	data = 0;
	if (rc_i2c_write_byte(config.i2c_bus, INT_ENABLE, data)) return -1;
	if (rc_i2c_write_byte(config.i2c_bus, FIFO_EN, data)) return -1;
	if (rc_i2c_write_byte(config.i2c_bus, USER_CTRL, data)) return -1;

	// reset fifo and wait
	data = BIT_FIFO_RST | BIT_DMP_RST;
	if (rc_i2c_write_byte(config.i2c_bus, USER_CTRL, data)) return -1;
	//rc_usleep(1000); // how I had it
	rc_usleep(50000); // invensense standard

	// enable the fifo and DMP fifo flags again
	// enabling DMP but NOT BIT_FIFO_EN gives quat out of bounds
	// but also no empty interrupts
	data = BIT_DMP_EN | BIT_FIFO_EN;
	if(rc_i2c_write_byte(config.i2c_bus, USER_CTRL, data)){
		return -1;
	}

	// turn on dmp interrupt enable bit again
	data = BIT_DMP_INT_EN;
	if (rc_i2c_write_byte(config.i2c_bus, INT_ENABLE, data)) return -1;
	data = 0;
	if (rc_i2c_write_byte(config.i2c_bus, FIFO_EN, data)) return -1;

	return 0;
}

/*******************************************************************************
* int __dmp_set_interrupt_mode(unsigned char mode)
*
* This is from the Invensense open source DMP code. It configures the DMP
* to trigger an interrupt either every sample or only on gestures. Here we
* only ever configure for continuous sampling.
*******************************************************************************/
int __dmp_set_interrupt_mode(unsigned char mode)
{
	const unsigned char regs_continuous[11] =
		{0xd8, 0xb1, 0xb9, 0xf3, 0x8b, 0xa3, 0x91, 0xb6, 0x09, 0xb4, 0xd9};
	const unsigned char regs_gesture[11] =
		{0xda, 0xb1, 0xb9, 0xf3, 0x8b, 0xa3, 0x91, 0xb6, 0xda, 0xb4, 0xda};
	switch(mode){
	case DMP_INT_CONTINUOUS:
		return __mpu_write_mem(CFG_FIFO_ON_EVENT, 11, (unsigned char*)regs_continuous);
	case DMP_INT_GESTURE:
		return __mpu_write_mem(CFG_FIFO_ON_EVENT, 11, (unsigned char*)regs_gesture);
	default:
		return -1;
	}
}

/**
 *  @brief      Set tap threshold for a specific axis.
 *  @param[in]  axis    1, 2, and 4 for XYZ accel, respectively.
 *  @param[in]  thresh  Tap threshold, in mg/ms.
 *  @return     0 if successful.
 */
int __dmp_set_tap_thresh(unsigned char axis, unsigned short thresh)
{
	unsigned char tmp[4];
	float scaled_thresh;
	unsigned short dmp_thresh, dmp_thresh_2;
	if (!(axis & TAP_XYZ) || thresh > 1600)
		return -1;

	scaled_thresh = (float)thresh / DMP_SAMPLE_RATE;

	switch (config.accel_fsr) {
	case ACCEL_FSR_2G:
		dmp_thresh = (unsigned short)(scaled_thresh * 16384);
		/* dmp_thresh * 0.75 */
		dmp_thresh_2 = (unsigned short)(scaled_thresh * 12288);
		break;
	case ACCEL_FSR_4G:
		dmp_thresh = (unsigned short)(scaled_thresh * 8192);
		/* dmp_thresh * 0.75 */
		dmp_thresh_2 = (unsigned short)(scaled_thresh * 6144);
		break;
	case ACCEL_FSR_8G:
		dmp_thresh = (unsigned short)(scaled_thresh * 4096);
		/* dmp_thresh * 0.75 */
		dmp_thresh_2 = (unsigned short)(scaled_thresh * 3072);
		break;
	case ACCEL_FSR_16G:
		dmp_thresh = (unsigned short)(scaled_thresh * 2048);
		/* dmp_thresh * 0.75 */
		dmp_thresh_2 = (unsigned short)(scaled_thresh * 1536);
		break;
	default:
		return -1;
	}
	tmp[0] = (unsigned char)(dmp_thresh >> 8);
	tmp[1] = (unsigned char)(dmp_thresh & 0xFF);
	tmp[2] = (unsigned char)(dmp_thresh_2 >> 8);
	tmp[3] = (unsigned char)(dmp_thresh_2 & 0xFF);

	if (axis & TAP_X) {
		if (__mpu_write_mem(DMP_TAP_THX, 2, tmp))
			return -1;
		if (__mpu_write_mem(D_1_36, 2, tmp+2))
			return -1;
	}
	if (axis & TAP_Y) {
		if (__mpu_write_mem(DMP_TAP_THY, 2, tmp))
			return -1;
		if (__mpu_write_mem(D_1_40, 2, tmp+2))
			return -1;
	}
	if (axis & TAP_Z) {
		if (__mpu_write_mem(DMP_TAP_THZ, 2, tmp))
			return -1;
		if (__mpu_write_mem(D_1_44, 2, tmp+2))
			return -1;
	}
	return 0;
}

/**
 *  @brief      Set which axes will register a tap.
 *  @param[in]  axis    1, 2, and 4 for XYZ, respectively.
 *  @return     0 if successful.
 */
int __dmp_set_tap_axes(unsigned char axis)
{
	unsigned char tmp = 0;

	if (axis & TAP_X)
	tmp |= 0x30;
	if (axis & TAP_Y)
	tmp |= 0x0C;
	if (axis & TAP_Z)
	tmp |= 0x03;
	return __mpu_write_mem(D_1_72, 1, &tmp);
}

/**
 *  @brief      Set minimum number of taps needed for an interrupt.
 *  @param[in]  min_taps    Minimum consecutive taps (1-4).
 *  @return     0 if successful.
 */
int __dmp_set_tap_count(unsigned char min_taps)
{
	unsigned char tmp;

	if (min_taps < 1)
	min_taps = 1;
	else if (min_taps > 4)
	min_taps = 4;

	tmp = min_taps - 1;
	return __mpu_write_mem(D_1_79, 1, &tmp);
}

/**
 *  @brief      Set length between valid taps.
 *  @param[in]  time    Milliseconds between taps.
 *  @return     0 if successful.
 */
int __dmp_set_tap_time(unsigned short time)
{
	unsigned short dmp_time;
	unsigned char tmp[2];

	dmp_time = time / (1000 / DMP_SAMPLE_RATE);
	tmp[0] = (unsigned char)(dmp_time >> 8);
	tmp[1] = (unsigned char)(dmp_time & 0xFF);
	return __mpu_write_mem(DMP_TAPW_MIN, 2, tmp);
}

/**
 *  @brief      Set max time between taps to register as a multi-tap.
 *  @param[in]  time    Max milliseconds between taps.
 *  @return     0 if successful.
 */
int __dmp_set_tap_time_multi(unsigned short time)
{
	unsigned short dmp_time;
	unsigned char tmp[2];

	dmp_time = time / (1000 / DMP_SAMPLE_RATE);
	tmp[0] = (unsigned char)(dmp_time >> 8);
	tmp[1] = (unsigned char)(dmp_time & 0xFF);
	return __mpu_write_mem(D_1_218, 2, tmp);
}

/**
 *  @brief      Set shake rejection threshold.
 *  If the DMP detects a gyro sample larger than @e thresh, taps are rejected.
 *  @param[in]  sf      Gyro scale factor.
 *  @param[in]  thresh  Gyro threshold in dps.
 *  @return     0 if successful.
 */
int __dmp_set_shake_reject_thresh(long sf, unsigned short thresh)
{
	unsigned char tmp[4];
	long thresh_scaled = sf / 1000 * thresh;
	tmp[0] = (unsigned char)(((long)thresh_scaled >> 24) & 0xFF);
	tmp[1] = (unsigned char)(((long)thresh_scaled >> 16) & 0xFF);
	tmp[2] = (unsigned char)(((long)thresh_scaled >> 8) & 0xFF);
	tmp[3] = (unsigned char)((long)thresh_scaled & 0xFF);
	return __mpu_write_mem(D_1_92, 4, tmp);
}

/**
 *  @brief      Set shake rejection time.
 *  Sets the length of time that the gyro must be outside of the threshold set
 *  by @e gyro_set_shake_reject_thresh before taps are rejected. A mandatory
 *  60 ms is added to this parameter.
 *  @param[in]  time    Time in milliseconds.
 *  @return     0 if successful.
 */
int __dmp_set_shake_reject_time(unsigned short time)
{
	unsigned char tmp[2];

	time /= (1000 / DMP_SAMPLE_RATE);
	tmp[0] = time >> 8;
	tmp[1] = time & 0xFF;
	return __mpu_write_mem(D_1_90,2,tmp);
}

/**
 *  @brief      Set shake rejection timeout.
 *  Sets the length of time after a shake rejection that the gyro must stay
 *  inside of the threshold before taps can be detected again. A mandatory
 *  60 ms is added to this parameter.
 *  @param[in]  time    Time in milliseconds.
 *  @return     0 if successful.
 */
int __dmp_set_shake_reject_timeout(unsigned short time)
{
	unsigned char tmp[2];

	time /= (1000 / DMP_SAMPLE_RATE);
	tmp[0] = time >> 8;
	tmp[1] = time & 0xFF;
	return __mpu_write_mem(D_1_88,2,tmp);
}

/*******************************************************************************
* int __dmp_enable_feature(unsigned short mask)
*
* This is mostly taken from the Invensense DMP code and serves to turn on and
* off DMP features based on the feature mask. We modified to remove some
* irrelevant features and set our own fifo-length variable. This probably
* isn't necessary to remain in its current form as rc_mpu_initialize_dmp uses
* a fixed set of features but we keep it as is since it works fine.
*******************************************************************************/
int __dmp_enable_feature(unsigned short mask)
{
	unsigned char tmp[10];
	// Set integration scale factor.
	tmp[0] = (unsigned char)((GYRO_SF >> 24) & 0xFF);
	tmp[1] = (unsigned char)((GYRO_SF >> 16) & 0xFF);
	tmp[2] = (unsigned char)((GYRO_SF >> 8) & 0xFF);
	tmp[3] = (unsigned char)(GYRO_SF & 0xFF);
	if(__mpu_write_mem(D_0_104, 4, tmp)<0){
		fprintf(stderr, "ERROR: in dmp_enable_feature, failed to write mpu mem\n");
		return -1;
	}
	// Send sensor data to the FIFO.
	tmp[0] = 0xA3;
	if (mask & DMP_FEATURE_SEND_RAW_ACCEL) {
		tmp[1] = 0xC0;
		tmp[2] = 0xC8;
		tmp[3] = 0xC2;
	} else {
		tmp[1] = 0xA3;
		tmp[2] = 0xA3;
		tmp[3] = 0xA3;
	}
	if (mask & DMP_FEATURE_SEND_ANY_GYRO) {
		tmp[4] = 0xC4;
		tmp[5] = 0xCC;
		tmp[6] = 0xC6;
	} else {
		tmp[4] = 0xA3;
		tmp[5] = 0xA3;
		tmp[6] = 0xA3;
	}
	tmp[7] = 0xA3;
	tmp[8] = 0xA3;
	tmp[9] = 0xA3;
	if(__mpu_write_mem(CFG_15,10,tmp)<0){
		fprintf(stderr, "ERROR: in dmp_enable_feature, failed to write mpu mem\n");
		return -1;
	}
	// Send gesture data to the FIFO.
	if (mask & (DMP_FEATURE_TAP | DMP_FEATURE_ANDROID_ORIENT)){
		tmp[0] = DINA20;
	}
	else{
		tmp[0] = 0xD8;
	}
	if(__mpu_write_mem(CFG_27,1,tmp)){
		fprintf(stderr, "ERROR: in dmp_enable_feature, failed to write mpu mem\n");
		return -1;
	}

	if(mask & DMP_FEATURE_GYRO_CAL) __dmp_enable_gyro_cal(1);
	else __dmp_enable_gyro_cal(0);

	if (mask & DMP_FEATURE_SEND_ANY_GYRO) {
		if (mask & DMP_FEATURE_SEND_CAL_GYRO) {
			tmp[0] = 0xB2;
			tmp[1] = 0x8B;
			tmp[2] = 0xB6;
			tmp[3] = 0x9B;
		} else {
			tmp[0] = DINAC0;
			tmp[1] = DINA80;
			tmp[2] = DINAC2;
			tmp[3] = DINA90;
		}
		__mpu_write_mem(CFG_GYRO_RAW_DATA, 4, tmp);
	}

	// configure tap feature
	if (mask & DMP_FEATURE_TAP) {
		/* Enable tap. */
		tmp[0] = 0xF8;
		__mpu_write_mem(CFG_20, 1, tmp);
		__dmp_set_tap_thresh(TAP_XYZ, config.tap_threshold);
		__dmp_set_tap_axes(TAP_XYZ);
		__dmp_set_tap_count(1);
		__dmp_set_tap_time(100);
		__dmp_set_tap_time_multi(500);

		// shake rejection ignores taps when system is moving, set threshold
		// high so this doesn't happen too often
		__dmp_set_shake_reject_thresh(GYRO_SF, 600); // default was 200
		__dmp_set_shake_reject_time(40);
		__dmp_set_shake_reject_timeout(10);
	} else {
		tmp[0] = 0xD8;
		__mpu_write_mem(CFG_20, 1, tmp);
	}


	if (mask & DMP_FEATURE_ANDROID_ORIENT) {
		tmp[0] = 0xD9;
	} else
		tmp[0] = 0xD8;
	__mpu_write_mem(CFG_ANDROID_ORIENT_INT, 1, tmp);

	if (mask & DMP_FEATURE_LP_QUAT){
		__dmp_enable_lp_quat(1);
	}
	else{
		__dmp_enable_lp_quat(0);
	}
	if (mask & DMP_FEATURE_6X_LP_QUAT){
		__dmp_enable_6x_lp_quat(1);
	}
	else{
		__dmp_enable_6x_lp_quat(0);
	}
	__mpu_reset_fifo();
	packet_len = 0;
	if(mask & DMP_FEATURE_SEND_RAW_ACCEL){
		packet_len += 6;
	}
	if(mask & DMP_FEATURE_SEND_ANY_GYRO){
		packet_len += 6;
	}
	if(mask & (DMP_FEATURE_LP_QUAT | DMP_FEATURE_6X_LP_QUAT)){
		packet_len += 16;
	}
	if(mask & (DMP_FEATURE_TAP | DMP_FEATURE_ANDROID_ORIENT)){
		packet_len += 4;
	}
	return 0;
}
/*******************************************************************************
* int __set_int_enable(unsigned char enable)
*
* This is a vestige of the invensense mpu open source code and is probably
* not necessary but remains here anyway.
*******************************************************************************/
int __set_int_enable(unsigned char enable)
{
	unsigned char tmp;
	if (enable){
		tmp = BIT_DMP_INT_EN;
	}
	else{
		tmp = 0x00;
	}
	if(rc_i2c_write_byte(config.i2c_bus, INT_ENABLE, tmp)){
		fprintf(stderr, "ERROR: in set_int_enable, failed to write INT_ENABLE register\n");
		return -1;
	}
	// disable all other FIFO features leaving just DMP
	if (rc_i2c_write_byte(config.i2c_bus, FIFO_EN, 0)){
		fprintf(stderr, "ERROR: in set_int_enable, failed to write FIFO_EN register\n");
		return -1;
	}
	return 0;
}

/*******************************************************************************
int __mpu_set_sample_rate(int rate)
Sets the clock rate divider for sensor sampling
*******************************************************************************/
int __mpu_set_sample_rate(int rate)
{
	if(rate>1000 || rate<4){
		fprintf(stderr,"ERROR: sample rate must be between 4 & 1000\n");
		return -1;
	}
	/* Keep constant sample rate, FIFO rate controlled by DMP. */
	uint8_t div = (1000/rate) - 1;
	#ifdef DEBUG
	printf("setting divider to %d\n", div);
	#endif
	if(rc_i2c_write_byte(config.i2c_bus, SMPLRT_DIV, div)){
		fprintf(stderr,"ERROR: in mpu_set_sample_rate, failed to write SMPLRT_DIV register\n");
		return -1;
	}
	return 0;
}

/*******************************************************************************
*  int __mpu_set_dmp_state(unsigned char enable)
*
* This turns on and off the DMP interrupt and resets the FIFO. This probably
* isn't necessary as rc_mpu_initialize_dmp sets these registers but it remains
* here as a vestige of the invensense open source dmp code.
*******************************************************************************/
int __mpu_set_dmp_state(unsigned char enable)
{
	if (enable) {
		// Disable data ready interrupt.
		__set_int_enable(0);
		// make sure bypass mode is enabled
		__mpu_set_bypass(1);
		// Remove FIFO elements.
		rc_i2c_write_byte(config.i2c_bus, FIFO_EN , 0);
		// Enable DMP interrupt.
		__set_int_enable(1);
		__mpu_reset_fifo();
	}
	else {
		// Disable DMP interrupt.
		__set_int_enable(0);
		// Restore FIFO settings.
		rc_i2c_write_byte(config.i2c_bus, FIFO_EN , 0);
		__mpu_reset_fifo();
	}
	return 0;
}

/*******************************************************************************
* void* __imu_interrupt_handler(void* ptr)
*
* Here is where the magic happens. This function runs as its own thread and
* monitors the gpio pin config.gpio_interrupt_pin with the blocking function call
* poll(). If a valid interrupt is received from the IMU then mark the timestamp,
* read in the IMU data, and call the user-defined interrupt function if set.
*******************************************************************************/
void* __imu_interrupt_handler( __unused void* ptr)
{
	struct pollfd fdset[1];
	int ret;
	// start magnetometer read divider at the end of the counter
	// so it reads on the first run
	int mag_div_step = config.mag_sample_rate_div;
	char buf[64];
	int first_run = 1;
	int imu_gpio_fd = rc_gpio_get_value_fd(config.gpio_interrupt_pin);
	if(imu_gpio_fd == -1){
		fprintf(stderr,"ERROR: can't open config.gpio_interrupt_pin gpio fd\n");
		fprintf(stderr,"aborting imu_interrupt_handler\n");
		return NULL;
	}
	fdset[0].fd = imu_gpio_fd;
	fdset[0].events = POLLPRI;
	// keep running until the program closes
	__mpu_reset_fifo();
	while(imu_shutdown_flag!=1) {
		// system hangs here until IMU FIFO interrupt
		poll(fdset, 1, IMU_POLL_TIMEOUT);
		if(imu_shutdown_flag==1){
			break;
		}
		else if (fdset[0].revents & POLLPRI) {
			lseek(fdset[0].fd, 0, SEEK_SET);
			read(fdset[0].fd, buf, 64);
			// interrupt received, mark the timestamp
			last_interrupt_timestamp_nanos = rc_nanos_since_epoch();
			// try to load fifo no matter the claim bus state
			if(rc_i2c_get_lock(config.i2c_bus)){
				fprintf(stderr,"WARNING: Something has claimed the I2C bus when an\n");
				fprintf(stderr,"IMU interrupt was received. Reading IMU anyway.\n");
			}
			// aquires bus
			rc_i2c_lock_bus(config.i2c_bus);
			// aquires mutex
			pthread_mutex_lock( &read_mutex );
			pthread_mutex_lock( &tap_mutex );
			// read data
			ret = __read_dmp_fifo(data_ptr);
			rc_i2c_unlock_bus(config.i2c_bus);
			// record if it was successful or not
			if(ret==0){
				last_read_successful=1;
				if(data_ptr->tap_detected){
					last_tap_timestamp_nanos = last_interrupt_timestamp_nanos;
				}
			}
			else{
				last_read_successful=0;
			}
			// if reading mag before callback, check divider and do it now
			if(config.enable_magnetometer && !config.read_mag_after_callback){
				if(mag_div_step>=config.mag_sample_rate_div){
					#ifdef DEBUG
					printf("reading mag before callback\n");
					#endif
					rc_mpu_read_mag(data_ptr);
					// reset address back for next read
					rc_i2c_set_device_address(config.i2c_bus,config.i2c_addr);
					mag_div_step=1;
				}
				else mag_div_step++;
			}
			// releases bus
			rc_i2c_unlock_bus(config.i2c_bus);
			// call the user function if not the first run
			if(first_run == 1){
				first_run = 0;
			}
			else if(last_read_successful){
				if(dmp_callback_func!=NULL) dmp_callback_func();
				// signals that a measurement is available to blocking function
				pthread_cond_broadcast(&read_condition);
				// additionally call tap callback if one was received
				if(data_ptr->tap_detected){
					if(tap_callback_func!=NULL) tap_callback_func(data_ptr->last_tap_direction);
					pthread_cond_broadcast(&tap_condition);
				}
			}

			// releases mutex
			pthread_mutex_unlock(&read_mutex);
			pthread_mutex_unlock(&tap_mutex);

			// if reading mag after interrupt, check divider and do it now
			if(config.enable_magnetometer && config.read_mag_after_callback){
				if(mag_div_step>=config.mag_sample_rate_div){
					#ifdef DEBUG
					printf("reading mag after ISR\n");
					#endif
					rc_i2c_lock_bus(config.i2c_bus);
					rc_mpu_read_mag(data_ptr);
					rc_i2c_unlock_bus(config.i2c_bus);
					// reset address back for next read
					rc_i2c_set_device_address(config.i2c_bus,config.i2c_addr);
					mag_div_step=1;
				}
				else mag_div_step++;
			}
		}
	}

	// aquires mutex
	pthread_mutex_lock( &read_mutex );
	// /releases other threads
	pthread_cond_broadcast( &read_condition );
	// releases mutex
	pthread_mutex_unlock( &read_mutex );
	thread_running_flag = 0;
	return 0;
}

/*******************************************************************************
* int rc_mpu_set_dmp_callback(void (*func)(void))
*
* sets a user function to be called when new data is read
*******************************************************************************/
int rc_mpu_set_dmp_callback(void (*func)(void))
{
	if(func==NULL){
		fprintf(stderr,"ERROR: trying to assign NULL pointer to dmp_callback_func\n");
		return -1;
	}
	dmp_callback_func = func;
	return 0;
}

int rc_mpu_set_tap_callback(void (*func)(int dir))
{
	if(func==NULL){
		fprintf(stderr,"ERROR: trying to assign NULL pointer to tap_callback_func\n");
		return -1;
	}
	tap_callback_func = func;
	return 0;
}


/*******************************************************************************
* int __read_dmp_fifo(rc_mpu_data_t* data)
*
* Reads the FIFO buffer and populates the data struct. Here is where we see
* bad/empty/double packets due to i2c bus errors and the IMU failing to have
* data ready in time. enabling warnings in the config struct will let this
* function print out warnings when these conditions are detected. If write
* errors are detected then this function tries some i2c transfers a second time.
*******************************************************************************/
int __read_dmp_fifo(rc_mpu_data_t* data)
{
	unsigned char raw[MAX_FIFO_BUFFER];
	long quat_q14[4], quat[4], quat_mag_sq;
	uint16_t fifo_count;
	int ret;
	int i = 0; // position of beginning of quaternion
	int j = 0; // position of beginning of accel/gyro data
	static int first_run = 1; // set to 0 after first call
	double q_tmp[4];
	double sum,qlen;

	if(!dmp_en){
		printf("only use mpu_read_fifo in dmp mode\n");
		return -1;
	}

	// if the fifo packet_len variable not set up yet, this function must
	// have been called prematurely
	if(packet_len!=FIFO_LEN_QUAT_ACCEL_GYRO_TAP && packet_len!=FIFO_LEN_QUAT_TAP){
		fprintf(stderr,"ERROR: packet_len is set incorrectly for read_dmp_fifo\n");
		return -1;
	}

	// make sure the i2c address is set correctly.
	// this shouldn't take any time at all if already set
	rc_i2c_set_device_address(config.i2c_bus, config.i2c_addr);
	int is_new_dmp_data = 0;

	// check fifo count register to make sure new data is there
	if(rc_i2c_read_word(config.i2c_bus, FIFO_COUNTH, &fifo_count)<0){
		if(config.show_warnings){
			printf("fifo_count i2c error: %s\n",strerror(errno));
		}
		return -1;
	}
	#ifdef DEBUG
	printf("fifo_count: %d\n", fifo_count);
	#endif

	/***********************************************************************
	* Check how many packets are in the fifo buffer
	***********************************************************************/

	// if empty FIFO, just return, nothing else to do
	if(fifo_count==0){
		if(config.show_warnings && first_run!=1){
			printf("WARNING: empty fifo\n");
		}
		return -1;
	}
	// one packet, perfect!
	else if(fifo_count==packet_len){
		i = 0; // set quaternion offset to 0
	}
	// if exactly 2 or 3 packets are there we just missed some (whoops)
	// read both in and set the offset i to one packet length
	// the last packet data will be read normally
	else if(fifo_count==2*packet_len){
		if(config.show_warnings&& first_run!=1){
			printf("warning: imu fifo contains two packets\n");
		}
		i=packet_len;
	}
	else if(fifo_count==3*packet_len){
		if(config.show_warnings&& first_run!=1){
			printf("warning: imu fifo contains three packets\n");
		}
		i=2*packet_len;
	}
	else if(fifo_count==4*packet_len){
		if(config.show_warnings&& first_run!=1){
			printf("warning: imu fifo contains four packets\n");
		}
		i=2*packet_len;
	}
	else if(fifo_count==5*packet_len){
		if(config.show_warnings&& first_run!=1){
			printf("warning: imu fifo contains five packets\n");
		}
		i=2*packet_len;
	}
	// finally, if we got a weird packet length, reset the fifo
	else{
		if(config.show_warnings && first_run!=1){
			printf("warning: %d bytes in FIFO, expected %d\n", fifo_count,packet_len);
		}
		__mpu_reset_fifo();
		return -1;
	}

	/***********************************************************************
	* read in the fifo
	******************\\\**************************************************/
	memset(raw,0,MAX_FIFO_BUFFER);
	// read it in!
	ret = rc_i2c_read_bytes(config.i2c_bus, FIFO_R_W, fifo_count, &raw[0]);
	if(ret<0){
		// if i2c_read returned -1 there was an error, try again
		ret = rc_i2c_read_bytes(config.i2c_bus, FIFO_R_W, fifo_count, &raw[0]);
	}
	if(ret!=fifo_count){
		if(config.show_warnings){
			fprintf(stderr,"ERROR: failed to read fifo buffer register\n");
			printf("read %d bytes, expected %d\n", ret, packet_len);
		}
		return -1;
	}


	// now we can read the quaternion which is always first
	// parse the quaternion data from the buffer
	quat[0] = ((long)raw[i+0] << 24) | ((long)raw[i+1] << 16) |
		((long)raw[i+2] << 8) | raw[i+3];
	quat[1] = ((long)raw[i+4] << 24) | ((long)raw[i+5] << 16) |
		((long)raw[i+6] << 8) | raw[i+7];
	quat[2] = ((long)raw[i+8] << 24) | ((long)raw[i+9] << 16) |
		((long)raw[i+10] << 8) | raw[i+11];
	quat[3] = ((long)raw[i+12] << 24) | ((long)raw[i+13] << 16) |
		((long)raw[i+14] << 8) | raw[i+15];

	// increment poisition in buffer after 16 bits of quaternion
	i+=16;

	// check the quaternion size, make sure it's correct
	quat_q14[0] = quat[0] >> 16;
	quat_q14[1] = quat[1] >> 16;
	quat_q14[2] = quat[2] >> 16;
	quat_q14[3] = quat[3] >> 16;
	quat_mag_sq = quat_q14[0] * quat_q14[0] + quat_q14[1] * quat_q14[1] + \
		quat_q14[2] * quat_q14[2] + quat_q14[3] * quat_q14[3];
	if ((quat_mag_sq < QUAT_MAG_SQ_MIN)||(quat_mag_sq > QUAT_MAG_SQ_MAX)){
		if(config.show_warnings){
			printf("warning: Quaternion out of bounds, fifo_count: %d\n", fifo_count);
		}
		__mpu_reset_fifo();
		return -1;
	}

	// do double-precision quaternion normalization since the numbers
	// in raw format are huge
	for(j=0;j<4;j++) q_tmp[j]=(double)quat[j];
	sum = 0.0;
	for(j=0;j<4;j++) sum+=q_tmp[j]*q_tmp[j];
	qlen=sqrt(sum);
	for(j=0;j<4;j++) q_tmp[j]/=qlen;
	// make floating point and put in output
	for(j=0;j<4;j++) data->dmp_quat[j]=(float)q_tmp[j];

	// fill in tait-bryan angles to the data struct
	rc_quaternion_to_tb_array(data->dmp_quat, data->dmp_TaitBryan);
	is_new_dmp_data=1;


	if(packet_len==FIFO_LEN_QUAT_ACCEL_GYRO_TAP){
		// Read Accel values and load into imu_data struct
		// Turn the MSB and LSB into a signed 16-bit value
		data->raw_accel[0] = (int16_t)(((uint16_t)raw[i+0]<<8)|raw[i+1]);
		data->raw_accel[1] = (int16_t)(((uint16_t)raw[i+2]<<8)|raw[i+3]);
		data->raw_accel[2] = (int16_t)(((uint16_t)raw[i+4]<<8)|raw[i+5]);
		i+=6;
		// Fill in real unit values
		data->accel[0] = data->raw_accel[0] * data->accel_to_ms2;
		data->accel[1] = data->raw_accel[1] * data->accel_to_ms2;
		data->accel[2] = data->raw_accel[2] * data->accel_to_ms2;


		// Read gyro values and load into imu_data struct
		// Turn the MSB and LSB into a signed 16-bit value

		data->raw_gyro[0] = (int16_t)(((int16_t)raw[0+i]<<8)|raw[1+i]);
		data->raw_gyro[1] = (int16_t)(((int16_t)raw[2+i]<<8)|raw[3+i]);
		data->raw_gyro[2] = (int16_t)(((int16_t)raw[4+i]<<8)|raw[5+i]);
		i+=6;
		// Fill in real unit values
		data->gyro[0] = data->raw_gyro[0] * data->gyro_to_degs;
		data->gyro[1] = data->raw_gyro[1] * data->gyro_to_degs;
		data->gyro[2] = data->raw_gyro[2] * data->gyro_to_degs;
	}

	// TODO read in tap data
	unsigned char tap;
	//android_orient = gesture[3] & 0xC0;
	tap = 0x3F & raw[i+3];

	if(raw[i+1] & INT_SRC_TAP){
		#ifdef DEBUG
		unsigned char direction, count;
		direction = tap >> 3;
		count = (tap % 8) + 1;
		printf("tap dir: %d count: %d\n", direction, count);
		#endif
		data_ptr->last_tap_direction = tap>>3;
		data_ptr->tap_detected=1;
	}
	else data_ptr->tap_detected=0;

	// run data_fusion to filter yaw with compass
	if(is_new_dmp_data && config.enable_magnetometer){
		#ifdef DEBUG
		printf("running data_fusion\n");
		#endif
		__data_fusion(data);
	}

	// if we finally got dmp data, turn off the first run flag
	if(is_new_dmp_data) first_run=0;

	// finally, our return value is based on the presence of DMP data only
	// even if new magnetometer data was read, the expected timing must come
	// from the DMP samples only
	if(is_new_dmp_data) return 0;
	else return -1;
}


// /*******************************************************************************
// * We can detect a corrupted FIFO by monitoring the quaternion data and
// * ensuring that the magnitude is always normalized to one. This
// * shouldn't happen in normal operation, but if an I2C error occurs,
// * the FIFO reads might become misaligned.
// *
// * Let's start by scaling down the quaternion data to avoid long long
// * math.
// *******************************************************************************/
// int __check_quaternion_validity(unsigned char* raw, int i)
// {
// 	long quat_q14[4], quat[4], quat_mag_sq;
// 	// parse the quaternion data from the buffer
// 	quat[0] = ((long)raw[i+0] << 24) | ((long)raw[i+1] << 16) |
// 		((long)raw[i+2] << 8) | raw[i+3];
// 	quat[1] = ((long)raw[i+4] << 24) | ((long)raw[i+5] << 16) |
// 		((long)raw[i+6] << 8) | raw[i+7];
// 	quat[2] = ((long)raw[i+8] << 24) | ((long)raw[i+9] << 16) |
// 		((long)raw[i+10] << 8) | raw[i+11];
// 	quat[3] = ((long)raw[i+12] << 24) | ((long)raw[i+13] << 16) |
// 		((long)raw[i+14] << 8) | raw[i+15];


// 	quat_q14[0] = quat[0] >> 16;
// 	quat_q14[1] = quat[1] >> 16;
// 	quat_q14[2] = quat[2] >> 16;
// 	quat_q14[3] = quat[3] >> 16;
// 	quat_mag_sq = quat_q14[0] * quat_q14[0] + quat_q14[1] * quat_q14[1] +
// 		quat_q14[2] * quat_q14[2] + quat_q14[3] * quat_q14[3];
// 	if ((quat_mag_sq < QUAT_MAG_SQ_MIN) ||(quat_mag_sq > QUAT_MAG_SQ_MAX)){
// 		return 0;
// 	}
// 	return 1;
// }

/*******************************************************************************
* int __data_fusion(rc_mpu_data_t* data)
*
* This fuses the magnetometer data with the quaternion straight from the DMP
* to correct the yaw heading to a compass heading. Much thanks to Pansenti for
* open sourcing this routine. In addition to the Pansenti implementation I also
* correct the magnetometer data for DMP orientation, initialize yaw with the
* magnetometer to prevent initial rise time, and correct the yaw_mixing_factor
* with the sample rate so the filter rise time remains constant with different
* sample rates.
*******************************************************************************/
int __data_fusion(rc_mpu_data_t* data)
{
	float tilt_tb[3], tilt_q[4], mag_vec[3];
	static float newMagYaw = 0;
	static float newDMPYaw = 0;
	float lastDMPYaw, lastMagYaw, newYaw;
	static int dmp_spin_counter = 0;
	static int mag_spin_counter = 0;
	static int first_run = 1; // set to 0 after first call to this function


	// start by filling in the roll/pitch components of the fused euler
	// angles from the DMP generated angles. Ignore yaw for now, we have to
	// filter that later.
	tilt_tb[0] = data->dmp_TaitBryan[TB_PITCH_X];
	tilt_tb[1] = data->dmp_TaitBryan[TB_ROLL_Y];
	tilt_tb[2] = 0.0f;

	// generate a quaternion rotation of just roll/pitch
	rc_quaternion_from_tb_array(tilt_tb,tilt_q);

	// create a quaternion vector from the current magnetic field vector
	// in IMU body coordinate frame. Since the DMP quaternion is aligned with
	// a particular orientation, we must be careful to orient the magnetometer
	// data to match.
	switch(config.orient){
	case ORIENTATION_Z_UP:
		mag_vec[0] = data->mag[TB_PITCH_X];
		mag_vec[1] = data->mag[TB_ROLL_Y];
		mag_vec[2] = data->mag[TB_YAW_Z];
		break;
	case ORIENTATION_Z_DOWN:
		mag_vec[0] = -data->mag[TB_PITCH_X];
		mag_vec[1] = data->mag[TB_ROLL_Y];
		mag_vec[2] = -data->mag[TB_YAW_Z];
		break;
	case ORIENTATION_X_UP:
		mag_vec[0] = data->mag[TB_YAW_Z];
		mag_vec[1] = data->mag[TB_ROLL_Y];
		mag_vec[2] = data->mag[TB_PITCH_X];
		break;
	case ORIENTATION_X_DOWN:
		mag_vec[0] = -data->mag[TB_YAW_Z];
		mag_vec[1] = data->mag[TB_ROLL_Y];
		mag_vec[2] = -data->mag[TB_PITCH_X];
		break;
	case ORIENTATION_Y_UP:
		mag_vec[0] = data->mag[TB_PITCH_X];
		mag_vec[1] = -data->mag[TB_YAW_Z];
		mag_vec[2] = data->mag[TB_ROLL_Y];
		break;
	case ORIENTATION_Y_DOWN:
		mag_vec[0] = data->mag[TB_PITCH_X];
		mag_vec[1] = data->mag[TB_YAW_Z];
		mag_vec[2] = -data->mag[TB_ROLL_Y];
		break;
	case ORIENTATION_X_FORWARD:
		mag_vec[0] = data->mag[TB_ROLL_Y];
		mag_vec[1] = -data->mag[TB_PITCH_X];
		mag_vec[2] = data->mag[TB_YAW_Z];
		break;
	case ORIENTATION_X_BACK:
		mag_vec[0] = -data->mag[TB_ROLL_Y];
		mag_vec[1] = data->mag[TB_PITCH_X];
		mag_vec[2] = data->mag[TB_YAW_Z];
		break;
	default:
		fprintf(stderr,"ERROR: invalid orientation\n");
		return -1;
	}
	// tilt that vector by the roll/pitch of the IMU to align magnetic field
	// vector such that Z points vertically
	rc_quaternion_rotate_vector_array(mag_vec,tilt_q);
	// from the aligned magnetic field vector, find a yaw heading
	// check for validity and make sure the heading is positive
	lastMagYaw = newMagYaw; // save from last loop
	newMagYaw = -atan2(mag_vec[1], mag_vec[0]);
	if (newMagYaw != newMagYaw) {
		#ifdef WARNINGS
		printf("newMagYaw NAN\n");
		#endif
		return -1;
	}
	data->compass_heading_raw = newMagYaw;
	// save DMP last from time and record newDMPYaw for this time
	lastDMPYaw = newDMPYaw;
	newDMPYaw = data->dmp_TaitBryan[TB_YAW_Z];

	// the outputs from atan2 and dmp are between -PI and PI.
	// for our filters to run smoothly, we can't have them jump between -PI
	// to PI when doing a complete spin. Therefore we check for a skip and
	// increment or decrement the spin counter
	if(newMagYaw-lastMagYaw < -PI) mag_spin_counter++;
	else if (newMagYaw-lastMagYaw > PI) mag_spin_counter--;
	if(newDMPYaw-lastDMPYaw < -PI) dmp_spin_counter++;
	else if (newDMPYaw-lastDMPYaw > PI) dmp_spin_counter--;

	// if this is the first run, set up filters
	if(first_run){
		lastMagYaw = newMagYaw;
		lastDMPYaw = newDMPYaw;
		mag_spin_counter = 0;
		dmp_spin_counter = 0;
		// generate complementary filters
		float dt = 1.0/config.dmp_sample_rate;
		rc_filter_first_order_lowpass(&low_pass,dt,config.compass_time_constant);
		rc_filter_first_order_highpass(&high_pass,dt,config.compass_time_constant);
		rc_filter_prefill_inputs(&low_pass,newMagYaw);
		rc_filter_prefill_outputs(&low_pass,newMagYaw);
		rc_filter_prefill_inputs(&high_pass,newDMPYaw);
		rc_filter_prefill_outputs(&high_pass,0);
		first_run = 0;
	}

	// new Yaw is the sum of low and high pass complementary filters.
	newYaw = rc_filter_march(&low_pass,newMagYaw+(TWO_PI*mag_spin_counter)) \
			+ rc_filter_march(&high_pass,newDMPYaw+(TWO_PI*dmp_spin_counter));

	newYaw = fmod(newYaw,TWO_PI); // remove the effect of the spins
	if (newYaw > PI) newYaw -= TWO_PI; // bound between +- PI
	else if (newYaw < -PI) newYaw += TWO_PI; // bound between +- PI

	// TB angles expect a yaw between -pi to pi so slide it again and
	// store in the user-accessible fused tb angle
	data->compass_heading = newYaw;
	data->fused_TaitBryan[2] = newYaw;
	data->fused_TaitBryan[0] = data->dmp_TaitBryan[0];
	data->fused_TaitBryan[1] = data->dmp_TaitBryan[1];

	// Also generate a new quaternion from the filtered tb angles
	rc_quaternion_from_tb_array(data->fused_TaitBryan, data->fused_quat);
	return 0;
}

/*******************************************************************************
* int write_gyro_offsets_to_disk(int16_t offsets[3])
*
* Reads steady state gyro offsets from the disk and puts them in the IMU's
* gyro offset register. If no calibration file exists then make a new one.
*******************************************************************************/
int write_gyro_offets_to_disk(int16_t offsets[3])
{
	FILE *cal;
	char file_path[100];

	// construct a new file path string and open for writing
	strcpy(file_path, CONFIG_DIRECTORY);
	strcat(file_path, GYRO_CAL_FILE);
	cal = fopen(file_path, "w+");
	// if opening for writing failed, the directory may not exist yet
	if (cal == 0) {
		mkdir(CONFIG_DIRECTORY, 0777);
		cal = fopen(file_path, "w+");
		if (cal == 0){
			fprintf(stderr,"could not open config directory\n");
			fprintf(stderr,CONFIG_DIRECTORY);
			fprintf(stderr,"\n");
			return -1;
		}
	}
	// write to the file, close, and exit
	if(fprintf(cal,"%d\n%d\n%d\n", offsets[0],offsets[1],offsets[2])<0){
		printf("Failed to write gyro offsets to file\n");
		fclose(cal);
		return -1;
	}
	fclose(cal);
	return 0;
}

/*******************************************************************************
* int load_gyro_offsets()
*
* Loads steady state gyro offsets from the disk and puts them in the IMU's
* gyro offset register. If no calibration file exists then make a new one.
*******************************************************************************/
int __load_gyro_offets()
{
	FILE *cal;
	char file_path[100];
	uint8_t data[6];
	int x,y,z;

	// construct a new file path string and open for reading
	strcpy (file_path, CONFIG_DIRECTORY);
	strcat (file_path, GYRO_CAL_FILE);
	cal = fopen(file_path, "r");

	if (cal == 0) {
		// calibration file doesn't exist yet
		fprintf(stderr,"WARNING: no gyro calibration data found\n");
		fprintf(stderr,"Please run rc_calibrate_gyro\n\n");
		// use zero offsets
		x = 0;
		y = 0;
		z = 0;
	}
	else {
		// read in data
		fscanf(cal,"%d\n%d\n%d\n", &x,&y,&z);
		fclose(cal);
	}

	#ifdef DEBUG
	printf("offsets: %d %d %d\n", x, y, z);
	#endif

	// Divide by 4 to get 32.9 LSB per deg/s to conform to expected bias input
	// format. also make negative since we wish to subtract out the steady
	// state offset
	data[0] = (-x/4  >> 8) & 0xFF;
	data[1] = (-x/4)       & 0xFF;
	data[2] = (-y/4  >> 8) & 0xFF;
	data[3] = (-y/4)       & 0xFF;
	data[4] = (-z/4  >> 8) & 0xFF;
	data[5] = (-z/4)       & 0xFF;

	// Push gyro biases to hardware registers
	if(rc_i2c_write_bytes(config.i2c_bus, XG_OFFSET_H, 6, &data[0])){
		fprintf(stderr,"ERROR: failed to load gyro offsets into IMU register\n");
		return -1;
	}
	return 0;
}

/*******************************************************************************
* int rc_mpu_calibrate_gyro_routine()
*
* Initializes the IMU and samples the gyro for a short period to get steady
* state gyro offsets. These offsets are then saved to disk for later use.
*******************************************************************************/
int rc_mpu_calibrate_gyro_routine(rc_mpu_config_t conf)
{
	uint8_t c, data[6];
	int32_t gyro_sum[3] = {0, 0, 0};
	int16_t offsets[3];
	int was_last_steady = 1;

	// wipe global config with defaults to avoid problems
	config = rc_mpu_default_config();
	// configure with user's i2c bus info
	config.i2c_bus = conf.i2c_bus;
	config.i2c_addr = conf.i2c_addr;

	// make sure the bus is not currently in use by another thread
	// do not proceed to prevent interfering with that process
	if(rc_i2c_get_lock(config.i2c_bus)){
		fprintf(stderr,"i2c bus claimed by another process\n");
		fprintf(stderr,"aborting gyro calibration()\n");
		return -1;
	}

	// if it is not claimed, start the i2c bus
	if(rc_i2c_init(config.i2c_bus, config.i2c_addr)){
		fprintf(stderr,"rc_mpu_initialize_dmp failed at rc_i2c_init\n");
		return -1;
	}

	// claiming the bus does no guarantee other code will not interfere
	// with this process, but best to claim it so other code can check
	// like we did above
	rc_i2c_lock_bus(config.i2c_bus);

	// reset device, reset all registers
	if(__reset_mpu9250()<0){
		fprintf(stderr,"ERROR: failed to reset MPU9250\n");
		return -1;
	}

	// set up the IMU specifically for calibration.
	rc_i2c_write_byte(config.i2c_bus, PWR_MGMT_1, 0x01);
	rc_i2c_write_byte(config.i2c_bus, PWR_MGMT_2, 0x00);
	rc_usleep(200000);

	// // set bias registers to 0
	// // Push gyro biases to hardware registers
	// uint8_t zeros[] = {0,0,0,0,0,0};
	// if(rc_i2c_write_bytes(config.i2c_bus, XG_OFFSET_H, 6, zeros)){
		// fprintf(stderr,"ERROR: failed to load gyro offsets into IMU register\n");
		// return -1;
	// }

	rc_i2c_write_byte(config.i2c_bus, INT_ENABLE, 0x00);  // Disable all interrupts
	rc_i2c_write_byte(config.i2c_bus, FIFO_EN, 0x00);     // Disable FIFO
	rc_i2c_write_byte(config.i2c_bus, PWR_MGMT_1, 0x00);  // Turn on internal clock source
	rc_i2c_write_byte(config.i2c_bus, I2C_MST_CTRL, 0x00);// Disable I2C master
	rc_i2c_write_byte(config.i2c_bus, USER_CTRL, 0x00);   // Disable FIFO and I2C master
	rc_i2c_write_byte(config.i2c_bus, USER_CTRL, 0x0C);   // Reset FIFO and DMP
	rc_usleep(15000);

	// Configure MPU9250 gyro and accelerometer for bias calculation
	rc_i2c_write_byte(config.i2c_bus, CONFIG, 0x01);      // Set low-pass filter to 188 Hz
	rc_i2c_write_byte(config.i2c_bus, SMPLRT_DIV, 0x04);  // Set sample rate to 200hz
	// Set gyro full-scale to 250 degrees per second, maximum sensitivity
	rc_i2c_write_byte(config.i2c_bus, GYRO_CONFIG, 0x00);
	// Set accelerometer full-scale to 2 g, maximum sensitivity
	rc_i2c_write_byte(config.i2c_bus, ACCEL_CONFIG, 0x00);

COLLECT_DATA:

	if(imu_shutdown_flag){
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// Configure FIFO to capture gyro data for bias calculation
	rc_i2c_write_byte(config.i2c_bus, USER_CTRL, 0x40);   // Enable FIFO
	// Enable gyro sensors for FIFO (max size 512 bytes in MPU-9250)
	c = FIFO_GYRO_X_EN|FIFO_GYRO_Y_EN|FIFO_GYRO_Z_EN;
	rc_i2c_write_byte(config.i2c_bus, FIFO_EN, c);
	// 6 bytes per sample. 200hz. wait 0.4 seconds
	rc_usleep(400000);

	// At end of sample accumulation, turn off FIFO sensor read
	rc_i2c_write_byte(config.i2c_bus, FIFO_EN, 0x00);
	// read FIFO sample count and log number of samples
	rc_i2c_read_bytes(config.i2c_bus, FIFO_COUNTH, 2, &data[0]);
	int16_t fifo_count = ((uint16_t)data[0] << 8) | data[1];
	int samples = fifo_count/6;

	#ifdef DEBUG
	printf("calibration samples: %d\n", samples);
	#endif

	int i;
	int16_t x,y,z;
	rc_vector_t vx = rc_vector_empty();
	rc_vector_t vy = rc_vector_empty();
	rc_vector_t vz = rc_vector_empty();
	rc_vector_alloc(&vx,samples);
	rc_vector_alloc(&vy,samples);
	rc_vector_alloc(&vz,samples);
	float dev_x, dev_y, dev_z;
	gyro_sum[0] = 0;
	gyro_sum[1] = 0;
	gyro_sum[2] = 0;
	for (i=0; i<samples; i++) {
		// read data for averaging
		if(rc_i2c_read_bytes(config.i2c_bus, FIFO_R_W, 6, data)<0){
			fprintf(stderr,"ERROR: failed to read FIFO\n");
			return -1;
		}
		x = (int16_t)(((int16_t)data[0] << 8) | data[1]) ;
		y = (int16_t)(((int16_t)data[2] << 8) | data[3]) ;
		z = (int16_t)(((int16_t)data[4] << 8) | data[5]) ;
		gyro_sum[0]  += (int32_t) x;
		gyro_sum[1]  += (int32_t) y;
		gyro_sum[2]  += (int32_t) z;
		vx.d[i] = (float)x;
		vy.d[i] = (float)y;
		vz.d[i] = (float)z;
	}
	dev_x = rc_vector_std_dev(vx);
	dev_y = rc_vector_std_dev(vy);
	dev_z = rc_vector_std_dev(vz);
	rc_vector_free(&vx);
	rc_vector_free(&vy);
	rc_vector_free(&vz);

	#ifdef DEBUG
	printf("gyro sums: %d %d %d\n", gyro_sum[0], gyro_sum[1], gyro_sum[2]);
	printf("std_deviation: %6.2f %6.2f %6.2f\n", dev_x, dev_y, dev_z);
	#endif

	// try again is standard deviation is too high
	if(dev_x>GYRO_CAL_THRESH||dev_y>GYRO_CAL_THRESH||dev_z>GYRO_CAL_THRESH){
		printf("Gyro data too noisy, put me down on a solid surface!\n");
		printf("trying again\n");
		was_last_steady = 0;
		goto COLLECT_DATA;
	}
	// this skips the first steady reading after a noisy reading
	// to make sure IMU has settled after being picked up.
	if(was_last_steady == 0){
		was_last_steady = 1;
		goto COLLECT_DATA;
	}
	// average out the samples
	offsets[0] = (int16_t) (gyro_sum[0]/(int32_t)samples);
	offsets[1] = (int16_t) (gyro_sum[1]/(int32_t)samples);
	offsets[2] = (int16_t) (gyro_sum[2]/(int32_t)samples);

	// also check for values that are way out of bounds
	if(abs(offsets[0])>GYRO_OFFSET_THRESH || abs(offsets[1])>GYRO_OFFSET_THRESH \
										|| abs(offsets[2])>GYRO_OFFSET_THRESH){
		printf("Gyro data out of bounds, put me down on a solid surface!\n");
		printf("trying again\n");
		goto COLLECT_DATA;
	}
	// done with I2C for now
	rc_i2c_unlock_bus(config.i2c_bus);
	#ifdef DEBUG
	printf("offsets: %d %d %d\n", offsets[0], offsets[1], offsets[2]);
	#endif
	// write to disk
	if(write_gyro_offets_to_disk(offsets)<0){
		fprintf(stderr,"ERROR in rc_mpu_calibrate_gyro_routine, failed to write to disk\n");
		return -1;
	}
	return 0;
}

/*******************************************************************************
* unsigned short inv_row_2_scale(signed char row[])
*
* takes a single row on a rotation matrix and returns the associated scalar
* for use by inv_orientation_matrix_to_scalar.
*******************************************************************************/
unsigned short inv_row_2_scale(signed char row[])
{
	unsigned short b;

	if (row[0] > 0)
		b = 0;
	else if (row[0] < 0)
		b = 4;
	else if (row[1] > 0)
		b = 1;
	else if (row[1] < 0)
		b = 5;
	else if (row[2] > 0)
		b = 2;
	else if (row[2] < 0)
		b = 6;
	else
		b = 7;      // error
	return b;
}

/*******************************************************************************
* unsigned short inv_orientation_matrix_to_scalar(signed char mtx[])
*
* This take in a rotation matrix and returns the corresponding 16 bit short
* which is sent to the DMP to set the orientation. This function is actually
* not used in normal operation and only served to retrieve the orientation
* scalars once to populate the rc_imu_orientation_t enum during development.
*******************************************************************************/
unsigned short inv_orientation_matrix_to_scalar(signed char mtx[])
{
	unsigned short scalar;

	scalar = inv_row_2_scale(mtx);
	scalar |= inv_row_2_scale(mtx + 3) << 3;
	scalar |= inv_row_2_scale(mtx + 6) << 6;
	return scalar;
}

/*******************************************************************************
* void print_orientation_info()
*
* this function purely serves to print out orientation values and rotation
* matrices which form the rc_imu_orientation_t enum. This is not called inside
* this C file and is not exposed to the user.
*******************************************************************************/
void print_orientation_info()
{
	printf("\n");
	//char mtx[9];
	unsigned short orient;

	// Z-UP (identity matrix)
	signed char zup[] = {1,0,0, 0,1,0, 0,0,1};
	orient = inv_orientation_matrix_to_scalar(zup);
	printf("Z-UP: %d\n", orient);

	// Z-down
	signed char zdown[] = {-1,0,0, 0,1,0, 0,0,-1};
	orient = inv_orientation_matrix_to_scalar(zdown);
	printf("Z-down: %d\n", orient);

	// X-up
	signed char xup[] = {0,0,-1, 0,1,0, 1,0,0};
	orient = inv_orientation_matrix_to_scalar(xup);
	printf("x-up: %d\n", orient);

	// X-down
	signed char xdown[] = {0,0,1, 0,1,0, -1,0,0};
	orient = inv_orientation_matrix_to_scalar(xdown);
	printf("x-down: %d\n", orient);

	// Y-up
	signed char yup[] = {1,0,0, 0,0,-1, 0,1,0};
	orient = inv_orientation_matrix_to_scalar(yup);
	printf("y-up: %d\n", orient);

	// Y-down
	signed char ydown[] = {1,0,0, 0,0,1, 0,-1,0};
	orient = inv_orientation_matrix_to_scalar(ydown);
	printf("y-down: %d\n", orient);

	// X-forward
	signed char xforward[] = {0,-1,0, 1,0,0, 0,0,1};
	orient = inv_orientation_matrix_to_scalar(xforward);
	printf("x-forward: %d\n", orient);

	// X-back
	signed char xback[] = {0,1,0, -1,0,0, 0,0,1};
	orient = inv_orientation_matrix_to_scalar(xback);
	printf("yx-back: %d\n", orient);
}


/*******************************************************************************
* uint64_t rc_mpu_nanos_since_last_dmp_interrupt()
*
* Immediately after the IMU triggers an interrupt saying new data is ready,
* a timestamp is logged in microseconds. The user's dmp_callback_function
* will be called after all data has been read in through the I2C bus and
* the user's rc_mpu_data_t struct has been populated. If the user wishes to see
* how long it has been since that interrupt was received they may use this
* function.
*******************************************************************************/
int64_t rc_mpu_nanos_since_last_dmp_interrupt()
{
	if(last_interrupt_timestamp_nanos==0) return -1;
	return rc_nanos_since_epoch() - last_interrupt_timestamp_nanos;
}

int64_t rc_mpu_nanos_since_last_tap()
{
	if(last_tap_timestamp_nanos==0) return -1;
	return rc_nanos_since_epoch() - last_tap_timestamp_nanos;
}

/*******************************************************************************
* int __write_mag_cal_to_disk(float offsets[3], float scale[3])
*
* Reads steady state gyro offsets from the disk and puts them in the IMU's
* gyro offset register. If no calibration file exists then make a new one.
*******************************************************************************/
int __write_mag_cal_to_disk(float offsets[3], float scale[3])
{
	FILE *cal;
	char file_path[100];
	int ret;

	// construct a new file path string and open for writing
	strcpy(file_path, CONFIG_DIRECTORY);
	strcat(file_path, MAG_CAL_FILE);
	cal = fopen(file_path, "w+");
	// if opening for writing failed, the directory may not exist yet
	if (cal == 0) {
		mkdir(CONFIG_DIRECTORY, 0777);
		cal = fopen(file_path, "w+");
		if (cal == 0){
			fprintf(stderr,"could not open config directory\n");
			fprintf(stderr, CONFIG_DIRECTORY);
			fprintf(stderr,"\n");
			return -1;
		}
	}

	// write to the file, close, and exit
	ret = fprintf(cal,"%f\n%f\n%f\n%f\n%f\n%f\n",	offsets[0],\
							offsets[1],\
							offsets[2],\
							scale[0],\
							scale[1],\
							scale[2]);
	if(ret<0){
		fprintf(stderr,"Failed to write mag calibration to file\n");
		fclose(cal);
		return -1;
	}
	fclose(cal);
	return 0;
}

/*******************************************************************************
* int __load_mag_calibration()
*
* Loads steady state magnetometer offsets and scale from the disk into global
* variables for correction later by read_magnetometer and FIFO read functions
*******************************************************************************/
int __load_mag_calibration()
{
	FILE *cal;
	char file_path[100];
	float x,y,z,sx,sy,sz;

	// construct a new file path string and open for reading
	strcpy (file_path, CONFIG_DIRECTORY);
	strcat (file_path, MAG_CAL_FILE);
	cal = fopen(file_path, "r");

	if (cal == 0) {
		// calibration file doesn't exist yet
		fprintf(stderr,"WARNING: no magnetometer calibration data found\n");
		fprintf(stderr,"Please run rc_calibrate_mag\n\n");
		mag_offsets[0]=0.0;
		mag_offsets[1]=0.0;
		mag_offsets[2]=0.0;
		mag_scales[0]=1.0;
		mag_scales[1]=1.0;
		mag_scales[2]=1.0;
		return -1;
	}
	// read in data
	fscanf(cal,"%f\n%f\n%f\n%f\n%f\n%f\n", &x,&y,&z,&sx,&sy,&sz);

	#ifdef DEBUG
	printf("magcal: %f %f %f %f %f %f\n", x,y,z,sx,sy,sz);
	#endif

	// write to global variables fo use by rc_mpu_read_mag
	mag_offsets[0]=x;
	mag_offsets[1]=y;
	mag_offsets[2]=z;
	mag_scales[0]=sx;
	mag_scales[1]=sy;
	mag_scales[2]=sz;

	fclose(cal);
	return 0;
}

/*******************************************************************************
* int rc_mpu_calibrate_mag_routine()
*
* Initializes the IMU and samples the magnetometer until sufficient samples
* have been collected from each octant. From there, fit an ellipse to the data
* and save the correct offsets and scales to the disk which will later be
* applied to correct the uncalibrated magnetometer data to map calibrated
* field vectors to a sphere.
*******************************************************************************/
int rc_mpu_calibrate_mag_routine(rc_mpu_config_t conf)
{
	const int samples = 200;
	const int sample_rate_hz = 15;
	int i;
	float new_scale[3];
	rc_matrix_t A = rc_matrix_empty();
	rc_vector_t center = rc_vector_empty();
	rc_vector_t lengths = rc_vector_empty();
	rc_mpu_data_t imu_data; // to collect magnetometer data
	// wipe it with defaults to avoid problems
	config = rc_mpu_default_config();
	// configure with user's i2c bus info
	config.enable_magnetometer = 1;
	config.i2c_bus = conf.i2c_bus;
	config.i2c_addr = conf.i2c_addr;

	// make sure the bus is not currently in use by another thread
	// do not proceed to prevent interfering with that process
	if(rc_i2c_get_lock(config.i2c_bus)){
		fprintf(stderr,"i2c bus claimed by another process\n");
		fprintf(stderr,"aborting magnetometer calibration()\n");
		return -1;
	}

	// if it is not claimed, start the i2c bus
	if(rc_i2c_init(config.i2c_bus, config.i2c_addr)){
		fprintf(stderr,"ERROR rc_mpu_calibrate_mag_routine failed at rc_i2c_init\n");
		return -1;
	}

	// claiming the bus does no guarantee other code will not interfere
	// with this process, but best to claim it so other code can check
	// like we did above
	rc_i2c_lock_bus(config.i2c_bus);

	// reset device, reset all registers
	if(__reset_mpu9250()<0){
		fprintf(stderr,"ERROR: failed to reset MPU9250\n");
		return -1;
	}
	//check the who am i register to make sure the chip is alive
	if(__check_who_am_i()){
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}
	if(__init_magnetometer()){
		fprintf(stderr,"ERROR: failed to initialize_magnetometer\n");
		rc_i2c_unlock_bus(config.i2c_bus);
		return -1;
	}

	// set local calibration to initial values and prepare variables
	mag_offsets[0] = 0.0;
	mag_offsets[1] = 0.0;
	mag_offsets[2] = 0.0;
	mag_scales[0]  = 1.0;
	mag_scales[1]  = 1.0;
	mag_scales[2]  = 1.0;
	rc_matrix_alloc(&A,samples,3);
	i = 0;

	// sample data
	while(i<samples && imu_shutdown_flag==0){
		if(rc_mpu_read_mag(&imu_data)<0){
			fprintf(stderr,"ERROR: failed to read magnetometer\n");
			break;
		}
		// make sure the data is non-zero
		if(imu_data.mag[0]==0 && imu_data.mag[1]==0 && imu_data.mag[2]==0){
			fprintf(stderr,"ERROR: retreived all zeros from magnetometer\n");
			break;
		}
		// save data to matrix for ellipse fitting
		A.d[i][0] = imu_data.mag[0];
		A.d[i][1] = imu_data.mag[1];
		A.d[i][2] = imu_data.mag[2];
		i++;

		// print "keep going" every 4 seconds
		if(i%(sample_rate_hz*4) == sample_rate_hz*2){
			printf("keep spinning\n");
		}
		// print "you're doing great" every 4 seconds
		if(i%(sample_rate_hz*4) == 0){
			printf("you're doing great\n");
		}

		rc_usleep(1000000/sample_rate_hz);
	}
	// done with I2C for now
	rc_mpu_power_off();
	rc_i2c_unlock_bus(config.i2c_bus);

	printf("\n\nOkay Stop!\n");
	printf("Calculating calibration constants.....\n");
	fflush(stdout);

	// if data collection loop exited without getting enough data, warn the
	// user and return -1, otherwise keep going normally
	if(i<samples){
		printf("exiting rc_mpu_calibrate_mag_routine without saving new data\n");
		return -1;
	}
	// make empty vectors for ellipsoid fitting to populate
	if(rc_algebra_fit_ellipsoid(A,&center,&lengths)<0){
		fprintf(stderr,"failed to fit ellipsoid to magnetometer data\n");
		rc_matrix_free(&A);
		return -1;
	}
	// empty memory, we are done with A
	rc_matrix_free(&A);
	// do some sanity checks to make sure data is reasonable
	if(fabs(center.d[0])>200 || fabs(center.d[1])>200 || \
							fabs(center.d[2])>200){
		fprintf(stderr,"ERROR: center of fitted ellipsoid out of bounds\n");
		rc_vector_free(&center);
		rc_vector_free(&lengths);
		return -1;
	}
	if( lengths.d[0]>200 || lengths.d[0]<5 || \
		lengths.d[1]>200 || lengths.d[1]<5 || \
		lengths.d[2]>200 || lengths.d[2]<5){
		fprintf(stderr,"WARNING: length of fitted ellipsoid out of bounds\n");
		//rc_vector_free(&center);
		//rc_vector_free(&lengths);
		//return -1;
	}
	// all seems well, calculate scaling factors to map ellipse lengths to
	// a sphere of radius 70uT, this scale will later be multiplied by the
	// factory corrected data
	new_scale[0] = 70.0f/lengths.d[0];
	new_scale[1] = 70.0f/lengths.d[1];
	new_scale[2] = 70.0f/lengths.d[2];
	// print results
	printf("\n");
	printf("Offsets X: %7.3f Y: %7.3f Z: %7.3f\n",	center.d[0],\
							center.d[1],\
							center.d[2]);
	printf("Scales  X: %7.3f Y: %7.3f Z: %7.3f\n",	new_scale[0],\
							new_scale[1],\
							new_scale[2]);
	// write to disk
	if(__write_mag_cal_to_disk(center.d,new_scale)<0){
		rc_vector_free(&center);
		rc_vector_free(&lengths);
		return -1;
	}
	rc_vector_free(&center);
	rc_vector_free(&lengths);
	return 0;
}

/*******************************************************************************
* int rc_mpu_is_gyro_calibrated()
*
* return 1 is a gyro calibration file exists, otherwise 0
*******************************************************************************/
int rc_mpu_is_gyro_calibrated()
{
	char file_path[100];
	strcpy (file_path, CONFIG_DIRECTORY);
	strcat (file_path, GYRO_CAL_FILE);
	if(!access(file_path, F_OK)) return 1;
	else return 0;
}

/*******************************************************************************
* int rc_mpu_is_mag_calibrated()
*
* return 1 is a magnetometer calibration file exists, otherwise 0
*******************************************************************************/
int rc_mpu_is_mag_calibrated()
{
	char file_path[100];
	strcpy (file_path, CONFIG_DIRECTORY);
	strcat (file_path, MAG_CAL_FILE);
	if(!access(file_path, F_OK)) return 1;
	else return 0;
}


int rc_mpu_block_until_dmp_data()
{
	if(imu_shutdown_flag!=0){
		fprintf(stderr,"ERROR: call to rc_mpu_block_until_dmp_data after shutting down mpu\n");
		return -1;
	}
	if(!thread_running_flag){
		fprintf(stderr,"ERROR: call to rc_mpu_block_until_dmp_data when DMP handler not running\n");
		return -1;
	}
	// wait for condition signal which unlocks mutex
	pthread_mutex_lock(&read_mutex);
	pthread_cond_wait(&read_condition, &read_mutex);
	pthread_mutex_unlock(&read_mutex);
	// check if condition was broadcast due to shutdown
	if(imu_shutdown_flag) return 1;
	// otherwise return 0 on actual button press
	return 0;
}

int rc_mpu_block_until_tap()
{
	if(imu_shutdown_flag!=0){
		fprintf(stderr,"ERROR: call to rc_mpu_block_until_tap after shutting down mpu\n");
		return -1;
	}
	if(!thread_running_flag){
		fprintf(stderr,"ERROR: call to rc_mpu_block_until_tap when DMP handler not running\n");
		return -1;
	}
	// wait for condition signal which unlocks mutex
	pthread_mutex_lock(&tap_mutex);
	pthread_cond_wait(&tap_condition, &tap_mutex);
	pthread_mutex_unlock(&tap_mutex);
	// check if condition was broadcast due to shutdown
	if(imu_shutdown_flag) return 1;
	// otherwise return 0 on actual button press
	return 0;
}


// Phew, that was a lot of code....