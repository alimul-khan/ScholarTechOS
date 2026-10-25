#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cJSON.h"
#include "config.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "link.h"
#include "places.h"
#include "storage.h"
#include "ui.h"
#include "azan.h"
#include "bsp/esp-bsp.h"
#include "home.h"
#include "settings.h"
#include "schedule.h"
#include "web.h"
#include "wifi.h"

static const char *TAG = "web";
#define MAX_CONFIG (4 * 1024 * 1024)
#define MAX_MEDIA (3 * 1024 * 1024 / 2)

/* A home screen has no passphrase: anyone on the home Wi-Fi can change the settings. */
static int retry_after(void) { return 0; }
static int check_pin(httpd_req_t *req) { return 0; }

static esp_err_t send_json(httpd_req_t *req, int code, const char *body)
{
    char status[32];
    snprintf(status, sizeof(status), "%d %s", code, code == 200 ? "OK" : code == 400 ? "Bad Request" : code == 401 ? "Unauthorized" :
             code == 404 ? "Not Found" : code == 413 ? "Payload Too Large" : code == 429 ? "Too Many Requests" : "Error");
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, body ? body : "{}");
}

static esp_err_t send_owned(httpd_req_t *req, int code, char *body)
{
    esp_err_t e = send_json(req, code, body);
    cJSON_free(body);
    return e;
}

static esp_err_t refuse(httpd_req_t *req, int code)
{
    char body[64];
    if (code == 429) snprintf(body, sizeof(body), "{\"error\":\"Too many tries\",\"retryAfter\":%d}", retry_after());
    else snprintf(body, sizeof(body), "{\"error\":\"The passphrase is needed\"}");
    return send_json(req, code, body);
}

/* The request body, NUL-terminated, in PSRAM. NULL when too big or broken (an answer has been sent). */
static char *read_body(httpd_req_t *req, size_t max, size_t *len)
{
    if (req->content_len > max) { send_json(req, 413, "{\"error\":\"Too large\"}"); return NULL; }
    char *buf = heap_caps_malloc(req->content_len + 1, MALLOC_CAP_SPIRAM);
    if (!buf) { send_json(req, 413, "{\"error\":\"Too large\"}"); return NULL; }
    size_t got = 0;
    while (got < req->content_len) {
        int r = httpd_req_recv(req, buf + got, req->content_len - got);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) { free(buf); return NULL; }
        got += r;
    }
    buf[got] = 0;
    if (len) *len = got;
    return buf;
}

static bool media_id_ok(const char *id)
{
    size_t n = strlen(id);
    if (!n || n > 80) return false;
    for (size_t i = 0; i < n; i++) if (!isalnum((unsigned char)id[i]) && !strchr("._-", id[i])) return false;
    return strcmp(id, ".") && strcmp(id, "..");
}

static void url_decode(char *s)
{
    char *o = s;
    for (; *s; s++) {
        if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
            char h[3] = {s[1], s[2], 0};
            *o++ = (char)strtol(h, NULL, 16);
            s += 2;
        } else *o++ = *s == '+' ? ' ' : *s;
    }
    *o = 0;
}

static const char *type_of(const char *path)
{
    const char *dot = strrchr(path, '.');
    static const char *types[][2] = {
        {".html", "text/html; charset=utf-8"}, {".css", "text/css; charset=utf-8"}, {".js", "text/javascript; charset=utf-8"},
        {".json", "application/json"}, {".svg", "image/svg+xml"}, {".png", "image/png"}, {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"}, {".gif", "image/gif"}, {".webp", "image/webp"}, {".woff2", "font/woff2"}, {".ttf", "font/ttf"},
        {".txt", "text/plain; charset=utf-8"}};
    if (dot) for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) if (!strcasecmp(dot, types[i][0])) return types[i][1];
    return "application/octet-stream";
}

