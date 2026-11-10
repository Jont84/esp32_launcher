#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Start polling the controller (creates the shared I2C bus). */
void input_init(void);

/* Buttons held now (BTN_* bits). */
uint32_t input_get(void);

/* Is the controller answering? */
bool input_connected(void);

/* The shared I2C bus (i2c_master_bus_handle_t), for the touch and audio drivers later. */
void *input_i2c_bus(void);
