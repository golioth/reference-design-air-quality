/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_WORK_H__
#define __APP_WORK_H__

#include <net/golioth/system_client.h>

/** The `app_work.c` file performs the important work of this application which
 * is to read sensor values and report them to the Golioth LightDB Stream as
 * time-series data.
 *
 * https://docs.golioth.io/firmware/zephyr-device-sdk/light-db-stream/
 */

void app_work_init(struct golioth_client *work_client);
void app_work_sensor_read(void);

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

#endif /* __APP_WORK_H__ */
