#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "config.h"
#include "storage.h"

static const char *TAG = "config";

/* display_template's defaults (js/core.js DEFAULTS and the ones js/prayertimes.js adds), so a screen that was never
 * set up looks the same as a new TV. */
static const char *DEFAULTS =
    "{\"templateId\":\"sanctuary\",\"themes\":{},"
    "\"mosque\":{\"name\":\"My Home\",\"city\":\"Your city\",\"address\":\"Your address\",\"logo\":\"\",\"showBrand\":true},"
    "\"times\":{\"fajr\":{\"azan\":\"05:45\",\"iqamah\":\"06:00\"},\"dhuhr\":{\"azan\":\"13:00\",\"iqamah\":\"13:30\"},"
    "\"asr\":{\"azan\":\"16:15\",\"iqamah\":\"16:30\"},\"maghrib\":{\"azan\":\"18:20\",\"iqamah\":\"18:25\"},"
    "\"isha\":{\"azan\":\"20:00\",\"iqamah\":\"20:15\"},\"sunrise\":\"07:15\",\"sunset\":\"18:20\",\"jumua1\":\"13:15\",\"jumua2\":\"14:15\"},"
    "\"options\":{\"clock24\":false,\"seconds\":true,\"hijriOffset\":0,\"orientation\":\"auto\",\"timeZone\":\"\",\"timeZoneAsked\":false,"
    "\"messages\":\"Welcome home\"},"
    "\"pages\":{\"prayer\":{\"show\":true,\"seconds\":10},\"notices\":{\"show\":false,\"seconds\":6},\"combined\":{\"show\":false,\"seconds\":6}},"
    "\"noticeDesign\":{\"templateId\":\"match\",\"themeId\":\"match\"},"
    "\"combinedDesign\":{\"templateId\":\"match\",\"themeId\":\"match\",\"arrangement\":\"prayer-left\"},"
    "\"notices\":["
    "{\"id\":\"n1\",\"enabled\":true,\"layout\":\"image-right\",\"tag\":\"Announcement\",\"title\":\"Jumu\xe2\x80\x98" "ah Khutbah\","
    "\"body\":\"The khutbah begins at 1:15 PM. Please arrive early and keep the car park entrance clear.\","
    "\"meta\":\"Every Friday \xc2\xb7 Main prayer hall\",\"imageId\":\"samples/notices/jumuah.svg\",\"fit\":\"cover\",\"duration\":12,\"from\":\"\",\"until\":\"\"},"
    "{\"id\":\"n2\",\"enabled\":true,\"layout\":\"image-left\",\"tag\":\"Event\",\"title\":\"Weekly Halaqah\","
    "\"body\":\"Join us for tafsir and open discussion after Isha. Everyone is welcome.\","
    "\"meta\":\"Saturdays after Isha \xc2\xb7 Community room\",\"imageId\":\"samples/notices/halaqah.svg\",\"fit\":\"cover\",\"duration\":12,\"from\":\"\",\"until\":\"\"},"
    "{\"id\":\"n3\",\"enabled\":true,\"layout\":\"image-left\",\"tag\":\"Donate\",\"title\":\"Support Your Masjid\","
    "\"body\":\"Your sadaqah keeps our doors open. Donate at the front desk or online.\","
    "\"meta\":\"Jazakum Allahu khairan\",\"imageId\":\"samples/notices/donate.svg\",\"fit\":\"cover\",\"duration\":12,\"from\":\"\",\"until\":\"\"}],"
    "\"prayerSource\":\"manual\","
    "\"calc\":{\"lat\":\"\",\"lng\":\"\",\"method\":\"NorthAmerica\",\"madhab\":\"shafi\",\"highLat\":\"auto\"},"
    "\"iqamahRules\":{\"fajr\":{\"mode\":\"offset\",\"minutes\":20,\"time\":\"\"},\"dhuhr\":{\"mode\":\"offset\",\"minutes\":15,\"time\":\"\"},"
    "\"asr\":{\"mode\":\"offset\",\"minutes\":15,\"time\":\"\"},\"maghrib\":{\"mode\":\"offset\",\"minutes\":5,\"time\":\"\"},"
    "\"isha\":{\"mode\":\"offset\",\"minutes\":15,\"time\":\"\"}},"
    "\"prayerCurtain\":{\"enabled\":true,\"darkness\":70,\"test\":\"\",\"minutes\":{\"fajr\":10,\"dhuhr\":10,\"asr\":10,\"maghrib\":10,\"isha\":10,\"jumua\":10}},"
    "\"nightDim\":{\"enabled\":false,\"start\":\"23:00\",\"end\":\"05:00\",\"darkness\":90}}";

static SemaphoreHandle_t lock;
static cJSON *saved;       /* what was saved, or NULL */
static cJSON *merged;      /* saved over the defaults */
static int rev;
static void (*local_save_cb)(void);
volatile int config_generation;

