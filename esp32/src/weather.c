/* The weather, as the Home Display app gets it (MainActivity.fetchWeather): open-meteo.com for the place in the
 * settings, every 30 minutes, kept in memory. No account or key. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "config.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "schedule.h"
#include "weather.h"
#include "wifi.h"

static const char *TAG = "weather";
static SemaphoreHandle_t lock;
static weather_t now_wx;
static TaskHandle_t task;

typedef struct { char *buf; int len; } reply_t;

static esp_err_t on_data(esp_http_client_event_t *e)
{
    reply_t *r = e->user_data;
    if (e->event_id == HTTP_EVENT_ON_DATA && r->len + e->data_len < 64 * 1024) {
        char *grown = heap_caps_realloc(r->buf, r->len + e->data_len + 1, MALLOC_CAP_SPIRAM);
        if (!grown) return ESP_FAIL;
        r->buf = grown;
        memcpy(r->buf + r->len, e->data, e->data_len);
        r->len += e->data_len;
        r->buf[r->len] = 0;
    }
    return ESP_OK;
}

static double num(const cJSON *o, const char *k) { const cJSON *v = cJSON_GetObjectItem(o, k); return cJSON_IsNumber(v) ? v->valuedouble : NAN; }
static double at(const cJSON *o, const char *k, int i) { const cJSON *v = cJSON_GetArrayItem(cJSON_GetObjectItem(o, k), i); return cJSON_IsNumber(v) ? v->valuedouble : NAN; }

static bool fetch(double lat, double lng)
{
    char url[400];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m"
             "&hourly=temperature_2m,weather_code&daily=temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=2",
             lat, lng);
    reply_t r = {0};
    esp_http_client_config_t c = {.url = url, .timeout_ms = 20000, .event_handler = on_data, .user_data = &r,
                                  .crt_bundle_attach = esp_crt_bundle_attach, .buffer_size = 4096};
    esp_http_client_handle_t h = esp_http_client_init(&c);
    esp_err_t err = esp_http_client_perform(h);
    int code = err == ESP_OK ? esp_http_client_get_status_code(h) : -1;
    esp_http_client_cleanup(h);
    cJSON *j = code == 200 && r.buf ? cJSON_Parse(r.buf) : NULL;
    free(r.buf);
    if (!j) { ESP_LOGW(TAG, "no weather (%s, %d)", esp_err_to_name(err), code); return false; }

    weather_t w = {.valid = true, .fetched = time(NULL), .lat = lat, .lng = lng};
    const cJSON *cur = cJSON_GetObjectItem(j, "current"), *daily = cJSON_GetObjectItem(j, "daily"), *hourly = cJSON_GetObjectItem(j, "hourly");
    w.temp = num(cur, "temperature_2m");
    w.feels = num(cur, "apparent_temperature");
    w.wind = num(cur, "wind_speed_10m");
    w.humidity = num(cur, "relative_humidity_2m");
    w.code = (int)num(cur, "weather_code");
    w.high = at(daily, "temperature_2m_max", 0);
    w.low = at(daily, "temperature_2m_min", 0);
    /* The next hours, as the app picks them: 3, 6, 9 and 12 hours on from this hour (local time of the place). */
    const cJSON *times = cJSON_GetObjectItem(hourly, "time"), *now = cJSON_GetObjectItem(cur, "time");
    int start = -1;
    if (cJSON_IsString(now)) {
        char hour[14];
        snprintf(hour, sizeof(hour), "%.13s", now->valuestring);          /* "2026-10-24T13" */
        for (int i = 0; i < cJSON_GetArraySize(times) && start < 0; i++) {
            const cJSON *t = cJSON_GetArrayItem(times, i);
            if (cJSON_IsString(t) && strncmp(t->valuestring, hour, 13) >= 0) start = i;
        }
    }
    for (int k = 1; k <= 4 && start >= 0; k++) {
        int i = start + 3 * k - 1;
        const cJSON *t = cJSON_GetArrayItem(times, i);
        if (!cJSON_IsString(t) || strlen(t->valuestring) < 13) break;
        w.hour[w.hours] = atoi(t->valuestring + 11);
        w.hour_temp[w.hours] = at(hourly, "temperature_2m", i);
        w.hour_code[w.hours] = (int)at(hourly, "weather_code", i);
        w.hours++;
    }
    cJSON_Delete(j);
    xSemaphoreTake(lock, portMAX_DELAY);
    now_wx = w;
    xSemaphoreGive(lock);
    ESP_LOGI(TAG, "%.1f C, code %d, %d hours ahead", w.temp, w.code, w.hours);
    return true;
}

static void loop(void *arg)
{
    while (1) {
        config_lock();
        const cJSON *cfg = config_get();
        double lat = jnum(cfg, "calc.lat", NAN), lng = jnum(cfg, "calc.lng", NAN);
        config_unlock();
        weather_t w;
        weather_get(&w);
        bool moved = !w.valid || fabs(w.lat - lat) > 1e-4 || fabs(w.lng - lng) > 1e-4;
        bool old = !w.valid || time(NULL) - w.fetched > 30 * 60;
        bool ok = true;
        if (isfinite(lat) && isfinite(lng) && wifi_connected() && clock_is_set() && (moved || old)) ok = fetch(lat, lng);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ok ? 60 * 1000 : 5 * 60 * 1000));
    }
}

void weather_start(void)
{
    lock = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(loop, "weather", 6144, NULL, 3, &task, 0);
}

void weather_wake(void) { if (task) xTaskNotifyGive(task); }

void weather_get(weather_t *out)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    *out = now_wx;
    xSemaphoreGive(lock);
}

int weather_kind(int code)
{
    if (code == 0) return 0;                                         /* clear */
    if (code <= 3) return 1;                                         /* partly cloudy */
    if (code <= 48) return 2;                                        /* fog, cloudy */
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return 4;   /* snow */
    return 3;                                                        /* rain, showers, storms */
}

const char *weather_text(int code)
{
    if (code == 0) return "Clear sky";
    if (code <= 3) return "Partly cloudy";
    if (code <= 48) return "Fog";
    if (code <= 57) return "Drizzle";
    if (code <= 67) return "Rain";
    if (code <= 77) return "Snow";
    if (code <= 82) return "Rain showers";
    if (code <= 86) return "Snow showers";
    return "Thunderstorms";
}
