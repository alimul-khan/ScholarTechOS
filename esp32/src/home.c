/* The home prayer page: the Home Display app's prayer dashboard (ScreenRenderer.prayer) on the 480x480 screen.
 * The fixed artwork is one image (src/gen/home_bg.c, drawn by tools/home_art/HomeArt.java); this file puts the live
 * text on it. Positions match HomeArt.java. */
#include <stdio.h>
#include <string.h>
#include "home.h"
#include "lvgl.h"
#include "schedule.h"

LV_IMAGE_DECLARE(home_bg);
LV_FONT_DECLARE(serif_clock);
LV_FONT_DECLARE(serif_time);
LV_FONT_DECLARE(serif_ampm);
LV_FONT_DECLARE(serif_b30);
LV_FONT_DECLARE(serif_b18);
LV_FONT_DECLARE(serif_r17);
LV_FONT_DECLARE(serif_r11);

#define INK 0x04332A
#define WHITE 0xFFFBEF
#define GOLD_TEXT 0xECCF99
#define MUTED 0x6A7461
#define CART_Y 186
#define CARD_Y 290
#define FOOT 368
static const int CARD_X[3] = {10, 165, 320};

static struct {
    lv_obj_t *root, *city, *clock, *ampm, *date, *hijri, *next, *count, *solar[3], *status, *mark, *time[PRAYER_N], *tap[PRAYER_N];
} hp;

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, int w, lv_text_align_t align)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, align, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_width(l, w);
    lv_label_set_text(l, "");
    return l;
}

static void set(lv_obj_t *l, const char *s)
{
    if (strcmp(lv_label_get_text(l), s)) lv_label_set_text(l, s);
}

