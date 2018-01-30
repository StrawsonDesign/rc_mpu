/**
 * @file i2c.c
 *
 * @author James Strawson
 */

#include <stdint.h> // for uint8_t types etc
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h> //for IOCTL defs

#include <rc/io/i2c.h>

// preposessor macros
#define unlikely(x)	__builtin_expect (!!(x), 0)
#define likely(x)	__builtin_expect (!!(x), 1)


#define MAX_I2C_LENGTH 128

/*******************************************************************************
* struct rc_i2c_t
* contains the current state of a bus.
* you don't need to create your own instance of this,
* one for each bus is allocated here
*******************************************************************************/
typedef struct rc_i2c_t {
	/* data */
	uint8_t devAddr;
	int bus;
	int file;
	int initialized;
	int lock;
} rc_i2c_t;

static rc_i2c_t i2c[I2C_MAX_BUS+1];


// local function
int __check_bus_range(int bus){
	if(unlikely(bus<0 || bus>I2C_MAX_BUS)){
		fprintf(stderr,"ERROR: i2c bus must be between 0 & %d\n", I2C_MAX_BUS);
		return -1;
	}
	return 0;
}


int rc_i2c_init(int bus, uint8_t devAddr)
{
	if(unlikely(__check_bus_range(bus))) return -1;

	// claim the bus during this operation
	int old_lock = i2c[bus].lock;
	i2c[bus].lock = 1;

	// start filling in the i2c state struct
	i2c[bus].file = 0;
	i2c[bus].devAddr = devAddr;
	i2c[bus].bus = bus;
	i2c[bus].initialized = 1;

	// open file descriptor
	char str[16];
	sprintf(str,"/dev/i2c-%d",bus);
	i2c[bus].file = open(str, O_RDWR);
	if(i2c[bus].file==-1){
		printf("failed to open /dev/i2c\n");
		return -1;
	}
	#ifdef DEBUG
	printf("calling ioctl slave address change\n");
	#endif
	if(ioctl(i2c[bus].file, I2C_SLAVE, devAddr) < 0){
		printf("ioctl slave address change failed\n");
		return -1;
	}
	i2c[bus].devAddr = devAddr;
	// return the lock state to previous state.
	i2c[bus].lock = old_lock;

	#ifdef DEBUG
	printf("successfully initialized rc_i2c_%d\n", bus);
	#endif
	return 0;
}

/*******************************************************************************
* rc_i2c_set_device_address
*******************************************************************************/
int rc_i2c_set_device_address(int bus, uint8_t devAddr)
{
	if(unlikely(__check_bus_range(bus))) return -1;
	// if the device address is already correct, just return
	if(i2c[bus].devAddr == devAddr){
		return 0;
	}
	// if not, change it with ioctl
	#ifdef DEBUG
	printf("calling ioctl slave address change\n");
	#endif
	if(ioctl(i2c[bus].file, I2C_SLAVE, devAddr) < 0){
		printf("ioctl slave address change failed\n");
		return -1;
	}
	i2c[bus].devAddr = devAddr;
	return 0;
}

/*******************************************************************************
* rc_i2c_close
*******************************************************************************/
int rc_i2c_close(int bus)
{
	if(unlikely(__check_bus_range(bus))) return -1;
	i2c[bus].devAddr = 0;
	if(close(i2c[bus].file) < 0) return -1;
	i2c[bus].initialized = 0;
	return 0;
}

/*******************************************************************************
* rc_i2c_read_bytes
*******************************************************************************/
int rc_i2c_read_bytes(int bus, uint8_t regAddr, uint8_t length, uint8_t *data)
{
	int ret;
	// Boundary checks
	if(unlikely(__check_bus_range(bus))) return -1;
	if(length > MAX_I2C_LENGTH){
		printf("rc_i2c_read_byte data length is enforced as MAX_I2C_LENGTH!\n");
	}
	// claim the bus during this operation
	int old_lock = i2c[bus].lock;
	i2c[bus].lock = 1;

	#ifdef DEBUG
	printf("i2c devAddr:0x%x  ", i2c[bus].devAddr);
	printf("reading %d bytes from 0x%x\n", length, regAddr);
	#endif

	// write register to device
	ret = write(i2c[bus].file, &regAddr, 1);
	if(ret!=1){
		printf("write to i2c bus failed\n");
		return -1;
	}

	// then read the response
	//usleep(300);
	ret = read(i2c[bus].file, data, length);

	// return the lock state to previous state.
	i2c[bus].lock = old_lock;
	return ret;
}

