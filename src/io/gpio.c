/**
 * @file rc_gpio.c
 *
 * @author     James Strawson
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h> // for O_WRONLY
// #include <poll.h>

#include <rc/gpio.h>

// preposessor macros
#define unlikely(x)	__builtin_expect (!!(x), 0)
#define likely(x)	__builtin_expect (!!(x), 1)

#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define MAX_BUF 64
#define NUM_PINS 128

// normal file handle variables
static int value_fd[NUM_PINS];

// local function declaration
static int __init_pin_fd(int pin)
{
	// sanity check
	if(unlikely(pin<0 || pin>NUM_PINS)){
		fprintf(stderr,"ERROR: gpio pin must be between 0 & %d\n", NUM_PINS);
		return -1;
	}
	// if value FD is already set, just return
	if(likely(value_fd[pin])) return 0;
	// otherwise set up the value fd
	int temp_fd;
	char buf[MAX_BUF];
	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", pin);
	temp_fd = open(buf, O_RDWR);
	if(temp_fd<0){
		perror("ERROR in rc_gpio");
		fprintf(stderr, "probably need to export pin first\n");
		return -1;
	}
	value_fd[pin]=temp_fd;
	return 0;
}

// public functions
int rc_gpio_export(int pin)
{
	int fd, len;
	char buf[MAX_BUF];
	// sanity check
	if(unlikely(pin<0 || pin>NUM_PINS)){
		fprintf(stderr,"ERROR: gpio pin must be between 0 & %d\n", NUM_PINS);
		return -1;
	}
	fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
	if(unlikely(fd<0)){
		perror("ERROR: in rc_gpio_export");
		return -1;
	}
	len = snprintf(buf, sizeof(buf), "%d", pin);
	if(unlikely(write(fd, buf, len)!=len)){
		perror("ERROR: in rc_gpio_export");
		return -1;
	}
	close(fd);
	return __init_pin_fd(pin);
}


int rc_gpio_unexport(int pin)
{
	int fd, len;
	char buf[MAX_BUF];
	// sanity check
	if(pin<0 || pin>NUM_PINS){
		fprintf(stderr,"ERROR: gpio pin must be between 0 & %d\n", NUM_PINS);
		return -1;
	}
	fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
	if (fd < 0) {
		fprintf(stderr,"ERROR: in rc_gpio_unexport, failed to open gpio file handle\n");
		return -1;
	}
	len = snprintf(buf, sizeof(buf), "%d", pin);
	if(unlikely(write(fd, buf, len)!=len)){
		perror("ERROR: in rc_gpio_unexport");
		return -1;
	}
	close(fd);
	close(value_fd[pin]);
	value_fd[pin]=0;
	return 0;
}


int rc_gpio_set_dir(int pin, rc_pin_direction_t out_flag)
{
	int fd;
	char buf[MAX_BUF];
	snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%i/direction", pin);
	fd = open(buf, O_WRONLY);
	if(fd<0){
		fprintf(stderr,"ERROR: in rc_gpio_set_dir, failed to open gpio %d file handle\n", pin);
		fprintf(stderr, "probably need to export pin first\n");
		return -1;
	}
	if(out_flag==GPIO_OUTPUT_PIN)	write(fd, "out", 4);
	else				write(fd, "in", 3);
	// close and return
	close(fd);
	return 0;
}


int rc_gpio_set_value(int pin, int value)
{
	int ret;
	if(unlikely(__init_pin_fd(pin))) return -1;
	if(value) write(value_fd[pin], "1", 2);
	else write(value_fd[pin], "0", 2);
	// write to pre-saved file descriptor
	if(unlikely(ret!=2)){
		perror("ERROR in rc_gpio_set_value");
		return -1;
	}
	return 0;
}


int rc_gpio_get_value(int pin)
{
	char ch[2];
	if(unlikely(__init_pin_fd(pin))) return -1;

	if(unlikely(read(value_fd[pin], ch, 2)!=2)){
		perror("ERROR in rc_pgio_get_value");
		return -1;
	}
	if(ch[0] == '0') return 0;
	else if(likely(ch[0]=='1')) return 1;
	fprintf(stderr, "ERROR: gpio value returned: %s, expect 0 or 1\n", ch);
	return -1;
}


int rc_gpio_set_edge(int pin, rc_pin_edge_t edge)
{
	int fd, ret, bytes;
	char buf[MAX_BUF];
	if(unlikely(__init_pin_fd(pin))) return -1;
	// path to edge fd
	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/edge", pin);
	fd = open(buf, O_WRONLY);
	if(unlikely(fd<0)){
		fprintf(stderr,"ERROR: in rc_gpio_set_edge, failed to open gpio file handle\n");
		return fd;
	}
	// write correct string
	switch(edge){
	case GPIO_EDGE_NONE:
		bytes=5;
		ret=write(fd, "none", bytes);
		break;
	case GPIO_EDGE_RISING:
		bytes=7;
		ret=write(fd, "rising", bytes);
		break;
	case GPIO_EDGE_FALLING:
		bytes=8;
		ret=write(fd, "falling", bytes);
		break;
	case GPIO_EDGE_BOTH:
		bytes=5;
		ret=write(fd, "both", bytes);
		break;
	default:
		printf("ERROR: invalid edge direction\n");
		return -1;
	}
	// make sure write worked
	if(unlikely(ret!=bytes)){
		perror("ERROR: in rc_gpio_set_edge, failed to write to gpio file handle");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}


int rc_gpio_get_value_fd(int pin)
{
	if(unlikely(__init_pin_fd(pin))) return -1;
	return value_fd[pin];
}


