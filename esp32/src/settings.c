/* Settings on the screen, opened with the gear: five large tiles (Wi-Fi, location, prayer times, volume, dimming),
 * each a simple page of large buttons, in the Home Display app's emerald and gold. Changes are saved like the
 * settings page saves them (config_replace), so prayerdisplay.online follows. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "azan.h"
#include "cJSON.h"
#include "config.h"
#include "fonts.h"
#include "lvgl.h"
#include "places.h"
#include "schedule.h"
#include "settings.h"
#include "weather.h"
#include "wifi.h"

LV_FONT_DECLARE(serif_temp);
LV_FONT_DECLARE(serif_b30);
LV_FONT_DECLARE(serif_b18);
LV_FONT_DECLARE(serif_r17);
LV_FONT_DECLARE(icons_26);

#define DEEP 0x03251F
#define PANEL 0x0A4436
#define GOLD 0xC19749
#define GOLD_TEXT 0xECCF99
#define WHITE 0xFFFBEF
#define SOFT 0xD3D3B6
#define I_GEAR "\xEF\x80\x93"
#define I_WIFI "\xEF\x87\xAB"
#define I_PIN "\xEF\x8F\x85"
#define I_MOSQUE "\xEF\x99\xB8"
#define I_VOLUME "\xEF\x80\xA8"
#define I_MOON "\xEF\x86\x86"
#define I_CLOSE "\xEF\x80\x8D"
#define I_BACK "\xEF\x81\x93"
#define I_NEXT "\xEF\x81\x94"
#define I_PLUS "\xEF\x81\xA7"
#define I_MINUS "\xEF\x81\xA8"
#define I_SEARCH "\xEF\x80\x82"

static void (*open_wifi)(void);
static lv_obj_t *menu, *loc, *pray, *vol, *dim;
static lv_obj_t *summary[5];

/* ---- saving --------------------------------------------------------------------------------------------------- */

static cJSON *edit_begin(void)
{
    char *s = config_saved_json();
    cJSON *c = s ? cJSON_Parse(s) : NULL;
    free(s);
    return c ? c : cJSON_CreateObject();
}

/* Sets "a.b.c" in the tree, making the objects on the way. Takes the item. */
static void put(cJSON *c, const char *path, cJSON *item)
{
    char key[40];
    const char *dot;
    while ((dot = strchr(path, '.'))) {
        snprintf(key, sizeof(key), "%.*s", (int)(dot - path), path);
        cJSON *next = cJSON_GetObjectItemCaseSensitive(c, key);
        if (!cJSON_IsObject(next)) { cJSON_DeleteItemFromObjectCaseSensitive(c, key); next = cJSON_AddObjectToObject(c, key); }
        c = next;
        path = dot + 1;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(c, path);
    cJSON_AddItemToObject(c, path, item);
}

static void edit_commit(cJSON *c) { config_replace(c, false); }

/* Reading the current (merged) settings. */
static double cfg_num(const char *path, double fallback) { config_lock(); double v = jnum(config_get(), path, fallback); config_unlock(); return v; }
static bool cfg_bool(const char *path, bool fallback) { config_lock(); bool v = jbool(config_get(), path, fallback); config_unlock(); return v; }
static void cfg_str(const char *path, char *out, size_t len) { config_lock(); snprintf(out, len, "%s", jstr(config_get(), path, "")); config_unlock(); }

/* ---- pieces ---------------------------------------------------------------------------------------------------- */

static lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, int w, lv_text_align_t align, const char *s)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, align, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_width(l, w);
    lv_label_set_text(l, s);
    return l;
}

static lv_obj_t *screen(lv_obj_t *parent)
{
    lv_obj_t *s = lv_obj_create(parent);
    lv_obj_remove_style_all(s);
    lv_obj_set_size(s, 480, 480);
    lv_obj_set_style_bg_color(s, lv_color_hex(DEEP), 0);
    lv_obj_set_style_bg_grad_color(s, lv_color_hex(0x0B4A3B), 0);
    lv_obj_set_style_bg_grad_dir(s, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);        /* takes the taps, so the page under it does not */
    lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s, LV_OBJ_FLAG_HIDDEN);
    return s;
}

