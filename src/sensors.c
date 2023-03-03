/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensors, LOG_LEVEL_DBG);
#include <zephyr/drivers/sensor.h>

#include "sensors.h"
#include "app_settings.h"
#include "sensirion_common.h"
#include "sensirion_i2c_hal.h"
#include "scd4x_i2c.h"
#include "sps30.h"

K_MUTEX_DEFINE(bme280_mutex);
K_MUTEX_DEFINE(scd4x_mutex);
K_MUTEX_DEFINE(sps30_mutex);

const struct device *bme280_dev = DEVICE_DT_GET(DT_NODELABEL(bme280));

int bme280_sensor_init(void)
{
	LOG_DBG("Requesting BME280 mutex lock");
	k_mutex_lock(&bme280_mutex, K_FOREVER);
	LOG_DBG("Locked BME280 mutex");

	LOG_INF("Initializing BME280 weather sensor");

	if (bme280_dev == NULL) {
		/* No such node, or the node does not have status "okay". */
		LOG_ERR("Device \"%s\" not found", bme280_dev->name);
		k_mutex_unlock(&bme280_mutex);
		LOG_DBG("Unlocked BME280 mutex");
		return -ENODEV;
	}

	if (!device_is_ready(bme280_dev)) {
		LOG_ERR("Device \"%s\" is not ready", bme280_dev->name);
		k_mutex_unlock(&bme280_mutex);
		LOG_DBG("Unlocked BME280 mutex");
		return -ENODEV;
	}

	k_mutex_unlock(&bme280_mutex);
	LOG_DBG("Unlocked BME280 mutex");

	return 0;
}

int bme280_sensor_read(struct bme280_sensor_measurement *measurement)
{
	int err;

	LOG_DBG("Requesting BME280 mutex lock");
	k_mutex_lock(&bme280_mutex, K_FOREVER);
	LOG_DBG("Locked BME280 mutex");

	LOG_INF("Reading BME280 weather sensor");

	err = sensor_sample_fetch(bme280_dev);
	if (err) {
		LOG_ERR("Error fetching weather sensor sample: %d", err);
		k_mutex_unlock(&bme280_mutex);
		LOG_DBG("Unlocked BME280 mutex");
		return err;
	}

	sensor_channel_get(bme280_dev, SENSOR_CHAN_AMBIENT_TEMP,
		&measurement->temperature);
	sensor_channel_get(bme280_dev, SENSOR_CHAN_PRESS,
		&measurement->pressure);
	sensor_channel_get(bme280_dev, SENSOR_CHAN_HUMIDITY,
		&measurement->humidity);

	LOG_INF("bme280: Temperature=%d.%d °C; Pressure=%d.%d kPa;"
		" Humidity=%d.%d %%RH",
		measurement->temperature.val1, measurement->temperature.val2,
		measurement->pressure.val1, measurement->pressure.val2,
		measurement->humidity.val1, measurement->humidity.val2);

	k_mutex_unlock(&bme280_mutex);
	LOG_DBG("Unlocked BME280 mutex");

	return err;
}

int scd4x_sensor_init(void)
{
	int err;
	uint16_t serial_0, serial_1, serial_2;
	struct scd4x_sensor_measurement measurement;

	LOG_DBG("Requesting SCD4x mutex lock");
	k_mutex_lock(&scd4x_mutex, K_FOREVER);
	LOG_DBG("Locked SCD4x mutex");

	LOG_INF("Initializing SCD4x CO₂ sensor");

	/* After VDD reaches 2.25V, SCD4x needs 1000 ms to enter idle state */
	/* Sleep the full 1000 ms here just to be safe */
	sensirion_i2c_hal_sleep_usec(SCD4X_POWER_UP_DELAY_USEC);

	/* Wake up and reinitialize SCD4x to the default state */
	err = scd4x_wake_up();
	if (err) {
		LOG_ERR("Error %d: SCD4x wakeup failed", err);
		k_mutex_unlock(&scd4x_mutex);
		LOG_DBG("Unlocked SCD4x mutex");
		return err;
	}

	err = scd4x_stop_periodic_measurement();
	if (err) {
		LOG_ERR("Error %d: SCD4x stop periodic measurement failed",
			err);
		k_mutex_unlock(&scd4x_mutex);
		LOG_DBG("Unlocked SCD4x mutex");
		return err;
	}

	err = scd4x_reinit();
	if (err) {
		LOG_ERR("Error %d: SCD4x reinit failed", err);
		k_mutex_unlock(&scd4x_mutex);
		LOG_DBG("Unlocked SCD4x mutex");
		return err;
	}

	/* Since SCD4x doesn't ack wake_up, read SCD4x serial number instead */
	err = scd4x_get_serial_number(&serial_0, &serial_1, &serial_2);
	if (err) {
		LOG_ERR("Cannot read SCD4x serial number (error: %d)", err);
		k_mutex_unlock(&scd4x_mutex);
		LOG_DBG("Unlocked SCD4x mutex");
		return err;
	} else {
		LOG_INF("SCD4x serial number: 0x%04x%04x%04x", serial_0,
			serial_1, serial_2);
	}

	k_mutex_unlock(&scd4x_mutex);
	LOG_DBG("Unlocked SCD4x mutex");

	/* According to the datasheet, the first reading obtained after waking
	up the sensor must be discarded, so do a throw-away measurement now */
	err = scd4x_sensor_read(&measurement);

	return err;
}