/*******************************************************************************
* rc_i2c_read_byte
*******************************************************************************/
int rc_i2c_read_byte(int bus, uint8_t regAddr, uint8_t *data) {
	return rc_i2c_read_bytes(bus, regAddr, 1, data);
}

/*******************************************************************************
* rc_i2c_read_words
*******************************************************************************/
int rc_i2c_read_words(int bus, uint8_t regAddr, uint8_t length, uint16_t *data)
{
	int ret,i;
	char buf[MAX_I2C_LENGTH];

	// Boundary checks
	if(unlikely(__check_bus_range(bus))) return -1;
	if(length>(MAX_I2C_LENGTH/2)){
		printf("rc_i2c_read_words length must be less than MAX_I2C_LENGTH/2\n");
		return -1;
	}
	// claim the bus during this operation
	int old_lock = i2c[bus].lock;
	i2c[bus].lock = 1;

	#ifdef DEBUG
	printf("i2c devAddr:0x%x  ", i2c[bus].devAddr);
	printf("reading %d words from 0x%x\n", length, regAddr);
	#endif

	// write first
	ret = write(i2c[bus].file, &regAddr, 1);
	if(ret!=1){
		printf("write to i2c bus failed\n");
		return -1;
	}

	// then read the response
	ret = read(i2c[bus].file, buf, length*2);
	if(ret!=(length*2)){
		printf("i2c device returned %d bytes\n",ret);
		printf("expected %d bytes instead\n",length);
		return -1;
	}

	// form words from bytes and put into user's data array
	for(i=0;i<length;i++){
		data[i] = (((uint16_t)buf[0])<<8 | buf[1]);
	}

	// return the lock state to previous state.
	i2c[bus].lock = old_lock;

	return 0;
}

/*******************************************************************************
* rc_i2c_read_word
*******************************************************************************/
int rc_i2c_read_word(int bus, uint8_t regAddr, uint16_t *data) {
	return rc_i2c_read_words(bus, regAddr, 1, data);
}

/*******************************************************************************
* rc_i2c_read_bit
*******************************************************************************/
int rc_i2c_read_bit(int bus, uint8_t regAddr, uint8_t bitNum, uint8_t *data)
{
	uint8_t b;
	uint8_t count = rc_i2c_read_byte(bus, regAddr, &b);
	*data = b & (1 << bitNum);
	return count;
}

/*******************************************************************************
* rc_i2c_write_bytes
*******************************************************************************/
int rc_i2c_write_bytes(int bus, uint8_t regAddr, uint8_t length, uint8_t* data)
{
	int i,ret;
	uint8_t writeData[length+1];

	if(unlikely(__check_bus_range(bus))) return -1;

	// claim the bus during this operation
	int old_lock = i2c[bus].lock;
	i2c[bus].lock = 1;

	// assemble array to send, starting with the register address
	writeData[0] = regAddr;
	for(i=0; i<length; i++){
		writeData[i+1] = data[i];
	}

	#ifdef DEBUG
	printf("i2c devAddr:0x%x  ", i2c[bus].devAddr);
	printf("writing %d bytes to 0x%x\n", length, regAddr);
	printf("0x");
	for (i=0; i<length; i++){
		printf("%x\t", data[i]);
	}
	printf("\n");
	#endif

	// send the bytes
	ret = write(i2c[bus].file, writeData, length+1);
	// write should have returned the correct # bytes written
	if( ret!=(length+1)){
		printf("rc_i2c_write failed\n");
		return -1;
	}
	// return the lock state to previous state.
	i2c[bus].lock = old_lock;
	return 0;
}

