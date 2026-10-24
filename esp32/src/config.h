#pragma once
#include <stdbool.h>
#include <time.h>
#include "cJSON.h"

/* The display settings: the same JSON as display_template ({rev, config}), kept in /data/config.json.
 * Everything that reads it takes the lock; config_changed() is called after each change. */

#define CFG_PATH "/data/config.json"

void config_init(void);
void config_lock(void);
void config_unlock(void);

/* The settings merged over the defaults (read-only; hold the lock while using it). */
const cJSON *config_get(void);
int config_rev(void);
/* {"rev":..,"config":..} as text, as GET /api/config answers; "config" is null before the first save. Caller frees. */
char *config_state_json(void);
/* The saved settings only (not merged), or NULL when none were saved. Caller frees. */
char *config_saved_json(void);
/* Replaces the settings with this JSON object (takes ownership). from_server: do not tell the link (it came from there).
 * Returns the new rev, or -1 when it could not be saved. */
int config_replace(cJSON *cfg, bool from_server);

/* Told about saves made on this screen (the local settings page), so the settings server can follow. */
void config_on_local_save(void (*cb)(void));
/* Bumped on every change; the screen redraws when it moves. */
extern volatile int config_generation;

/* Helpers over a cJSON tree. */
const char *jstr(const cJSON *o, const char *path, const char *fallback);   /* path like "options.timeZone" */
double jnum(const cJSON *o, const char *path, double fallback);
bool jbool(const cJSON *o, const char *path, bool fallback);
const cJSON *jget(const cJSON *o, const char *path);
