/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_RPC_H__
#define __APP_RPC_H__

extern bool request_sps30_manual_fan_cleaning;

int app_register_rpc(struct golioth_client *rpc_client);

#endif /* __APP_RPC_H__ */
