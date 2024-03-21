/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SENSOR_SPS30_H__
#define __SENSOR_SPS30_H__

#include <zephyr/drivers/sensor.h>

struct sps30_sensor_measurement {
	struct sensor_value mc_1p0;
	struct sensor_value mc_2p5;
	struct sensor_value mc_4p0;
	struct sensor_value mc_10p0;
	struct sensor_value nc_0p5;
	struct sensor_value nc_1p0;
	struct sensor_value nc_2p5;
	struct sensor_value nc_4p0;
	struct sensor_value nc_10p0;
	struct sensor_value typical_particle_size;
};

int sps30_sensor_init(void);
int sps30_sensor_read(struct sps30_sensor_measurement *measurement);
void sps30_log_measurements(struct sps30_sensor_measurement *measurement);
int sps30_sensor_set_fan_auto_cleaning_interval(uint32_t interval_seconds);
int sps30_sensor_clean_fan(void);

#endif
