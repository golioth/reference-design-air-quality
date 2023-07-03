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
	LOG_INF("Initializing BME280 weather sensor");

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

	k_mutex_lock(&bme280_mutex, K_FOREVER);

	LOG_DBG("Reading BME280 weather sensor");

	err = sensor_sample_fetch(bme280_dev);
	if (err) {
		LOG_ERR("Error fetching weather sensor sample: %d", err);
		k_mutex_unlock(&bme280_mutex);
		return err;
	}

	sensor_channel_get(bme280_dev, SENSOR_CHAN_AMBIENT_TEMP,
		&measurement->temperature);
	sensor_channel_get(bme280_dev, SENSOR_CHAN_PRESS,
		&measurement->pressure);
	sensor_channel_get(bme280_dev, SENSOR_CHAN_HUMIDITY,
		&measurement->humidity);

	LOG_INF("bme280: Temperature=%f °C, Pressure=%f kPa,"
		" Humidity=%f %%RH",
		sensor_value_to_double(&measurement->temperature),
		sensor_value_to_double(&measurement->pressure),
		sensor_value_to_double(&measurement->humidity)
	);

	k_mutex_unlock(&bme280_mutex);

	return err;
}

int scd4x_sensor_init(void)
{
	int err;
	uint16_t serial_0, serial_1, serial_2;
	struct scd4x_sensor_measurement measurement;

	k_mutex_lock(&scd4x_mutex, K_FOREVER);

	LOG_INF("Initializing SCD4x CO₂ sensor");

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
		LOG_ERR("Error %d: SCD4x stop periodic measurement failed",
			err);
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
	} else {
		LOG_INF("SCD4x serial number: 0x%04x%04x%04x", serial_0,
			serial_1, serial_2);
	}

	/* According to the datasheet, the first reading obtained after waking
	up the sensor must be discarded, so do a throw-away measurement now */
	err = scd4x_sensor_read(&measurement);

	k_mutex_unlock(&scd4x_mutex);

	return err;
}

int scd4x_sensor_read(struct scd4x_sensor_measurement *measurement)
{
	int16_t err;
	bool data_ready_flag = false;
	uint16_t co2_ppm;
	int32_t temperature_m_deg_c, humidity_m_percent_rh;
	float temperature_deg_c, humidity_percent_rh;

	k_mutex_lock(&scd4x_mutex, K_FOREVER);

	LOG_DBG("Reading SCD4x CO₂ sensor (~%d seconds)",
		SCD4X_MEASUREMENT_DURATION_USEC / 1000000);

	/* Request a single-shot measurement */
	err = scd4x_measure_single_shot();
	if (err) {
		LOG_ERR("Error entering SCD4x single-shot measurement mode"
			" (error: %d)", err);
		k_mutex_unlock(&scd4x_mutex);
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
		return err;
	} else if (co2_ppm == 0) {
		LOG_ERR("Invalid SCD4x measurement sample");
		k_mutex_unlock(&scd4x_mutex);
		return err;
	}

	temperature_deg_c = temperature_m_deg_c / 1000.0;
	humidity_percent_rh = humidity_m_percent_rh / 1000.0;

	measurement->co2 = co2_ppm;
	sensor_value_from_double(&measurement->temperature,
		temperature_deg_c);
	sensor_value_from_double(&measurement->humidity,
		humidity_percent_rh);

	LOG_INF("scd4x: CO₂=%u ppm, Temperature=%f °C,"
		" Humidity=%f %%RH",
		co2_ppm,
		temperature_deg_c,
		humidity_percent_rh
	);

	k_mutex_unlock(&scd4x_mutex);

	return err;
}

int scd4x_sensor_set_temperature_offset(int32_t t_offset_m_deg_c)
{
	int err;

	k_mutex_lock(&scd4x_mutex, K_FOREVER);

	err = scd4x_set_temperature_offset(t_offset_m_deg_c);
	if (err) {
		LOG_ERR("Error setting SCD4x temperature offset (error: %d)",
			err);
	} else {
		LOG_INF("Set SCD4x temperature offset setting to %d m°C",
			t_offset_m_deg_c);
	}

	k_mutex_unlock(&scd4x_mutex);

	return err;
}

