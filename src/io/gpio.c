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

#include <rc/gpio.h>

// preposessor macros
#define unlikely(x)	__builtin_expect (!!(x), 0)
#define likely(x)	__builtin_expect (!!(x), 1)

#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define MAX_BUF 64
#define NUM_PINS 128

// value file handle for each pin, uninitialized if ==0
static int value_fd[NUM_PINS];


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
	// warn user if pin is already configured,
	// but keep going anyway in case this was intentional
	if(value_fd[pin]==0){
		#ifdef DEBUG
		printf("WARNING: in rc_gpio_export, pin %d is already exported\n",pin);
		#endif
	}
	// check if pin has already been exported
	len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", pin);
	if(access(buf, F_OK)==0){
		#ifdef DEBUG
		fprintf(stderr,"WARNING tried to export gpio %d when already exported\n", pin);
		#endif
	}
	// if not exported, write to export file
	else{
		// open export file
		fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
		if(unlikely(fd<0)){
			perror("ERROR: in rc_gpio_export failed to open gpio/export file");
			return -1;
		}
		// write gpio number to the export file
		len = snprintf(buf, sizeof(buf), "%d", pin);
		if(unlikely(write(fd, buf, len)!=len)){
			perror("ERROR: in rc_gpio_export failed to write to gpio/export file");
			close(fd);
			return -1;
		}
		close(fd);
		// wait at least 50ms for the gpio driver to figure itself out
		// this is a hacky workaround but is necessary right now on the
		// BBB or the driver will misbehave
		usleep(100000);
	}
	// get the value FD to save for later
	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", pin);
	int temp_fd = open(buf, O_RDWR);
	if(temp_fd<0){
		perror("ERROR in rc_gpio_export, failed to open gpio value fd");
		return -1;
	}
	value_fd[pin]=temp_fd;
	return 0;
}


int rc_gpio_unexport(int pin)
{
	int fd, len;
	char buf[MAX_BUF];
	// sanity check
	if(unlikely(pin<0 || pin>NUM_PINS)){
		fprintf(stderr,"ERROR: gpio pin must be between 0 & %d\n", NUM_PINS);
		return -1;
	}
	// check if pin needs unexporting
	len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", pin);
	if(access(buf, F_OK)!=0){
		#ifdef DEBUG
		fprintf(stderr,"WARNING, trying to unexport pin which is not exported\n");
		#endif
	}
	else{
		// open unexport fd
		fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
		if(fd<0){
			perror("ERROR: in rc_gpio_unexport, failed to open unexport file handle\n");
			close(value_fd[pin]);
			value_fd[pin]=0;
			return -1;
		}
		// write the pin to unexport
		len = snprintf(buf, sizeof(buf), "%d", pin);
		if(unlikely(write(fd, buf, len)!=len)){
			perror("ERROR: in rc_gpio_unexport writing to unexport file");
			close(value_fd[pin]);
			value_fd[pin]=0;
			return -1;
		}
		close(fd);
	}
	close(value_fd[pin]);
	value_fd[pin]=0;
	return 0;
}


int rc_gpio_set_dir(int pin, rc_pin_direction_t dir)
{
	int fd, ret;
	char buf[MAX_BUF];
	//sanity checkss
	if(unlikely(pin<0 || pin>NUM_PINS)){
		fprintf(stderr,"ERROR: in rc_gpio_set_dir, gpio pin must be between 0 & %d\n", NUM_PINS);
		return -1;
	}
	// open direction fd
	snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%i/direction", pin);
	fd = open(buf, O_WRONLY);
	if(fd<0){
		perror("ERROR: in rc_gpio_set_dir, failed to open gpio direction handle");
		fprintf(stderr,"probably need to export pin first\n");
		return -1;
	}
	// write direction
	errno=0;
	if(dir==GPIO_OUTPUT_PIN)	ret=write(fd, "out", 4);
	else if(dir==GPIO_INPUT_PIN)	ret=write(fd, "in", 3);
	else{
		fprintf(stderr,"ERROR: in rc_gpio_set_dir, invalid direction\n");
		return -1;
	}
	// check write for success
	if(ret==-1){
		perror("ERROR in rc_gpio_set_dir, failed to write to direction fd");
		close(fd);
		return -1;
	}

	// close and return
	close(fd);
	return 0;
}


int rc_gpio_set_value(int pin, int value)
{
	int ret;
	if(unlikely(value_fd[pin]==0)){
		fprintf(stderr,"ERROR: trying to call rc_gpio_set_value without exporting first\n");
		return -1;
	}
	if(value) ret=write(value_fd[pin], "1", 2);
	else ret=write(value_fd[pin], "0", 2);
	// write to pre-saved file descriptor
	if(unlikely(ret!=2)){
		perror("ERROR in rc_gpio_set_value");
		return -1;
	}
	return 0;
}


int rc_gpio_get_value(int pin)
{
	char buf[MAX_BUF];
	char ch;
	int ret;
	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", pin);
	int fd;
	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		perror("ERROR in rc_gpio_print_value, can't read value fd");
		return -1;
	}
	ret=read(fd, &ch, 1);
	if(ret==-1){
		perror("ERROR: in rc_gpio_get_value while reading from fd");
		return -1;
	}
	if(ch == '0') ret=0;
	else if(likely(ch == '1')) ret=1;
	else{
		fprintf(stderr, "ERROR: gpio value returned: %c expected 0 or 1\n", ch);
		ret = -1;
	}
	close(fd);
	return ret;
}


int rc_gpio_set_edge(int pin, rc_pin_edge_t edge)
{
	int fd, ret, bytes;
	char buf[MAX_BUF];
	if(unlikely(value_fd[pin]==0)){
		fprintf(stderr,"ERROR: trying to call rc_gpio_get_edge without exporting first\n");
		return -1;
	}
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
	if(unlikely(value_fd[pin]==0)){
		fprintf(stderr,"ERROR: trying to call rc_gpio_get_value_fd without exporting first\n");
		return -1;
	}
	return value_fd[pin];
}


int rc_gpio_print_value(int pin)
{
	int ret = rc_gpio_get_value(pin);
	if(ret==0) fprintf(stderr,"0");
	else if(ret==1) fprintf(stderr,"1");
	return ret;
}


int rc_gpio_print_dir(int pin)
{
	int fd, i;
	char buf[MAX_BUF];
	char dir[4];
	//sanity checkss
	if(unlikely(pin<0 || pin>NUM_PINS)){
		fprintf(stderr,"ERROR: in rc_gpio_print_dir, gpio pin must be between 0 & %d\n", NUM_PINS);
		return -1;
	}
	// open direction fd
	snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%i/direction", pin);
	fd = open(buf, O_RDONLY);
	if(fd<0){
		perror("ERROR: in rc_gpio_print_dir, failed to open gpio direction handle");
		fprintf(stderr,"probably need to export pin first\n");
		return -1;
	}
	// read direction
	memset(dir,0,sizeof(dir));
	errno=0;
	if(unlikely(read(fd, dir, sizeof(dir))==-1)){
		perror("ERROR in rc_gpio_print_value, can't read direction fd");
		return -1;
	}

	// replace newline character with null character
	for(i=0;i<4;i++) if(dir[i]==0x0A) dir[i]=0;
	fprintf(stderr,"%s",dir);
	return 0;
}

