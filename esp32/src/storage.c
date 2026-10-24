#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "storage.h"

static const char *TAG = "storage";

static bool mount(const char *label, const char *path, bool format)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = path,
        .partition_label = label,
        .format_if_mount_failed = format,
        .dont_mount = false,
    };
    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: mount failed: %s", label, esp_err_to_name(err));
        return false;
    }
    size_t used, total;
    storage_usage(label, &used, &total);
    ESP_LOGI(TAG, "%s: %u of %u KB used", label, (unsigned)(used / 1024), (unsigned)(total / 1024));
    return true;
}

bool storage_init(void)
{
    bool www = mount("www", "/www", false);        /* the page: written by tools/flash_files.sh, never formatted here */
    bool data = mount("data", "/data", true);
    mkdir("/data/media", 0775);
    return www && data;
}

void storage_usage(const char *label, size_t *used, size_t *total)
{
    *used = *total = 0;
    esp_littlefs_info(label, total, used);
}

char *storage_read(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, size, f);
    fclose(f);
    buf[got] = 0;
    if (len) *len = got;
    return buf;
}

bool storage_write(const char *path, const void *data, size_t len)
{
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, len, f) == len;
    ok = fflush(f) == 0 && ok;
    fsync(fileno(f));
    fclose(f);
    if (!ok) { unlink(tmp); return false; }
    unlink(path);
    return rename(tmp, path) == 0;
}