lv_obj_t *home_build(lv_obj_t *parent)
{
    hp.root = lv_image_create(parent);
    lv_image_set_src(hp.root, &home_bg);
    lv_obj_set_pos(hp.root, 0, 0);
    lv_obj_remove_flag(hp.root, LV_OBJ_FLAG_CLICKABLE);

    hp.city = label(hp.root, &serif_b18, INK, 92, 37, 280, LV_TEXT_ALIGN_LEFT);

    lv_obj_t *row = lv_obj_create(hp.root);
    lv_obj_remove_style_all(row);
    lv_obj_set_pos(row, 0, 45);
    lv_obj_set_size(row, 480, 84);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
    hp.clock = lv_label_create(row);
    lv_obj_set_style_text_font(hp.clock, &serif_clock, 0);
    lv_obj_set_style_text_color(hp.clock, lv_color_hex(INK), 0);
    hp.ampm = lv_label_create(row);
    lv_obj_set_style_text_font(hp.ampm, &serif_time, 0);
    lv_obj_set_style_text_color(hp.ampm, lv_color_hex(INK), 0);
    lv_obj_set_style_pad_left(hp.ampm, 6, 0);
    lv_obj_set_style_pad_bottom(hp.ampm, 14, 0);

    hp.date = label(hp.root, &serif_b18, INK, 10, 136, 460, LV_TEXT_ALIGN_CENTER);
    hp.hijri = label(hp.root, &serif_b18, 0x906323, 10, 158, 460, LV_TEXT_ALIGN_CENTER);

    hp.next = label(hp.root, &serif_b30, WHITE, 112, CART_Y + 24, 312, LV_TEXT_ALIGN_CENTER);
    hp.count = label(hp.root, &serif_r17, GOLD_TEXT, 112, CART_Y + 61, 312, LV_TEXT_ALIGN_CENTER);

    for (int i = 0; i < 3; i++) hp.solar[i] = label(hp.root, &serif_b18, INK, CARD_X[i] + 42, CARD_Y + 25, 104, LV_TEXT_ALIGN_LEFT);
    hp.status = label(hp.root, &serif_r11, MUTED, 10, 349, 460, LV_TEXT_ALIGN_CENTER);

    hp.mark = lv_obj_create(hp.root);
    lv_obj_remove_style_all(hp.mark);
    lv_obj_set_size(hp.mark, 88, 98);
    lv_obj_set_style_radius(hp.mark, 8, 0);
    lv_obj_set_style_bg_color(hp.mark, lv_color_hex(0xFFE3A8), 0);
    lv_obj_set_style_bg_opa(hp.mark, 30, 0);
    lv_obj_remove_flag(hp.mark, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(hp.mark, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < PRAYER_N; i++) {
        hp.time[i] = label(hp.root, &serif_time, WHITE, i * 96, FOOT + 54, 96, LV_TEXT_ALIGN_CENTER);
        hp.tap[i] = label(hp.root, &serif_ampm, GOLD_TEXT, i * 96, FOOT + 81, 96, LV_TEXT_ALIGN_CENTER);
    }
    return hp.root;
}

/* "04:57" and "AM" (or "16:57" and "" on a 24-hour clock), as the app writes times. */
static void hm(int min, bool h24, char *out, size_t len, const char **ap)
{
    *ap = "";
    if (min < 0) { snprintf(out, len, "--:--"); return; }
    int h = min / 60 % 24, m = min % 60;
    if (!h24) { *ap = h < 12 ? "AM" : "PM"; h = h % 12 ? h % 12 : 12; }
    snprintf(out, len, "%02d:%02d", h, m);
}

static void hm_full(int min, bool h24, char *out, size_t len)
{
    const char *ap;
    char t[8];
    hm(min, h24, t, sizeof(t), &ap);
    snprintf(out, len, "%s%s%s", t, *ap ? " " : "", ap);
}

void home_update(const display_state_t *st, const char *city, bool h24, const char *status)
{
    static const char *const MONTHS[] = {"January", "February", "March", "April", "May", "June", "July", "August",
                                         "September", "October", "November", "December"};
    static const char *const DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    const struct tm *n = &st->now;
    char buf[96];

    set(hp.city, city);
    int h = n->tm_hour;
    if (!h24) h = h % 12 ? h % 12 : 12;
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, n->tm_min, n->tm_sec);
    set(hp.clock, buf);
    set(hp.ampm, h24 ? "" : n->tm_hour < 12 ? "AM" : "PM");
    snprintf(buf, sizeof(buf), "%s \xe2\x80\xa2 %d %s %d", DAYS[n->tm_wday], n->tm_mday, MONTHS[n->tm_mon], n->tm_year + 1900);
    set(hp.date, buf);
    set(hp.hijri, st->hijri);

    /* The next Azan (the app counts to the Azan, not the Iqamah); after Isha, tomorrow's Fajr. */
    const day_times_t *t = &st->times;
    int now_s = n->tm_hour * 3600 + n->tm_min * 60 + n->tm_sec, next = -1, wait = 0;
    for (int i = 0; i < PRAYER_N && next < 0; i++)
        if (t->azan[i] >= 0 && t->azan[i] * 60 > now_s) { next = i; wait = t->azan[i] * 60 - now_s; }
    if (next < 0 && t->azan[FAJR] >= 0) { next = FAJR; wait = 86400 - now_s + t->azan[FAJR] * 60; }
    if (next >= 0) {
        char at[16];
        hm_full(t->azan[next], h24, at, sizeof(at));
        snprintf(buf, sizeof(buf), "%s \xe2\x80\xa2 %s", PRAYER_NAME[next], at);
        set(hp.next, buf);
        int hh = wait / 3600, mm = wait / 60 % 60, ss = wait % 60;
        if (hh) snprintf(buf, sizeof(buf), "In %d h %d min %d sec", hh, mm, ss);
        else snprintf(buf, sizeof(buf), "In %d min %d sec", mm, ss);
        set(hp.count, buf);
        lv_obj_set_pos(hp.mark, next * 96 + 4, FOOT + 8);
        lv_obj_remove_flag(hp.mark, LV_OBJ_FLAG_HIDDEN);
    } else {
        set(hp.next, "No prayer times");
        set(hp.count, "Set the location in Settings");
        lv_obj_add_flag(hp.mark, LV_OBJ_FLAG_HIDDEN);
    }

    int noon = t->sunrise >= 0 && t->sunset >= 0 ? (t->sunrise + t->sunset) / 2 : -1;   /* solar noon: halfway */
    int solar[3] = {t->sunrise, noon, t->sunset};
    for (int i = 0; i < 3; i++) { hm_full(solar[i], h24, buf, sizeof(buf)); set(hp.solar[i], buf); }

    for (int i = 0; i < PRAYER_N; i++) {
        const char *ap;
        hm(t->azan[i], h24, buf, sizeof(buf), &ap);
        set(hp.time[i], buf);
        set(hp.tap[i], ap);
    }
    set(hp.status, status ? status : "");
}