static esp_err_t send_file(httpd_req_t *req, const char *path, const char *type, bool gz, bool cache)
{
    FILE *f = fopen(path, "rb");
    if (!f) return ESP_FAIL;
    httpd_resp_set_type(req, type);
    if (gz) httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", cache ? "max-age=86400" : "no-cache");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    char *buf = heap_caps_malloc(8192, MALLOC_CAP_SPIRAM);
    size_t n;
    esp_err_t e = ESP_OK;
    while ((n = fread(buf, 1, 8192, f)) > 0) if ((e = httpd_resp_send_chunk(req, buf, n)) != ESP_OK) break;
    fclose(f);
    free(buf);
    if (e == ESP_OK) httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t static_file(httpd_req_t *req, const char *path)
{
    if (strstr(path, "..")) return send_json(req, 404, "{\"error\":\"Not found\"}");
    char file[160];
    bool cache = strstr(req->uri, "?v=") != NULL;
    const char *type = type_of(path);
    snprintf(file, sizeof(file), "/www%s.gz", path);
    if (send_file(req, file, type, true, cache) == ESP_OK) return ESP_OK;
    snprintf(file, sizeof(file), "/www%s", path);
    if (send_file(req, file, type, false, cache) == ESP_OK) return ESP_OK;
    return send_json(req, 404, "{\"error\":\"Not found\"}");
}

/* ---- notice pictures ---- */

static esp_err_t media_list(httpd_req_t *req)
{
    cJSON *a = cJSON_CreateArray();
    DIR *d = opendir("/data/media");
    struct dirent *e;
    while (d && (e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (e->d_name[0] == '.' || (n > 5 && !strcmp(e->d_name + n - 5, ".type")) || (n > 4 && !strcmp(e->d_name + n - 4, ".tmp"))) continue;
        cJSON_AddItemToArray(a, cJSON_CreateString(e->d_name));
    }
    if (d) closedir(d);
    char *s = cJSON_PrintUnformatted(a);
    cJSON_Delete(a);
    return send_owned(req, 200, s);
}

static esp_err_t media_get(httpd_req_t *req, const char *id)
{
    if (!media_id_ok(id)) return send_json(req, 400, "{\"error\":\"Bad image id\"}");
    char path[128], type[64] = "application/octet-stream";
    snprintf(path, sizeof(path), "/data/media/%s.type", id);
    char *t = storage_read(path, NULL);
    if (t) { snprintf(type, sizeof(type), "%s", t); free(t); }
    snprintf(path, sizeof(path), "/data/media/%s", id);
    if (send_file(req, path, type, false, false) == ESP_OK) return ESP_OK;
    return send_json(req, 404, "{\"error\":\"Not found\"}");
}

static esp_err_t media_save(httpd_req_t *req, const char *id)
{
    if (!media_id_ok(id)) return send_json(req, 400, "{\"error\":\"Bad image id\"}");
    size_t len;
    char *body = read_body(req, MAX_MEDIA, &len);
    if (!body) return ESP_OK;
    size_t used, total;
    storage_usage("data", &used, &total);
    if (used + len + 65536 > total) { free(body); return send_json(req, 413, "{\"error\":\"The screen has no room for more pictures\"}"); }
    char path[128], type[64] = "application/octet-stream";
    httpd_req_get_hdr_value_str(req, "Content-Type", type, sizeof(type));
    snprintf(path, sizeof(path), "/data/media/%s", id);
    bool ok = storage_write(path, body, len);
    free(body);
    snprintf(path, sizeof(path), "/data/media/%s.type", id);
    ok = ok && storage_write(path, type, strlen(type));
    if (!ok) return send_json(req, 500, "{\"error\":\"Could not save the picture\"}");
    char reply[128];
    snprintf(reply, sizeof(reply), "{\"id\":\"%s\"}", id);
    config_generation++;            /* a notice may show this picture */
    return send_json(req, 200, reply);
}

static esp_err_t media_delete(httpd_req_t *req, const char *id)
{
    if (!media_id_ok(id)) return send_json(req, 400, "{\"error\":\"Bad image id\"}");
    char path[128];
    snprintf(path, sizeof(path), "/data/media/%s", id);
    unlink(path);
    snprintf(path, sizeof(path), "/data/media/%s.type", id);
    unlink(path);
    return send_json(req, 200, "{\"ok\":true}");
}

/* ---- the handlers ---- */

static esp_err_t config_put(httpd_req_t *req)
{
    char *body = read_body(req, MAX_CONFIG, NULL);
    if (!body) return ESP_OK;
    cJSON *o = cJSON_Parse(body);
    free(body);
    cJSON *cfg = o ? cJSON_DetachItemFromObjectCaseSensitive(o, "config") : NULL;
    cJSON_Delete(o);
    if (!cJSON_IsObject(cfg)) { cJSON_Delete(cfg); return send_json(req, 400, "{\"error\":\"Expected {\\\"config\\\": ...}\"}"); }
    int rev = config_replace(cfg, false);
    if (rev < 0) return send_json(req, 500, "{\"error\":\"Could not save\"}");
    char reply[32];
    snprintf(reply, sizeof(reply), "{\"rev\":%d}", rev);
    return send_json(req, 200, reply);
}

static esp_err_t wifi_post(httpd_req_t *req)
{
    char *body = read_body(req, 4096, NULL);
    if (!body) return ESP_OK;
    cJSON *o = cJSON_Parse(body);
    free(body);
    const char *action = jstr(o, "action", "");
    esp_err_t e;
    if (!strcmp(action, "apply")) {
        const char *ssid = jstr(o, "ssid", ""), *pass = jstr(o, "password", "");
        if (!*ssid || strlen(ssid) > 32 || strlen(pass) > 63 || (*pass && strlen(pass) < 8)) e = send_json(req, 400, "{\"error\":\"Check the name and password\"}");
        else { wifi_save(ssid, pass, jbool(o, "join", false)); e = send_json(req, 200, "{\"ok\":true}"); }
    } else if (!strcmp(action, "connect")) { wifi_connect_saved(jstr(o, "name", "")); e = send_json(req, 200, "{\"ok\":true}"); }
    else if (!strcmp(action, "forget")) { wifi_forget(jstr(o, "name", "")); e = send_json(req, 200, "{\"ok\":true}"); }
    else if (!strcmp(action, "refresh")) { wifi_scan_start(); e = send_json(req, 200, "{\"ok\":true}"); }
    else e = send_json(req, 400, "{\"error\":\"Unknown action\"}");
    cJSON_Delete(o);
    return e;
}

static esp_err_t device_get(httpd_req_t *req)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "id", link_id());
    cJSON_AddStringToObject(o, "version", FW_VERSION);
    cJSON_AddStringToObject(o, "model", "Waveshare ESP32-S3-Touch-LCD-4B");
    cJSON_AddStringToObject(o, "server", link_server());
    cJSON_AddStringToObject(o, "link", link_status());
    cJSON_AddStringToObject(o, "ip", wifi_ip());
    cJSON_AddStringToObject(o, "wifi", wifi_ssid());
    cJSON_AddNumberToObject(o, "uptime", (double)(esp_timer_get_time() / 1000000));
    cJSON_AddNumberToObject(o, "freeHeap", (double)heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return send_owned(req, 200, s);
}

static bool send_piece(void *req, const void *data, size_t len) { return httpd_resp_send_chunk(req, data, len) == ESP_OK; }

static esp_err_t handle(httpd_req_t *req)
{
    char path[256], query[256] = "";
    const char *q = strchr(req->uri, '?');
    size_t n = q ? (size_t)(q - req->uri) : strlen(req->uri);
    if (n >= sizeof(path)) return send_json(req, 404, "{\"error\":\"Not found\"}");
    memcpy(path, req->uri, n);
    path[n] = 0;
    url_decode(path);
    if (q) snprintf(query, sizeof(query), "%s", q + 1);
    int m = req->method;
    bool get = m == HTTP_GET || m == HTTP_HEAD;
    bool change = m == HTTP_PUT || m == HTTP_POST || m == HTTP_DELETE;
    char arg[128];

    if (change && strcmp(path, "/api/auth")) {
        int pin = check_pin(req);
        if (pin) {
            /* Let the request body go, so the connection stays usable. */
            char sink[512];
            int left = req->content_len;
            while (left > 0) { int r = httpd_req_recv(req, sink, left < (int)sizeof(sink) ? left : (int)sizeof(sink)); if (r <= 0) break; left -= r; }
            return refuse(req, pin);
        }
    }

    if (!strcmp(path, "/")) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/manage.html");
        return httpd_resp_send(req, NULL, 0);
    }
    if (!strcmp(path, "/api/config")) {
        if (get) return send_owned(req, 200, config_state_json());
        if (m == HTTP_PUT) return config_put(req);
    }
    if (!strcmp(path, "/api/auth") && get) {
        char pin[16] = "", body[96];
        bool given = httpd_req_get_hdr_value_str(req, "X-STOS-PIN", pin, sizeof(pin)) == ESP_OK && pin[0];
        bool ok = true; (void)given;
        snprintf(body, sizeof(body), "{\"required\":false,\"ok\":%s,\"retryAfter\":%d}", ok ? "true" : "false", retry_after());
        return send_json(req, 200, body);
    }
    /* Open a settings page on the screen, to check it with a screenshot: POST /api/ui?open=menu|wifi|location|prayer|volume|dimming|close */
    if (!strcmp(path, "/api/ui") && m == HTTP_POST) {
        static const char *const names[] = {"wifi", "location", "prayer", "volume", "dimming", "menu"};
        int p = -1;
        if (httpd_query_key_value(query, "open", arg, sizeof(arg)) == ESP_OK)
            for (int i = 0; i < 6; i++) if (!strcmp(arg, names[i])) p = i;
        if (!bsp_display_lock(2000)) return send_json(req, 503, "{\"error\":\"Busy\"}");
        if (httpd_query_key_value(query, "page", arg, sizeof(arg)) == ESP_OK) {
            ui_close_overlays();
            int pg = atoi(arg);
            ui_home_page(pg);
            if (httpd_query_key_value(query, "months", arg, sizeof(arg)) == ESP_OK) home_cal_step(pg, atoi(arg));
        }
        else if (p < 0) ui_close_overlays(); else settings_show(p);
        bsp_display_unlock();
        return send_json(req, 200, "{\"ok\":true}");
    }
    /* Try the Azan: POST /api/azan?prayer=fajr|dhuhr|asr|maghrib|isha, DELETE to stop. */
    if (!strcmp(path, "/api/azan") && change) {
        if (m == HTTP_DELETE) { azan_stop(); return send_json(req, 200, "{\"ok\":true}"); }
        int p = DHUHR;
        if (httpd_query_key_value(query, "prayer", arg, sizeof(arg)) == ESP_OK)
            for (int i = 0; i < PRAYER_N; i++) if (!strcmp(arg, PRAYER_ID[i])) p = i;
        if (!azan_ready(p)) return send_json(req, 404, "{\"error\":\"No Azan loaded\"}");
        return azan_play(p) ? send_json(req, 200, "{\"ok\":true}") : send_json(req, 409, "{\"error\":\"Already playing\"}");
    }
    if (!strcmp(path, "/api/media")) {
        if (get) return media_list(req);
        if (m == HTTP_POST) {
            if (httpd_query_key_value(query, "id", arg, sizeof(arg)) != ESP_OK) arg[0] = 0;
            url_decode(arg);
            return media_save(req, arg);
        }
    }
    if (!strncmp(path, "/api/media/", 11) && m == HTTP_DELETE) return media_delete(req, path + 11);
    if (!strncmp(path, "/media/", 7) && get) return media_get(req, path + 7);
    if (!strcmp(path, "/api/places") && get) {
        char q2[160] = "", lim[8] = "";
        httpd_query_key_value(query, "q", q2, sizeof(q2));
        httpd_query_key_value(query, "limit", lim, sizeof(lim));
        url_decode(q2);
        return send_owned(req, 200, places_search_json(q2, atoi(lim)));
    }
    if (!strcmp(path, "/api/wifi/status") && get) return send_owned(req, 200, wifi_status_json());
    if (!strcmp(path, "/api/wifi/scan") && get) return send_owned(req, 200, wifi_scan_json());
    if (!strcmp(path, "/api/wifi") && m == HTTP_POST) return wifi_post(req);
    if (!strcmp(path, "/api/device") && get) return device_get(req);
    if (!strcmp(path, "/api/screenshot") && get) {
        int pin = check_pin(req);
        if (pin) return refuse(req, pin);
        httpd_resp_set_type(req, "image/bmp");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        if (!ui_screenshot_send(send_piece, req)) return send_json(req, 500, "{\"error\":\"No screenshot\"}");
        return httpd_resp_send_chunk(req, NULL, 0);
    }
    if (get && strncmp(path, "/api/", 5)) return static_file(req, path);
    return send_json(req, 404, "{\"error\":\"Not found\"}");
}

void web_start(void)
{
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.uri_match_fn = httpd_uri_match_wildcard;
    c.stack_size = 8192;
    c.task_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;     /* internal RAM is short; the cache stays on during flash writes */
    c.max_open_sockets = 7;
    c.lru_purge_enable = true;
    c.recv_wait_timeout = 15;
    c.send_wait_timeout = 15;
    httpd_handle_t server;
    if (httpd_start(&server, &c) != ESP_OK) { ESP_LOGE(TAG, "could not start"); return; }
    httpd_method_t methods[] = {HTTP_GET, HTTP_HEAD, HTTP_PUT, HTTP_POST, HTTP_DELETE};
    for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]); i++) {
        httpd_uri_t u = {.uri = "/*", .method = methods[i], .handler = handle};
        httpd_register_uri_handler(server, &u);
    }
    ESP_LOGI(TAG, "settings page on port 80");
}
