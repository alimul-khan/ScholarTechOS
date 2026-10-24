#pragma once
#include <stdbool.h>

#define FW_VERSION "esp32-1.0"
#define DEFAULT_SERVER "https://prayerdisplay.online"

/* This screen's ID (ABC-123-XYZ) and 9-digit passphrase, and its link to the settings server (prayerdisplay.online):
 * registers, asks for settings at start-up and every pollSeconds (the server decides; 15 s to 1 h), sends changes made
 * on the local settings page. The same protocol as the Android app's ScreenLink.java (server/SCREENS.md). */
void link_init(void);            /* identity only: call early (the hostname uses it) */
void link_start(void);
void link_wake(void);
const char *link_id(void);
const char *link_id_short(void);  /* the ID without dashes, lower case, for the hostname */
const char *link_pass(void);
const char *link_server(void);
const char *link_status(void);
bool link_set_server(const char *address, char *problem, int len);
