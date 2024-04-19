/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_sps30, LOG_LEVEL_DBG);

#include <zephyr/drivers/sensor.h>

#include "sensor_sps30.h"
#include "app_settings.h"
#include "sensirion_common.h"
#include "sensirion_i2c_hal.h"
#include "sps30.h"

#define SPS30_MUTEX_TIMEOUT 60000

K_MUTEX_DEFINE(sps30_mutex);

int sps30_sensor_init(void)
{
	int err;
	uint8_t fw_major;
	uint8_t fw_minor;
	char serial_number[SPS30_MAX_SERIAL_LEN];

	LOG_DBG("Initializing SPS30 PM sensor (~30 seconds)");

	err = k_mutex_lock(&sps30_mutex, K_MSEC(SPS30_MUTEX_TIMEOUT));
	if (err) {
		LOG_ERR("Error locking SPS30 mutex (lock count: %u): %d", sps30_mutex.lock_count,
			err);
		return err;
	}

	err = sps30_reset();
	if (err) {
		LOG_ERR("SPS30 sensor reset failed");
		k_mutex_unlock(&sps30_mutex);
		return err;
	}
	sensirion_i2c_hal_sleep_usec(SPS30_RESET_DELAY_USEC);

	uint8_t tries = 10;
	while (tries != 0) {
		LOG_DBG("Probing for SPS30 sensor");
		err = sps30_probe();
		if (err == 0) {
			break;
		}
		tries--;
		/* Sleep 1s and try again */
		sensirion_i2c_hal_sleep_usec(1000000);
	}

	if (err) {
		LOG_ERR("SPS30 sensor probing failed");
		k_mutex_unlock(&sps30_mutex);
		return err;
	}

	err = sps30_read_firmware_version(&fw_major, &fw_minor);
	if (err) {
		LOG_ERR("Error reading SPS30 firmware version (error: %d)", err);
		k_mutex_unlock(&sps30_mutex);
		return err;
	} else {
		LOG_DBG("SPS30 firmware version: %u.%u", fw_major, fw_minor);
	}

	err = sps30_get_serial(serial_number);
	if (err) {
		LOG_ERR("Error reading SPS30 serial number (error: %d)", err);
		k_mutex_unlock(&sps30_mutex);
		return err;
	} else {
		LOG_DBG("SPS30 serial number: %s", serial_number);
	}

	err = sps30_start_measurement();
	if (err) {
		LOG_ERR("Error entering SPS30 measurement mode (error: %d)", err);
		k_mutex_unlock(&sps30_mutex);
		return err;
	}

	/* Sleep 30s for the measurements to stabilize */
	sensirion_i2c_hal_sleep_usec(30000000);

	k_mutex_unlock(&sps30_mutex);

	return err;
}

int sps30_sensor_read(struct sps30_sensor_measurement *measurement)
{
	int err;
	struct sps30_measurement sps30_meas;
	struct sps30_measurement sps30_meas_avg = {0};

	/* Get the number of samples to average from Golioth settings */
	uint32_t samples = get_sps30_samples_per_measurement_s();

	LOG_DBG("Reading SPS30 PM sensor (averaging %u samples over ~%u seconds)", samples,
		samples);

	int count = 0;
	while (count < samples) {
		err = k_mutex_lock(&sps30_mutex, K_MSEC(SPS30_MUTEX_TIMEOUT));
		if (err) {
			LOG_ERR("Error locking SPS30 mutex (lock count: %u): %d",
				sps30_mutex.lock_count, err);
			return err;
		}

		/* Poll the sensor every 0.1s waiting for the data ready status */
		/* Data should be ready every 1s */
		int16_t data_ready_flag = 0;
		int tries = 100;

		while (tries > 0) {
			err = sps30_read_data_ready(&data_ready_flag);
			if (err) {
				LOG_ERR("Error reading SPS30 data ready status flag: %d", err);
				k_mutex_unlock(&sps30_mutex);
				return err;
			}

			if (data_ready_flag)
				break;

			/* Sleep 0.1s and try again */
			sensirion_i2c_hal_sleep_usec(100000);
			tries--;
		}

		if (tries == 0) {
			LOG_ERR("SPS30 data ready flag was never asserted");
			k_mutex_unlock(&sps30_mutex);
			return -1;
		}

		err = sps30_read_measurement(&sps30_meas);
		if (err) {
			LOG_ERR("Error reading SPS30 measurement: %d", err);
			k_mutex_unlock(&sps30_mutex);
			return err;
		}

		k_mutex_unlock(&sps30_mutex);

		sps30_meas_avg.mc_1p0 += sps30_meas.mc_1p0;
		sps30_meas_avg.mc_2p5 += sps30_meas.mc_2p5;
		sps30_meas_avg.mc_4p0 += sps30_meas.mc_4p0;
		sps30_meas_avg.mc_10p0 += sps30_meas.mc_10p0;
		sps30_meas_avg.nc_0p5 += sps30_meas.nc_0p5;
		sps30_meas_avg.nc_1p0 += sps30_meas.nc_1p0;
		sps30_meas_avg.nc_2p5 += sps30_meas.nc_2p5;
		sps30_meas_avg.nc_4p0 += sps30_meas.nc_4p0;
		sps30_meas_avg.nc_10p0 += sps30_meas.nc_10p0;
		sps30_meas_avg.typical_particle_size += sps30_meas.typical_particle_size;

		/* Wait for a new sample to be ready */
		sensirion_i2c_hal_sleep_usec(SPS30_MEASUREMENT_DURATION_USEC);

		count++;
	}

	sps30_meas_avg.mc_1p0 = sps30_meas_avg.mc_1p0 / samples;
	sps30_meas_avg.mc_2p5 = sps30_meas_avg.mc_2p5 / samples;
	sps30_meas_avg.mc_4p0 = sps30_meas_avg.mc_4p0 / samples;
	sps30_meas_avg.mc_10p0 = sps30_meas_avg.mc_10p0 / samples;
	sps30_meas_avg.nc_0p5 = sps30_meas_avg.nc_0p5 / samples;
	sps30_meas_avg.nc_1p0 = sps30_meas_avg.nc_1p0 / samples;
	sps30_meas_avg.nc_2p5 = sps30_meas_avg.nc_2p5 / samples;
	sps30_meas_avg.nc_4p0 = sps30_meas_avg.nc_4p0 / samples;
	sps30_meas_avg.nc_10p0 = sps30_meas_avg.nc_10p0 / samples;
	sps30_meas_avg.typical_particle_size = sps30_meas_avg.typical_particle_size / samples;

	sensor_value_from_double(&measurement->mc_1p0, sps30_meas_avg.mc_1p0);
	sensor_value_from_double(&measurement->mc_2p5, sps30_meas_avg.mc_2p5);
	sensor_value_from_double(&measurement->mc_4p0, sps30_meas_avg.mc_4p0);
	sensor_value_from_double(&measurement->mc_10p0, sps30_meas_avg.mc_10p0);
	sensor_value_from_double(&measurement->nc_0p5, sps30_meas_avg.nc_0p5);
	sensor_value_from_double(&measurement->nc_1p0, sps30_meas_avg.nc_1p0);
	sensor_value_from_double(&measurement->nc_2p5, sps30_meas_avg.nc_2p5);
	sensor_value_from_double(&measurement->nc_4p0, sps30_meas_avg.nc_4p0);
	sensor_value_from_double(&measurement->nc_10p0, sps30_meas_avg.nc_10p0);
	sensor_value_from_double(&measurement->typical_particle_size,
				 sps30_meas_avg.typical_particle_size);

	return err;
}

