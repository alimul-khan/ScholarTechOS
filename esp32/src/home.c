/* The home pages: the Home Display app's prayer dashboard (ScreenRenderer.prayer) and weather page
 * (ScreenRenderer.weather) on the 480x480 screen. The fixed artwork is an image per page (src/gen, drawn by
 * tools/home_art/HomeArt.java); this file puts the live text on it. Positions match HomeArt.java. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "home.h"
#include "src/libs/lodepng/lodepng.h"
#include "lvgl.h"
#include "schedule.h"

/* The artwork, as PNG (src/gen/home_art.c), decoded once at start-up into PSRAM. */
extern const uint8_t home_bg_png[], home_wx_png[], home_cal_png[];
extern const unsigned home_bg_png_len, home_wx_png_len, home_cal_png_len;
extern const uint8_t *const wx_png_l[5], *const wx_png_s[5];
extern const unsigned wx_png_l_len[5], wx_png_s_len[5];
static const lv_image_dsc_t *home_bg, *home_wx, *home_cal, *wx_icon_l[5], *wx_icon_s[5];
LV_FONT_DECLARE(serif_clock);
LV_FONT_DECLARE(serif_temp);
LV_FONT_DECLARE(serif_time);
LV_FONT_DECLARE(serif_ampm);
LV_FONT_DECLARE(serif_b30);
LV_FONT_DECLARE(serif_b18);
LV_FONT_DECLARE(serif_r17);
LV_FONT_DECLARE(serif_r11);
LV_FONT_DECLARE(icons_26);

#define INK 0x04332A
#define WHITE 0xFFFBEF
#define GOLD_TEXT 0xECCF99
#define SOFT 0xECE2C8
#define MUTED 0x6A7461
#define CART_Y 186
#define CARD_Y 290
#define FOOT 368
static const int CARD_X[3] = {10, 165, 320};

typedef struct { lv_obj_t *mark, *time[PRAYER_N], *tap[PRAYER_N]; } bar_t;

static struct {
    lv_obj_t *root, *city, *clock, *ampm, *date, *hijri, *next, *count, *solar[3], *status;
    bar_t bar;
} hp;

static struct {
    lv_obj_t *root, *title, *temp, *text, *icon, *line, *detail[4], *hour[4], *hour_icon[4], *hour_temp[4], *status;
    bar_t bar;
} wp;

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int x, int y, int w, lv_text_align_t align)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, align, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_height(l, LV_SIZE_CONTENT);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_width(l, w);
    lv_label_set_text(l, "");
    return l;
}

static void set(lv_obj_t *l, const char *s)
{
    if (strcmp(lv_label_get_text(l), s)) lv_label_set_text(l, s);
}

/* PNG to an LVGL image: RGB565 for the backgrounds, ARGB8888 (B, G, R, A) where it needs transparency. */
static const lv_image_dsc_t *decode(const uint8_t *png, unsigned len, bool alpha)
{
    /* LVGL's lodepng hands back an lv_draw_buf_t holding R, G, B, A bytes. */
    lv_draw_buf_t *raw = NULL;
    unsigned w, h;
    unsigned err = lodepng_decode32((unsigned char **)&raw, &w, &h, png, len);
    if (err || !raw) { printf("home: png decode error %u\n", err); if (raw) lv_draw_buf_destroy(raw); return NULL; }
    size_t bpp = alpha ? 4 : 2;
    lv_image_dsc_t *d = heap_caps_calloc(1, sizeof(*d), MALLOC_CAP_SPIRAM);
    uint8_t *px = heap_caps_malloc(w * h * bpp, MALLOC_CAP_SPIRAM);
    if (!d || !px) { printf("home: no memory for a %ux%u image\n", w, h); lv_draw_buf_destroy(raw); free(d); free(px); return NULL; }
    for (unsigned y = 0; y < h; y++) {
        const uint8_t *row = raw->data + y * raw->header.stride;
        for (unsigned x = 0; x < w; x++) {
            const uint8_t *s = row + x * 4;
            size_t i = (size_t)y * w + x;
            if (alpha) { px[i * 4] = s[2]; px[i * 4 + 1] = s[1]; px[i * 4 + 2] = s[0]; px[i * 4 + 3] = s[3]; }
            else { uint16_t v = ((s[0] >> 3) << 11) | ((s[1] >> 2) << 5) | (s[2] >> 3); px[i * 2] = v & 255; px[i * 2 + 1] = v >> 8; }
        }
    }
    lv_draw_buf_destroy(raw);
    d->header.magic = LV_IMAGE_HEADER_MAGIC;
    d->header.cf = alpha ? LV_COLOR_FORMAT_ARGB8888 : LV_COLOR_FORMAT_RGB565;
    d->header.w = w;
    d->header.h = h;
    d->header.stride = w * bpp;
    d->data_size = w * h * bpp;
    d->data = px;
    return d;
}

