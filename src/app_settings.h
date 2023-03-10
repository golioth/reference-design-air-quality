/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_SETTINGS_H__
#define __APP_SETTINGS_H__

#include <net/golioth.h>

uint32_t get_loop_delay_s(void);
uint32_t get_co2_warning_threshold_s(void);
int32_t get_scd4x_temperature_offset_s(void);
uint16_t get_scd4x_altitude_s(void);
bool get_scd4x_asc_s(void);
uint32_t get_sps30_samples_per_measurement_s(void);
uint32_t get_sps30_cleaning_interval_s(void);
int app_register_settings(struct golioth_client *settings_client);

#endif /* __APP_SETTINGS_H__ */
