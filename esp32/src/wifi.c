/* Wi-Fi: joins a saved network that is in range (strongest first), keeps trying, and sets the clock (SNTP).
 * Saved networks live in NVS ("wifi"/"saved", a JSON list). The network saved by the factory firmware is taken over
 * on the first start. No hotspot: a screen without a usable network asks on its touchscreen (ui.c). */
#include <string.h>
#include <time.h>
#include "cJSON.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs.h"
#include "link.h"
#include "wifi.h"

static const char *TAG = "wifi";

typedef struct {
    char ssid[33];
    char pass[65];
} saved_t;

static SemaphoreHandle_t lock;
static saved_t saved[WIFI_MAX_SAVED];
static int saved_n;
static wifi_net_t scanned[24];
static int scanned_n;
static volatile bool connected, scanning, connecting;
static char ip[16], ssid_now[33], last_msg[96], wanted[33];
static int64_t last_at;
static int8_t rssi_now;
static esp_netif_t *netif;
static TaskHandle_t task;
static bool sntp_started;

static void note(const char *fmt, const char *arg)
{
    snprintf(last_msg, sizeof(last_msg), fmt, arg);
    last_at = esp_timer_get_time() / 1000;
    ESP_LOGI(TAG, "%s", last_msg);
}

static void persist(void)
{
    cJSON *a = cJSON_CreateArray();
    for (int i = 0; i < saved_n; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "ssid", saved[i].ssid);
        cJSON_AddStringToObject(o, "pass", saved[i].pass);
        cJSON_AddItemToArray(a, o);
    }
    char *s = cJSON_PrintUnformatted(a);
    cJSON_Delete(a);
    nvs_handle_t h;
    if (s && nvs_open("wifi", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "saved", s);
        nvs_commit(h);
        nvs_close(h);
    }
    cJSON_free(s);
}

static void load(void)
{
    nvs_handle_t h;
    size_t len = 0;
    saved_n = 0;
    /* Keyed on "saved", not on the namespace: the factory firmware already made a "wifi" namespace. */
    bool have = false;
    if (nvs_open("wifi", NVS_READONLY, &h) == ESP_OK) {
        have = nvs_get_str(h, "saved", NULL, &len) == ESP_OK;
        if (!have) nvs_close(h);
    }
    if (have) {
        if (len) {
            char *s = malloc(len);
            nvs_get_str(h, "saved", s, &len);
            cJSON *a = cJSON_Parse(s);
            free(s);
            const cJSON *o;
            cJSON_ArrayForEach(o, a) {
                if (saved_n >= WIFI_MAX_SAVED) break;
                const cJSON *n = cJSON_GetObjectItem(o, "ssid"), *p = cJSON_GetObjectItem(o, "pass");
                if (!cJSON_IsString(n) || !*n->valuestring) continue;
                snprintf(saved[saved_n].ssid, 33, "%s", n->valuestring);
                snprintf(saved[saved_n].pass, 65, "%s", cJSON_IsString(p) ? p->valuestring : "");
                saved_n++;
            }
            cJSON_Delete(a);
        }
        nvs_close(h);
        return;
    }
    /* First start: take over the network the factory firmware saved (kept by the Wi-Fi driver in NVS). */
    wifi_config_t c = {0};
    if (esp_wifi_get_config(WIFI_IF_STA, &c) == ESP_OK && c.sta.ssid[0]) {
        snprintf(saved[0].ssid, 33, "%s", (char *)c.sta.ssid);
        snprintf(saved[0].pass, 65, "%s", (char *)c.sta.password);
        saved_n = 1;
        ESP_LOGI(TAG, "took over the saved network \"%s\"", saved[0].ssid);
    } else {
        ESP_LOGW(TAG, "no saved network to take over");
    }
    persist();
}

