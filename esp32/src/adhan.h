#pragma once
#include <stdbool.h>
#include <time.h>

/* A C port of adhan.js (the library the web display uses), so the screen shows the same calculated times.
 * Times are UTC instants (seconds since 1970), or -1 when they cannot be calculated (polar day or night). */
typedef struct {
    time_t fajr, sunrise, dhuhr, asr, sunset, maghrib, isha;
} adhan_times_t;

typedef enum { HL_MIDDLE, HL_SEVENTH, HL_TWILIGHT, HL_AUTO } adhan_highlat_t;

/* method: the names on the settings page (NorthAmerica, MuslimWorldLeague, Egyptian, Karachi, UmmAlQura, Dubai,
 * MoonsightingCommittee, Kuwait, Qatar, Singapore, Tehran, Turkey); unknown means NorthAmerica. */
bool adhan_calculate(double lat, double lng, int year, int month, int day, const char *method, bool hanafi,
                     adhan_highlat_t highlat, adhan_times_t *out);
