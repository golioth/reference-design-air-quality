#ifndef __JSON_HELPER_H_
#define __JSON_HELPER_H_

#include <zephyr/data/json.h>

struct air_quality_state {
    int32_t warning_indicator;
};

static const struct json_obj_descr air_quality_state_descr[] = {
    JSON_OBJ_DESCR_PRIM(struct air_quality_state, warning_indicator, JSON_TOK_NUMBER)
};

#endif
