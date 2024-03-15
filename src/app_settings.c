/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_settings, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/settings.h>
#include "main.h"
#include "app_settings.h"
#include "sensors.h"

static int32_t _loop_delay_s = 60;
#define LOOP_DELAY_S_MAX 43200
#define LOOP_DELAY_S_MIN 0

static int32_t _scd4x_temperature_offset_s = 4;
static uint16_t _scd4x_altitude_s;
static bool _scd4x_asc_s = true;
static uint32_t _sps30_samples_per_measurement_s = 30;
static uint32_t _sps30_cleaning_interval_s = 604800;

int32_t get_loop_delay_s(void)
{
	return _loop_delay_s;
}

int32_t get_scd4x_temperature_offset_s(void)
{
	return _scd4x_temperature_offset_s;
}

uint16_t get_scd4x_altitude_s(void)
{
	return _scd4x_altitude_s;
}

bool get_scd4x_asc_s(void)
{
	return _scd4x_asc_s;
}

uint32_t get_sps30_samples_per_measurement_s(void)
{
	return _sps30_samples_per_measurement_s;
}

uint32_t get_sps30_cleaning_interval_s(void)
{
	return _sps30_cleaning_interval_s;
}

/* Work items for settings that need to be written to hardware sensors */
static void scd4x_sensor_set_temperature_offset_work_handler(struct k_work *work)
{
	scd4x_sensor_set_temperature_offset(_scd4x_temperature_offset_s);
}
K_WORK_DEFINE(scd4x_sensor_set_temperature_offset_work,
	      scd4x_sensor_set_temperature_offset_work_handler);

static void scd4x_sensor_set_sensor_altitude_work_handler(struct k_work *work)
{
	scd4x_sensor_set_sensor_altitude(_scd4x_altitude_s);
}
K_WORK_DEFINE(scd4x_sensor_set_sensor_altitude_work, scd4x_sensor_set_sensor_altitude_work_handler);

static void scd4x_sensor_set_automatic_self_calibration_work_handler(struct k_work *work)
{
	scd4x_sensor_set_automatic_self_calibration(_scd4x_asc_s);
}
K_WORK_DEFINE(scd4x_sensor_set_automatic_self_calibration_work,
	      scd4x_sensor_set_automatic_self_calibration_work_handler);

static void sps30_sensor_set_fan_auto_cleaning_interval_work_handler(struct k_work *work)
{
	sps30_sensor_set_fan_auto_cleaning_interval(_sps30_cleaning_interval_s);
}
K_WORK_DEFINE(sps30_sensor_set_fan_auto_cleaning_interval_work,
	      sps30_sensor_set_fan_auto_cleaning_interval_work_handler);

static enum golioth_settings_status on_loop_delay_setting(int32_t new_value, void *arg)
{
	_loop_delay_s = new_value;
	LOG_INF("Set loop delay to %i seconds", new_value);
	wake_system_thread();
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_scd4x_temperature_offset_setting(int32_t new_value,
									void *arg)
{
	_scd4x_temperature_offset_s = new_value;
	LOG_INF("Set SCD4x temperature offset to %i degrees", _scd4x_temperature_offset_s);
	/* Submit a work item to write this setting to the sensor */
	k_work_submit(&scd4x_sensor_set_temperature_offset_work);
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_scd4x_altitude_setting(int32_t new_value, void *arg)
{
	_scd4x_altitude_s = (uint16_t)new_value;
	LOG_INF("Set SCD4x altitude to %u feet", _scd4x_altitude_s);
	/* Submit a work item to write this setting to the sensor */
	k_work_submit(&scd4x_sensor_set_sensor_altitude_work);
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_scd4x_asc_setting(bool new_value, void *arg)
{
	_scd4x_asc_s = new_value;
	LOG_INF("Set SCD4x ASC to %s", _scd4x_altitude_s ? "true" : "false");
	/* Submit a work item to write this setting to the sensor */
	k_work_submit(&scd4x_sensor_set_automatic_self_calibration_work);
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_sps30_samples_per_measurement_setting(int32_t new_value,
									     void *arg)
{
	_sps30_samples_per_measurement_s = (uint32_t)new_value;
	LOG_INF("Set SPS30 samples per measurement to %i", _sps30_samples_per_measurement_s);
	return GOLIOTH_SETTINGS_SUCCESS;
}

static enum golioth_settings_status on_sps30_cleaning_interval_setting(int32_t new_value, void *arg)
{
	_sps30_cleaning_interval_s = (uint32_t)new_value;
	LOG_INF("Set SPS30 cleaning interval to %i seconds", _sps30_cleaning_interval_s);
	return GOLIOTH_SETTINGS_SUCCESS;
}

int app_settings_register(struct golioth_client *client)
{
	int err;

	struct golioth_settings *settings = golioth_settings_init(client);

	err = golioth_settings_register_int_with_range(settings,
							   "LOOP_DELAY_S",
							   LOOP_DELAY_S_MIN,
							   LOOP_DELAY_S_MAX,
							   on_loop_delay_setting,
							   NULL);
	if (err) {
		LOG_ERR("Failed to register on_loop_delay_setting callback: %d", err);
		return err;
	}

	err = golioth_settings_register_int_with_range(settings,
							   "CO2_SENSOR_TEMPERATURE_OFFSET",
							   INT32_MIN,
							   INT32_MAX,
							   on_scd4x_temperature_offset_setting,
							   NULL);
	if (err) {
		LOG_ERR("Failed to register on_scd4x_temperature_offset_setting callback: %d", err);
		return err;
	}

	err = golioth_settings_register_int_with_range(settings,
							   "CO2_SENSOR_ALTITUDE",
							   0,
							   UINT16_MAX,
							   on_scd4x_altitude_setting,
							   NULL);
	if (err) {
		LOG_ERR("Failed to register on_scd4x_altitude_setting callback: %d", err);
		return err;
	}

	err = golioth_settings_register_bool(settings,
						 "CO2_SENSOR_ASC_ENABLE",
						 on_scd4x_asc_setting,
						 NULL);
	if (err) {
		LOG_ERR("Failed to register on_scd4x_asc_setting callback: %d", err);
		return err;
	}

	err = golioth_settings_register_int_with_range(settings,
							   "PM_SENSOR_SAMPLES_PER_MEASUREMENT",
							   0,
							   INT32_MAX,
							   on_sps30_samples_per_measurement_setting,
							   NULL);
	if (err) {
		LOG_ERR("Failed to register on_sps30_samples_per_measurement_setting callback: %d",
			err);
			return err;
	}

	err = golioth_settings_register_int_with_range(settings,
							   "PM_SENSOR_AUTO_CLEANING_INTERVAL",
							   0,
							   INT32_MAX,
							   on_sps30_cleaning_interval_setting,
							   NULL);
	if (err) {
		LOG_ERR("Failed to register on_sps30_samples_per_measurement_setting callback: %d",
			err);
		return err;
	}

	return 0;
}
