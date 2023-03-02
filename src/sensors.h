/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/sensor.h>

struct bme280_sensor_measurement {
    struct sensor_value temperature;
	struct sensor_value pressure;
	struct sensor_value humidity;
};

struct scd4x_sensor_measurement {
    uint16_t co2;
	struct sensor_value temperature;
	struct sensor_value humidity;
};

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

int bme280_sensor_read(struct bme280_sensor_measurement *measurement);
int bme280_sensor_init(void);
int scd4x_sensor_read(struct scd4x_sensor_measurement *measurement);
int scd4x_sensor_init(void);
int sps30_sensor_read(struct sps30_sensor_measurement *measurement);
int sps30_clean_fan(void);
int sps30_sensor_init(void);
void update_sensor_settings(void);