static lv_obj_t *page_root(lv_obj_t *parent, const lv_image_dsc_t *art)
{
    lv_obj_t *r = lv_image_create(parent);
    lv_image_set_src(r, art);
    lv_obj_set_pos(r, 0, 0);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
    return r;
}

/* The gear, top right: opens the settings. A large touch area round a small icon. */
static void gear(lv_obj_t *root, uint32_t color, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_obj_create(root);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 64, 64);
    lv_obj_set_pos(b, 368, 18);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &icons_26, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, "\xEF\x80\x93");      /* U+F013 gear */
    lv_obj_center(l);
}

static void bar_build(lv_obj_t *root, bar_t *b)
{
    b->mark = lv_obj_create(root);
    lv_obj_remove_style_all(b->mark);
    lv_obj_set_size(b->mark, 88, 98);
    lv_obj_set_style_radius(b->mark, 8, 0);
    lv_obj_set_style_bg_color(b->mark, lv_color_hex(0xFFE3A8), 0);
    lv_obj_set_style_bg_opa(b->mark, 30, 0);
    lv_obj_remove_flag(b->mark, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(b->mark, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < PRAYER_N; i++) {
        b->time[i] = label(root, &serif_time, WHITE, i * 96, FOOT + 54, 96, LV_TEXT_ALIGN_CENTER);
        b->tap[i] = label(root, &serif_ampm, GOLD_TEXT, i * 96, FOOT + 81, 96, LV_TEXT_ALIGN_CENTER);
    }
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

/* The next Azan (the app counts to the Azan, not the Iqamah); after Isha, tomorrow's Fajr. -1 when none. */
static int next_azan(const display_state_t *st, int *wait)
{
    const day_times_t *t = &st->times;
    int now_s = st->now.tm_hour * 3600 + st->now.tm_min * 60 + st->now.tm_sec;
    for (int i = 0; i < PRAYER_N; i++)
        if (t->azan[i] >= 0 && t->azan[i] * 60 > now_s) { *wait = t->azan[i] * 60 - now_s; return i; }
    if (t->azan[FAJR] >= 0) { *wait = 86400 - now_s + t->azan[FAJR] * 60; return FAJR; }
    return -1;
}

static void bar_update(bar_t *b, const display_state_t *st, bool h24, int next)
{
    char buf[16];
    for (int i = 0; i < PRAYER_N; i++) {
        const char *ap;
        hm(st->times.azan[i], h24, buf, sizeof(buf), &ap);
        set(b->time[i], buf);
        set(b->tap[i], ap);
    }
    if (next >= 0) {
        lv_obj_set_pos(b->mark, next * 96 + 4, FOOT + 8);
        lv_obj_remove_flag(b->mark, LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(b->mark, LV_OBJ_FLAG_HIDDEN);
}

/* ---- the prayer page ---------------------------------------------------------------------------------------- */

static lv_obj_t *prayer_build(lv_obj_t *parent, lv_event_cb_t on_gear)
{
    hp.root = page_root(parent, home_bg);
    hp.city = label(hp.root, &serif_b18, INK, 92, 37, 270, LV_TEXT_ALIGN_LEFT);

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
    bar_build(hp.root, &hp.bar);
    gear(hp.root, INK, on_gear);
    return hp.root;
}

static void prayer_update(const display_state_t *st, const char *city, bool h24, const char *status)
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

    const day_times_t *t = &st->times;
    int wait = 0, next = next_azan(st, &wait);
    if (next >= 0) {
        char at[16];
        hm_full(t->azan[next], h24, at, sizeof(at));
        snprintf(buf, sizeof(buf), "%s \xe2\x80\xa2 %s", PRAYER_NAME[next], at);
        set(hp.next, buf);
        int hh = wait / 3600, mm = wait / 60 % 60, ss = wait % 60;
        if (hh) snprintf(buf, sizeof(buf), "In %d h %d min %d sec", hh, mm, ss);
        else snprintf(buf, sizeof(buf), "In %d min %d sec", mm, ss);
        set(hp.count, buf);
    } else {
        set(hp.next, "No prayer times");
        set(hp.count, "Set the location in Settings");
    }
    int noon = t->sunrise >= 0 && t->sunset >= 0 ? (t->sunrise + t->sunset) / 2 : -1;   /* solar noon: halfway */
    int solar[3] = {t->sunrise, noon, t->sunset};
    for (int i = 0; i < 3; i++) { hm_full(solar[i], h24, buf, sizeof(buf)); set(hp.solar[i], buf); }
    set(hp.status, status ? status : "");
    bar_update(&hp.bar, st, h24, next);
}

/* ---- the weather page --------------------------------------------------------------------------------------- */

static lv_obj_t *weather_build(lv_obj_t *parent, lv_event_cb_t on_gear)
{
    wp.root = page_root(parent, home_wx);
    wp.title = label(wp.root, &serif_b18, WHITE, 92, 37, 270, LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(wp.title, LV_LABEL_LONG_DOT);
    lv_obj_set_height(wp.title, 24);
    wp.temp = label(wp.root, &serif_temp, WHITE, 40, 80, 260, LV_TEXT_ALIGN_LEFT);
    wp.text = label(wp.root, &serif_b18, WHITE, 44, 158, 260, LV_TEXT_ALIGN_LEFT);
    wp.icon = lv_image_create(wp.root);
    lv_obj_set_pos(wp.icon, 318, 66);
    wp.line = label(wp.root, &serif_r17, GOLD_TEXT, 10, 188, 460, LV_TEXT_ALIGN_CENTER);
    static const char *const names[4] = {"Feels like", "High / Low", "Wind", "Humidity"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *l = label(wp.root, &serif_r11, SOFT, i * 120, 216, 120, LV_TEXT_ALIGN_CENTER);
        lv_label_set_text(l, names[i]);
        wp.detail[i] = label(wp.root, &serif_b18, WHITE, i * 120, 232, 120, LV_TEXT_ALIGN_CENTER);
    }
    for (int i = 0; i < 4; i++) {
        int x = 12 + i * 116;
        wp.hour[i] = label(wp.root, &serif_r11, WHITE, x, 270, 108, LV_TEXT_ALIGN_CENTER);
        wp.hour_icon[i] = lv_image_create(wp.root);
        lv_obj_set_pos(wp.hour_icon[i], x + 30, 282);
        wp.hour_temp[i] = label(wp.root, &serif_b18, WHITE, x, 326, 108, LV_TEXT_ALIGN_CENTER);
    }
    wp.status = label(wp.root, &serif_r11, 0x7FA08E, 10, 352, 460, LV_TEXT_ALIGN_CENTER);
    bar_build(wp.root, &wp.bar);
    gear(wp.root, GOLD_TEXT, on_gear);
    return wp.root;
}

static void deg(double v, char *out, size_t len)
{
    if (isnan(v)) snprintf(out, len, "\xe2\x80\x94");
    else snprintf(out, len, "%ld\xc2\xb0", lround(v));
}

static void weather_update(const display_state_t *st, const char *city, bool h24, const weather_t *w)
{
    char buf[96], a[16], b[16];
    snprintf(buf, sizeof(buf), "Weather in %s", city);
    set(wp.title, buf);
    if (!w->valid) {
        set(wp.temp, "");
        set(wp.text, "Waiting for the weather\xe2\x80\xa6");
        lv_image_set_src(wp.icon, NULL);
        for (int i = 0; i < 4; i++) { set(wp.detail[i], "\xe2\x80\x94"); set(wp.hour[i], ""); set(wp.hour_temp[i], ""); lv_image_set_src(wp.hour_icon[i], NULL); }
    } else {
        deg(w->temp, a, sizeof(a));
        snprintf(buf, sizeof(buf), "%sC", a);
        set(wp.temp, buf);
        set(wp.text, weather_text(w->code));
        lv_image_set_src(wp.icon, wx_icon_l[weather_kind(w->code)]);
        deg(w->feels, buf, sizeof(buf));
        set(wp.detail[0], buf);
        deg(w->high, a, sizeof(a));
        deg(w->low, b, sizeof(b));
        snprintf(buf, sizeof(buf), "%s / %s", a, b);
        set(wp.detail[1], buf);
        snprintf(buf, sizeof(buf), "%ld km/h", lround(w->wind));
        set(wp.detail[2], buf);
        snprintf(buf, sizeof(buf), "%ld%%", lround(w->humidity));
        set(wp.detail[3], buf);
        for (int i = 0; i < 4; i++) {
            if (i >= w->hours) { set(wp.hour[i], ""); set(wp.hour_temp[i], ""); lv_image_set_src(wp.hour_icon[i], NULL); continue; }
            int hr = w->hour[i];
            if (h24) snprintf(buf, sizeof(buf), "%02d:00", hr);
            else snprintf(buf, sizeof(buf), "%d %s", hr % 12 ? hr % 12 : 12, hr < 12 ? "AM" : "PM");
            set(wp.hour[i], buf);
            lv_image_set_src(wp.hour_icon[i], wx_icon_s[weather_kind(w->hour_code[i])]);
            deg(w->hour_temp[i], buf, sizeof(buf));
            set(wp.hour_temp[i], buf);
        }
    }
    /* The time and the next prayer, as the app's weather header and "Next:" pill. */
    int wait = 0, next = next_azan(st, &wait);
    hm_full(st->now.tm_hour * 60 + st->now.tm_min, h24, a, sizeof(a));
    if (next >= 0) {
        int mins = (wait + 59) / 60;
        if (mins >= 60) snprintf(b, sizeof(b), "%d h %d min", mins / 60, mins % 60);
        else snprintf(b, sizeof(b), "%d min", mins);
        snprintf(buf, sizeof(buf), "%s  \xe2\x80\xa2  Next: %s in %s", a, PRAYER_NAME[next], b);
    } else snprintf(buf, sizeof(buf), "%s", a);
    set(wp.line, buf);
    set(wp.status, "Open-Meteo \xe2\x80\xa2 CC BY 4.0");
    bar_update(&wp.bar, st, h24, next);
}

/* ---- the calendars: this month, Gregorian or Hijri, with today marked ------------------------------------------ */

typedef struct { lv_obj_t *root, *title, *sub, *today, *day[42]; bar_t bar; } cal_t;
static cal_t gc, hc;

static void cal_build(cal_t *c, lv_obj_t *parent, lv_event_cb_t on_gear)
{
    c->root = page_root(parent, home_cal);
    c->title = label(c->root, &serif_b30, INK, 50, 48, 330, LV_TEXT_ALIGN_CENTER);     /* clear of the gear */
    c->sub = label(c->root, &serif_r17, 0x906323, 20, 84, 440, LV_TEXT_ALIGN_CENTER);
    c->today = lv_obj_create(c->root);
    lv_obj_remove_style_all(c->today);
    lv_obj_set_size(c->today, 36, 34);
    lv_obj_set_style_radius(c->today, 17, 0);
    lv_obj_set_style_bg_color(c->today, lv_color_hex(0x06352B), 0);
    lv_obj_set_style_bg_opa(c->today, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(c->today, lv_color_hex(0xC19749), 0);
    lv_obj_set_style_border_width(c->today, 2, 0);
    lv_obj_remove_flag(c->today, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < 42; i++)
        c->day[i] = label(c->root, &serif_b18, i % 7 == 5 ? 0x906323 : INK, 32 + (i % 7) * 60 + (i % 7) / 2, 140 + (i / 7) * 36, 60, LV_TEXT_ALIGN_CENTER);
    bar_build(c->root, &c->bar);
    gear(c->root, INK, on_gear);
}

/* first: weekday of day 1 (0 Sunday); today: its day in the month, 0 when not this month. */
static void cal_fill(cal_t *c, const char *title, const char *sub, int first, int length, int today)
{
    char buf[4];
    /* A long month name ("Dhu'l-Qi'dah 1448 AH") in the smaller font, so it stays clear of the gear. */
    lv_point_t size;
    lv_text_get_size(&size, title, &serif_b30, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int32_t tw = size.x;
    lv_obj_set_style_text_font(c->title, tw > 330 ? &serif_b18 : &serif_b30, 0);
    lv_obj_set_y(c->title, tw > 330 ? 58 : 48);
    set(c->title, title);
    set(c->sub, sub);
    lv_obj_add_flag(c->today, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 42; i++) {
        int d = i - first + 1;
        if (d < 1 || d > length) { set(c->day[i], ""); continue; }
        snprintf(buf, sizeof(buf), "%d", d);
        set(c->day[i], buf);
        bool on = d == today;
        lv_obj_set_style_text_color(c->day[i], lv_color_hex(on ? WHITE : i % 7 == 5 ? 0x906323 : INK), 0);
        if (on) {
            lv_obj_set_pos(c->today, lv_obj_get_x(c->day[i]) + 12, lv_obj_get_y(c->day[i]) - 6);
            lv_obj_remove_flag(c->today, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static const char *const MONTH_NAME[] = {"January", "February", "March", "April", "May", "June", "July", "August",
                                         "September", "October", "November", "December"};

static void gregorian_update(const display_state_t *st, bool h24)
{
    const struct tm *n = &st->now;
    int y = n->tm_year + 1900, m = n->tm_mon + 1;
    static const int len[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int length = len[m - 1] + (m == 2 && ((y % 4 == 0 && y % 100) || y % 400 == 0));
    long first = schedule_jdn(y, m, 1);
    char title[40], sub[96] = "";
    snprintf(title, sizeof(title), "%s %d", MONTH_NAME[m - 1], y);
    /* The Hijri months this one spans. */
    int hy, hm, hl, hy2, hm2;
    long hf;
    if (hijri_month(first + st->hijri_offset, &hy, &hm, &hf, &hl) && hijri_month(first + length - 1 + st->hijri_offset, &hy2, &hm2, &hf, &hl)) {
        if (hy == hy2) snprintf(sub, sizeof(sub), "%s \xe2\x80\x93 %s %d AH", hijri_month_name(hm), hijri_month_name(hm2), hy2);
        else snprintf(sub, sizeof(sub), "%s %d \xe2\x80\x93 %s %d AH", hijri_month_name(hm), hy, hijri_month_name(hm2), hy2);
    }
    cal_fill(&gc, title, sub, (int)((first + 1) % 7), length, n->tm_mday);
    int wait;
    bar_update(&gc.bar, st, h24, next_azan(st, &wait));
}

static void hijri_update(const display_state_t *st, bool h24)
{
    const struct tm *n = &st->now;
    long today = schedule_jdn(n->tm_year + 1900, n->tm_mon + 1, n->tm_mday) + st->hijri_offset;
    int hy, hm, hl;
    long hf;
    int wait;
    bar_update(&hc.bar, st, h24, next_azan(st, &wait));
    if (!hijri_month(today, &hy, &hm, &hf, &hl)) { cal_fill(&hc, "Hijri calendar", "", 0, 0, 0); return; }
    char title[48], sub[64];
    snprintf(title, sizeof(title), "%s %d AH", hijri_month_name(hm), hy);
    /* The Gregorian dates it spans (Julian day back to a civil date). */
    long g1 = hf - st->hijri_offset, g2 = hf + hl - 1 - st->hijri_offset;
    int y1, m1, d1, y2, m2, d2;
    for (int k = 0; k < 2; k++) {
        long j = k ? g2 : g1, a = j + 32044, b = (4 * a + 3) / 146097, c = a - 146097 * b / 4, d = (4 * c + 3) / 1461, e = c - 1461 * d / 4, mm = (5 * e + 2) / 153;
        int day = e - (153 * mm + 2) / 5 + 1, mon = mm + 3 - 12 * (mm / 10), yr = 100 * b + d - 4800 + mm / 10;
        if (k) { y2 = yr; m2 = mon; d2 = day; } else { y1 = yr; m1 = mon; d1 = day; }
    }
    snprintf(sub, sizeof(sub), "%d %.3s %d \xe2\x80\x93 %d %.3s %d", d1, MONTH_NAME[m1 - 1], y1, d2, MONTH_NAME[m2 - 1], y2);
    cal_fill(&hc, title, sub, (int)((hf - st->hijri_offset + 1) % 7), hl, (int)(today - hf + 1));
}

/* ---- all pages ------------------------------------------------------------------------------------------------------ */

void home_build(lv_obj_t *parent, lv_event_cb_t on_gear)
{
    home_bg = decode(home_bg_png, home_bg_png_len, false);
    home_wx = decode(home_wx_png, home_wx_png_len, false);
    home_cal = decode(home_cal_png, home_cal_png_len, false);
    for (int i = 0; i < 5; i++) {
        wx_icon_l[i] = decode(wx_png_l[i], wx_png_l_len[i], true);
        wx_icon_s[i] = decode(wx_png_s[i], wx_png_s_len[i], true);
    }
    prayer_build(parent, on_gear);
    weather_build(parent, on_gear);
    cal_build(&gc, parent, on_gear);
    cal_build(&hc, parent, on_gear);
    home_show(HOME_PRAYER);
}

void home_show(int page)
{
    lv_obj_set_flag(hp.root, LV_OBJ_FLAG_HIDDEN, page != HOME_PRAYER);
    lv_obj_set_flag(wp.root, LV_OBJ_FLAG_HIDDEN, page != HOME_WEATHER);
    lv_obj_set_flag(gc.root, LV_OBJ_FLAG_HIDDEN, page != HOME_GREGORIAN);
    lv_obj_set_flag(hc.root, LV_OBJ_FLAG_HIDDEN, page != HOME_HIJRI);
}

void home_update(int page, const display_state_t *st, const char *city, bool h24, const char *status, const weather_t *w)
{
    if (page == HOME_WEATHER) weather_update(st, city, h24, w);
    else if (page == HOME_GREGORIAN) gregorian_update(st, h24);
    else if (page == HOME_HIJRI) hijri_update(st, h24);
    else prayer_update(st, city, h24, status);
}
