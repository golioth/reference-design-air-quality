/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_scd4x, LOG_LEVEL_DBG);

#include <zephyr/drivers/sensor.h>

#include "sensor_scd4x.h"
#include "app_settings.h"
#include "sensirion_common.h"
#include "sensirion_i2c_hal.h"
#include "scd4x_i2c.h"

#define SCD4X_MUTEX_TIMEOUT 6000

K_MUTEX_DEFINE(scd4x_mutex);

int scd4x_sensor_init(void)
{
	int err;
	uint16_t serial_0, serial_1, serial_2;
	struct scd4x_sensor_measurement measurement;

	LOG_DBG("Initializing SCD4x CO₂ sensor");

	err = k_mutex_lock(&scd4x_mutex, K_MSEC(SCD4X_MUTEX_TIMEOUT));
	if (err) {
		LOG_ERR("Error locking SCD4x mutex (lock count: %u): %d", scd4x_mutex.lock_count,
			err);
		return err;
	}

	/* After VDD reaches 2.25V, SCD4x needs 1000 ms to enter idle state */
	/* Sleep the full 1000 ms here just to be safe */
	sensirion_i2c_hal_sleep_usec(SCD4X_POWER_UP_DELAY_USEC);

	/* Wake up and reinitialize SCD4x to the default state */
	err = scd4x_wake_up();
	if (err) {
		LOG_ERR("Error %d: SCD4x wakeup failed", err);
		k_mutex_unlock(&scd4x_mutex);
		return err;
	}

	err = scd4x_stop_periodic_measurement();
	if (err) {
		LOG_ERR("Error %d: SCD4x stop periodic measurement failed", err);
		k_mutex_unlock(&scd4x_mutex);
		return err;
	}

	err = scd4x_reinit();
	if (err) {
		LOG_ERR("Error %d: SCD4x reinit failed", err);
		k_mutex_unlock(&scd4x_mutex);
		return err;
	}

	/* Since SCD4x doesn't ack wake_up, read SCD4x serial number instead */
	err = scd4x_get_serial_number(&serial_0, &serial_1, &serial_2);
	if (err) {
		LOG_ERR("Cannot read SCD4x serial number (error: %d)", err);
		k_mutex_unlock(&scd4x_mutex);
		return err;
	}

	LOG_DBG("SCD4x serial number: 0x%04x%04x%04x", serial_0, serial_1, serial_2);

	/* According to the datasheet, the first reading obtained after waking
	 * up the sensor must be discarded, so do a throw-away measurement now
	 */
	err = scd4x_sensor_read(&measurement);

	k_mutex_unlock(&scd4x_mutex);

	return err;
}

int scd4x_sensor_read(struct scd4x_sensor_measurement *measurement)
{
	int err;
	bool data_ready_flag = false;
	uint16_t co2_ppm;
	int32_t temperature_m_deg_c, humidity_m_percent_rh;
	float temperature_deg_c, humidity_percent_rh;

	LOG_DBG("Reading SCD4x CO₂ sensor (~%d seconds)",
		(SCD4X_MEASUREMENT_DURATION_USEC / 1000000));

	err = k_mutex_lock(&scd4x_mutex, K_MSEC(SCD4X_MUTEX_TIMEOUT));
	if (err) {
		LOG_ERR("Error locking SCD4x mutex (lock count: %u): %d", scd4x_mutex.lock_count,
			err);
		return err;
	}

	/* Request a single-shot measurement */
	err = scd4x_measure_single_shot();
	if (err) {
		LOG_ERR("Error entering SCD4x single-shot measurement mode (error: %d)", err);
		k_mutex_unlock(&scd4x_mutex);
		return err;
	}

	/* Sleep while measurement is being taken */
	sensirion_i2c_hal_sleep_usec(SCD4X_MEASUREMENT_DURATION_USEC);

	/* Poll the sensor every 0.1s waiting for the data ready status */
	while (!data_ready_flag) {
		err = scd4x_get_data_ready_flag(&data_ready_flag);
		if (err) {
			LOG_ERR("Error reading SCD4x data ready status flag: %d", err);
			k_mutex_unlock(&scd4x_mutex);
			return err;
		}

		sensirion_i2c_hal_sleep_usec(100000);
	}

	/* Read the single-shot measurement */
	err = scd4x_read_measurement(&co2_ppm, &temperature_m_deg_c, &humidity_m_percent_rh);
	if (err) {
		LOG_ERR("Error reading SCD4x measurement: %d", err);
		k_mutex_unlock(&scd4x_mutex);
		return err;
	} else if (co2_ppm == 0) {
		LOG_ERR("Invalid SCD4x measurement sample");
		k_mutex_unlock(&scd4x_mutex);
		return err;
	}

	temperature_deg_c = temperature_m_deg_c / 1000.0;
	humidity_percent_rh = humidity_m_percent_rh / 1000.0;

	measurement->co2 = co2_ppm;
	sensor_value_from_double(&measurement->temperature, temperature_deg_c);
	sensor_value_from_double(&measurement->humidity, humidity_percent_rh);

	k_mutex_unlock(&scd4x_mutex);

	return err;
}

