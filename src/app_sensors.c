/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_sensors, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/stream.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>

#include "app_sensors.h"
#include "sensors.h"

#ifdef CONFIG_LIB_OSTENTUS
#include <libostentus.h>
#endif
#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#include "battery_monitor/battery.h"
#endif

static struct golioth_client *client;

/* Sensor readings */
static struct bme280_sensor_measurement bme280_sm;
static struct scd4x_sensor_measurement scd4x_sm;
static struct sps30_sensor_measurement sps30_sm;

/* Formatting string for sending sensor JSON to Golioth */
#define JSON_FMT "{\"tem\":%f,\"pre\":%f,\"hum\":%f,\"co2\":%u,\"mc_1p0\":%f,\"mc_2p5\":%f,\"mc_4p0\":%f,\"mc_10p0\":%f,\"nc_0p5\":%f,\"nc_1p0\":%f,\"nc_2p5\":%f,\"nc_4p0\":%f,\"nc_10p0\":%f,\"tps\":%f}"

/* Callback for LightDB Stream */

static void async_error_handler(struct golioth_client *client,
				const struct golioth_response *response,
				const char *path,
				void *arg)
{
	if (response->status != GOLIOTH_OK) {
		LOG_ERR("Async task failed: %d", response->status);
		return;
	}
}

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_sensors_read_and_stream(void)
{
	int err;
	char json_buf[512];

	LOG_DBG("Collecting battery measurements...");

	IF_ENABLED(CONFIG_ALUDEL_BATTERY_MONITOR, (
		read_and_report_battery(client);
		IF_ENABLED(CONFIG_LIB_OSTENTUS, (
			slide_set(BATTERY_V, get_batt_v_str(), strlen(get_batt_v_str()));
			slide_set(BATTERY_LVL, get_batt_lvl_str(), strlen(get_batt_lvl_str()));
		));
	));

	LOG_DBG("Collecting sensor measurements...");

	/* Read the weather sensor */
	err = bme280_sensor_read(&bme280_sm);
	if (err) {
		LOG_ERR("Failed to read from Weather Sensor BME280: %d", err);
	} else {
		LOG_DBG("BME280: Temperature=%.2f °C, Pressure=%.2f kPa, Humidity=%.2f %%RH",
			sensor_value_to_double(&bme280_sm.temperature),
			sensor_value_to_double(&bme280_sm.pressure),
			sensor_value_to_double(&bme280_sm.humidity));
	}

	/* Read the CO₂ sensor */
	err = scd4x_sensor_read(&scd4x_sm);
	if (err) {
		LOG_ERR("Failed to read from Co2 Sensor SCD4x: %d", err);
	} else {
		LOG_DBG("scd4x: CO₂=%u ppm, Temperature=%.2lf °C, Humidity=%.2lf %%RH",
		scd4x_sm.co2, sensor_value_to_double(&scd4x_sm.temperature), sensor_value_to_double(&scd4x_sm.humidity));

	}

	/* Read the PM sensor */
	err = sps30_sensor_read(&sps30_sm);
	if (err) {
		LOG_ERR("Failed to read from PM Sensor SPS30: %d", err);
	} else {
		LOG_DBG("sps30: "
		"PM1.0=%f μg/m³, PM2.5=%f μg/m³, "
		"PM4.0=%f μg/m³, PM10.0=%f μg/m³, "
		"NC0.5=%f #/cm³, NC1.0=%f #/cm³, "
		"NC2.5=%f #/cm³, NC4.0=%f #/cm³, "
		"NC10.0=%f #/cm³, Typical Particle Size=%f μm",
		sensor_value_to_double(&sps30_sm.mc_1p0), sensor_value_to_double(&sps30_sm.mc_2p5), sensor_value_to_double(&sps30_sm.mc_4p0),
		sensor_value_to_double(&sps30_sm.mc_10p0), sensor_value_to_double(&sps30_sm.nc_0p5), sensor_value_to_double(&sps30_sm.nc_1p0),
		sensor_value_to_double(&sps30_sm.nc_2p5), sensor_value_to_double(&sps30_sm.nc_4p0), sensor_value_to_double(&sps30_sm.nc_10p0),
		sensor_value_to_double(&sps30_sm.typical_particle_size));
	}

	/* Send sensor data to Golioth */
	if (golioth_client_is_connected(client)) {
		LOG_DBG("Sending sensor data to Golioth");

		snprintk(json_buf, sizeof(json_buf), JSON_FMT,
			sensor_value_to_double(&bme280_sm.temperature),
			sensor_value_to_double(&bme280_sm.pressure),
			sensor_value_to_double(&bme280_sm.humidity), scd4x_sm.co2,
			sensor_value_to_double(&sps30_sm.mc_1p0), sensor_value_to_double(&sps30_sm.mc_2p5),
			sensor_value_to_double(&sps30_sm.mc_4p0),
			sensor_value_to_double(&sps30_sm.mc_10p0),
			sensor_value_to_double(&sps30_sm.nc_0p5), sensor_value_to_double(&sps30_sm.nc_1p0),
			sensor_value_to_double(&sps30_sm.nc_2p5), sensor_value_to_double(&sps30_sm.nc_4p0),
			sensor_value_to_double(&sps30_sm.nc_10p0),
			sensor_value_to_double(&sps30_sm.typical_particle_size));

		/* LOG_DBG("%s", json_buf); */

		err = golioth_stream_set_async(client,
					"sensor",
					GOLIOTH_CONTENT_TYPE_JSON,
					json_buf,
					strlen(json_buf),
					async_error_handler,
					NULL);
		if (err) {
			LOG_ERR("Failed to send sensor data to Golioth: %d", err);
		}
	} else {
		LOG_WRN("Device is not connected to Golioth, unable to send sensor data");
	}

	IF_ENABLED(CONFIG_LIB_OSTENTUS, (
		/* Update slide values on Ostentus
		 *  -values should be sent as strings
		 *  -use the enum from app_sensors.h for slide key values
		 */
		snprintk(json_buf, sizeof(json_buf), "%.2f °C", sensor_value_to_double(&bme280_sm.temperature));
		slide_set(TEMPERATURE, json_buf, strlen(json_buf));

		snprintk(json_buf, sizeof(json_buf), "%.2f kPa", sensor_value_to_double(&bme280_sm.pressure));
		slide_set(PRESSURE, json_buf, strlen(json_buf));

		snprintk(json_buf, sizeof(json_buf), "%.2f %%RH", sensor_value_to_double(&bme280_sm.humidity));
		slide_set(HUMIDITY, json_buf, strlen(json_buf));

		snprintk(json_buf, sizeof(json_buf), "%u ppm", scd4x_sm.co2);
		slide_set(CO2, json_buf, strlen(json_buf));

		snprintk(json_buf, sizeof(json_buf), "%d ug/m^3", sps30_sm.mc_2p5.val1);
		slide_set(PM2P5, json_buf, strlen(json_buf));

		snprintk(json_buf, sizeof(json_buf), "%d ug/m^3", sps30_sm.mc_10p0.val1);
		slide_set(PM10P0, json_buf, strlen(json_buf));
	));
}

void app_sensors_set_client(struct golioth_client *sensors_client)
{
	client = sensors_client;
}
