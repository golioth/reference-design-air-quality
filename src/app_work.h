/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_WORK_H__
#define __APP_WORK_H__

#define O_LABEL_TEMPERATURE "Temperature"
#define O_LABEL_PRESSURE    "Pressure"
#define O_LABEL_HUMIDITY    "Humidity"
#define O_LABEL_CO2	    "CO2"
#define O_LABEL_PM2P5	    "PM2.5"
#define O_LABEL_PM10P0	    "PM10.0"
#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#define O_LABEL_BATTERY "Battery"
#endif
#define O_LABEL_FIRMWARE "Firmware"
#define O_SUMMARY_TITLE	 "Air Quality"

void app_work_init(struct golioth_client *work_client);
void app_work_sensor_read(void);

/**
 * Each Ostentus slide needs a unique key. You may add additional slides by
 * inserting elements with the name of your choice to this enum.
 */
typedef enum {
	O_TEMPERATURE,
	O_PRESSURE,
	O_HUMIDITY,
	O_CO2,
	O_PM2P5,
	O_PM10P0,
#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
	O_BATTERY_V,
	O_BATTERY_LVL,
#endif
	O_FIRMWARE
} slide_key;

#endif /* __APP_WORK_H__ */
