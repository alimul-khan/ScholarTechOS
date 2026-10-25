#pragma once
#include <stddef.h>
#include <stdint.h>

/* The screen: the three display pages (prayer times, notices, prayer times + notices) in the theme chosen in the
 * settings, rotating as set under Display; an admin screen (hold the screen for 3 seconds) with the Screen ID,
 * passphrase, settings address and Wi-Fi; and the Wi-Fi setup (network list and keyboard). */
void ui_start(void);
/* The screen as a 16-bit BMP (for checking the layout from a computer: GET /api/screenshot), handed to write() in
 * pieces. false when it could not be taken or written. */
bool ui_screenshot_send(bool (*write)(void *ctx, const void *data, size_t len), void *ctx);

/* Closes the settings, Wi-Fi and admin screens (back to the home pages). Hold the display lock. */
void ui_close_overlays(void);
/* Shows a home page now (0 prayer, 1 weather, 2 Gregorian, 3 Hijri). Hold the display lock. */
void ui_home_page(int page);
