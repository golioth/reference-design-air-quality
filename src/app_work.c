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

#include "app_settings.h"
#include "app_work.h"
#include "app_rpc.h"
#include "libostentus/libostentus.h"
#include "sensirion_common.h"
#include "sensirion_i2c_hal.h"
#include "scd4x_i2c.h"
#include "sps30_i2c.h"

static struct golioth_client *client;
const struct device *weather_sensor = DEVICE_DT_GET(DT_NODELABEL(bme280));

struct scd4x_measurement {
	uint16_t co2;
	float temperature;
	float humidity;
};

/* Formatting string for sending sensor JSON to Golioth */
#define JSON_FMT "{\
\"tem\":%d.%d,\
\"pre\":%d.%d,\
\"hum\":%d.%d,\
\"co2\":%d.%d,\
\"mc_1p0\":%d.%d,\
\"mc_2p5\":%d.%d,\
\"mc_4p0\":%d.%d,\
\"mc_10p0\":%d.%d,\
\"nc_0p5\":%d.%d,\
\"nc_1p0\":%d.%d,\
\"nc_2p5\":%d.%d,\
\"nc_4p0\":%d.%d,\
\"nc_10p0\":%d.%d,\
\"tps\":%d.%d}"

/* Callback for LightDB Stream */
static int async_error_handler(struct golioth_req_rsp *rsp) {
	if (rsp->err) {
		LOG_ERR("Async task failed: %d", rsp->err);
		return rsp->err;
	}
	return 0;
}

int bme280_read(struct sensor_value *temperature,
	struct sensor_value *pressure,
	struct sensor_value *humidity)
{
	int err;

	LOG_DBG("Reading BME280 weather sensor");

	err = sensor_sample_fetch(weather_sensor);
	if (err) {
		LOG_ERR("Error reading weather sensor: %d", err);
		return err;
	}

	sensor_channel_get(weather_sensor, SENSOR_CHAN_AMBIENT_TEMP,
		temperature);
	sensor_channel_get(weather_sensor, SENSOR_CHAN_PRESS, pressure);
	sensor_channel_get(weather_sensor, SENSOR_CHAN_HUMIDITY, humidity);

	LOG_INF("bme280: temperature=%d.%d (°C); pressure=%d.%d "
		"(kPa); humidity=%d.%d (%%RH)",
		temperature->val1, temperature->val2, pressure->val1,
		pressure->val2, humidity->val1, humidity->val2);

	return err;
}

int scd4x_read(struct scd4x_measurement *measurement)
{
	int16_t err;
	bool data_ready_flag = false;
	uint16_t co2_ppm;
	int32_t temperature_m_deg_c, humidity_m_percent_rh;

	LOG_DBG("Reading SCD4x CO₂ sensor");

	/* Request a single-shot measurement */
	err = scd4x_measure_single_shot();
	if (err) {
		LOG_ERR("Error entering SCD4x single-shot measurement mode"
			" (error: %d)", err);
		return err;
	}

	/* Sleep while measurement is being taken */
	sensirion_i2c_hal_sleep_usec(SCD4X_MEASUREMENT_DURATION_USEC);

	/* Poll the sensor every 0.1s waiting for the data ready status */
	while (!data_ready_flag)
	{
		err = scd4x_get_data_ready_flag(&data_ready_flag);
		if (err) {
			LOG_ERR("Error reading SCD4x data ready status flag: "
				"%d", err);
			return err;
		}

		sensirion_i2c_hal_sleep_usec(100000);
	}

	/* Read the single-shot measurement */
	err = scd4x_read_measurement(&co2_ppm, &temperature_m_deg_c,
		&humidity_m_percent_rh);
	if (err) {
		LOG_ERR("Error reading SCD4x measurement: %d", err);
		return err;
	} else if (co2_ppm == 0) {
		LOG_ERR("Invalid SCD4x measurement sample");
		return err;
	}

	measurement->co2 = co2_ppm;
	measurement->temperature = temperature_m_deg_c / 1000.0;
	measurement->humidity = humidity_m_percent_rh / 1000.0;

	LOG_INF("scd4x: CO₂=%u (ppm); temperature=%f (°C);"
		" humidity=%f (%%RH)", measurement->co2,
		measurement->temperature, measurement->humidity);

	return err;
}

