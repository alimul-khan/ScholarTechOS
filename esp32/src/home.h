#pragma once
#include <stdbool.h>
#include "lvgl.h"
#include "schedule.h"
#include "weather.h"

/* The home pages (src/home.c): the prayer dashboard and the weather, each with the prayer bar and the gear. */
enum { HOME_PRAYER, HOME_WEATHER };
void home_build(lv_obj_t *parent, lv_event_cb_t on_gear);
void home_show(int page);
void home_update(int page, const display_state_t *st, const char *city, bool h24, const char *status, const weather_t *w);
