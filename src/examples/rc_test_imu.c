/*******************************************************************************
* rc_test_imu.c
*
* This serves as an example of how to read the IMU with direct reads to the
* sensor registers. To use the DMP or interrupt-driven timing see test_dmp.c
*******************************************************************************/

#include <stdio.h>
#include <getopt.h>
#include <rc/mpu9250.h>
#include <rc/flow.h>
#include <rc/time.h>

#define DEG_TO_RAD	0.0174532925199
#define MS2_TO_G	0.10197162129

// possible modes, user selected with command line arguments
typedef enum g_mode_t{
	G_MODE_RAD,
	G_MODE_DEG,
	G_MODE_RAW
} g_mode_t;

typedef enum a_mode_t{
	A_MODE_MS2,
	A_MODE_G,
	A_MODE_RAW
} a_mode_t;

int enable_magnetometer = 0;
int enable_thermometer = 0;
int enable_warnings=0;

// printed if some invalid argument was given
void print_usage(){
	printf("\n");
	printf("-a	print raw adc values instead of radians\n");
	printf("-r	print gyro in radians/s instead of degrees/s\n");
	printf("-g	print acceleration in G instead of m/s^2\n");
	printf("-m	print magnetometer data as well as accel/gyro\n");
	printf("-t	print thermometer data as well as accel/gyro\n");
	printf("-w	print i2c warnings\n");
	printf("-h	print this help message\n");
	printf("\n");
}

int main(int argc, char *argv[]){
	rc_imu_data_t data; //struct to hold new data
	int c;
	g_mode_t g_mode = G_MODE_DEG; // gyro default to degree mode.
	a_mode_t a_mode = A_MODE_MS2; // accel default to m/s^2

	// parse arguments
	opterr = 0;
	while ((c = getopt(argc, argv, "argmtwh")) != -1){
		switch (c){
		case 'a':
			g_mode = G_MODE_RAW;
			a_mode = A_MODE_RAW;
			printf("\nRaw values are from 16-bit ADC\n");
			break;
		case 'r':
			g_mode = G_MODE_RAD;
			break;
		case 'g':
			a_mode = A_MODE_G;
			break;
		case 'm':
			enable_magnetometer = 1;
			break;
		case 't':
			enable_thermometer = 1;
			break;
		case 'w':
			enable_warnings = 1;
			break;
		case 'h':
			print_usage();
			return 0;
		default:
			print_usage();
			return -1;
		}
	}

	// enable signal handler for ctrl-c
	rc_enable_signal_handler();
	rc_set_state(UNINITIALIZED);

	// use defaults for now, except also enable magnetometer.
	rc_imu_config_t conf = rc_default_imu_config();
	conf.enable_magnetometer = enable_magnetometer;
	conf.show_warnings = enable_warnings;

	if(rc_initialize_imu(&data, conf)){
		fprintf(stderr,"rc_initialize_imu_failed\n");
		return -1;
	}

	// print the header
	printf("\ntry 'rc_test_imu -h' to see other options\n\n");
	switch(a_mode){
	case A_MODE_MS2:
		printf("   Accel XYZ(m/s^2)  |");
		break;
	case A_MODE_G:
		printf("     Accel XYZ(G)    |");
		break;
	case A_MODE_RAW:
		printf("  Accel XYZ(raw ADC) |");
		break;
	default:
		printf("ERROR: invalid mode\n");
		return -1;
	}
	switch(g_mode){
	case G_MODE_RAD:
		printf("   Gyro XYZ (rad/s)  |");
		break;
	case G_MODE_DEG:
		printf("   Gyro XYZ (deg/s)  |");
		break;
	case G_MODE_RAW:
		printf("  Gyro XYZ (raw ADC) |");
		break;
	default:
		printf("ERROR: invalid mode\n");
		return -1;
	}

	if(enable_magnetometer)	printf("  Mag Field XYZ(uT)  |");
	if(enable_thermometer) printf(" Temp (C)");
	printf("\n");

	//now just wait, print_data will run
	rc_set_state(RUNNING);
	while (rc_get_state() != EXITING) {
		printf("\r");

		// read sensor data
		if(rc_read_accel_data(&data)<0){
			printf("read accel data failed\n");
		}
		if(rc_read_gyro_data(&data)<0){
			printf("read gyro data failed\n");
		}
		if(enable_magnetometer && rc_read_mag_data(&data)){
			printf("read mag data failed\n");
		}
		if(enable_thermometer && rc_read_imu_temp(&data)){
			printf("read imu thermometer failed\n");
		}


		switch(a_mode){
		case A_MODE_MS2:
			printf("%6.2f %6.2f %6.2f |",	data.accel[0],\
							data.accel[1],\
							data.accel[2]);
			break;
		case A_MODE_G:
			printf("%6.2f %6.2f %6.2f |",	data.accel[0]*MS2_TO_G,\
							data.accel[1]*MS2_TO_G,\
							data.accel[2]*MS2_TO_G);
			break;
		case A_MODE_RAW:
			printf("%6d %6d %6d |",		data.raw_accel[0],\
							data.raw_accel[1],\
							data.raw_accel[2]);
		}

		switch(g_mode){
		case G_MODE_RAD:
			printf("%6.1f %6.1f %6.1f |",	data.gyro[0]*DEG_TO_RAD,\
							data.gyro[1]*DEG_TO_RAD,\
							data.gyro[2]*DEG_TO_RAD);
			break;
		case G_MODE_DEG:
			printf("%6.1f %6.1f %6.1f |",	data.gyro[0],\
							data.gyro[1],\
							data.gyro[2]);
			break;
		case G_MODE_RAW:
			printf("%6d %6d %6d |",		data.raw_gyro[0],\
							data.raw_gyro[1],\
							data.raw_gyro[2]);
		}

		// read magnetometer
		if(enable_magnetometer){
			printf("%6.1f %6.1f %6.1f |",	data.mag[0],\
							data.mag[1],\
							data.mag[2]);
		}
		// read temperature
		if(enable_thermometer){
			printf(" %4.1f    ", data.temp);
		}

		fflush(stdout);
		rc_usleep(100000);
	}

	rc_power_off_imu();
	return 0;
}