static void on_time(struct timeval *tv)
{
    char buf[32];
    struct tm t;
    gmtime_r(&tv->tv_sec, &t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &t);
    ESP_LOGI(TAG, "clock set: %s", buf);
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = data;
        bool was = connected;
        connected = false;
        ip[0] = 0;
        if (connecting) {
            const char *why = d->reason == WIFI_REASON_AUTH_FAIL || d->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                              d->reason == WIFI_REASON_HANDSHAKE_TIMEOUT || d->reason == 15
                                  ? "Wrong password for \"%s\"" : "Could not join \"%s\"";
            note(why, wanted);
        } else if (was) {
            note("Lost the connection to \"%s\"", ssid_now);
        }
        connecting = false;
        ssid_now[0] = 0;
        if (task) xTaskNotifyGive(task);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        uint16_t n = 0;
        esp_wifi_scan_get_ap_num(&n);
        wifi_ap_record_t *recs = calloc(n ? n : 1, sizeof(*recs));
        esp_wifi_scan_get_ap_records(&n, recs);
        xSemaphoreTake(lock, portMAX_DELAY);
        scanned_n = 0;
        for (int i = 0; i < n && scanned_n < (int)(sizeof(scanned) / sizeof(scanned[0])); i++) {   /* records come strongest first */
            if (!recs[i].ssid[0]) continue;
            bool dup = false;
            for (int j = 0; j < scanned_n; j++) if (!strcmp(scanned[j].ssid, (char *)recs[i].ssid)) dup = true;
            if (dup) continue;
            snprintf(scanned[scanned_n].ssid, 33, "%s", (char *)recs[i].ssid);
            scanned[scanned_n].rssi = recs[i].rssi;
            scanned[scanned_n].secure = recs[i].authmode != WIFI_AUTH_OPEN;
            scanned_n++;
        }
        xSemaphoreGive(lock);
        free(recs);
        scanning = false;
        if (task) xTaskNotifyGive(task);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&e->ip_info.ip));
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            snprintf(ssid_now, sizeof(ssid_now), "%s", (char *)ap.ssid);
            rssi_now = ap.rssi;
        }
        connected = true;
        connecting = false;
        note("Connected to \"%s\"", ssid_now);
        if (!sntp_started) {
            esp_sntp_config_t c = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2, ESP_SNTP_SERVER_LIST("pool.ntp.org", "time.google.com"));
            c.sync_cb = on_time;
            esp_netif_sntp_init(&c);
            sntp_started = true;
        }
        link_wake();
    }
}

static bool join(const char *ssid, const char *pass)
{
    wifi_config_t c = {0};
    snprintf((char *)c.sta.ssid, sizeof(c.sta.ssid), "%s", ssid);
    snprintf((char *)c.sta.password, sizeof(c.sta.password), "%s", pass);
    c.sta.threshold.authmode = pass[0] ? WIFI_AUTH_WEP : WIFI_AUTH_OPEN;
    c.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    c.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    snprintf(wanted, sizeof(wanted), "%s", ssid);
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(300));       /* let that disconnect's event pass before this attempt counts */
    connecting = true;
    esp_wifi_set_config(WIFI_IF_STA, &c);
    ESP_LOGI(TAG, "joining \"%s\"", ssid);
    esp_wifi_connect();
    for (int i = 0; i < 200 && connecting; i++) vTaskDelay(pdMS_TO_TICKS(100));    /* up to 20 s */
    if (connecting) { connecting = false; note("No answer from \"%s\"", ssid); esp_wifi_disconnect(); }
    return connected;
}

/* Saved networks that the last scan saw, strongest first; then the rest (a hidden network does not show in scans). */
static void try_saved(void)
{
    saved_t order[WIFI_MAX_SAVED];
    int n = 0;
    xSemaphoreTake(lock, portMAX_DELAY);
    for (int s = 0; s < scanned_n; s++)
        for (int i = 0; i < saved_n; i++)
            if (!strcmp(saved[i].ssid, scanned[s].ssid)) order[n++] = saved[i];
    for (int i = 0; i < saved_n; i++) {
        bool seen = false;
        for (int j = 0; j < n; j++) if (!strcmp(order[j].ssid, saved[i].ssid)) seen = true;
        if (!seen) order[n++] = saved[i];
    }
    xSemaphoreGive(lock);
    for (int i = 0; i < n && !connected; i++) join(order[i].ssid, order[i].pass);
}

static void loop(void *arg)
{
    int idle = 0;
    while (1) {
        if (!connected && saved_n && !connecting) {
            wifi_scan_start();
            for (int i = 0; i < 80 && scanning; i++) vTaskDelay(pdMS_TO_TICKS(100));
            try_saved();
            idle = 0;
        }
        if (connected) {
            wifi_ap_record_t ap;
            if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) rssi_now = ap.rssi;
        }
        /* Not connected: try again after 10 s at first, then every 30 s. */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(connected ? 30000 : idle++ < 3 ? 10000 : 30000));
    }
}

void wifi_start(void)
{
    lock = xSemaphoreCreateMutex();
    esp_netif_init();
    esp_event_loop_create_default();
    netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL);
    esp_wifi_set_mode(WIFI_MODE_STA);
    load();                                      /* after init: reads the driver's saved network on the first start */
    esp_wifi_set_storage(WIFI_STORAGE_RAM);      /* our own list from now on */
    char host[32];
    snprintf(host, sizeof(host), "prayer-%s", link_id_short());
    esp_netif_set_hostname(netif, host);
    esp_wifi_start();
    esp_wifi_set_ps(WIFI_PS_NONE);
    if (mdns_init() == ESP_OK) {
        mdns_hostname_set(host);
        mdns_instance_name_set("Prayer display");
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    }
    xTaskCreate(loop, "wifi", 4096, NULL, 4, &task);
}

bool wifi_connected(void) { return connected; }
const char *wifi_ip(void) { return ip; }
const char *wifi_ssid(void) { return ssid_now; }
int wifi_saved_count(void) { return saved_n; }
int wifi_signal(void)
{
    int q = 2 * (rssi_now + 100);
    return connected ? (q < 0 ? 0 : q > 100 ? 100 : q) : 0;
}