/* A large button: emerald with a gold edge; "on" fills it with gold. */
static lv_obj_t *button(lv_obj_t *parent, int x, int y, int w, int h, const lv_font_t *font, const char *label, lv_event_cb_t cb, void *data)
{
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(PANEL), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x16624F), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(b, lv_color_hex(GOLD), 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0xDDB46A), LV_STATE_CHECKED);
    lv_obj_set_style_text_color(b, lv_color_hex(WHITE), 0);
    lv_obj_set_style_text_color(b, lv_color_hex(0x04332A), LV_STATE_CHECKED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, data);
    if (label) {
        lv_obj_t *l = lv_label_create(b);
        lv_obj_set_style_text_font(l, font, 0);
        lv_label_set_text(l, label);
        lv_obj_center(l);
    }
    return b;
}

static void button_text(lv_obj_t *b, const char *s) { lv_label_set_text(lv_obj_get_child(b, 0), s); }
static void button_on(lv_obj_t *b, bool on) { lv_obj_set_state(b, LV_STATE_CHECKED, on); }

static void show(lv_obj_t *s)
{
    for (lv_obj_t *const *p = (lv_obj_t *const[]){menu, loc, pray, vol, dim, NULL}; *p; p++) lv_obj_set_flag(*p, LV_OBJ_FLAG_HIDDEN, *p != s);
}

static void refresh_menu(void);
static void back_to_menu(lv_event_t *e);

/* Title and the back (or close) button. */
static void header(lv_obj_t *s, const char *title, bool close)
{
    text(s, &serif_b30, GOLD_TEXT, 80, 16, 320, LV_TEXT_ALIGN_CENTER, title);
    lv_obj_t *b = button(s, 12, 10, 64, 50, &icons_26, close ? I_CLOSE : I_BACK, close ? NULL : back_to_menu, NULL);
    if (close) lv_obj_add_event_cb(b, settings_close_cb, LV_EVENT_CLICKED, NULL);
}

/* ---- the menu -------------------------------------------------------------------------------------------------- */

static void open_page(lv_event_t *e);

static void tile(int i, const char *icon, const char *title)
{
    lv_obj_t *b = button(menu, 16, 72 + i * 80, 448, 70, NULL, NULL, open_page, (void *)(intptr_t)i);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &icons_26, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(GOLD_TEXT), 0);
    lv_label_set_text(l, icon);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 18, 0);
    text(b, &serif_b18, WHITE, 66, 10, 330, LV_TEXT_ALIGN_LEFT, title);
    summary[i] = text(b, &serif_r17, SOFT, 66, 36, 330, LV_TEXT_ALIGN_LEFT, "");
    l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &icons_26, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(GOLD), 0);
    lv_label_set_text(l, I_NEXT);
    lv_obj_align(l, LV_ALIGN_RIGHT_MID, -16, 0);
}

static const char *const METHOD_ID[4] = {"NorthAmerica", "MuslimWorldLeague", "Egyptian", "UmmAlQura"};
static const char *const METHOD_NAME[4] = {"ISNA", "MWL", "Egypt", "Umm al-Qura"};
static const int LEVELS[] = {2, 3, 5, 7, 10, 15, 20, 30, 40, 50, 70};
#define LEVEL_N (int)(sizeof(LEVELS) / sizeof(LEVELS[0]))

static void time_text(int min, char *out, size_t len)
{
    int h = min / 60 % 24, m = min % 60;
    if (cfg_bool("options.clock24", false)) snprintf(out, len, "%02d:%02d", h, m);
    else snprintf(out, len, "%d:%02d %s", h % 12 ? h % 12 : 12, m, h < 12 ? "AM" : "PM");
}

static int hhmm(const char *s, int fallback)
{
    int h, m;
    return s && sscanf(s, "%d:%d", &h, &m) == 2 ? h * 60 + m : fallback;
}

