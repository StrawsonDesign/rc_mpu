/**
 * @example    rc_test_gpio.c
 *  this is an example of how to use the gpio routines in <rc/gpio.h>
 *
 * Optionally provide the pin number as an argument.
 */




#include <stdio.h>
#include <stdlib.h> // for atoi and 'system'
#include <unistd.h> // for sleep

#include <rc/gpio.h>

#define PIN 70

int main(int argc, char *argv[]){
	int pin;
	if(argc==1) pin=PIN;
	else if(argc==2) pin=atoi(argv[1]);
	else{
		fprintf(stderr, "too many arguments\n");
		return -1;
	}

	fprintf(stderr,"exporting pin %d\n", pin);
	if(rc_gpio_export(pin)) return -1;

	fprintf(stderr,"setting direction to input\n");
	if(rc_gpio_set_dir(pin, GPIO_INPUT_PIN)) return -1;

	fprintf(stderr,"direction: ");
	if(rc_gpio_print_dir(pin)<0) return -1;
	fprintf(stderr,"\n");

	fprintf(stderr,"value: ");
	if(rc_gpio_print_value(pin)<0) return -1;
	fprintf(stderr,"\n");

	fprintf(stderr,"setting edge to falling\n");
	if(rc_gpio_set_edge(pin, GPIO_EDGE_FALLING)) return -1;

	fprintf(stderr,"setting edge to none\n");
	if(rc_gpio_set_edge(pin, GPIO_EDGE_NONE)) return -1;

	fprintf(stderr,"setting direction to output\n");
	if(rc_gpio_set_dir(pin, GPIO_OUTPUT_PIN)) return -1;

	fprintf(stderr,"direction: ");
	if(rc_gpio_print_dir(pin)<0) return -1;
	fprintf(stderr,"\n");

	fprintf(stderr,"value: ");
	if(rc_gpio_print_value(pin)<0) return -1;
	fprintf(stderr,"\n");

	fprintf(stderr,"setting value to 0\n");
	if(rc_gpio_set_value(pin,0)<0) return -1;

	fprintf(stderr,"value: ");
	if(rc_gpio_print_value(pin)<0) return -1;
	fprintf(stderr,"\n");

	fprintf(stderr,"setting value to 1\n");
	if(rc_gpio_set_value(pin,1)<0) return -1;

	fprintf(stderr,"value: ");
	if(rc_gpio_print_value(pin)<0) return -1;
	fprintf(stderr,"\n");


	fprintf(stderr,"unexporting\n");
	if(rc_gpio_unexport(pin)) return -1;

	fprintf(stderr,"successful\n");
	return 0;

}
