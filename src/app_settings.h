/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Process changes received from the Golioth Settings Service and return a code
 * to Golioth to indicate the success or failure of the update.
 *
 * https://docs.golioth.io/firmware/zephyr-device-sdk/device-settings-service
 */

#ifndef __APP_SETTINGS_H__
#define __APP_SETTINGS_H__

#include <stdint.h>
#include <net/golioth/system_client.h>

int app_settings_init(struct golioth_client *state_client);
int app_settings_observe(void);
int app_settings_register(struct golioth_client *settings_client);

int32_t get_loop_delay_s(void);
int32_t get_scd4x_temperature_offset_s(void);
uint16_t get_scd4x_altitude_s(void);
bool get_scd4x_asc_s(void);
uint32_t get_sps30_samples_per_measurement_s(void);

#endif /* __APP_SETTINGS_H__ */