int scd4x_sensor_read(struct scd4x_sensor_measurement *measurement)
{
	int16_t err;
	bool data_ready_flag = false;
	uint16_t co2_ppm;
	int32_t temperature_m_deg_c, humidity_m_percent_rh;

	LOG_DBG("Requesting SCD4x mutex lock");
	k_mutex_lock(&scd4x_mutex, K_FOREVER);
	LOG_DBG("Locked SCD4x mutex");

	LOG_INF("Reading SCD4x CO₂ sensor (~5 seconds)");

	/* Request a single-shot measurement */
	err = scd4x_measure_single_shot();
	if (err) {
		LOG_ERR("Error entering SCD4x single-shot measurement mode"
			" (error: %d)", err);
		k_mutex_unlock(&scd4x_mutex);
		LOG_DBG("Unlocked SCD4x mutex");
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
			k_mutex_unlock(&scd4x_mutex);
			LOG_DBG("Unlocked SCD4x mutex");
			return err;
		}

		sensirion_i2c_hal_sleep_usec(100000);
	}

	/* Read the single-shot measurement */
	err = scd4x_read_measurement(&co2_ppm, &temperature_m_deg_c,
		&humidity_m_percent_rh);
	if (err) {
		LOG_ERR("Error reading SCD4x measurement: %d", err);
		k_mutex_unlock(&scd4x_mutex);
		LOG_DBG("Unlocked SCD4x mutex");
		return err;
	} else if (co2_ppm == 0) {
		LOG_ERR("Invalid SCD4x measurement sample");
		k_mutex_unlock(&scd4x_mutex);
		LOG_DBG("Unlocked SCD4x mutex");
		return err;
	}

	measurement->co2 = co2_ppm;
	sensor_value_from_double(&measurement->temperature,
		temperature_m_deg_c / 1000.0);
	sensor_value_from_double(&measurement->humidity,
		humidity_m_percent_rh / 1000.0);

	LOG_INF("scd4x: CO₂=%u ppm; Temperature=%d.%d °C;"
		" Humidity=%d.%d %%RH", measurement->co2,
		measurement->temperature.val1, measurement->temperature.val2,
		measurement->humidity.val1, measurement->humidity.val2);

	k_mutex_unlock(&scd4x_mutex);

	return err;
}

int scd4x_sensor_set_temperature_offset(int32_t t_offset_m_deg_c)
{
	int err;

	LOG_DBG("Requesting SCD4x mutex lock");
	k_mutex_lock(&scd4x_mutex, K_FOREVER);
	LOG_DBG("Locked SCD4x mutex");

	err = scd4x_set_temperature_offset(t_offset_m_deg_c);
	if (err) {
		LOG_ERR("Error setting SCD4x temperature offset (error: %d)",
			err);
	} else {
		LOG_INF("Set SCD4x temperature offset setting to %d m°C",
			t_offset_m_deg_c);
	}

	k_mutex_unlock(&scd4x_mutex);
	LOG_DBG("Unlocked SCD4x mutex");

	return err;
}

int scd4x_sensor_set_sensor_altitude(int16_t sensor_altitude)
{
	int err;

	LOG_DBG("Requesting SCD4x mutex lock");
	k_mutex_lock(&scd4x_mutex, K_FOREVER);
	LOG_DBG("Locked SCD4x mutex");

	err = scd4x_set_sensor_altitude(sensor_altitude);
	if (err) {
		LOG_ERR("Error setting SCD4x altitude (error: %d)",
			err);
	} else {
		LOG_INF("Set SCD4x altitude setting to %d meters",
			sensor_altitude);
	}

	k_mutex_unlock(&scd4x_mutex);
	LOG_DBG("Unlocked SCD4x mutex");

	return err;
}