static void refresh_menu(void)
{
    char buf[96], a[48], b[48];
    lv_label_set_text(summary[0], wifi_connected() ? wifi_ssid() : "Not connected");
    cfg_str("calc.city", a, sizeof(a));
    cfg_str("calc.country", b, sizeof(b));
    snprintf(buf, sizeof(buf), "%s%s%s", a, *a && *b ? ", " : "", b);
    lv_label_set_text(summary[1], *buf ? buf : "Not set");
    cfg_str("calc.method", a, sizeof(a));
    const char *m = "ISNA";
    for (int i = 0; i < 4; i++) if (!strcmp(a, METHOD_ID[i])) m = METHOD_NAME[i];
    cfg_str("calc.madhab", b, sizeof(b));
    snprintf(buf, sizeof(buf), "%s \xe2\x80\xa2 %s Asr", m, !strcmp(b, "hanafi") ? "Hanafi" : "Standard");
    lv_label_set_text(summary[2], buf);
    snprintf(buf, sizeof(buf), "Azan %d%%", (int)cfg_num("options.volume", 100));
    lv_label_set_text(summary[3], buf);
    if (!cfg_bool("nightDim.enabled", false)) lv_label_set_text(summary[4], "Off");
    else {
        cfg_str("nightDim.start", a, sizeof(a));
        cfg_str("nightDim.end", b, sizeof(b));
        char s[16], e[16];
        time_text(hhmm(a, 22 * 60), s, sizeof(s));
        time_text(hhmm(b, 6 * 60), e, sizeof(e));
        snprintf(buf, sizeof(buf), "%s to %s \xe2\x80\xa2 %d%%", s, e, 100 - (int)cfg_num("nightDim.darkness", 95));
        lv_label_set_text(summary[4], buf);
    }
}

/* ---- location ------------------------------------------------------------------------------------------------- */

static struct {
    lv_obj_t *ta, *kb, *result[3], *note;
    cJSON *found;
} lp;

static void loc_pick(lv_event_t *e)
{
    const cJSON *r = cJSON_GetArrayItem(lp.found, (int)(intptr_t)lv_event_get_user_data(e));
    if (!r) return;
    cJSON *c = edit_begin();
    put(c, "calc.city", cJSON_CreateString(jstr(r, "name", "")));
    put(c, "calc.country", cJSON_CreateString(jstr(r, "country", "")));
    put(c, "calc.lat", cJSON_CreateString(jstr(r, "latitude", "")));
    put(c, "calc.lng", cJSON_CreateString(jstr(r, "longitude", "")));
    put(c, "prayerSource", cJSON_CreateString("calculated"));
    if (*jstr(r, "zone", "")) put(c, "options.timeZone", cJSON_CreateString(jstr(r, "zone", "")));
    edit_commit(c);
    weather_wake();
    back_to_menu(NULL);
}

static void loc_search(void)
{
    const char *q = lv_textarea_get_text(lp.ta);
    if (strlen(q) < 2) { lv_label_set_text(lp.note, "Type at least 2 letters of the city"); return; }
    if (!wifi_connected()) { lv_label_set_text(lp.note, "Connect to Wi-Fi first: the search is online."); return; }
    char *json = places_search_json(q, 3);
    cJSON_Delete(lp.found);
    cJSON *o = json ? cJSON_Parse(json) : NULL;
    free(json);
    lp.found = o ? cJSON_DetachItemFromObject(o, "results") : NULL;
    cJSON_Delete(o);
    int n = cJSON_GetArraySize(lp.found);
    for (int i = 0; i < 3; i++) {
        const cJSON *r = cJSON_GetArrayItem(lp.found, i);
        lv_obj_set_flag(lp.result[i], LV_OBJ_FLAG_HIDDEN, !r);
        if (!r) continue;
        char buf[160];
        const char *region = jstr(r, "region", "");
        snprintf(buf, sizeof(buf), "%s%s%s, %s", jstr(r, "name", ""), *region ? ", " : "", region, jstr(r, "country", ""));
        lv_label_set_text(lv_obj_get_child(lp.result[i], 0), buf);
    }
    lv_label_set_text(lp.note, n ? "Tap your city" : "No city found. Try another spelling.");
}

static void loc_search_cb(lv_event_t *e) { loc_search(); }
static void loc_kb(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_READY) loc_search(); else if (lv_event_get_code(e) == LV_EVENT_CANCEL) back_to_menu(NULL); }

