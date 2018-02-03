/*******************************************************************************
* rc_calibrate_mag.c
*
* James Strawson - 2016
* This function serves as a command line interface for the calibrate_mag
* routine. If the routine is successful, a new magnetometer calibration file
* will be saved which is loaded automatically the next time the IMU is used.
*******************************************************************************/

#include <stdio.h>
#include <rc/mpu.h>
#include <rc/time.h>

int main(){

	printf("\n");
	printf("This will sample the magnetometer for the next 15 seconds\n");
	printf("Rotate the cape around in the air through as many orientations\n");
	printf("as possible to collect sufficient data for calibration\n");
	printf("Press any key to continue\n");
	getchar();


	printf("spin spin spin!!!\n\n");
	// wait for the user to actually start
	rc_usleep(2000000);

	if(rc_mpu_calibrate_mag_routine()<0){
		printf("Failed to complete magnetometer calibration\n");
		return -1;
	}

	printf("\nmagnetometer calibration file written\n");
	printf("run rc_test_mpu to check performance\n");
	return 0;
}
