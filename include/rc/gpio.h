/**
 * @headerfile gpio.h <rc/gpio.h>
 *
 * @brief      userspace C interface for the Linux GPIO driver
 *
 *             Developed and tested on the BeagleBone Black but should work fine
 *             on any Linux system.
 *
 * @author     James Strawson
 *
 * @date       1/19/2018
 *
 *
 *
 * @addtogroup GPIO
 * @ingroup    IO
 * @{
 */

#ifndef RC_GPIO_H
#define RC_GPIO_H

#ifdef  __cplusplus
extern "C" {
#endif

#define GPIO_HIGH 1
#define GPIO_LOW 0


/**
 * @brief      Maximum number of pins supported. This is 128 for BeagleBone, but
 *             may be increased for other platforms if needed.
 */
#define MAX_GPIO_PINS 128


/**
 * @brief      Pin direction enum for configuring GPIO pins as input or output.
 */
typedef enum rc_pin_direction_t{
	GPIO_INPUT_PIN,
	GPIO_OUTPUT_PIN
}rc_pin_direction_t;

/**
 * @brief      Enum for setting GPIO edge detection.
 */
typedef enum rc_pin_edge_t{
	GPIO_EDGE_NONE,
	GPIO_EDGE_RISING,
	GPIO_EDGE_FALLING,
	GPIO_EDGE_BOTH
}rc_pin_edge_t;


/**
 * @brief      Exports (initializes) a gpio pin with the system driver.
 *
 * @param[in]  pin   The pin ID
 *
 * @return     0 on success or -1 on failure.
 */
int rc_gpio_export(int pin);

/**
 * @brief      Unexports (uninitializes) a gpio pin with the system driver. Not
 *             normally needed.
 *
 * @param[in]  pin   The pin ID
 *
 * @return     0 on success or -1 on failure.
 */
int rc_gpio_unexport(int pin);

/**
 * @brief      Sets the direction of a pin as input or output, see enum
 *             rc_pin_direction_t.
 *
 * @param[in]  pin   The pin ID
 * @param[in]  dir   Direction
 *
 * @return     0 on success or -1 on failure
 */
int rc_gpio_set_dir(int pin, rc_pin_direction_t dir);

/**
 * @brief      Sets the value of a GPIO pin when in output mode
 *
 * @param[in]  pin    The pin ID
 * @param[in]  value  The value
 *
 * @return     0 on success or -1 on failure
 */
int rc_gpio_set_value(int pin, int value);

/**
 * @brief      Reads the value of a PGIO pin when in input mode or output mode.
 *
 * @param[in]  pin   The pin ID
 *
 * @return     1 if pin is high, 0 if pin is low, -1 on error
 */
int rc_gpio_get_value(int pin);

/**
 * @brief      Enbales edge detection (triggering) for the pin
 *
 * @param[in]  pin   The pin ID
 * @param[in]  edge  The edge_dir, see rc_pin_edge_t
 *
 * @return     0 on success, -1 on failure
 */
int rc_gpio_set_edge(int pin, rc_pin_edge_t edge_dir);

/**
 * @brief      Fetches a file descriptor to the gpio value.
 *
 *             This is mainly used when the user wants to poll the gpio value
 *             after setting up edge detection with rc_gpio_set_edge.
 *
 * @param[in]  pin   The pin ID
 *
 * @return     Returns a file descriptor to the gpio value, or -1 on error
 */
int rc_gpio_get_value_fd(int pin);

/**
 * @brief      prints the current value of a pin, "0" or "1"
 *
 * @param[in]  pin   The pin ID
 *
 * @return     0 on success or -1 on failure
 */
int rc_gpio_print_value(int pin);

/**
 * @brief      prints the direction of a pin as it's currently set "in" or "out"
 *
 * @param[in]  pin   The pin ID
 *
 * @return     0 on success or -1 on failure
 */
int rc_gpio_print_dir(int pin);


#ifdef  __cplusplus
}
#endif

#endif // RC_GPIO_H

///@} end group IO