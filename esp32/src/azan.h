#pragma once
#include <stdbool.h>

/* The Azan player (src/azan.c). prayer: FAJR..ISHA (schedule.h); Fajr uses the Fajr Azan when one is loaded. */
void azan_init(void);
bool azan_play(int prayer);
void azan_stop(void);
bool azan_playing(void);
int azan_prayer(void);            /* the prayer playing now, -1 when quiet */
bool azan_ready(int prayer);      /* a file to play for this prayer */
void azan_set_volume(int percent);
