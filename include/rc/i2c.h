/*******************************************************************************
* I2C functions
*
* I2C bus 1 is broken out on the robotics cape on socket "I2C1" and is free for
* the user to have full authority over. Bus 0 is used internally on the cape
* for the IMU and barometer. The user should not use bus 0 unless they know what
* they are doing. The IMU and barometer functions
*
* @ int rc_i2c_init(int bus, uint8_t devAddr)
* This initializes an I2C bus (0 or 1) at 400khz as defined in the device tree.
* The bus speed cannot be modified. devAddr is the 8-bit i2c address of the
* device you wish to communicate with. This devAddr can be changed later
* without initializing. rc_i2c_init only needs to be called once per bus.
*
* @ int set_device_address(int bus, uint8_t devAddr)
* Use this to change to another device address after initialization.
*
* @ int rc_i2c_close(int bus)
* Closes the bus and device file descriptors.
*
* @int rc_i2c_claim_bus(int bus)
* @int rc_i2c_release_bus(int bus)
* @int rc_i2c_get_in_use_state(int bus)
* Claim and release bus are purely for the convenience of the user and are not
* necessary. They simply set a flag indicating that the bus is in use to help
* manage multiple device access in multithreaded applications.
*
* @ int rc_i2c_read_byte(int bus, uint8_t regAddr, uint8_t *data)
* @ int rc_i2c_read_bytes(int bus, uint8_t regAddr, uint8_t length, uint8_t *data)
* @ int rc_i2c_read_word(int bus, uint8_t regAddr, uint16_t *data)
* @ int rc_i2c_read_words(int bus, uint8_t regAddr, uint8_t length, uint16_t *data)
* @ int rc_i2c_read_bit(int bus, uint8_t regAddr, uint8_t bitNum, uint8_t *data)
* These rc_i2c_read functions are for reading data from a particular register.
* This sends the device address and register address to be read from before
* reading the response.
*
* @ int rc_i2c_write_byte(int bus, uint8_t regAddr, uint8_t data);
* @ int rc_i2c_write_bytes(int bus, uint8_t regAddr, uint8_t length, uint8_t* data)
* @ int rc_i2c_write_word(int bus, uint8_t regAddr, uint16_t data);
* @ int rc_i2c_write_words(int bus,uint8_t regAddr, uint8_t length, uint16_t* data)
* @ int rc_i2c_write_bit(int bus, uint8_t regAddr, uint8_t bitNum, uint8_t data)
* These write values write a value to a particular register on the previously
* selected device.
*
* @ int rc_i2c_send_bytes(int bus, uint8_t length, uint8_t* data)
* @ int rc_i2c_send_byte(int bus, uint8_t data)
* Instead of automatically sending a device address before the data which is
* what happens in the above read and write functions, the rc_i2c_send functions
* send only the data given by the data argument. This is useful for more
* complicated IO such as uploading firmware to a device.
*******************************************************************************/
#ifndef RC_I2C_H
#define RC_I2C_H

#include <stdint.h>

/**
 * @brief      Maximum I2C bus identifier. Default is 5 for a total of 6 busses.
 *             This can be increased by the user for special cases.
 */
#define I2C_MAX_BUS 5

/**
 * @brief      Initializes a bus and sets it to talk to a particular device
 *             address.
 *
 * @param[in]  bus      The bus
 * @param[in]  devAddr  The device address
 *
 * @return     0 on success or -1 on failure
 */
int rc_i2c_init(int bus, uint8_t devAddr);

/**
 * @brief      Closes an I2C bus
 *
 * @param[in]  bus   The bus
 *
 * @return     0 on success or -1 on failure
 */
int rc_i2c_close(int bus);

/**
 * @brief      Changes the device address the bus is configured to talk to.
 *
 *             Actually changing the device address in the I2C driver requires a
 *             system call and is relatively slow. This function records which
 *             device address the bus is set to and will only make the system
 *             call if the requested address is different than the set address.
 *             This makes it safe to call this function repeatedly with no
 *             performance penalty.
 *
 * @param[in]  bus      The bus
 * @param[in]  devAddr  The new device address
 *
 * @return     { description_of_the_return_value }
 */
int rc_i2c_set_device_address(int bus, uint8_t devAddr);

int rc_i2c_claim_bus(int bus);
int rc_i2c_release_bus(int bus);
int rc_i2c_get_in_use_state(int bus);

int rc_i2c_read_byte(int bus, uint8_t regAddr, uint8_t *data);
int rc_i2c_read_bytes(int bus, uint8_t regAddr, uint8_t length,  uint8_t *data);
int rc_i2c_read_word(int bus, uint8_t regAddr, uint16_t *data);
int rc_i2c_read_words(int bus, uint8_t regAddr, uint8_t length, uint16_t *data);
int rc_i2c_read_bit(int bus, uint8_t regAddr, uint8_t bitNum, uint8_t *data);

int rc_i2c_write_byte(int bus, uint8_t regAddr, uint8_t data);
int rc_i2c_write_bytes(int bus, uint8_t regAddr, uint8_t length, uint8_t* data);
int rc_i2c_write_word(int bus, uint8_t regAddr, uint16_t data);
int rc_i2c_write_words(int bus, uint8_t regAddr, uint8_t length, uint16_t* data);
int rc_i2c_write_bit(int bus, uint8_t regAddr, uint8_t bitNum, uint8_t data);

int rc_i2c_send_bytes(int bus, uint8_t length, uint8_t* data);
int rc_i2c_send_byte(int bus, uint8_t data);

#endif // RC_I2C_H
