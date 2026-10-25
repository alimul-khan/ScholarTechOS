/* The place search behind Settings > Location and the settings page (GET /api/places): online, from Open-Meteo's free
 * geocoding (no account or key), so the board keeps no city list. Answers in the shape the Android app's Places.java
 * gives: name, region, country, latitude, longitude, zone (and the zone's offset today). */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "config.h"
#include "places.h"
#include "tzdb.h"

static const char *TAG = "places";

/* ---- the offset a zone has today, for "timezone" in the reply (GMT-06:00, as the settings page writes it) ---- */

static const char *posix_offset(const char *p, int *seconds)
{
    int sign = 1, h = 0, m = 0, s = 0;
    if (*p == '+' || *p == '-') sign = *p++ == '-' ? -1 : 1;
    while (isdigit((unsigned char)*p)) h = h * 10 + (*p++ - '0');
    if (*p == ':') { p++; while (isdigit((unsigned char)*p)) m = m * 10 + (*p++ - '0'); }
    if (*p == ':') { p++; while (isdigit((unsigned char)*p)) s = s * 10 + (*p++ - '0'); }
    *seconds = -sign * (h * 3600 + m * 60 + s);     /* POSIX counts west as positive; we want east */
    return p;
}

static const char *posix_name(const char *p)
{
    if (*p == '<') { while (*p && *p != '>') p++; return *p ? p + 1 : p; }
    while (isalpha((unsigned char)*p)) p++;
    return p;
}

/* The day of the month for "Mm.w.d" in a year. */
static int rule_day(int year, int m, int w, int d)
{
    struct tm t = {.tm_year = year - 1900, .tm_mon = m - 1, .tm_mday = 1, .tm_hour = 12};
    time_t x = timegm(&t);
    gmtime_r(&x, &t);
    int first = 1 + (d - t.tm_wday + 7) % 7;
    int day = first + (w - 1) * 7;
    static const int len[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int ml = len[m - 1] + (m == 2 && ((year % 4 == 0 && year % 100) || year % 400 == 0));
    while (day > ml) day -= 7;
    return day;
}

static const char *posix_rule(const char *p, int year, int offset, time_t *at)
{
    int m = 3, w = 2, d = 0, secs = 7200;
    if (*p == 'M') {
        p++;
        m = strtol(p, (char **)&p, 10);
        if (*p == '.') w = strtol(p + 1, (char **)&p, 10);
        if (*p == '.') d = strtol(p + 1, (char **)&p, 10);
    }
    if (*p == '/') { int s; p = posix_offset(p + 1, &s); secs = -s; }
    struct tm t = {.tm_year = year - 1900, .tm_mon = m - 1, .tm_mday = rule_day(year, m, w, d)};
    *at = timegm(&t) + secs - offset;
    return p;
}

static void zone_now(const char *zone, size_t zl, int *minutes, bool *dst, bool *known)
{
    char name[64], posix[64];
    snprintf(name, sizeof(name), "%.*s", (int)zl, zone);
    *known = tz_posix(name, posix, sizeof(posix));
    *dst = false;
    *minutes = 0;
    if (!*known) return;
    int std, dsto;
    const char *p = posix_offset(posix_name(posix), &std);
    *minutes = std / 60;
    if (!*p || *p == ',') return;
    p = posix_name(p);
    *dst = true;
    dsto = std + 3600;
    if (*p && *p != ',') p = posix_offset(p, &dsto);
    if (*p != ',') return;
    time_t now = time(NULL), start, end;
    struct tm t;
    gmtime_r(&now, &t);
    p = posix_rule(p + 1, t.tm_year + 1900, std, &start);
    if (*p != ',') return;
    posix_rule(p + 1, t.tm_year + 1900, dsto, &end);
    bool in = start < end ? (now >= start && now < end) : (now >= start || now < end);
    if (in) *minutes = dsto / 60;
}

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

char *places_search_json(const char *query, int limit)
{
    cJSON *o = cJSON_CreateObject();
    cJSON *results = cJSON_AddArrayToObject(o, "results");
    if (limit < 1) limit = 8;
    if (limit > 20) limit = 20;
    /* The city part only ("regina, canada" -> "regina"): the service matches names. */
    char name[96], url[256];
    size_t k = 0;
    for (const char *q = query ? query : ""; *q && *q != ',' && k + 4 < sizeof(name); q++) {
        unsigned char c = *q;
        if (isalnum(c) || c >= 0x80 || c == '-' || c == '.' || c == '\'') name[k++] = c;
        else if (c == ' ') { name[k++] = '%'; name[k++] = '2'; name[k++] = '0'; }
    }
    name[k] = 0;
    if (k >= 2) {
        snprintf(url, sizeof(url), "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=%d&language=en&format=json", name, limit);
        reply_t r = {0};
        esp_http_client_config_t c = {.url = url, .timeout_ms = 15000, .event_handler = on_data, .user_data = &r,
                                      .crt_bundle_attach = esp_crt_bundle_attach, .buffer_size = 4096};
        esp_http_client_handle_t h = esp_http_client_init(&c);
        esp_err_t err = esp_http_client_perform(h);
        int code = err == ESP_OK ? esp_http_client_get_status_code(h) : -1;
        esp_http_client_cleanup(h);
        cJSON *j = code == 200 && r.buf ? cJSON_Parse(r.buf) : NULL;
        free(r.buf);
        if (!j) ESP_LOGW(TAG, "search failed (%s, %d)", esp_err_to_name(err), code);
        const cJSON *p;
        cJSON_ArrayForEach(p, cJSON_GetObjectItem(j, "results")) {
            char buf[32];
            const char *zone = jstr(p, "timezone", "");
            cJSON *x = cJSON_CreateObject();
            cJSON_AddStringToObject(x, "name", jstr(p, "name", ""));
            cJSON_AddStringToObject(x, "region", jstr(p, "admin1", ""));
            cJSON_AddStringToObject(x, "country", jstr(p, "country", ""));
            cJSON_AddStringToObject(x, "country_code", jstr(p, "country_code", ""));
            snprintf(buf, sizeof(buf), "%.4f", jnum(p, "latitude", 0)); cJSON_AddStringToObject(x, "latitude", buf);
            snprintf(buf, sizeof(buf), "%.4f", jnum(p, "longitude", 0)); cJSON_AddStringToObject(x, "longitude", buf);
            cJSON_AddStringToObject(x, "zone", zone);
            int minutes;
            bool dst, known;
            zone_now(zone, strlen(zone), &minutes, &dst, &known);
            if (known) {
                minutes = (int)(lround(minutes / 15.0) * 15);
                int a = abs(minutes);
                snprintf(buf, sizeof(buf), "GMT%c%02d:%02d", minutes < 0 ? '-' : '+', a / 60, a % 60);
            } else buf[0] = 0;
            cJSON_AddStringToObject(x, "timezone", buf);
            cJSON_AddBoolToObject(x, "dst", dst);
            cJSON_AddItemToArray(results, x);
        }
        cJSON_Delete(j);
    }
    char *out = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return out;
}
