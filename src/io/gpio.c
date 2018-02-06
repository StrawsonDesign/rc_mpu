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
static int init_flag[NUM_PINS];


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
	if(init_flag[pin]){
		printf("WARNING: in rc_gpio_export, pin %d is already exported\n",pin);
	}
	// check if pin has already been exported
	len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", pin);
	if(access(buf, F_OK)==0){
		fprintf(stderr,"tried to export gpio %d when already exported by another process\n", pin);
		rc_gpio_unexport(pin);
	}
	// open export file
	fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
	if(unlikely(fd<0)){
		perror("ERROR: in rc_gpio_export failed to open gpio/export file: ");
		return -1;
	}
	// write gpio number to the export file
	len = snprintf(buf, sizeof(buf), "%d", pin);
	if(unlikely(write(fd, buf, len)!=len)){
		perror("ERROR: in rc_gpio_export failed to write to gpio/export file: ");
		close(fd);
		return -1;
	}
	if(close(fd)){
		perror("ERROR: in rc_gpio_export failed to close export fd: ");
		return -1;
	}
	// get the value FD to save for later
	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", pin);
	int temp_fd = open(buf, O_RDONLY);
	if(temp_fd<0){
		perror("ERROR in rc_gpio_export, failed to open gpio value");
		return -1;
	}
	value_fd[pin]=temp_fd;
	init_flag[pin]=1;

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
		fprintf(stderr,"WARNING, trying to unexport pin which is not exported\n");
	}
	// open unexport fd
	fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
	if(fd<0){
		perror("ERROR: in rc_gpio_unexport, failed to open unexport file handle\n");
		return -1;
	}
	// write the pin to unexport
	len = snprintf(buf, sizeof(buf), "%d", pin);
	if(unlikely(write(fd, buf, len)!=len)){
		perror("ERROR: in rc_gpio_unexport");
		return -1;
	}
	close(fd);
	close(value_fd[pin]);
	value_fd[pin]=0;
	init_flag[pin]=0;
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

	// check it exists
	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/direction", pin);
	errno=0;
	if(access(buf, F_OK)!=0){
		perror("ERROR: in rc_gpio_set_dir, can't access direction handle");
		fprintf(stderr, "probably need to export pin first\n");
		return -1;
	}
	// close value fd
	close(value_fd[pin]);
	// open direction fd
	snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%i/direction", pin);
	fd = open(buf, O_WRONLY);
	if(fd<0){
		perror("ERROR: in rc_gpio_set_dir, failed to open gpio direction handle");
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
	if(unlikely(init_flag[pin]==0)){
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
	char ch[2];
	if(unlikely(init_flag[pin]==0)){
		fprintf(stderr,"ERROR: trying to call rc_gpio_get_value without exporting first\n");
		return -1;
	}
	if(unlikely(read(value_fd[pin], ch, 2)!=2)){
		perror("ERROR in rc_gpio_get_value, can't read value fd");
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
	if(unlikely(init_flag[pin]==0)){
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
	if(unlikely(init_flag[pin]==0)){
		fprintf(stderr,"ERROR: trying to call rc_gpio_get_value_fd without exporting first\n");
		return -1;
	}
	return value_fd[pin];
}

int rc_gpio_print_value(int pin)
{
	char ch[2];
	if(unlikely(init_flag[pin]==0)){
		fprintf(stderr,"ERROR: trying to call rc_gpio_print_value without exporting first\n");
		return -1;
	}
	if(unlikely(read(value_fd[pin], ch, 2)==-1)){
		perror("ERROR in rc_gpio_print_value, can't read value fd");
		return -1;
	}
	if(ch[0] == '0'){
		printf("0");
		return 0;
	}
	else if(likely(ch[0]=='1')){
		printf("1");
		return 0;
	}
	fprintf(stderr, "ERROR: gpio value returned: %s, expect 0 or 1\n", ch);
	return -1;
}


int rc_gpio_print_dir(int pin){
	int fd;
	char buf[MAX_BUF];
	char dir[4];
	//sanity checkss
	if(unlikely(pin<0 || pin>NUM_PINS)){
		fprintf(stderr,"ERROR: in rc_gpio_print_dir, gpio pin must be between 0 & %d\n", NUM_PINS);
		return -1;
	}

	// check it exists
	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/direction", pin);
	errno=0;
	if(access(buf, F_OK)!=0){
		perror("ERROR: in rc_gpio_print_dir, can't access direction handle");
		fprintf(stderr, "probably need to export pin first\n");
		return -1;
	}
	// open direction fd
	snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%i/direction", pin);
	fd = open(buf, O_RDONLY);
	if(fd<0){
		perror("ERROR: in rc_gpio_print_dir, failed to open gpio direction handle");
		return -1;
	}
	// read direction
	errno=0;
	if(unlikely(read(fd, dir, 4)==-1)){
		perror("ERROR in rc_gpio_print_value, can't read direction fd");
		return -1;
	}
	printf("%s",dir);
	printf("insdie end of printdir\n");
	return 0;
}

