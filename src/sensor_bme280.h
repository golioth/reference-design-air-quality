/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SENSOR_BME280_H__
#define __SENSOR_BME280_H__

#include <zephyr/drivers/sensor.h>

struct bme280_sensor_measurement {
	struct sensor_value temperature;
	struct sensor_value pressure;
	struct sensor_value humidity;
};

int bme280_sensor_init(void);
int bme280_sensor_read(struct bme280_sensor_measurement *measurement);
void bme280_log_measurements(struct bme280_sensor_measurement *measurement);

#endif
