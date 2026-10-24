#pragma once
#include <stdbool.h>
#include <stddef.h>
/* Two LittleFS partitions: "www" at /www holds the settings page (replaced whole by an update), "data" at /data holds
 * this screen's settings (/data/config.json) and notice pictures (/data/media), which an update never touches. */
bool storage_init(void);
char *storage_read(const char *path, size_t *len);       /* NUL-terminated, in PSRAM; NULL when missing. Caller frees. */
bool storage_write(const char *path, const void *data, size_t len);   /* atomic: tmp file then rename */
void storage_usage(const char *label, size_t *used, size_t *total);
