/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_SENSORS_H__
#define __APP_SENSORS_H__

/** The `app_sensors.c` file performs the important work of this application
 * which is to read sensor values and report them to the Golioth LightDB Stream
 * as time-series data.
 *
 * https://docs.golioth.io/firmware/zephyr-device-sdk/light-db-stream/
 */

#include <golioth/client.h>

void app_sensors_init(void);
void app_sensors_set_client(struct golioth_client *sensors_client);
void app_sensors_read_and_stream(void);

#define LABEL_TEMPERATURE "Temperature"
#define LABEL_PRESSURE	  "Pressure"
#define LABEL_HUMIDITY	  "Humidity"
#define LABEL_CO2	  "CO2"
#define LABEL_PM2P5	  "PM2.5"
#define LABEL_PM10P0	  "PM10.0"
#define LABEL_BATTERY	  "Battery"
#define LABEL_FIRMWARE	  "Firmware"
#define SUMMARY_TITLE	  "Air Quality"

/**
 * Each Ostentus slide needs a unique key. You may add additional slides by
 * inserting elements with the name of your choice to this enum.
 */
typedef enum {
	TEMPERATURE,
	PRESSURE,
	HUMIDITY,
	CO2,
	PM2P5,
	PM10P0,
#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
	BATTERY_V,
	BATTERY_LVL,
#endif
	FIRMWARE
} slide_key;

#endif /* __APP_SENSORS_H__ */