int scd4x_sensor_set_sensor_altitude(int16_t sensor_altitude)
{
	int err;

	k_mutex_lock(&scd4x_mutex, K_FOREVER);

	err = scd4x_set_sensor_altitude(sensor_altitude);
	if (err) {
		LOG_ERR("Error setting SCD4x altitude (error: %d)",
			err);
	} else {
		LOG_INF("Set SCD4x altitude setting to %d meters",
			sensor_altitude);
	}

	k_mutex_unlock(&scd4x_mutex);

	return err;
}

int scd4x_sensor_set_automatic_self_calibration(bool asc_enabled)
{
	int err;

	k_mutex_lock(&scd4x_mutex, K_FOREVER);

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

	return err;
}

int sps30_sensor_init(void)
{
	int16_t err;
	uint8_t fw_major;
	uint8_t fw_minor;
	char serial_number[SPS30_MAX_SERIAL_LEN];

	k_mutex_lock(&sps30_mutex, K_FOREVER);

	LOG_INF("Initializing SPS30 PM sensor (~30 seconds)");

	err = sps30_reset();
	if (err) {
		LOG_ERR("SPS30 sensor reset failed");
		k_mutex_unlock(&sps30_mutex);
		return err;
	}
	sensirion_i2c_hal_sleep_usec(SPS30_RESET_DELAY_USEC);

	int tries = 10;
	while (tries > 0) {
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
		LOG_ERR("Error reading SPS30 firmware version (error: %d)",
			err);
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
		LOG_ERR("Error entering SPS30 measurement mode (error: %d)",
			err);
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
	int16_t err;
	int16_t data_ready_flag = 0;
	struct sps30_measurement sps30_meas;
	struct sps30_measurement sps30_meas_avg = {0};

	k_mutex_lock(&sps30_mutex, K_FOREVER);

	/* Get the number of samples to average from Golioth settings */
	uint32_t samples = get_sps30_samples_per_measurement_s();

	LOG_DBG("Reading SPS30 PM sensor (averaging %u samples over ~%u"
		" seconds)", samples, samples);

	int count = 0;
	while (count < samples) {
		/* Poll the sensor every 0.1s waiting for the data ready status */
		/* Data should be ready every 1s */
		while (!data_ready_flag)
		{
			err = sps30_read_data_ready(&data_ready_flag);
			if (err) {
				LOG_ERR("Error reading SPS30 data ready status flag: "
					"%d", err);
				k_mutex_unlock(&sps30_mutex);
				return err;
			}

			sensirion_i2c_hal_sleep_usec(100000);
		}

		err = sps30_read_measurement(&sps30_meas);
		if (err) {
			LOG_ERR("Error reading SPS30 measurement: %d", err);
			k_mutex_unlock(&sps30_mutex);
			return err;
		}

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

	LOG_INF("sps30: "
		"PM1.0=%f μg/m³, PM2.5=%f μg/m³, "
		"PM4.0=%f μg/m³, PM10.0=%f μg/m³, "
		"NC0.5=%f #/cm³, NC1.0=%f #/cm³, "
		"NC2.5=%f #/cm³, NC4.0=%f #/cm³, "
		"NC10.0=%f #/cm³, Typical Particle Size=%f μm",
		sps30_meas_avg.mc_1p0,
		sps30_meas_avg.mc_2p5,
		sps30_meas_avg.mc_4p0,
		sps30_meas_avg.mc_10p0,
		sps30_meas_avg.nc_0p5,
		sps30_meas_avg.nc_1p0,
		sps30_meas_avg.nc_2p5,
		sps30_meas_avg.nc_4p0,
		sps30_meas_avg.nc_10p0,
		sps30_meas_avg.typical_particle_size
	);

	k_mutex_unlock(&sps30_mutex);

	return err;
}

int sps30_sensor_set_fan_auto_cleaning_interval(uint32_t interval_seconds)
{
	int err;

	k_mutex_lock(&sps30_mutex, K_FOREVER);

	err = sps30_set_fan_auto_cleaning_interval(interval_seconds);
	if (err) {
		LOG_ERR("Error setting SPS30 automatic fan cleaning"
			" interval (error: %d)", err);
	} else {
		LOG_INF("Set SPS30 automatic fan cleaning interval to %d"
			" second(s)", interval_seconds);
	}

	k_mutex_unlock(&sps30_mutex);

	return err;
}

int sps30_sensor_clean_fan(void)
{
	int16_t err;

	k_mutex_lock(&sps30_mutex, K_FOREVER);

	LOG_INF("Cleaning SPS30 PM sensor fan (~10 seconds)");

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
