#pragma once
#include <stdbool.h>
#include "lvgl.h"

/* Settings on the screen (src/settings.c), opened with the gear. wifi: opens the Wi-Fi screen (ui.c). */
void settings_build(lv_obj_t *parent, void (*wifi)(void));
void settings_open(void);
void settings_close_cb(lv_event_t *e);
bool settings_visible(void);
void settings_show(int page);      /* -1 close, 0 Wi-Fi .. 4 dimming, 5 the menu */
void settings_tick(void);
