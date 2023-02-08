/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_SETTINGS_H__
#define __APP_SETTINGS_H__

extern bool update_scd4x_temperature_offset;
extern bool update_scd4x_altitude;
extern bool update_scd4x_asc;
extern bool update_sps30_cleaning_interval;

int32_t get_loop_delay_s(void);
int32_t get_scd4x_temperature_offset_s(void);
uint16_t get_scd4x_altitude_s(void);
bool get_scd4x_asc_s(void);
uint32_t get_sps30_cleaning_interval(void);
int app_register_settings(struct golioth_client *settings_client);

#endif /* __APP_SETTINGS_H__ */