static void loc_build(lv_obj_t *parent)
{
    loc = screen(parent);
    header(loc, "Location", false);
    lp.ta = lv_textarea_create(loc);
    lv_obj_set_pos(lp.ta, 16, 70);
    lv_obj_set_size(lp.ta, 344, 54);
    lv_textarea_set_one_line(lp.ta, true);
    lv_textarea_set_placeholder_text(lp.ta, "City name");
    lv_obj_set_style_text_font(lp.ta, &serif_b18, 0);
    lv_obj_set_style_radius(lp.ta, 12, 0);
    lv_obj_set_style_border_color(lp.ta, lv_color_hex(GOLD), 0);
    button(loc, 368, 70, 96, 54, &icons_26, I_SEARCH, loc_search_cb, NULL);
    for (int i = 0; i < 3; i++) {
        lp.result[i] = button(loc, 16, 132 + i * 50, 448, 44, &serif_b18, "", loc_pick, (void *)(intptr_t)i);
        lv_obj_t *l = lv_obj_get_child(lp.result[i], 0);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, 420);
        lv_obj_add_flag(lp.result[i], LV_OBJ_FLAG_HIDDEN);
    }
    lp.note = text(loc, &serif_r17, SOFT, 16, 140, 448, LV_TEXT_ALIGN_CENTER, "");
    lp.kb = lv_keyboard_create(loc);
    lv_obj_set_size(lp.kb, 480, 196);
    lv_obj_align(lp.kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(lp.kb, &font_text, 0);
    lv_keyboard_set_textarea(lp.kb, lp.ta);
    lv_obj_add_event_cb(lp.kb, loc_kb, LV_EVENT_ALL, NULL);
}

static void loc_open(void)
{
    char a[48], b[48], buf[120];
    lv_textarea_set_text(lp.ta, "");
    for (int i = 0; i < 3; i++) lv_obj_add_flag(lp.result[i], LV_OBJ_FLAG_HIDDEN);
    cfg_str("calc.city", a, sizeof(a));
    cfg_str("calc.country", b, sizeof(b));
    snprintf(buf, sizeof(buf), "Now: %s%s%s. Type a city to change it.", a, *b ? ", " : "", b);
    lv_label_set_text(lp.note, buf);
    lv_obj_set_y(lp.note, 236);
}

/* ---- prayer times -------------------------------------------------------------------------------------------- */

static lv_obj_t *method_btn[4], *asr_btn[2], *hijri_value;

static void pray_refresh(void)
{
    char a[40];
    cfg_str("calc.method", a, sizeof(a));
    for (int i = 0; i < 4; i++) button_on(method_btn[i], !strcmp(a, METHOD_ID[i]));
    cfg_str("calc.madhab", a, sizeof(a));
    button_on(asr_btn[0], strcmp(a, "hanafi"));
    button_on(asr_btn[1], !strcmp(a, "hanafi"));
    int off = (int)cfg_num("options.hijriOffset", 0);
    char buf[24];
    snprintf(buf, sizeof(buf), "%s%d day%s", off > 0 ? "+" : "", off, off == 1 || off == -1 ? "" : "s");
    lv_label_set_text(hijri_value, buf);
}

/* The Hijri date can be a day or two off from the local moon sighting: move it here. */
static void hijri_cb(lv_event_t *e)
{
    int off = (int)cfg_num("options.hijriOffset", 0) + (lv_event_get_user_data(e) ? 1 : -1);
    off = off < -3 ? -3 : off > 3 ? 3 : off;
    cJSON *c = edit_begin();
    put(c, "options.hijriOffset", cJSON_CreateNumber(off));
    edit_commit(c);
    pray_refresh();
}

static void method_cb(lv_event_t *e)
{
    cJSON *c = edit_begin();
    put(c, "calc.method", cJSON_CreateString(METHOD_ID[(int)(intptr_t)lv_event_get_user_data(e)]));
    edit_commit(c);
    pray_refresh();
}

static void asr_cb(lv_event_t *e)
{
    cJSON *c = edit_begin();
    put(c, "calc.madhab", cJSON_CreateString(lv_event_get_user_data(e) ? "hanafi" : "shafi"));
    edit_commit(c);
    pray_refresh();
}