int scd4x_init()
{
	int err;
	uint16_t serial_0, serial_1, serial_2;
	struct scd4x_measurement measurement;

	LOG_DBG("Initializing SCD4x CO₂ sensor");

	/* After VDD reaches 2.25V, SCD4x needs 1000 ms to enter idle state */
	/* Sleep the full 1000 ms here just to be safe */
	sensirion_i2c_hal_sleep_usec(SCD4X_POWER_UP_DELAY_USEC);

	/* Wake up and reinitialize SCD4x to the default state */
	err = scd4x_wake_up();
	if (err) {
		LOG_ERR("Error %d: SCD4x wakeup failed", err);
		return err;
	}

	err = scd4x_stop_periodic_measurement();
	if (err) {
		LOG_ERR("Error %d: SCD4x stop periodic measurement failed",
			err);
		return err;
	}

	err = scd4x_reinit();
	if (err) {
		LOG_ERR("Error %d: SCD4x reinit failed", err);
		return err;
	}

	/* Since SCD4x doesn't ack wake_up, read SCD4x serial number instead */
	err = scd4x_get_serial_number(&serial_0, &serial_1, &serial_2);
	if (err) {
		LOG_ERR("Cannot read SCD4x serial number (error: %d)", err);
		return err;
	} else {
		LOG_INF("SCD4x serial number: 0x%04x%04x%04x", serial_0,
			serial_1, serial_2);
	}

	/* According to the datasheet, the first reading obtained after waking
	up the sensor must be discarded, so do a throw-away measurement now */
	err = scd4x_read(&measurement);

	return err;
}

int sps30_read(struct sps30_measurement *measurement)
{
	int16_t err;
	int16_t data_ready_flag = 0;

	LOG_DBG("Reading SPS30 particulate matter sensor");

	err = sps30_start_measurement();
	if (err) {
		LOG_ERR("Error entering SPS30 measurement mode (error: %d)",
			err);
		return err;
	}

	/* Sleep while measurement is being taken */
	sensirion_i2c_hal_sleep_usec(SPS30_MEASUREMENT_DURATION_USEC);

	/* Poll the sensor every 0.1s waiting for the data ready status */
	while (!data_ready_flag)
	{
		err = sps30_read_data_ready(&data_ready_flag);
		if (err) {
			LOG_ERR("Error reading SPS30 data ready status flag: "
				"%d", err);
			return err;
		}

		sensirion_i2c_hal_sleep_usec(100000);
	}

	err = sps30_read_measurement(measurement);
	if (err) {
		LOG_ERR("Error reading SPS30 measurement: %d", err);
		return err;
	}

	LOG_INF("sps30: "
		"PM1.0=%f (μg/m³); PM2.5=%f (μg/m³); "
		"PM4.0=%f (μg/m³); PM10.0=%f (μg/m³); "
		"NC0.5=%f (#/cm³); NC1.0=%f (#/cm³); "
		"NC2.5=%f (#/cm³); NC4.5=%f (#/cm³); "
		"NC10.0=%f (#/cm³); Typical Particle Size=%f (μm)",
		measurement->mc_1p0, measurement->mc_2p5, measurement->mc_4p0,
		measurement->mc_10p0, measurement->nc_0p5, measurement->nc_1p0,
		measurement->nc_2p5, measurement->nc_4p0, measurement->nc_10p0,
		measurement->typical_particle_size);

	err = sps30_stop_measurement();
	if (err) {
		LOG_ERR("Error exiting SPS30 measurement mode (error: "
			"%d)", err);
		return err;
	} else {
		LOG_DBG("SPS30 exited measurement mode");
	}
			
	return err;
}

int sps30_clean_fan(void)
{
	int16_t err;

	err = sps30_start_measurement();
	if (err) {
		LOG_ERR("Error entering SPS30 measurement mode (error: %d)",
			err);
		return err;
	}

	LOG_INF("Cleaning SPS30 fan (~10s)");
	err = sps30_start_manual_fan_cleaning();
	if (err) {
		LOG_ERR("Error starting SPS30 manual fan clearing: %d", err);
		return err;
	}

	sensirion_i2c_hal_sleep_usec(10000000);

	err = sps30_stop_measurement();
	if (err) {
		LOG_ERR("Error exiting SPS30 measurement mode (error: "
			"%d)", err);
		return err;
	}
			
	return err;
}