void scd4x_log_measurements(struct scd4x_sensor_measurement *measurement)
{
	LOG_DBG("scd4x: CO₂=%u ppm, Temperature=%.2lf °C, Humidity=%.2lf %%RH", measurement->co2,
		sensor_value_to_double(&measurement->temperature),
		sensor_value_to_double(&measurement->humidity));
}

int scd4x_sensor_set_temperature_offset(int32_t t_offset_m_deg_c)
{
	int err;

	err = k_mutex_lock(&scd4x_mutex, K_MSEC(SCD4X_MUTEX_TIMEOUT));
	if (err) {
		LOG_ERR("Error locking SCD4x mutex (lock count: %u): %d", scd4x_mutex.lock_count,
			err);
		return err;
	}

	err = scd4x_set_temperature_offset(t_offset_m_deg_c);
	if (err) {
		LOG_ERR("Error setting SCD4x temperature offset (error: %d)", err);
	} else {
		LOG_INF("Set SCD4x temperature offset setting to %d m°C", t_offset_m_deg_c);
	}

	k_mutex_unlock(&scd4x_mutex);

	return err;
}

int scd4x_sensor_set_sensor_altitude(int16_t sensor_altitude)
{
	int err;

	err = k_mutex_lock(&scd4x_mutex, K_MSEC(SCD4X_MUTEX_TIMEOUT));
	if (err) {
		LOG_ERR("Error locking SCD4x mutex (lock count: %u): %d", scd4x_mutex.lock_count,
			err);
		return err;
	}

	err = scd4x_set_sensor_altitude(sensor_altitude);
	if (err) {
		LOG_ERR("Error setting SCD4x altitude (error: %d)", err);
	} else {
		LOG_INF("Set SCD4x altitude setting to %d meters", sensor_altitude);
	}

	k_mutex_unlock(&scd4x_mutex);

	return err;
}

int scd4x_sensor_set_automatic_self_calibration(bool asc_enabled)
{
	int err;

	err = k_mutex_lock(&scd4x_mutex, K_MSEC(SCD4X_MUTEX_TIMEOUT));
	if (err) {
		LOG_ERR("Error locking SCD4x mutex (lock count: %u): %d", scd4x_mutex.lock_count,
			err);
		return err;
	}

	err = scd4x_set_automatic_self_calibration(asc_enabled);
	if (err) {
		LOG_ERR("Error setting SCD4x automatic self-calibration (error: %d)", err);
	} else {
		if (asc_enabled) {
			LOG_INF("Enabled SCD4x automatic self-calibration");
		} else {
			LOG_INF("Disabled SCD4x automatic self-calibration");
		}
	}

	k_mutex_unlock(&scd4x_mutex);

	return err;
}
