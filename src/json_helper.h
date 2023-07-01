/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __JSON_HELPER_H_
#define __JSON_HELPER_H_

#include <zephyr/data/json.h>

struct air_quality_state {
	int32_t example_int0;
	int32_t example_int1;
};

static const struct json_obj_descr air_quality_state_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct air_quality_state, example_int0, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct air_quality_state, example_int1, JSON_TOK_NUMBER)
};

#endif