int sps30_init()
{
	int16_t err;
	uint8_t fw_major;
	uint8_t fw_minor;
	char serial_number[SPS30_MAX_SERIAL_LEN];

	LOG_DBG("Initializing SPS30 particulate matter sensor");

	err = sps30_reset();
	if (err) {
		LOG_ERR("SPS30 sensor reset failed");
		return err;
		}
	sensirion_i2c_hal_sleep_usec(SPS30_RESET_DELAY_USEC);

	err = sps30_probe();
	if (err) {
		LOG_ERR("SPS30 sensor probing failed");
		return err;
	}

	err = sps30_read_firmware_version(&fw_major, &fw_minor);
	if (err) {
		LOG_ERR("Error reading SPS30 firmware version (error: %d)",
			err);
		return err;
	} else {
		LOG_DBG("SPS30 firmware version: %u.%u", fw_major, fw_minor);
	}

	err = sps30_get_serial(serial_number);
	if (err) {
		LOG_ERR("Error reading SPS30 serial number (error: %d)", err);
		return err;
	} else {
		LOG_DBG("SPS30 serial number: %s", serial_number);
	}

	return err;
}

void update_sensor_settings(void)
{
	int err;
	extern bool update_scd4x_temperature_offset;
	extern bool update_scd4x_altitude;
	extern bool update_scd4x_asc;
	extern bool update_sps30_cleaning_interval;

	if (update_scd4x_temperature_offset) {
		int32_t offset_m_deg_c = get_scd4x_temperature_offset_s();
		err = scd4x_set_temperature_offset(offset_m_deg_c);
		if (err) {
			LOG_ERR("Error setting SCD4x temperature offset (error:"
				" %d)", err);
		} else {
			LOG_INF("Wrote SCD4x temperature offset (%d m°C) to"
				" sensor", offset_m_deg_c);
			update_scd4x_temperature_offset = false;
		}
	}

	if (update_scd4x_altitude) {
		uint16_t altitude = get_scd4x_altitude_s();
		err = scd4x_set_sensor_altitude(altitude);
		if (err) {
			LOG_ERR("Error setting SCD4x altitude (error: %d)",
				err);
		} else {
			LOG_INF("Wrote SCD4x altitude (%d meters) to sensor",
				altitude);
			update_scd4x_altitude = false;
		}
	}

	if (update_scd4x_asc) {
		bool asc = get_scd4x_asc_s();
		err = scd4x_set_automatic_self_calibration(asc);
		if (err) {
			LOG_ERR("Error setting SCD4x automatic self-calibration"
				" (error: %d)", err);
		} else {
			LOG_INF("Wrote SCD4x automatic self-calibration setting"
				" (%d) to sensor", asc);
			update_scd4x_asc = false;
		}
	}

	if (update_sps30_cleaning_interval) {
		uint32_t interval = get_sps30_cleaning_interval_s();
		err = sps30_set_fan_auto_cleaning_interval(interval);
		if (err) {
			LOG_ERR("Error setting SPS30 automatic fan cleaning"
				" interval (error: %d)", err);
		} else {
			LOG_INF("Wrote SPS30 automatic fan cleaning interval"
				" (%d seconds) to sensor", interval);
			update_sps30_cleaning_interval = false;
		}
	}
}

