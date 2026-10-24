#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "config.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "link.h"
#include "nvs.h"
#include "wifi.h"

static const char *TAG = "link";
static const char ID_ALPHABET[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";   /* no I, L, O, 0 or 1, which are misread */
#define MAX_REPLY (4 * 1024 * 1024)

static char id[12], pass[10], id_short[10], server[128], status[64] = "Starting";
static volatile bool registered, push_pending;
static int poll_seconds = 60, applied;
static TaskHandle_t task;

static void nvs_str(const char *key, char *out, size_t len)
{
    nvs_handle_t h;
    out[0] = 0;
    if (nvs_open("link", NVS_READONLY, &h) != ESP_OK) return;
    nvs_get_str(h, key, out, &len);
    nvs_close(h);
}

static void nvs_put(const char *key, const char *value, int num)
{
    nvs_handle_t h;
    if (nvs_open("link", NVS_READWRITE, &h) != ESP_OK) return;
    if (value) nvs_set_str(h, key, value); else nvs_set_i32(h, key, num);
    nvs_commit(h);
    nvs_close(h);
}

static bool valid_identity(void)
{
    if (strlen(id) != 11 || id[3] != '-' || id[7] != '-' || strlen(pass) != 9) return false;
    for (int i = 0; i < 11; i++) if (i != 3 && i != 7 && !isupper((unsigned char)id[i]) && !isdigit((unsigned char)id[i])) return false;
    for (int i = 0; i < 9; i++) if (!isdigit((unsigned char)pass[i])) return false;
    return true;
}

static void make_identity(void)
{
    int k = 0;
    for (int i = 0; i < 9; i++) {
        if (i == 3 || i == 6) id[k++] = '-';
        id[k++] = ID_ALPHABET[esp_random() % (sizeof(ID_ALPHABET) - 1)];
    }
    id[k] = 0;
    snprintf(pass, sizeof(pass), "%09lu", (unsigned long)(esp_random() % 1000000000UL));
    nvs_put("id", id, 0);
    nvs_put("pass", pass, 0);
    nvs_put("applied", NULL, 0);
    applied = 0;
    registered = false;
}

static void short_id(void)
{
    int k = 0;
    for (int i = 0; id[i]; i++) if (id[i] != '-') id_short[k++] = tolower((unsigned char)id[i]);
    id_short[k] = 0;
}

void link_init(void)
{
    nvs_str("id", id, sizeof(id));
    nvs_str("pass", pass, sizeof(pass));
    if (!valid_identity()) make_identity();
    short_id();
    char s[128];
    nvs_str("server", s, sizeof(s));
    /* A screen that was never told otherwise uses the live server; "-" means "no server" (set on purpose). */
    snprintf(server, sizeof(server), "%s", !s[0] ? DEFAULT_SERVER : strcmp(s, "-") ? s : "");
    nvs_handle_t h;
    if (nvs_open("link", NVS_READONLY, &h) == ESP_OK) {
        int32_t a = 0;
        nvs_get_i32(h, "applied", &a);
        applied = a;
        nvs_close(h);
    }
    ESP_LOGI(TAG, "screen %s, server %s", id, server[0] ? server : "(none)");
}

const char *link_id(void) { return id; }
const char *link_id_short(void) { return id_short; }
const char *link_pass(void) { return pass; }
const char *link_server(void) { return server; }
const char *link_status(void) { return status; }
void link_wake(void) { if (task) xTaskNotifyGive(task); }

bool link_set_server(const char *address, char *problem, int len)
{
    char clean[128];
    snprintf(clean, sizeof(clean), "%s", address ? address : "");
    for (int n = strlen(clean); n && (clean[n - 1] == '/' || clean[n - 1] == ' '); n--) clean[n - 1] = 0;
    if (clean[0]) {
        bool https = !strncmp(clean, "https://", 8);
        const char *host = clean + 7;
        bool local = !strncmp(clean, "http://", 7) && (!strncmp(host, "10.", 3) || !strncmp(host, "192.168.", 8) ||
                     !strncmp(host, "127.", 4) || !strncmp(host, "localhost", 9) || !strncmp(host, "172.", 4) || strstr(host, ".local"));
        if (!https && !local) {
            snprintf(problem, len, "The server address must start with https:// (http:// only for a server on this network).");
            return false;
        }
    }
    snprintf(server, sizeof(server), "%s", clean);
    nvs_put("server", clean[0] ? clean : "-", 0);
    nvs_put("applied", NULL, 0);
    applied = 0;
    registered = false;
    snprintf(status, sizeof(status), clean[0] ? "Connecting" : "No server set");
    link_wake();
    return true;
}

typedef struct {
    char *buf;
    int len;
} reply_t;

static esp_err_t on_data(esp_http_client_event_t *e)
{
    reply_t *r = e->user_data;
    if (e->event_id == HTTP_EVENT_ON_DATA && r->len + e->data_len < MAX_REPLY) {
        char *grown = heap_caps_realloc(r->buf, r->len + e->data_len + 1, MALLOC_CAP_SPIRAM);
        if (!grown) return ESP_FAIL;
        r->buf = grown;
        memcpy(r->buf + r->len, e->data, e->data_len);
        r->len += e->data_len;
        r->buf[r->len] = 0;
    }
    return ESP_OK;
}

/* One request; returns the HTTP status (or -1) and the reply as JSON (an empty object when it is not JSON). */
static int call(const char *method, const char *url, bool with_pass, const char *body, cJSON **json)
{
    reply_t r = {0};
    esp_http_client_config_t c = {
        .url = url,
        .timeout_ms = 20000,
        .event_handler = on_data,
        .user_data = &r,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 4096,
        .buffer_size_tx = 2048,
    };
    esp_http_client_handle_t h = esp_http_client_init(&c);
    esp_http_client_set_method(h, !strcmp(method, "POST") ? HTTP_METHOD_POST : !strcmp(method, "PUT") ? HTTP_METHOD_PUT : HTTP_METHOD_GET);
    if (with_pass) esp_http_client_set_header(h, "X-Screen-Pass", pass);
    if (body) {
        esp_http_client_set_header(h, "Content-Type", "application/json");
        esp_http_client_set_post_field(h, body, strlen(body));
    }
    esp_err_t err = esp_http_client_perform(h);
    int code = err == ESP_OK ? esp_http_client_get_status_code(h) : -1;
    if (err != ESP_OK) ESP_LOGW(TAG, "%s %s: %s", method, url, esp_err_to_name(err));
    esp_http_client_cleanup(h);
    *json = r.buf ? cJSON_Parse(r.buf) : NULL;
    if (!*json) *json = cJSON_CreateObject();
    free(r.buf);
    return code;
}

static void do_register(void)
{
    char url[192], body[64];
    snprintf(url, sizeof(url), "%s/api/screen/register", server);
    snprintf(body, sizeof(body), "{\"id\":\"%s\",\"pass\":\"%s\"}", id, pass);
    cJSON *j;
    int code = call("POST", url, false, body, &j);
    cJSON_Delete(j);
    if (code == 200) {
        registered = true;
        snprintf(status, sizeof(status), "Registered");
        ESP_LOGI(TAG, "registered as %s", id);
    } else if (code == 401) {
        ESP_LOGW(TAG, "the server says %s belongs to another screen: making a new ID", id);
        make_identity();
        short_id();
    } else {
        snprintf(status, sizeof(status), code < 0 ? "Cannot reach the server" : "The server said %d", code);
    }
}

static void push(void)
{
    char *cfg = config_saved_json();
    if (!cfg) return;
    size_t n = strlen(cfg) + 16;
    char *body = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
    snprintf(body, n, "{\"config\":%s}", cfg);
    free(cfg);
    char url[192];
    snprintf(url, sizeof(url), "%s/api/screen/%s/config", server, id);
    cJSON *j;
    int code = call("PUT", url, true, body, &j);
    free(body);
    if (code == 200) {
        applied = (int)jnum(j, "rev", applied);
        nvs_put("applied", NULL, applied);
        ESP_LOGI(TAG, "sent this screen's settings (server version %d)", applied);
    } else {
        ESP_LOGW(TAG, "sending the settings failed (%d)", code);
        push_pending = true;
    }
    cJSON_Delete(j);
}

static void cycle(void)
{
    if (!registered) do_register();
    if (!registered) return;
    if (push_pending) { push_pending = false; push(); }
    char url[224];
    snprintf(url, sizeof(url), "%s/api/screen/%s/pull?rev=%d&v=%s", server, id, applied, FW_VERSION);
    cJSON *j;
    int code = call("GET", url, true, NULL, &j);
    if (code == 404) { registered = false; snprintf(status, sizeof(status), "Registering again"); }
    else if (code == 401) { ESP_LOGW(TAG, "this ID belongs to another screen: making a new one"); make_identity(); short_id(); }
    else if (code == 429) snprintf(status, sizeof(status), "The server asked this screen to wait");
    else if (code != 200) snprintf(status, sizeof(status), code < 0 ? "Cannot reach the server" : "The server said %d", code);
    else {
        int p = (int)jnum(j, "pollSeconds", 60);
        poll_seconds = p < 15 ? 15 : p > 3600 ? 3600 : p;
        int rev = (int)jnum(j, "rev", 0);
        cJSON *cfg = cJSON_GetObjectItemCaseSensitive(j, "config");
        if (cJSON_IsObject(cfg)) {
            cJSON_DetachItemViaPointer(j, cfg);
            if (config_replace(cfg, true) >= 0) {
                applied = rev;
                nvs_put("applied", NULL, applied);
                ESP_LOGI(TAG, "took new settings from the server (version %d)", rev);
            }
        } else if (rev == 0) {
            push();                                   /* the server has nothing yet: give it what this screen has */
        } else if (rev != applied) {
            applied = rev;
            nvs_put("applied", NULL, applied);
        }
        snprintf(status, sizeof(status), "Connected");
    }
    cJSON_Delete(j);
}

static void on_local_save(void)
{
    push_pending = true;
    link_wake();
}

static void loop(void *arg)
{
    while (1) {
        if (server[0] && wifi_connected()) cycle();
        else if (!server[0]) snprintf(status, sizeof(status), "No server set");
        else snprintf(status, sizeof(status), "Waiting for Wi-Fi");
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(poll_seconds * 1000));
    }
}

void link_start(void)
{
    config_on_local_save(on_local_save);
    xTaskCreatePinnedToCore(loop, "link", 8192, NULL, 3, &task, 0);
}