static void pray_build(lv_obj_t *parent)
{
    pray = screen(parent);
    header(pray, "Prayer times", false);
    text(pray, &serif_r17, GOLD_TEXT, 16, 72, 448, LV_TEXT_ALIGN_LEFT, "Calculation method");
    for (int i = 0; i < 4; i++)
        method_btn[i] = button(pray, 16 + (i % 2) * 228, 98 + (i / 2) * 72, 220, 62, &serif_b18, METHOD_NAME[i], method_cb, (void *)(intptr_t)i);
    text(pray, &serif_r17, GOLD_TEXT, 16, 252, 448, LV_TEXT_ALIGN_LEFT, "Asr");
    asr_btn[0] = button(pray, 16, 278, 220, 62, &serif_b18, "Standard", asr_cb, (void *)0);
    asr_btn[1] = button(pray, 244, 278, 220, 62, &serif_b18, "Hanafi", asr_cb, (void *)1);
    text(pray, &serif_r17, GOLD_TEXT, 16, 360, 448, LV_TEXT_ALIGN_LEFT, "Hijri date");
    button(pray, 16, 386, 100, 62, &icons_26, I_MINUS, hijri_cb, (void *)0);
    hijri_value = text(pray, &serif_b30, WHITE, 120, 398, 240, LV_TEXT_ALIGN_CENTER, "");
    button(pray, 364, 386, 100, 62, &icons_26, I_PLUS, hijri_cb, (void *)1);
}

/* ---- volume ---------------------------------------------------------------------------------------------------- */

static int volume_now;
static lv_obj_t *vol_value, *vol_try;

static void vol_show(void)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", volume_now);
    lv_label_set_text(vol_value, buf);
    azan_set_volume(volume_now);
}

static void vol_step(lv_event_t *e)
{
    volume_now += lv_event_get_user_data(e) ? 10 : -10;
    volume_now = volume_now < 0 ? 0 : volume_now > 100 ? 100 : volume_now;
    vol_show();
}

static void vol_try_cb(lv_event_t *e)
{
    if (azan_playing()) azan_stop(); else azan_play(DHUHR);
}

static void vol_build(lv_obj_t *parent)
{
    vol = screen(parent);
    header(vol, "Volume", false);
    text(vol, &serif_r17, GOLD_TEXT, 16, 80, 448, LV_TEXT_ALIGN_CENTER, "Azan volume");
    button(vol, 16, 116, 120, 110, &icons_26, I_MINUS, vol_step, (void *)0);
    vol_value = text(vol, &serif_temp, WHITE, 140, 132, 200, LV_TEXT_ALIGN_CENTER, "");
    button(vol, 344, 116, 120, 110, &icons_26, I_PLUS, vol_step, (void *)1);
    vol_try = button(vol, 16, 262, 448, 70, &serif_b18, "Play the Azan", vol_try_cb, NULL);
    text(vol, &serif_r17, SOFT, 16, 352, 448, LV_TEXT_ALIGN_CENTER, "A tap on the screen stops the Azan.");
}

/* ---- dimming --------------------------------------------------------------------------------------------------- */

static struct { bool on; int from, to, level; lv_obj_t *toggle, *value[3]; } dp;

static void dim_show(void)
{
    char buf[24];
    button_on(dp.toggle, dp.on);
    button_text(dp.toggle, dp.on ? "Dimming is on" : "Dimming is off");
    time_text(dp.from, buf, sizeof(buf));
    lv_label_set_text(dp.value[0], buf);
    time_text(dp.to, buf, sizeof(buf));
    lv_label_set_text(dp.value[1], buf);
    snprintf(buf, sizeof(buf), "%d%%", LEVELS[dp.level]);
    lv_label_set_text(dp.value[2], buf);
}

static void dim_toggle(lv_event_t *e) { dp.on = !dp.on; dim_show(); }

static void dim_step(lv_event_t *e)
{
    int k = (int)(intptr_t)lv_event_get_user_data(e), row = k / 2, d = k % 2 ? 1 : -1;
    if (row == 0) dp.from = (dp.from + d * 30 + 1440) % 1440;
    else if (row == 1) dp.to = (dp.to + d * 30 + 1440) % 1440;
    else dp.level = dp.level + d < 0 ? 0 : dp.level + d >= LEVEL_N ? LEVEL_N - 1 : dp.level + d;
    dp.on = true;
    dim_show();
}

static void dim_build(lv_obj_t *parent)
{
    dim = screen(parent);
    header(dim, "Dimming", false);
    dp.toggle = button(dim, 16, 70, 448, 62, &serif_b18, "", dim_toggle, NULL);
    static const char *const rows[3] = {"From", "To", "Level"};
    for (int r = 0; r < 3; r++) {
        int y = 150 + r * 82;
        text(dim, &serif_b18, GOLD_TEXT, 16, y + 20, 110, LV_TEXT_ALIGN_LEFT, rows[r]);
        button(dim, 124, y, 76, 64, &icons_26, I_MINUS, dim_step, (void *)(intptr_t)(r * 2));
        dp.value[r] = text(dim, &serif_b30, WHITE, 204, y + 14, 180, LV_TEXT_ALIGN_CENTER, "");
        button(dim, 388, y, 76, 64, &icons_26, I_PLUS, dim_step, (void *)(intptr_t)(r * 2 + 1));
    }
    text(dim, &serif_r17, SOFT, 16, 400, 448, LV_TEXT_ALIGN_CENTER, "A tap brightens the screen for a minute.");
}

