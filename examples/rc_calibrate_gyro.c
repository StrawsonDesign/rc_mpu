/*******************************************************************************
* rc_calibrate_gyro.c
* James Strawson - 2016
*
* This program exists as an interface to the rc_mpu_calibrate_gyro_routine which
* manages collecting gyroscope data for averaging to find the steady state
* offsets.
*******************************************************************************/

#include <stdio.h>
#include <rc/mpu.h>

int main(){

	printf("\nThis program will generate a new gyro calibration file\n");
	printf("keep your beaglebone very still for this procedure.\n");
	printf("Press any key to continue\n");
	getchar();
	printf("Starting calibration routine\n");

	rc_mpu_config_t config = rc_mpu_default_config();
	// uncomment these if you wish to change to bus/address
	// config.i2c_bus=your_bus;
	// config.i2c_addr=your_address;

	if(rc_mpu_calibrate_gyro_routine(config)<0){
		printf("Failed to complete gyro calibration\n");
		return -1;
	}

	printf("\ngyro calibration file written\n");
	printf("run rc_test_mpu to check performance\n");

	return 0;
}
