#pragma once
#include <stdbool.h>
#include "lvgl.h"
#include "schedule.h"
#include "weather.h"

/* The home pages (src/home.c): the prayer dashboard, the weather and the two calendars, each with the prayer bar and the gear. */
enum { HOME_PRAYER, HOME_WEATHER, HOME_GREGORIAN, HOME_HIJRI, HOME_PAGES };
void home_build(lv_obj_t *parent, lv_event_cb_t on_gear);
void home_show(int page);
void home_update(int page, const display_state_t *st, const char *city, bool h24, const char *status, const weather_t *w);
bool home_browsing(void);      /* someone is looking through the calendar months: do not turn the page */
bool home_take_redraw(void);   /* true once after a tap that needs the page drawn again */
void home_cal_step(int page, int months);   /* for checking from a computer */
