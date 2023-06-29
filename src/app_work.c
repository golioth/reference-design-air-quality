/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_work, LOG_LEVEL_DBG);

#include <net/golioth/system_client.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

#include "app_state.h"
#include "app_settings.h"
#include "app_work.h"
#include "sensors.h"
#include "battery.h"
#include "libostentus/libostentus.h"

static struct golioth_client *client;

/* Formatting string for sending sensor JSON to Golioth */
#define JSON_FMT "{\
\"tem\":%d.%d,\
\"pre\":%d.%d,\
\"hum\":%d.%d,\
\"co2\":%u,\
\"mc_1p0\":%d.%d,\
\"mc_2p5\":%d.%d,\
\"mc_4p0\":%d.%d,\
\"mc_10p0\":%d.%d,\
\"nc_0p5\":%d.%d,\
\"nc_1p0\":%d.%d,\
\"nc_2p5\":%d.%d,\
\"nc_4p0\":%d.%d,\
\"nc_10p0\":%d.%d,\
\"tps\":%d.%d,\
\"batt_v\":%.2f,\
\"batt_lvl\":%.2f}"

/* Callback for LightDB Stream */
static int async_error_handler(struct golioth_req_rsp *rsp)
{
	if (rsp->err) {
		LOG_ERR("Async task failed: %d", rsp->err);
		return rsp->err;
	}
	return 0;
}

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_work_sensor_read(void)
{
	int err;
	struct sensor_value batt_v = {0, 0};
	struct sensor_value batt_lvl = {0, 0};
	struct bme280_sensor_measurement bme280_sm;
	struct scd4x_sensor_measurement scd4x_sm;
	struct sps30_sensor_measurement sps30_sm;
	char json_buf[512];

	LOG_DBG("Collecting battery measurements");

	IF_ENABLED(CONFIG_ALUDEL_BATTERY_MONITOR, (
		read_battery_info(&batt_v, &batt_lvl);

		LOG_INF("Battery measurement: voltage=%.2f V, level=%d%%",
			sensor_value_to_double(&batt_v), batt_lvl.val1);
	));

	LOG_DBG("Collecting sensor measurements");

	/* Read the weather sensor */
	err = bme280_sensor_read(&bme280_sm);
	if (err) {
		return;
	}

	/* Read the CO₂ sensor */
	err = scd4x_sensor_read(&scd4x_sm);
	if (err) {
		return;
	}

	/* Read the PM sensor */
	err = sps30_sensor_read(&sps30_sm);
	if (err) {
		return;
	}

	/* Send sensor data to Golioth */
	LOG_DBG("Sending sensor data to Golioth");

	snprintk(json_buf, sizeof(json_buf), JSON_FMT,
		bme280_sm.temperature.val1, bme280_sm.temperature.val2,
		bme280_sm.pressure.val1, bme280_sm.pressure.val2,
		bme280_sm.humidity.val1, bme280_sm.humidity.val2,
		scd4x_sm.co2,
		sps30_sm.mc_1p0.val1, sps30_sm.mc_1p0.val2,
		sps30_sm.mc_2p5.val1, sps30_sm.mc_2p5.val2,
		sps30_sm.mc_4p0.val1, sps30_sm.mc_4p0.val2,
		sps30_sm.mc_10p0.val1, sps30_sm.mc_10p0.val2,
		sps30_sm.nc_0p5.val1, sps30_sm.nc_0p5.val2,
		sps30_sm.nc_1p0.val1, sps30_sm.nc_1p0.val2,
		sps30_sm.nc_2p5.val1, sps30_sm.nc_2p5.val2,
		sps30_sm.nc_4p0.val1, sps30_sm.nc_4p0.val2,
		sps30_sm.nc_10p0.val1, sps30_sm.nc_10p0.val2,
		sps30_sm.typical_particle_size.val1,
		sps30_sm.typical_particle_size.val2,
		sensor_value_to_double(&batt_v),
		sensor_value_to_double(&batt_lvl));

	err = golioth_stream_push_cb(client, "sensor",
		GOLIOTH_CONTENT_FORMAT_APP_JSON,
		json_buf, strlen(json_buf),
		async_error_handler, NULL);
	if (err) {
		LOG_ERR("Failed to send sensor data to Golioth: %d", err);
	}

	/* Update slide values on Ostentus
	 *  -values should be sent as strings
	 *  -use the enum from app_work.h for slide key values
	 */
	snprintk(json_buf, sizeof(json_buf), "%d.%d C",
		bme280_sm.temperature.val1, bme280_sm.temperature.val2 / 10000);
	slide_set(TEMPERATURE, json_buf, strlen(json_buf));
	snprintk(json_buf, sizeof(json_buf), "%d.%d kPa",
		bme280_sm.pressure.val1, bme280_sm.pressure.val2 / 10000);
	slide_set(PRESSURE, json_buf, strlen(json_buf));
	snprintk(json_buf, sizeof(json_buf), "%d.%d %%RH",
		bme280_sm.humidity.val1, bme280_sm.humidity.val2 / 10000);
	slide_set(HUMIDITY, json_buf, strlen(json_buf));
	snprintk(json_buf, sizeof(json_buf), "%u ppm", scd4x_sm.co2);
	slide_set(CO2, json_buf, strlen(json_buf));
	snprintk(json_buf, sizeof(json_buf), "%d.%d ug/m^3",
		sps30_sm.mc_2p5.val1, sps30_sm.mc_2p5.val1 / 10000);
	slide_set(PM2P5, json_buf, strlen(json_buf));
	snprintk(json_buf, sizeof(json_buf), "%d.%d ug/m^3",
		sps30_sm.mc_10p0.val1, sps30_sm.mc_10p0.val1 / 10000);
	slide_set(PM10P0, json_buf, strlen(json_buf));
}

void app_work_init(struct golioth_client *work_client)
{
	client = work_client;
}