void config_lock(void) { xSemaphoreTakeRecursive(lock, portMAX_DELAY); }
void config_unlock(void) { xSemaphoreGiveRecursive(lock); }
const cJSON *config_get(void) { return merged; }
int config_rev(void) { return rev; }
void config_on_local_save(void (*cb)(void)) { local_save_cb = cb; }

/* Like core.js merge(): objects merge key by key, anything else (arrays too) replaces. */
static void merge(cJSON *base, const cJSON *over)
{
    if (!cJSON_IsObject(over)) return;
    const cJSON *v;
    cJSON_ArrayForEach(v, over) {
        cJSON *b = cJSON_GetObjectItemCaseSensitive(base, v->string);
        if (cJSON_IsObject(v) && cJSON_IsObject(b)) merge(b, v);
        else if (cJSON_IsNull(v)) continue;
        else {
            cJSON *copy = cJSON_Duplicate(v, true);
            if (b) cJSON_ReplaceItemInObjectCaseSensitive(base, v->string, copy);
            else cJSON_AddItemToObject(base, v->string, copy);
        }
    }
}

static void rebuild(void)
{
    cJSON *m = cJSON_Parse(DEFAULTS);
    if (!m) { ESP_LOGE(TAG, "defaults do not parse"); abort(); }
    if (saved) merge(m, saved);
    if (merged) cJSON_Delete(merged);
    merged = m;
    config_generation++;
}

void config_init(void)
{
    lock = xSemaphoreCreateRecursiveMutex();
    size_t len;
    char *text = storage_read(CFG_PATH, &len);
    if (text) {
        cJSON *state = cJSON_Parse(text);
        free(text);
        if (state) {
            rev = (int)jnum(state, "rev", 0);
            cJSON *c = cJSON_DetachItemFromObjectCaseSensitive(state, "config");
            if (cJSON_IsObject(c)) saved = c; else cJSON_Delete(c);
            cJSON_Delete(state);
        } else {
            ESP_LOGW(TAG, "settings file does not parse; using the defaults");
        }
    }
    rebuild();
    ESP_LOGI(TAG, "settings: %s, rev %d", saved ? "saved" : "defaults", rev);
}

char *config_state_json(void)
{
    config_lock();
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "rev", rev);
    cJSON_AddItemToObject(o, "config", saved ? cJSON_Duplicate(saved, true) : cJSON_CreateNull());
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    config_unlock();
    return s;
}

char *config_saved_json(void)
{
    config_lock();
    char *s = saved ? cJSON_PrintUnformatted(saved) : NULL;
    config_unlock();
    return s;
}

int config_replace(cJSON *cfg, bool from_server)
{
    config_lock();
    cJSON *state = cJSON_CreateObject();
    cJSON_AddNumberToObject(state, "rev", rev + 1);
    cJSON_AddItemReferenceToObject(state, "config", cfg);
    char *text = cJSON_PrintUnformatted(state);
    cJSON_Delete(state);
    bool ok = text && storage_write(CFG_PATH, text, strlen(text));
    free(text);
    int result = -1;
    if (ok) {
        if (saved) cJSON_Delete(saved);
        saved = cfg;
        rev++;
        rebuild();
        result = rev;
    } else {
        cJSON_Delete(cfg);
        ESP_LOGE(TAG, "could not save the settings");
    }
    config_unlock();
    if (ok && !from_server && local_save_cb) local_save_cb();
    return result;
}

const cJSON *jget(const cJSON *o, const char *path)
{
    char key[48];
    while (o && *path) {
        const char *dot = strchr(path, '.');
        size_t n = dot ? (size_t)(dot - path) : strlen(path);
        if (n >= sizeof(key)) return NULL;
        memcpy(key, path, n);
        key[n] = 0;
        o = cJSON_GetObjectItemCaseSensitive(o, key);
        path = dot ? dot + 1 : path + n;
    }
    return o;
}

const char *jstr(const cJSON *o, const char *path, const char *fallback)
{
    const cJSON *v = jget(o, path);
    return cJSON_IsString(v) && v->valuestring ? v->valuestring : fallback;
}

double jnum(const cJSON *o, const char *path, double fallback)
{
    const cJSON *v = jget(o, path);
    if (cJSON_IsNumber(v)) return v->valuedouble;
    if (cJSON_IsString(v) && v->valuestring && *v->valuestring) {
        char *end;
        double d = strtod(v->valuestring, &end);
        if (end != v->valuestring) return d;
    }
    if (cJSON_IsBool(v)) return cJSON_IsTrue(v);
    return fallback;
}

bool jbool(const cJSON *o, const char *path, bool fallback)
{
    const cJSON *v = jget(o, path);
    if (cJSON_IsBool(v)) return cJSON_IsTrue(v);
    if (cJSON_IsNumber(v)) return v->valuedouble != 0;
    return fallback;
}
