#pragma once
typedef struct {
    const char *name;
    const char *posix;
} tz_entry_t;
extern const tz_entry_t TZ_DB[];
extern const int TZ_DB_COUNT;

/* The POSIX rule for a zone as the settings store it: a name ("America/Regina"), a fixed offset ("UTC+05:45",
 * "GMT-6"), or "" for none (then UTC). Writes into out; returns false when the zone is unknown (out gets UTC0). */
#include <stdbool.h>
#include <stddef.h>
bool tz_posix(const char *zone, char *out, size_t len);
