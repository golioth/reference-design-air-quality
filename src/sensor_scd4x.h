/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SENSOR_SCD4X_H__
#define __SENSOR_SCD4X_H__

#include <zephyr/drivers/sensor.h>

struct scd4x_sensor_measurement {
	uint16_t co2;
	struct sensor_value temperature;
	struct sensor_value humidity;
};

int scd4x_sensor_init(void);
int scd4x_sensor_read(struct scd4x_sensor_measurement *measurement);
void scd4x_log_measurements(struct scd4x_sensor_measurement *measurement);
int scd4x_sensor_set_temperature_offset(int32_t t_offset_m_deg_c);
int scd4x_sensor_set_sensor_altitude(int16_t sensor_altitude);
int scd4x_sensor_set_automatic_self_calibration(bool asc_enabled);

#endif
