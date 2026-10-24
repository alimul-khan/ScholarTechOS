#pragma once
#include <stdbool.h>
#include "lvgl.h"
#include "schedule.h"

/* The home prayer page (src/home.c). */
lv_obj_t *home_build(lv_obj_t *parent);
void home_update(const display_state_t *st, const char *city, bool h24, const char *status);
