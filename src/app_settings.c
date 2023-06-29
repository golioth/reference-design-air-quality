/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_settings, LOG_LEVEL_DBG);

#include <net/golioth/settings.h>
#include "app_settings.h"
#include "main.h"
#include "sensors.h"

static uint32_t _loop_delay_s = 60;
static int32_t _scd4x_temperature_offset_s = 4;
static uint16_t _scd4x_altitude_s;
static bool _scd4x_asc_s = true;
static uint32_t _sps30_samples_per_measurement_s = 30;
static uint32_t _sps30_cleaning_interval_s = 604800;

uint32_t get_loop_delay_s(void)
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
K_WORK_DEFINE(scd4x_sensor_set_sensor_altitude_work,
	scd4x_sensor_set_sensor_altitude_work_handler);

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

enum golioth_settings_status on_setting(
	const char *key,
	const struct golioth_settings_value *value)
{
	LOG_DBG("Received setting: key = %s, type = %d", key, value->type);
	if (strcmp(key, "LOOP_DELAY_S") == 0) {
		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			LOG_DBG("Received LOOP_DELAY_S is not an integer type.");
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to 12 hour max delay: [1, 43200] */
		if (value->i64 < 1 || value->i64 > 43200) {
			LOG_DBG("Received LOOP_DELAY_S setting is outside"
				" allowed range.");
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_loop_delay_s == (uint32_t)value->i64) {
			LOG_DBG("Received LOOP_DELAY_S setting already matches"
				" local value.");
		} else {
			_loop_delay_s = (uint32_t)value->i64;
			LOG_INF("Set main loop delay to %d seconds", _loop_delay_s);

			wake_system_thread();
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "CO2_SENSOR_TEMPERATURE_OFFSET") == 0) {
		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			LOG_DBG("Received CO2_SENSOR_TEMPERATURE_OFFSET is not an integer type.");
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to m°C in the range [INT32_MIN, INT32_MAX] */
		if (value->i64 < INT32_MIN || value->i64 > INT32_MAX) {
			LOG_DBG("Received CO2_SENSOR_TEMPERATURE_OFFSET setting"
				" is outside allowed range.");
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_scd4x_temperature_offset_s == (int32_t)value->i64) {
			LOG_DBG("Received CO2_SENSOR_TEMPERATURE_OFFSET setting"
				" already matches local value.");
		} else {
			_scd4x_temperature_offset_s = (int32_t)value->i64;
			/* Submit a work item to write this to the sensor */
			k_work_submit(&scd4x_sensor_set_temperature_offset_work);
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "CO2_SENSOR_ALTITUDE") == 0) {
		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			LOG_DBG("Received CO2_SENSOR_ALTITUDE is not an integer type.");
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to meters in the range [INT16_MIN, INT16_MAX] */
		if (value->i64 < INT16_MIN || value->i64 > INT16_MAX) {
			LOG_DBG("Received CO2_SENSOR_ALTITUDE setting is"
				" outside allowed range.");
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_scd4x_altitude_s == (uint16_t)value->i64) {
			LOG_DBG("Received CO2_SENSOR_ALTITUDE setting already"
				" matches local value.");
		} else {
			_scd4x_altitude_s = (uint16_t)value->i64;
			/* Submit a work item to write this to the sensor */
			k_work_submit(&scd4x_sensor_set_sensor_altitude_work);
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "CO2_SENSOR_ASC_ENABLE") == 0) {
		/* This setting is expected to be boolean, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_BOOL) {
			LOG_DBG("Received CO2_SENSOR_ASC_ENABLE is not a boolean type.");
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Only update if value has changed */
		if (_scd4x_asc_s == (bool)value->b) {
			LOG_DBG("Received CO2_SENSOR_ASC_ENABLE setting already"
				" matches local value.");
		} else {
			_scd4x_asc_s = (bool)value->b;
			/* Submit a work item to write this to the sensor */
			k_work_submit(&scd4x_sensor_set_automatic_self_calibration_work);
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "PM_SENSOR_SAMPLES_PER_MEASUREMENT") == 0) {
		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			LOG_DBG("Received PM_SENSOR_SAMPLES_PER_MEASUREMENT is not an integer"
				" type.");
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to seconds in the range [UINT32_MIN, UINT32_MAX] */
		if (value->i64 < 0 || value->i64 > UINT32_MAX) {
			LOG_DBG("Received PM_SENSOR_SAMPLES_PER_MEASUREMENT"
				" setting is outside allowed range.");
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_sps30_samples_per_measurement_s == (uint32_t)value->i64) {
			LOG_DBG("Received PM_SENSOR_SAMPLES_PER_MEASUREMENT"
				" setting already matches local value.");
		} else {
			_sps30_samples_per_measurement_s = (uint32_t)value->i64;
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	}

	if (strcmp(key, "PM_SENSOR_AUTO_CLEANING_INTERVAL") == 0) {
		/* This setting is expected to be numeric, return an error if it's not */
		if (value->type != GOLIOTH_SETTINGS_VALUE_TYPE_INT64) {
			LOG_DBG("Received PM_SENSOR_AUTO_CLEANING_INTERVAL is not an integer"
				" type.");
			return GOLIOTH_SETTINGS_VALUE_FORMAT_NOT_VALID;
		}

		/* Limit to seconds in the range [UINT32_MIN, UINT32_MAX] */
		if (value->i64 < 0 || value->i64 > UINT32_MAX) {
			LOG_DBG("Received PM_SENSOR_AUTO_CLEANING_INTERVAL"
				" setting is outside allowed range.");
			return GOLIOTH_SETTINGS_VALUE_OUTSIDE_RANGE;
		}

		/* Only update if value has changed */
		if (_sps30_cleaning_interval_s == (uint32_t)value->i64) {
			LOG_DBG("Received PM_SENSOR_AUTO_CLEANING_INTERVAL"
				" setting already matches local value.");
		} else {
			_sps30_cleaning_interval_s = (uint32_t)value->i64;
			/* Submit a work item to write this to the sensor */
			k_work_submit(&sps30_sensor_set_fan_auto_cleaning_interval_work);
		}
		return GOLIOTH_SETTINGS_SUCCESS;
	}

	/* If the setting is not recognized, we should return an error */
	return GOLIOTH_SETTINGS_KEY_NOT_RECOGNIZED;
}

int app_register_settings(struct golioth_client *settings_client)
{
	int err = golioth_settings_register_callback(settings_client,
		on_setting);

	if (err) {
		LOG_ERR("Failed to register settings callback: %d", err);
	}

	return err;
}
