#pragma once
#include <stdbool.h>
#include <stdint.h>

#define WIFI_MAX_SAVED 8

typedef struct {
    char ssid[33];
    int8_t rssi;
    bool secure;
} wifi_net_t;

void wifi_start(void);
bool wifi_connected(void);
const char *wifi_ip(void);                /* "" when not connected */
const char *wifi_ssid(void);              /* the network joined, "" when none */
int wifi_signal(void);                    /* 0-100 */
int wifi_saved_count(void);
/* Saves (or updates) a network; join: connect to it now. */
void wifi_save(const char *ssid, const char *pass, bool join);
void wifi_forget(const char *ssid);
void wifi_connect_saved(const char *ssid);
/* Starts a scan (results arrive a few seconds later). */
void wifi_scan_start(void);
int wifi_scan_results(wifi_net_t *out, int max);   /* strongest first, one per name */
/* The last thing that happened ("Connected", "Wrong password"...), for the screen and the settings page. */
const char *wifi_last_message(int64_t *at_ms);
/* JSON for the settings page's Wi-Fi tab. Caller frees. */
char *wifi_status_json(void);
char *wifi_scan_json(void);