int scd4x_sensor_set_automatic_self_calibration(bool asc_enabled)
{
	int err;

	LOG_DBG("Requesting SCD4x mutex lock");
	k_mutex_lock(&scd4x_mutex, K_FOREVER);
	LOG_DBG("Locked SCD4x mutex");

	err = scd4x_set_automatic_self_calibration(asc_enabled);
	if (err) {
		LOG_ERR("Error setting SCD4x automatic self-calibration"
			" (error: %d)", err);
	} else {
		if (asc_enabled) {
			LOG_INF("Enabled SCD4x automatic self-calibration");
		} else {
			LOG_INF("Disabled SCD4x automatic self-calibration");
		}
	}

	k_mutex_unlock(&scd4x_mutex);
	LOG_DBG("Unlocked SCD4x mutex");

	return err;
}

int sps30_sensor_init(void)
{
	int16_t err;
	uint8_t fw_major;
	uint8_t fw_minor;
	char serial_number[SPS30_MAX_SERIAL_LEN];

	LOG_DBG("Requesting SPS30 mutex lock");
	k_mutex_lock(&sps30_mutex, K_FOREVER);
	LOG_DBG("Locked SPS30 mutex");

	LOG_INF("Initializing SPS30 PM sensor");

	err = sps30_reset();
	if (err) {
		LOG_ERR("SPS30 sensor reset failed");
		k_mutex_unlock(&sps30_mutex);
		LOG_DBG("Unlocked SPS30 mutex");
		return err;
		}
	sensirion_i2c_hal_sleep_usec(SPS30_RESET_DELAY_USEC);

	err = sps30_probe();
	if (err) {
		LOG_ERR("SPS30 sensor probing failed");
		k_mutex_unlock(&sps30_mutex);
		LOG_DBG("Unlocked SPS30 mutex");
		return err;
	}

	err = sps30_read_firmware_version(&fw_major, &fw_minor);
	if (err) {
		LOG_ERR("Error reading SPS30 firmware version (error: %d)",
			err);
		k_mutex_unlock(&sps30_mutex);
		LOG_DBG("Unlocked SPS30 mutex");
		return err;
	} else {
		LOG_DBG("SPS30 firmware version: %u.%u", fw_major, fw_minor);
	}

	err = sps30_get_serial(serial_number);
	if (err) {
		LOG_ERR("Error reading SPS30 serial number (error: %d)", err);
		k_mutex_unlock(&sps30_mutex);
		LOG_DBG("Unlocked SPS30 mutex");
		return err;
	} else {
		LOG_DBG("SPS30 serial number: %s", serial_number);
	}

	k_mutex_unlock(&sps30_mutex);
	LOG_DBG("Unlocked SPS30 mutex");

	return err;
}

