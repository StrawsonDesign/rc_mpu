rc_mpu README                         {#mainpage}
=============


Source code homepage: <https://github.com/StrawsonDesign/rc_mpu>

Full documentation: <http://strawsondesign.com/docs/rc_mpu/index.html>

This is a self-contained Linux userspace C interface for the Invensense MPU series of IMUs and should work with the MPU6050, MPU6500, MPU9150, and MPU9250. I only have an MPU9250 to test on right now so please

It shares the code with, and is primarily developed on, the BeagleBone Black Robotics Cape and Beaglebone Blue. However it has also been tested on the Raspberry Pi and Nvidia TX1. The API and source files here are borrowed from the Robotics Cape library.project.

Building
--------

You will likely need to change the I2C bus in the example programs to match your connection. The examples default to using I2C bus 2 to match the Robotics Cape and Beaglebone Blue.


	~$ git clone https://github.com/StrawsonDesign/rc_mpu.git
	~$ cd rc_mpu
	~/rc_mpu$ make

Example Program Execution
-------------------------


The compiled example programs will be placed in the bin directory and can be executed from there. Each takes the '-h' argument to print usage.

	~/rc_mpu$ ./bin/rc_test_mpu

	try 'rc_test_imu -h' to see other options

	   Accel XYZ(m/s^2)  |   Gyro XYZ (deg/s)  |
	  0.51   0.25   9.16 |   0.2    0.1   -0.1 |



Example programs listed here: <http://strawsondesign.com/docs/rc_mpu/examples.html>


