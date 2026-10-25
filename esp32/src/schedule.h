#pragma once
#include <stdbool.h>
#include <time.h>
#include "cJSON.h"

/* What the screen shows, worked out from the settings and the clock: the same rules as display_template's
 * js/core.js (computeState), js/prayertimes.js (resolve), js/curtain.js and js/nightdim.js. */

enum { FAJR, DHUHR, ASR, MAGHRIB, ISHA, PRAYER_N };
extern const char *const PRAYER_ID[PRAYER_N];
extern const char *const PRAYER_NAME[PRAYER_N];

typedef struct {
    int azan[PRAYER_N], iqamah[PRAYER_N];      /* minutes since midnight, -1 when not set */
    int sunrise, sunset, jumua1, jumua2;
    char source[12];                            /* manual | calculated | calendar */
} day_times_t;

typedef struct {
    struct tm now;               /* wall clock in the screen's zone */
    day_times_t times;
    char hm[8], sec[4], ap[4];   /* "1:05", ":09", "PM" */
    char date_long[48], hijri[48];
    int hijri_offset;            /* days added to the Hijri date (options.hijriOffset) */
    int active;                  /* the prayer of the next event, -1 none */
    int next_kind;               /* 0 azan, 1 iqamah */
    char next_name[32], next_at[32], countdown[12];
    bool friday;
    float dim;                   /* 0..0.99: how dark the screen should be now (prayer curtain / night) */
} display_state_t;

/* Applies the zone from the settings (options.timeZone) to the C library. Call when the settings change. */
void schedule_apply_zone(const cJSON *cfg);
/* The day's times for a wall-clock date. */
void schedule_resolve(const cJSON *cfg, int year, int month, int day, day_times_t *out);
void schedule_state(const cJSON *cfg, display_state_t *s);
void fmt_min(int min, bool h24, char *out, size_t len);    /* "5:45 AM" or "05:45"; "--:--" when unset */
bool clock_is_set(void);
long schedule_jdn(int y, int m, int d);            /* Julian day number */
/* The Hijri month holding a Julian day: year, month (0 Muharram..11), its first day and length. false off the table. */
bool hijri_month(long day, int *year, int *month, long *first, int *length);
const char *hijri_month_name(int month);