void service_rpc_requests(void)
{
	extern bool request_sps30_manual_fan_cleaning;

	if (request_sps30_manual_fan_cleaning) {
		sps30_clean_fan();

		request_sps30_manual_fan_cleaning = false;
	}
}

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_work_sensor_read(void) {
	int err;
	struct sensor_value bme280_tem, bme280_pre, bme280_hum;
	struct scd4x_measurement scd4x_meas;
	struct sensor_value scd4x_co2, scd4x_tem, scd4x_hum;
	struct sps30_measurement sps30_meas;
	struct sensor_value sps30_mc_1p0, sps30_mc_2p5, sps30_mc_4p0,
		sps30_mc_10p0, sps30_nc_0p5, sps30_nc_1p0, sps30_nc_2p5,
		sps30_nc_4p0, sps30_nc_10p0, sps30_typical_particle_size;
	char json_buf[256];
	
	/* Update any on-device sensor settings that have changed */
	update_sensor_settings();

	/* Complete any actions requested via RPC */
	service_rpc_requests();

	LOG_DBG("Collecting sensor measurements");

	/* Read the weather sensor */
	err = bme280_read(&bme280_tem, &bme280_pre, &bme280_hum);
	if (err) {
		return;
	}

	/* Read the CO₂ sensor */
	err = scd4x_read(&scd4x_meas);
	if (err) {
		return;
	} else {
		scd4x_co2.val1 = scd4x_meas.co2;
		scd4x_co2.val2 = 0;
		sensor_value_from_double(&scd4x_tem, scd4x_meas.temperature);
		sensor_value_from_double(&scd4x_hum, scd4x_meas.humidity);
	}

	/* Read the PM sensor */
	err = sps30_read(&sps30_meas);
	if (err) {
		return;
	} else {
		sensor_value_from_double(&sps30_mc_1p0, sps30_meas.mc_1p0);
		sensor_value_from_double(&sps30_mc_2p5, sps30_meas.mc_2p5);
		sensor_value_from_double(&sps30_mc_4p0, sps30_meas.mc_4p0);
		sensor_value_from_double(&sps30_mc_10p0, sps30_meas.mc_10p0);
		sensor_value_from_double(&sps30_nc_0p5, sps30_meas.nc_0p5);
		sensor_value_from_double(&sps30_nc_1p0, sps30_meas.nc_1p0);
		sensor_value_from_double(&sps30_nc_2p5, sps30_meas.nc_2p5);
		sensor_value_from_double(&sps30_nc_4p0, sps30_meas.nc_4p0);
		sensor_value_from_double(&sps30_nc_10p0, sps30_meas.nc_10p0);
		sensor_value_from_double(&sps30_typical_particle_size,
			sps30_meas.typical_particle_size);
	}

	/* Send sensor data to Golioth */
	LOG_DBG("Sending sensor data to Golioth");

	snprintk(json_buf, sizeof(json_buf), JSON_FMT,
		bme280_tem.val1, bme280_tem.val2,
		bme280_pre.val1, bme280_pre.val2,
		bme280_hum.val1, bme280_hum.val2,
		scd4x_co2.val1, scd4x_co2.val2,
		sps30_mc_1p0.val1, sps30_mc_1p0.val2,
		sps30_mc_2p5.val1, sps30_mc_2p5.val2, 
		sps30_mc_4p0.val1, sps30_mc_4p0.val2, 
		sps30_mc_10p0.val1, sps30_mc_10p0.val2, 
		sps30_nc_0p5.val1, sps30_nc_0p5.val2, 
		sps30_nc_1p0.val1, sps30_nc_1p0.val2, 
		sps30_nc_2p5.val1, sps30_nc_2p5.val2, 
		sps30_nc_4p0.val1, sps30_nc_4p0.val2, 
		sps30_nc_10p0.val1, sps30_nc_10p0.val2, 
		sps30_typical_particle_size.val1,
		sps30_typical_particle_size.val2);

	err = golioth_stream_push_cb(client, "sensor",
		GOLIOTH_CONTENT_FORMAT_APP_JSON,
		json_buf, strlen(json_buf),
		async_error_handler, NULL);
	if (err) {
		LOG_ERR("Failed to send sensor data to Golioth: %d", err);
	}

	/* Update any on-device sensor settings that have changed */
	update_sensor_settings();

	/* Complete any actions requested via RPC */
	service_rpc_requests();
}

void app_work_init(struct golioth_client* work_client) {
	int err;

	client = work_client;

	/* Initialize weather sensor */
	LOG_DBG("Initializing BME280 weather sensor");
	if (weather_sensor == NULL) {
		/* No such node, or the node does not have status "okay". */
		LOG_ERR("Device \"%s\" not found", weather_sensor->name);
		return;
	}

	if (!device_is_ready(weather_sensor)) {
		LOG_ERR("Device \"%s\" is not ready", weather_sensor->name);
		return;
	}

	/* Initialize CO₂ sensor */
	err = scd4x_init();
	if (err) {
		return;
	}

	/* Initialize PM sensor */
	err = sps30_init();
	if (err) {
		return;
	}
}