int sps30_sensor_read(struct sps30_sensor_measurement *measurement)
{
	int16_t err;
	int16_t data_ready_flag = 0;
	struct sps30_measurement sps30_meas;

	LOG_DBG("Requesting SPS30 mutex lock");
	k_mutex_lock(&sps30_mutex, K_FOREVER);
	LOG_DBG("Locked SPS30 mutex");

	LOG_INF("Reading SPS30 PM sensor (~1 second)");

	err = sps30_start_measurement();
	if (err) {
		LOG_ERR("Error entering SPS30 measurement mode (error: %d)",
			err);
		k_mutex_unlock(&sps30_mutex);
		LOG_DBG("Unlocked SPS30 mutex");
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
			k_mutex_unlock(&sps30_mutex);
			LOG_DBG("Unlocked SPS30 mutex");
			return err;
		}

		sensirion_i2c_hal_sleep_usec(100000);
	}

	err = sps30_read_measurement(&sps30_meas);
	if (err) {
		LOG_ERR("Error reading SPS30 measurement: %d", err);
		k_mutex_unlock(&sps30_mutex);
		LOG_DBG("Unlocked SPS30 mutex");
		return err;
	}

	sensor_value_from_double(&measurement->mc_1p0, sps30_meas.mc_1p0);
	sensor_value_from_double(&measurement->mc_2p5, sps30_meas.mc_2p5);
	sensor_value_from_double(&measurement->mc_4p0, sps30_meas.mc_4p0);
	sensor_value_from_double(&measurement->mc_10p0, sps30_meas.mc_10p0);
	sensor_value_from_double(&measurement->nc_0p5, sps30_meas.nc_0p5);
	sensor_value_from_double(&measurement->nc_1p0, sps30_meas.nc_1p0);
	sensor_value_from_double(&measurement->nc_2p5, sps30_meas.nc_2p5);
	sensor_value_from_double(&measurement->nc_4p0, sps30_meas.nc_4p0);
	sensor_value_from_double(&measurement->nc_10p0, sps30_meas.nc_10p0);
	sensor_value_from_double(&measurement->typical_particle_size,
		sps30_meas.typical_particle_size);

	LOG_INF("sps30: "
		"PM1.0=%d.%d μg/m³; PM2.5=%d.%d μg/m³; "
		"PM4.0=%d.%d μg/m³; PM10.0=%d.%d μg/m³; "
		"NC0.5=%d.%d #/cm³; NC1.0=%d.%d #/cm³; "
		"NC2.5=%d.%d #/cm³; NC4.5=%d.%d #/cm³; "
		"NC10.0=%d.%d #/cm³; Typical Particle Size=%d.%d μm",
		measurement->mc_1p0.val1, measurement->mc_1p0.val2,
		measurement->mc_2p5.val1, measurement->mc_2p5.val2,
		measurement->mc_4p0.val1, measurement->mc_4p0.val2,
		measurement->mc_10p0.val1, measurement->mc_10p0.val2,
		measurement->nc_0p5.val1, measurement->nc_0p5.val2,
		measurement->nc_1p0.val1, measurement->nc_1p0.val2,
		measurement->nc_2p5.val1, measurement->nc_2p5.val2,
		measurement->nc_4p0.val1, measurement->nc_4p0.val2,
		measurement->nc_10p0.val1, measurement->nc_10p0.val2,
		measurement->typical_particle_size.val1,
			measurement->typical_particle_size.val2);

	err = sps30_stop_measurement();
	if (err) {
		LOG_ERR("Error exiting SPS30 measurement mode (error: "
			"%d)", err);
		k_mutex_unlock(&sps30_mutex);
		LOG_DBG("Unlocked SPS30 mutex");
		return err;
	}

	k_mutex_unlock(&sps30_mutex);
	LOG_DBG("Unlocked SPS30 mutex");

	return err;
}

int sps30_sensor_set_fan_auto_cleaning_interval(uint32_t interval_seconds)
{
	int err;

	LOG_DBG("Requesting SPS30 mutex lock");
	k_mutex_lock(&sps30_mutex, K_FOREVER);
	LOG_DBG("Locked SPS30 mutex");

	err = sps30_set_fan_auto_cleaning_interval(interval_seconds);
	if (err) {
		LOG_ERR("Error setting SPS30 automatic fan cleaning"
			" interval (error: %d)", err);
	} else {
		LOG_INF("Set SPS30 automatic fan cleaning interval to %d"
			" second(s)", interval_seconds);
	}

	k_mutex_unlock(&sps30_mutex);
	LOG_DBG("Unlocked SPS30 mutex");

	return err;
}

int sps30_sensor_clean_fan(void)
{
	int16_t err;

	LOG_DBG("Requesting SPS30 mutex lock");
	k_mutex_lock(&sps30_mutex, K_FOREVER);
	LOG_DBG("Locked SPS30 mutex");

	LOG_INF("Cleaning SPS30 PM sensor fan (~10 seconds)");

	err = sps30_start_measurement();
	if (err) {
		LOG_ERR("Error entering SPS30 measurement mode (error: %d)",
			err);
		k_mutex_unlock(&sps30_mutex);
		LOG_DBG("Unlocked SPS30 mutex");
		return err;
	}

	err = sps30_start_manual_fan_cleaning();
	if (err) {
		LOG_ERR("Error starting SPS30 manual fan clearing: %d", err);
		k_mutex_unlock(&sps30_mutex);
		LOG_DBG("Unlocked SPS30 mutex");
		return err;
	}

	sensirion_i2c_hal_sleep_usec(10000000);

	err = sps30_stop_measurement();
	if (err) {
		LOG_ERR("Error exiting SPS30 measurement mode (error: "
			"%d)", err);
		k_mutex_unlock(&sps30_mutex);
		LOG_DBG("Unlocked SPS30 mutex");
		return err;
	}

	k_mutex_unlock(&sps30_mutex);
	LOG_DBG("Unlocked SPS30 mutex");

	return err;
}