const char *wifi_last_message(int64_t *at_ms)
{
    if (at_ms) *at_ms = last_at;
    return last_msg;
}

void wifi_save(const char *ssid, const char *pass, bool now)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    int i;
    for (i = 0; i < saved_n; i++) if (!strcmp(saved[i].ssid, ssid)) break;
    if (i == saved_n) {
        if (saved_n == WIFI_MAX_SAVED) { memmove(saved, saved + 1, sizeof(saved_t) * (WIFI_MAX_SAVED - 1)); i = --saved_n; }
        saved_n++;
    }
    snprintf(saved[i].ssid, 33, "%s", ssid);
    snprintf(saved[i].pass, 65, "%s", pass ? pass : "");
    /* The newest goes first, so it is tried first when several are in range with equal signal. */
    saved_t s = saved[i];
    memmove(saved + 1, saved, sizeof(saved_t) * i);
    saved[0] = s;
    xSemaphoreGive(lock);
    persist();
    note("Saved \"%s\"", ssid);
    if (now) wifi_connect_saved(ssid);
    else if (!connected && task) xTaskNotifyGive(task);
}

void wifi_forget(const char *ssid)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    for (int i = 0; i < saved_n; i++)
        if (!strcmp(saved[i].ssid, ssid)) {
            memmove(saved + i, saved + i + 1, sizeof(saved_t) * (saved_n - i - 1));
            saved_n--;
            break;
        }
    xSemaphoreGive(lock);
    persist();
    note("Forgot \"%s\"", ssid);
}

typedef struct {
    char ssid[33], pass[65];
} join_req_t;

static void join_task(void *arg)
{
    join_req_t *r = arg;
    vTaskDelay(pdMS_TO_TICKS(500));       /* let the web request that asked for it answer first */
    if (!join(r->ssid, r->pass)) try_saved();
    free(r);
    vTaskDelete(NULL);
}

void wifi_connect_saved(const char *ssid)
{
    join_req_t *r = calloc(1, sizeof(*r));
    for (int i = 0; i < saved_n; i++)
        if (!strcmp(saved[i].ssid, ssid)) { snprintf(r->ssid, 33, "%s", saved[i].ssid); snprintf(r->pass, 65, "%s", saved[i].pass); }
    if (!r->ssid[0]) { free(r); return; }
    xTaskCreate(join_task, "wifi-join", 4096, r, 4, NULL);
}

void wifi_scan_start(void)
{
    if (scanning || connecting) return;
    scanning = true;
    wifi_scan_config_t sc = {.show_hidden = false};
    if (esp_wifi_scan_start(&sc, false) != ESP_OK) scanning = false;
}

int wifi_scan_results(wifi_net_t *out, int max)
{
    xSemaphoreTake(lock, portMAX_DELAY);
    int n = scanned_n < max ? scanned_n : max;
    memcpy(out, scanned, n * sizeof(*out));
    xSemaphoreGive(lock);
    return n;
}

char *wifi_status_json(void)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "supported", true);
    cJSON *c = cJSON_AddObjectToObject(o, "connection");
    if (connected) {
        cJSON_AddStringToObject(c, "ssid", ssid_now);
        cJSON_AddStringToObject(c, "name", ssid_now);
        cJSON_AddNumberToObject(c, "signal", wifi_signal());
        cJSON_AddStringToObject(c, "ip", ip);
    }
    cJSON *a = cJSON_AddArrayToObject(o, "saved");
    xSemaphoreTake(lock, portMAX_DELAY);
    for (int i = 0; i < saved_n; i++) {
        cJSON *n = cJSON_CreateObject();
        cJSON_AddStringToObject(n, "ssid", saved[i].ssid);
        cJSON_AddStringToObject(n, "name", saved[i].ssid);
        cJSON_AddBoolToObject(n, "ours", true);
        cJSON_AddItemToArray(a, n);
    }
    xSemaphoreGive(lock);
    if (last_msg[0]) {
        cJSON *l = cJSON_AddObjectToObject(o, "last");
        cJSON_AddNumberToObject(l, "at", (double)last_at);
        cJSON_AddStringToObject(l, "message", last_msg);
    }
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return s;
}

char *wifi_scan_json(void)
{
    cJSON *o = cJSON_CreateObject();
    cJSON *a = cJSON_AddArrayToObject(o, "networks");
    xSemaphoreTake(lock, portMAX_DELAY);
    for (int i = 0; i < scanned_n; i++) {
        cJSON *n = cJSON_CreateObject();
        int q = 2 * (scanned[i].rssi + 100);
        cJSON_AddStringToObject(n, "ssid", scanned[i].ssid);
        cJSON_AddNumberToObject(n, "signal", q < 0 ? 0 : q > 100 ? 100 : q);
        cJSON_AddBoolToObject(n, "secure", scanned[i].secure);
        cJSON_AddItemToArray(a, n);
    }
    xSemaphoreGive(lock);
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return s;
}