void sps30_log_measurements(struct sps30_sensor_measurement *measurement)
{
	LOG_DBG("sps30: "
		"PM1.0=%f μg/m³, PM2.5=%f μg/m³, "
		"PM4.0=%f μg/m³, PM10.0=%f μg/m³, "
		"NC0.5=%f #/cm³, NC1.0=%f #/cm³, "
		"NC2.5=%f #/cm³, NC4.0=%f #/cm³, "
		"NC10.0=%f #/cm³, Typical Particle Size=%f μm",
		sensor_value_to_double(&measurement->mc_1p0),
		sensor_value_to_double(&measurement->mc_2p5),
		sensor_value_to_double(&measurement->mc_4p0),
		sensor_value_to_double(&measurement->mc_10p0),
		sensor_value_to_double(&measurement->nc_0p5),
		sensor_value_to_double(&measurement->nc_1p0),
		sensor_value_to_double(&measurement->nc_2p5),
		sensor_value_to_double(&measurement->nc_4p0),
		sensor_value_to_double(&measurement->nc_10p0),
		sensor_value_to_double(&measurement->typical_particle_size));
}

int sps30_sensor_set_fan_auto_cleaning_interval(uint32_t interval_seconds)
{
	int err;

	err = k_mutex_lock(&sps30_mutex, K_MSEC(SPS30_MUTEX_TIMEOUT));
	if (err) {
		LOG_ERR("Error locking SPS30 mutex (lock count: %u): %d", sps30_mutex.lock_count,
			err);
		return err;
	}

	err = sps30_set_fan_auto_cleaning_interval(interval_seconds);
	if (err) {
		LOG_ERR("Error setting SPS30 automatic fan cleaning interval (error: %d)", err);
	} else {
		LOG_INF("Set SPS30 automatic fan cleaning interval to %d second(s)",
			interval_seconds);
	}

	k_mutex_unlock(&sps30_mutex);

	return err;
}

int sps30_sensor_clean_fan(void)
{
	int err;

	LOG_INF("Cleaning SPS30 PM sensor fan (~10 seconds)");

	err = k_mutex_lock(&sps30_mutex, K_MSEC(SPS30_MUTEX_TIMEOUT));
	if (err) {
		LOG_ERR("Error locking SPS30 mutex (lock count: %u): %d", sps30_mutex.lock_count,
			err);
		return err;
	}

	err = sps30_start_manual_fan_cleaning();
	if (err) {
		LOG_ERR("Error starting SPS30 manual fan clearing: %d", err);
		k_mutex_unlock(&sps30_mutex);
		return err;
	}

	/* Sleep 10s for the fan cleaning to finish */
	sensirion_i2c_hal_sleep_usec(10000000);

	k_mutex_unlock(&sps30_mutex);

	return err;
}