/*******************************************************************************
* rc_i2c_write_byte
*******************************************************************************/
int rc_i2c_write_byte(int bus, uint8_t regAddr, uint8_t data)
{
	return rc_i2c_write_bytes(bus, regAddr, 1, &data);
}


/*******************************************************************************
* rc_i2c_write_words
*******************************************************************************/
int rc_i2c_write_words(int bus, uint8_t regAddr, uint8_t length, uint16_t* data)
{
	int i,ret;
	uint8_t writeData[(length*2)+1];

	// claim the bus during this operation
	int old_lock = i2c[bus].lock;
	i2c[bus].lock = 1;

	// assemble bytes to send
	writeData[0] = regAddr;
	for (i=0; i<length; i++){
		writeData[(i*2)+1] = (uint8_t)(data[i] >> 8);
		writeData[(i*2)+2] = (uint8_t)(data[i]);
	}

#ifdef DEBUG
	printf("i2c devAddr:0x%x  ", i2c[bus].devAddr);
	printf("writing %d bytes to 0x%x\n", length, regAddr);
	printf("0x");
	for (i=0; i<(length*2)+1; i++){
		printf("%x\t", writeData[i]);
	}
	printf("\n");
#endif

	ret = write(i2c[bus].file, writeData, (length*2)+1);
	if(ret!=(length*2)+1){
		printf("i2c write failed\n");
		return -1;
	}
	// return the lock state to previous state.
	i2c[bus].lock = old_lock;
	return 0;
}

/******************************************************************************
* rc_i2c_write_word
*******************************************************************************/
int rc_i2c_write_word(int bus, uint8_t regAddr, uint16_t data)
{
	return rc_i2c_write_words(bus, regAddr, 1, &data);
}

/*******************************************************************************
* rc_i2c_write_bit
*******************************************************************************/
int rc_i2c_write_bit(int bus, uint8_t regAddr, uint8_t bitNum, uint8_t data)
{
	uint8_t b;
	// read back the current state of the register
	rc_i2c_read_byte(bus, regAddr, &b);
	// modify that bit in the register
	b = (data != 0) ? (b | (1 << bitNum)) : (b & ~(1 << bitNum));
	// write it back
	return rc_i2c_write_byte(bus, regAddr, b);
}

/*******************************************************************************
* rc_i2c_send_bytes
*******************************************************************************/
int rc_i2c_send_bytes(int bus, uint8_t length, uint8_t* data)
{
	int ret=0;

	if(unlikely(__check_bus_range(bus))) return -1;

	// claim the bus during this operation
	int old_lock= i2c[bus].lock;
	i2c[bus].lock = 1;

#ifdef DEBUG
	printf("i2c devAddr:0x%x  ", i2c[bus].devAddr);
	printf("sending %d bytes\n", length);
#endif

	// send the bytes
	ret = write(i2c[bus].file, data, length);
	// write should have returned the correct # bytes written
	if(ret!=length){
		printf("rc_i2c_send failed\n");
		return -1;
	}

#ifdef DEBUG
	int i=0;
	printf("0x");
	for (i=0; i<length; i++){
		printf("%x\t", data[i]);
	}
	printf("\n");
#endif

	// return the lock state to previous state.
	i2c[bus].lock = old_lock;

	return 0;
}

/*******************************************************************************
* rc_i2c_send_byte
*******************************************************************************/
int rc_i2c_send_byte(int bus, uint8_t data)
{
	return rc_i2c_send_bytes(bus,1,&data);
}



int rc_i2c_lock_bus(int bus)
{
	if(unlikely(__check_bus_range(bus))) return -1;
	int ret=i2c[bus].lock;
	i2c[bus].lock=1;
	return ret;
}


int rc_i2c_unlock_bus(int bus)
{
	if(unlikely(__check_bus_range(bus))) return -1;
	int ret=i2c[bus].lock;
	i2c[bus].lock=0;
	return ret;
}


int rc_i2c_get_lock(int bus)
{
	if(unlikely(__check_bus_range(bus))) return -1;
	return i2c[bus].lock;
}