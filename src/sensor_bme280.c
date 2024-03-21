/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_bme280, LOG_LEVEL_DBG);

#include <zephyr/drivers/sensor.h>

#include "sensor_bme280.h"

const struct device *bme280_dev = DEVICE_DT_GET(DT_NODELABEL(bme280));

int bme280_sensor_init(void)
{
	LOG_DBG("Initializing BME280 weather sensor");

	if (bme280_dev == NULL) {
		/* No such node, or the node does not have status "okay". */
		LOG_ERR("Device \"%s\" not found", bme280_dev->name);
		return -ENODEV;
	}

	if (!device_is_ready(bme280_dev)) {
		LOG_ERR("Device \"%s\" is not ready", bme280_dev->name);
		return -ENODEV;
	}

	return 0;
}

int bme280_sensor_read(struct bme280_sensor_measurement *measurement)
{
	int err;

	LOG_DBG("Reading BME280 weather sensor");

	err = sensor_sample_fetch(bme280_dev);
	if (err) {
		LOG_ERR("Error fetching weather sensor sample: %d", err);
		return err;
	}

	sensor_channel_get(bme280_dev, SENSOR_CHAN_AMBIENT_TEMP, &measurement->temperature);
	sensor_channel_get(bme280_dev, SENSOR_CHAN_PRESS, &measurement->pressure);
	sensor_channel_get(bme280_dev, SENSOR_CHAN_HUMIDITY, &measurement->humidity);

	return err;
}

void bme280_log_measurements(struct bme280_sensor_measurement *measurement)
{
	LOG_DBG("BME280: Temperature=%.2f °C, Pressure=%.2f kPa, Humidity=%.2f %%RH",
		sensor_value_to_double(&measurement->temperature),
		sensor_value_to_double(&measurement->pressure),
		sensor_value_to_double(&measurement->humidity));
}