static void dim_open(void)
{
    char a[16], b[16];
    dp.on = cfg_bool("nightDim.enabled", false);
    cfg_str("nightDim.start", a, sizeof(a));
    cfg_str("nightDim.end", b, sizeof(b));
    dp.from = hhmm(a, 22 * 60);
    dp.to = hhmm(b, 6 * 60);
    int bright = 100 - (int)cfg_num("nightDim.darkness", 95);
    dp.level = 0;
    for (int i = 0; i < LEVEL_N; i++) if (LEVELS[i] <= bright) dp.level = i;
    dim_show();
}

/* ---- moving between pages ------------------------------------------------------------------------------------- */

static lv_obj_t *showing;

/* Leaving a page saves what it holds (volume and dimming are saved once, not on every step). */
static void back_to_menu(lv_event_t *e)
{
    if (showing == vol && volume_now != (int)cfg_num("options.volume", 100)) {
        cJSON *c = edit_begin();
        put(c, "options.volume", cJSON_CreateNumber(volume_now));
        edit_commit(c);
    }
    if (showing == dim) {
        char a[8], b[8];
        snprintf(a, sizeof(a), "%02d:%02d", dp.from / 60, dp.from % 60);
        snprintf(b, sizeof(b), "%02d:%02d", dp.to / 60, dp.to % 60);
        cJSON *c = edit_begin();
        put(c, "nightDim.enabled", cJSON_CreateBool(dp.on));
        put(c, "nightDim.start", cJSON_CreateString(a));
        put(c, "nightDim.end", cJSON_CreateString(b));
        put(c, "nightDim.darkness", cJSON_CreateNumber(100 - LEVELS[dp.level]));
        edit_commit(c);
    }
    showing = menu;
    refresh_menu();
    show(menu);
}

static void open_index(int page)
{
    switch (page) {
    case 0: if (open_wifi) open_wifi(); return;
    case 1: loc_open(); showing = loc; break;
    case 2: pray_refresh(); showing = pray; break;
    case 3: volume_now = (int)cfg_num("options.volume", 100); vol_show(); showing = vol; break;
    default: dim_open(); showing = dim; break;
    }
    show(showing);
}

static void open_page(lv_event_t *e) { open_index((int)(intptr_t)lv_event_get_user_data(e)); }

void settings_close_cb(lv_event_t *e)
{
    if (showing && showing != menu) back_to_menu(NULL);
    showing = NULL;
    show(NULL);
}

void settings_open(void)
{
    showing = menu;
    refresh_menu();
    show(menu);
}

/* For checking the pages from a computer (POST /api/ui?open=...): -1 closes, 0-4 open a page, 5 the menu. */
void settings_show(int page)
{
    if (page < 0) { settings_close_cb(NULL); return; }
    settings_open();
    if (page < 5) open_index(page);
}

bool settings_visible(void) { return showing != NULL; }

/* Called every second while open: the Wi-Fi summary and the Azan button follow what is happening. */
void settings_tick(void)
{
    if (!showing) return;
    if (showing == menu) lv_label_set_text(summary[0], wifi_connected() ? wifi_ssid() : "Not connected");
    if (showing == vol) button_text(vol_try, azan_playing() ? "Stop the Azan" : "Play the Azan");
}

void settings_build(lv_obj_t *parent, void (*wifi)(void))
{
    open_wifi = wifi;
    menu = screen(parent);
    header(menu, "Settings", true);
    tile(0, I_WIFI, "Wi-Fi");
    tile(1, I_PIN, "Location");
    tile(2, I_MOSQUE, "Prayer times");
    tile(3, I_VOLUME, "Volume");
    tile(4, I_MOON, "Dimming");
    loc_build(parent);
    pray_build(parent);
    vol_build(parent);
    dim_build(parent);
}
