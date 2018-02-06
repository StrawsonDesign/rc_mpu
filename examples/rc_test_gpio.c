
#include <stdio.h>
#include <unistd.h> // for sleep

#include <rc/gpio.h>

#define PIN 30

int main(){

	if(rc_gpio_export(PIN)) return -1;
	printf("done exporting\n");

	if(rc_gpio_set_dir(PIN, GPIO_OUTPUT_PIN)) return -1;
	printf("done setting direction to output\n");

	if(rc_gpio_print_dir(PIN)<0) return -1;
	printf("< new direction\n");

	if(rc_gpio_get_value(PIN)<0) return -1;
	printf("done getting value\n");

	if(rc_gpio_print_value(PIN)<0) return -1;
	printf("< value\n");

	if(rc_gpio_set_value(PIN,0)<0) return -1;
	printf("done setting value\n");

	if(rc_gpio_print_value(PIN)<0) return -1;
	printf("< value\n");

	if(rc_gpio_set_value(PIN,1)<0) return -1;
	printf("done setting value\n");

	if(rc_gpio_print_value(PIN)<0) return -1;
	printf("< value\n");

	if(rc_gpio_set_dir(PIN, GPIO_INPUT_PIN)) return -1;
	printf("done setting direction to input\n");

	if(rc_gpio_print_dir(PIN)<0) return -1;
	printf("< new direction\n");

	if(rc_gpio_print_value(PIN)<0) return -1;
	printf("< value\n");

	if(rc_gpio_set_edge(PIN, GPIO_EDGE_FALLING)) return -1;
	printf("done setting edge\n");

	if(rc_gpio_unexport(PIN)) return -1;
	printf("done exporting");

	printf("successful\n");
	return 0;

}
