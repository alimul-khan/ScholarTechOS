#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "bsp/esp-bsp.h"
#include "config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "fonts.h"
#include "azan.h"
#include "home.h"
LV_FONT_DECLARE(serif_b30);
LV_FONT_DECLARE(serif_b18);
LV_FONT_DECLARE(serif_r17);
#include "settings.h"
#include "weather.h"
#include "link.h"
#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "mbedtls/base64.h"
#include "schedule.h"
#include "storage.h"
#include "themes.h"
#include "ui.h"
#include "wifi.h"

static const char *TAG = "ui";
#define W 480
#define H 480

enum { PAGE_PRAYER, PAGE_NOTICES, PAGE_COMBINED, PAGE_N };
static const char *const PAGE_KEY[PAGE_N] = {"prayer", "notices", "combined"};
static const int PAGE_DEFAULT_SECS[PAGE_N] = {10, 6, 6};

static lv_obj_t *scr;
static const theme_t *th;            /* the prayer page's theme */
static const theme_t *th_notice;     /* the notices page's */
static const theme_t *th_combined;
static int seen_generation = -1;
static int last_sec = -1;
static display_state_t st;
static bool h24;

/* Page rotation. */
static int page_secs[PAGE_N];
static int page = -1;
static int64_t page_until;

/* Notices that are on today: indexes into the settings' notices array. */
#define MAX_NOTICES 32
static int notice_idx[MAX_NOTICES], notice_n, notice_at = -1;
static int64_t notice_until;

/* Footer messages. */
#define MAX_MSG 8
static char messages[MAX_MSG][160];
static int message_n, message_at;

/* ---------------------------------------------------------------------------------------------- */
/* styling                                                                                        */

static lv_color_t col(uint32_t c) { return lv_color_hex(c); }

static void paint_bg(lv_obj_t *o, const paint_t *p)
{
    lv_obj_set_style_bg_color(o, col(p->c1), 0);
    lv_obj_set_style_bg_opa(o, p->opa, 0);
    if (p->grad && p->c1 != p->c2) {
        lv_obj_set_style_bg_grad_color(o, col(p->c2), 0);
        lv_obj_set_style_bg_grad_dir(o, p->grad == 2 ? LV_GRAD_DIR_HOR : LV_GRAD_DIR_VER, 0);
    } else {
        lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_NONE, 0);
    }
}

static void paint_text(lv_obj_t *o, const paint_t *p)
{
    lv_obj_set_style_text_color(o, col(p->c1), 0);
    lv_obj_set_style_text_opa(o, p->opa, 0);
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

static lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font, int x, int y, int w, lv_text_align_t align)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_pos(l, x, y);
    if (w > 0) {
        lv_obj_set_width(l, w);
        lv_obj_set_style_text_align(l, align, 0);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    }
    lv_label_set_text(l, "");
    return l;
}

static void set_text(lv_obj_t *l, const char *s)
{
    if (strcmp(lv_label_get_text(l), s)) lv_label_set_text(l, s);
}

static const theme_t *theme_for_template(const cJSON *cfg, const char *template_id)
{
    char path[64];
    snprintf(path, sizeof(path), "themes.%s", template_id);
    const theme_t *t = theme_find(jstr(cfg, path, NULL));
    if (!t) t = theme_find(template_default_theme(template_id));
    return t ? t : &THEMES[0];
}

/* Like core.js noticeDesign() / combinedDesign(): "match" follows the prayer screen. */
static const theme_t *theme_for_design(const cJSON *cfg, const char *design)
{
    char path[64];
    snprintf(path, sizeof(path), "%s.templateId", design);
    const char *tpl = jstr(cfg, path, "match");
    tpl = template_resolve(strcmp(tpl, "match") ? tpl : jstr(cfg, "templateId", "sanctuary"));
    snprintf(path, sizeof(path), "%s.themeId", design);
    const char *id = jstr(cfg, path, "match");
    const theme_t *t = strcmp(id, "match") ? theme_find(id) : NULL;
    return t ? t : theme_for_template(cfg, tpl);
}

/* ---------------------------------------------------------------------------------------------- */
/* pictures (notice images, the logo): PNG and JPEG, decoded by LVGL                              */

typedef struct {
    lv_image_dsc_t dsc;
    char key[96];
} picture_t;

static void picture_free(picture_t *p)
{
    if (p->dsc.data) {
        lv_image_cache_drop(&p->dsc);
        free((void *)p->dsc.data);
    }
    memset(p, 0, sizeof(*p));
}

/* Takes the bytes (PSRAM) and keeps them; false when they are not a PNG or JPEG of a size this screen can hold. */
static bool picture_set(picture_t *p, uint8_t *bytes, size_t len, const char *key)
{
    picture_free(p);
    bool png = len > 8 && !memcmp(bytes, "\x89PNG", 4);
    bool jpg = len > 3 && bytes[0] == 0xFF && bytes[1] == 0xD8;
    if (!png && !jpg) { free(bytes); return false; }
    p->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    p->dsc.header.cf = png ? LV_COLOR_FORMAT_RAW_ALPHA : LV_COLOR_FORMAT_RAW;
    p->dsc.data = bytes;
    p->dsc.data_size = len;
    lv_image_header_t head;
    if (lv_image_decoder_get_info(&p->dsc, &head) != LV_RESULT_OK || head.w > 1200 || head.h > 1200 || !head.w || !head.h) {
        ESP_LOGW(TAG, "picture %s: cannot show it (%dx%d)", key, head.w, head.h);
        free(bytes);
        memset(p, 0, sizeof(*p));
        return false;
    }
    p->dsc.header.w = head.w;
    p->dsc.header.h = head.h;
    snprintf(p->key, sizeof(p->key), "%s", key);
    return true;
}

static bool picture_load_file(picture_t *p, const char *path, const char *key)
{
    if (p->dsc.data && !strcmp(p->key, key)) return true;
    size_t len;
    uint8_t *bytes = (uint8_t *)storage_read(path, &len);
    if (!bytes) { picture_free(p); return false; }
    return picture_set(p, bytes, len, key);
}

/* A data: URL (the logo is kept in the settings as one). */
static bool picture_load_data_url(picture_t *p, const char *url)
{
    char key[96];
    snprintf(key, sizeof(key), "logo:%u:%.40s", (unsigned)strlen(url), url + (strlen(url) > 80 ? strlen(url) - 40 : 0));
    if (p->dsc.data && !strcmp(p->key, key)) return true;
    const char *comma = strchr(url, ',');
    if (strncmp(url, "data:image/", 11) || !comma || !strstr(url, ";base64")) { picture_free(p); return false; }
    comma++;
    size_t in = strlen(comma), out = 0;
    uint8_t *bytes = heap_caps_malloc(in * 3 / 4 + 4, MALLOC_CAP_SPIRAM);
    if (!bytes || mbedtls_base64_decode(bytes, in * 3 / 4 + 4, &out, (const uint8_t *)comma, in)) { free(bytes); picture_free(p); return false; }
    return picture_set(p, bytes, out, key);
}

/* ---------------------------------------------------------------------------------------------- */
/* the prayer page                                                                                */

static struct {
    lv_obj_t *root, *name, *date, *clock_row, *clock, *sec, *ap, *hero, *next, *count, *table, *head[3];
    lv_obj_t *row[PRAYER_N], *cell[PRAYER_N][3], *foot, *foot_text, *logo;
} pp;
static picture_t logo_pic;

static void prayer_build(void)
{
    pp.root = box(scr, 0, 0, W, H);
    pp.logo = lv_image_create(pp.root);
    lv_obj_set_size(pp.logo, 40, 40);
    lv_obj_set_pos(pp.logo, 14, 8);
    lv_image_set_inner_align(pp.logo, LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_add_flag(pp.logo, LV_OBJ_FLAG_HIDDEN);
    pp.name = text(pp.root, &font_big, 20, 10, W - 40, LV_TEXT_ALIGN_CENTER);
    pp.date = text(pp.root, &font_small, 16, 46, W - 32, LV_TEXT_ALIGN_CENTER);

    pp.clock_row = box(pp.root, 0, 66, W, 110);
    lv_obj_set_flex_flow(pp.clock_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pp.clock_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    pp.clock = lv_label_create(pp.clock_row);
    lv_obj_set_style_text_font(pp.clock, &font_clock, 0);
    pp.sec = lv_label_create(pp.clock_row);
    lv_obj_set_style_text_font(pp.sec, &font_title, 0);
    lv_obj_set_style_pad_bottom(pp.sec, 22, 0);
    pp.ap = lv_label_create(pp.clock_row);
    lv_obj_set_style_text_font(pp.ap, &font_text, 0);
    lv_obj_set_style_pad_bottom(pp.ap, 26, 0);
    lv_obj_set_style_pad_left(pp.ap, 6, 0);

    pp.hero = box(pp.root, 24, 180, W - 48, 76);
    lv_obj_set_style_radius(pp.hero, 16, 0);
    pp.next = text(pp.hero, &font_small, 0, 4, W - 48, LV_TEXT_ALIGN_CENTER);
    pp.count = text(pp.hero, &font_count, 0, 28, W - 48, LV_TEXT_ALIGN_CENTER);

    pp.table = box(pp.root, 16, 264, W - 32, 176);
    lv_obj_set_style_radius(pp.table, 14, 0);
    lv_obj_set_style_clip_corner(pp.table, true, 0);
    static const char *heads[] = {"", "Azan", "Iqamah"};
    for (int c = 0; c < 3; c++) {
        pp.head[c] = text(pp.table, &font_small, c == 0 ? 16 : c == 1 ? 168 : 300, 4, c == 0 ? 140 : 132, LV_TEXT_ALIGN_CENTER);
        set_text(pp.head[c], heads[c]);
    }
    for (int p = 0; p < PRAYER_N; p++) {
        pp.row[p] = box(pp.table, 0, 26 + p * 30, W - 32, 30);
        pp.cell[p][0] = text(pp.row[p], &font_text, 18, 2, 150, LV_TEXT_ALIGN_LEFT);
        pp.cell[p][1] = text(pp.row[p], &font_text, 168, 2, 132, LV_TEXT_ALIGN_CENTER);
        pp.cell[p][2] = text(pp.row[p], &font_text, 300, 2, 132, LV_TEXT_ALIGN_CENTER);
        set_text(pp.cell[p][0], PRAYER_NAME[p]);
    }
    pp.foot = box(pp.root, 0, 446, W, 34);
    pp.foot_text = text(pp.foot, &font_small, 12, 6, W - 24, LV_TEXT_ALIGN_CENTER);
    lv_label_set_long_mode(pp.foot_text, LV_LABEL_LONG_SCROLL_CIRCULAR);
}

static void prayer_theme(void)
{
    const paint_t *p = th->p;
    paint_bg(pp.root, &p[P_BG]);
    paint_text(pp.name, &p[P_HEADING]);
    paint_text(pp.date, &p[P_MUTED]);
    paint_text(pp.clock, &p[P_CLOCK]);
    paint_text(pp.sec, &p[P_CLOCK]);
    paint_text(pp.ap, &p[P_MUTED]);
    paint_bg(pp.hero, &p[P_HERO]);
    lv_obj_set_style_border_color(pp.hero, col(p[P_LINE].c1), 0);
    lv_obj_set_style_border_width(pp.hero, 1, 0);
    lv_obj_set_style_border_opa(pp.hero, 120, 0);
    paint_text(pp.next, &p[P_HERO_MUTED]);
    paint_text(pp.count, &p[P_COUNT]);
    paint_bg(pp.table, &p[P_PANEL]);
    lv_obj_set_style_border_color(pp.table, col(p[P_LINE].c1), 0);
    lv_obj_set_style_border_width(pp.table, 1, 0);
    lv_obj_set_style_border_opa(pp.table, 90, 0);
    for (int c = 0; c < 3; c++) paint_text(pp.head[c], &p[P_PANEL_MUTED]);
    paint_bg(pp.foot, &p[P_HERO]);
    paint_text(pp.foot_text, &p[P_HERO_TEXT]);
}

__attribute__((unused)) static void prayer_update(const cJSON *cfg)
{
    const paint_t *p = th->p;
    set_text(pp.clock, st.hm);
    set_text(pp.sec, st.sec);
    set_text(pp.ap, st.ap);
    char buf[160];
    snprintf(buf, sizeof(buf), "%s%s%s", st.date_long, st.hijri[0] ? "  \xc2\xb7  " : "", st.hijri);
    set_text(pp.date, buf);
    if (st.active >= 0) {
        char upper[40];
        snprintf(upper, sizeof(upper), "%s", st.next_name);
        for (char *c = upper; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
        snprintf(buf, sizeof(buf), "%s  \xc2\xb7  %s", upper, st.next_at);
    } else snprintf(buf, sizeof(buf), "Set the prayer times in Settings");
    set_text(pp.next, buf);
    set_text(pp.count, st.countdown);
    for (int i = 0; i < PRAYER_N; i++) {
        bool on = i == st.active;
        if (on) paint_bg(pp.row[i], &p[P_ACCENT]); else lv_obj_set_style_bg_opa(pp.row[i], 0, 0);
        for (int c = 0; c < 3; c++) paint_text(pp.cell[i][c], on ? &p[P_ON_ACCENT] : &p[P_PANEL_TEXT]);
        fmt_min(st.times.azan[i], h24, buf, sizeof(buf));
        set_text(pp.cell[i][1], buf);
        fmt_min(st.times.iqamah[i], h24, buf, sizeof(buf));
        set_text(pp.cell[i][2], buf);
    }
}

/* The footer: sunrise, Jumua, then the messages from the settings, one at a time. */
static void footer_text(char *out, size_t len)
{
    char a[16], b[16], c[16], d[16];
    fmt_min(st.times.sunrise, h24, a, sizeof(a));
    fmt_min(st.times.sunset, h24, d, sizeof(d));
    fmt_min(st.times.jumua1, h24, b, sizeof(b));
    fmt_min(st.times.jumua2, h24, c, sizeof(c));
    int slot = message_at % (message_n + 1);
    if (slot == 0 || !message_n) {
        int n = snprintf(out, len, "Sunrise %s  \xc2\xb7  Sunset %s", a, d);
        if (st.times.jumua1 >= 0) n += snprintf(out + n, len - n, "  \xc2\xb7  Jumu\xe2\x80\x98" "ah %s", b);
        if (st.times.jumua2 >= 0) snprintf(out + n, len - n, ", %s", c);
    } else {
        snprintf(out, len, "%s", messages[slot - 1]);
    }
}

/* ---------------------------------------------------------------------------------------------- */
/* a notice card (used by the notices page and the combined page)                                 */

typedef struct {
    lv_obj_t *root, *tag, *title, *image, *body, *meta;
    picture_t pic;
    int shown;          /* index in the notices array, -1 none */
} card_t;

static void card_build(card_t *c, lv_obj_t *parent, int x, int y, int w, int h)
{
    c->root = box(parent, x, y, w, h);
    lv_obj_set_flex_flow(c->root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c->root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(c->root, 14, 0);
    lv_obj_set_style_pad_row(c->root, 8, 0);
    lv_obj_set_style_radius(c->root, 16, 0);
    lv_obj_set_style_clip_corner(c->root, true, 0);
    c->tag = lv_label_create(c->root);
    lv_obj_set_style_text_font(c->tag, &font_small, 0);
    lv_obj_set_style_pad_hor(c->tag, 12, 0);
    lv_obj_set_style_pad_ver(c->tag, 2, 0);
    lv_obj_set_style_radius(c->tag, 12, 0);
    c->title = lv_label_create(c->root);
    lv_obj_set_style_text_font(c->title, &font_title, 0);
    lv_obj_set_width(c->title, w - 28);
    lv_obj_set_style_text_align(c->title, LV_TEXT_ALIGN_CENTER, 0);
    c->image = lv_image_create(c->root);
    lv_image_set_inner_align(c->image, LV_IMAGE_ALIGN_CONTAIN);
    c->body = lv_label_create(c->root);
    lv_obj_set_style_text_font(c->body, &font_text, 0);
    lv_obj_set_width(c->body, w - 28);
    lv_obj_set_style_text_align(c->body, LV_TEXT_ALIGN_CENTER, 0);
    c->meta = lv_label_create(c->root);
    lv_obj_set_style_text_font(c->meta, &font_small, 0);
    lv_obj_set_width(c->meta, w - 28);
    lv_obj_set_style_text_align(c->meta, LV_TEXT_ALIGN_CENTER, 0);
    c->shown = -2;
}

static void card_theme(card_t *c, const theme_t *t)
{
    paint_bg(c->root, &t->p[P_PANEL]);
    lv_obj_set_style_border_color(c->root, col(t->p[P_LINE].c1), 0);
    lv_obj_set_style_border_width(c->root, 1, 0);
    lv_obj_set_style_border_opa(c->root, 90, 0);
    paint_bg(c->tag, &t->p[P_ACCENT]);
    paint_text(c->tag, &t->p[P_ON_ACCENT]);
    paint_text(c->title, &t->p[P_HEADING]);
    if (t->p[P_PANEL].opa > 200) paint_text(c->title, &t->p[P_PANEL_TEXT]);
    paint_text(c->body, &t->p[P_PANEL_TEXT]);
    paint_text(c->meta, &t->p[P_PANEL_MUTED]);
    c->shown = -2;      /* redraw */
}

static void card_show(card_t *c, const cJSON *cfg, int index, int max_image_h)
{
    if (c->shown == index) return;
    c->shown = index;
    const cJSON *n = index >= 0 ? cJSON_GetArrayItem(jget(cfg, "notices"), index) : NULL;
    const char *tag = jstr(n, "tag", ""), *title = jstr(n, "title", ""), *body = jstr(n, "body", ""), *meta = jstr(n, "meta", "");
    if (!n) { title = "No notices"; body = "Add notices on the settings page (Notices tab)."; }
    lv_label_set_text(c->tag, tag);
    lv_obj_set_flag(c->tag, LV_OBJ_FLAG_HIDDEN, !*tag);
    lv_label_set_text(c->title, title);
    lv_obj_set_flag(c->title, LV_OBJ_FLAG_HIDDEN, !*title);
    lv_label_set_text(c->body, body);
    lv_obj_set_flag(c->body, LV_OBJ_FLAG_HIDDEN, !*body);
    lv_obj_set_flag(c->body, LV_OBJ_FLAG_HIDDEN, !*body || !strcmp(jstr(n, "layout", ""), "image-only"));
    lv_label_set_text(c->meta, meta);
    lv_obj_set_flag(c->meta, LV_OBJ_FLAG_HIDDEN, !*meta);

    /* The picture: one uploaded on this screen's settings page (kept in /data/media). The sample SVG pictures and
     * WebP files cannot be drawn here; the notice then shows as text. */
    const char *id = jstr(n, "imageId", "");
    bool ok = false;
    if (*id && !strchr(id, '/') && !strstr(id, "..")) {
        char path[128];
        snprintf(path, sizeof(path), "/data/media/%s", id);
        ok = picture_load_file(&c->pic, path, id);
    } else picture_free(&c->pic);
    if (ok && max_image_h > 0) {
        bool text_only = !strcmp(jstr(n, "layout", ""), "text-only");
        int box_w = lv_obj_get_width(c->root) - 28;
        int h = c->pic.dsc.header.h * box_w / c->pic.dsc.header.w;
        bool only = !strcmp(jstr(n, "layout", ""), "image-only");
        int max_h = only ? lv_obj_get_height(c->root) - 28 : max_image_h;
        if (h > max_h) h = max_h;
        lv_image_set_src(c->image, &c->pic.dsc);
        lv_obj_set_size(c->image, box_w, h);
        lv_obj_set_flag(c->image, LV_OBJ_FLAG_HIDDEN, text_only);
        if (only) { lv_obj_add_flag(c->title, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(c->tag, LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(c->meta, LV_OBJ_FLAG_HIDDEN); }
    } else {
        lv_image_set_src(c->image, NULL);
        lv_obj_add_flag(c->image, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(c->body, LV_OBJ_FLAG_HIDDEN);
        if (!*body) lv_obj_add_flag(c->body, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ---------------------------------------------------------------------------------------------- */
/* the notices page and the combined page                                                         */

static struct {
    lv_obj_t *root, *bar, *name, *clock, *strip, *next, *count;
    card_t card;
} np;

static struct {
    lv_obj_t *root, *hero, *clock, *date, *next, *count, *cell[PRAYER_N], *cname[PRAYER_N], *ctime[PRAYER_N], *ciq[PRAYER_N];
    card_t card;
} cp;

static void notices_build(void)
{
    np.root = box(scr, 0, 0, W, H);
    np.bar = box(np.root, 0, 0, W, 54);
    np.name = text(np.bar, &font_text, 16, 14, 300, LV_TEXT_ALIGN_LEFT);
    np.clock = text(np.bar, &font_big, W - 176, 10, 160, LV_TEXT_ALIGN_RIGHT);
    card_build(&np.card, np.root, 14, 64, W - 28, 340);
    np.strip = box(np.root, 14, 414, W - 28, 54);
    lv_obj_set_style_radius(np.strip, 14, 0);
    np.next = text(np.strip, &font_small, 14, 16, 230, LV_TEXT_ALIGN_LEFT);
    np.count = text(np.strip, &font_big, W - 28 - 184, 11, 170, LV_TEXT_ALIGN_RIGHT);
}

static void notices_theme(void)
{
    const paint_t *p = th_notice->p;
    paint_bg(np.root, &p[P_BG]);
    paint_bg(np.bar, &p[P_HERO]);
    paint_text(np.name, &p[P_HERO_TEXT]);
    paint_text(np.clock, &p[P_HERO_TEXT]);
    card_theme(&np.card, th_notice);
    paint_bg(np.strip, &p[P_HERO]);
    paint_text(np.next, &p[P_HERO_MUTED]);
    paint_text(np.count, &p[P_COUNT]);
}

static void combined_build(void)
{
    cp.root = box(scr, 0, 0, W, H);
    cp.hero = box(cp.root, 0, 0, W, 92);
    cp.clock = text(cp.hero, &font_title, 16, 8, 220, LV_TEXT_ALIGN_LEFT);
    cp.date = text(cp.hero, &font_small, 16, 52, 230, LV_TEXT_ALIGN_LEFT);
    cp.next = text(cp.hero, &font_small, 230, 12, 234, LV_TEXT_ALIGN_RIGHT);
    cp.count = text(cp.hero, &font_title, 230, 36, 234, LV_TEXT_ALIGN_RIGHT);
    int cw = (W - 28 - 4 * 6) / PRAYER_N;
    for (int i = 0; i < PRAYER_N; i++) {
        cp.cell[i] = box(cp.root, 14 + i * (cw + 6), 100, cw, 78);
        lv_obj_set_style_radius(cp.cell[i], 12, 0);
        cp.cname[i] = text(cp.cell[i], &font_small, 0, 4, cw, LV_TEXT_ALIGN_CENTER);
        cp.ctime[i] = text(cp.cell[i], &font_text, 0, 26, cw, LV_TEXT_ALIGN_CENTER);
        cp.ciq[i] = text(cp.cell[i], &font_small, 0, 52, cw, LV_TEXT_ALIGN_CENTER);
        set_text(cp.cname[i], PRAYER_NAME[i]);
    }
    card_build(&cp.card, cp.root, 14, 188, W - 28, 280);
}

static void combined_theme(void)
{
    const paint_t *p = th_combined->p;
    paint_bg(cp.root, &p[P_BG]);
    paint_bg(cp.hero, &p[P_HERO]);
    paint_text(cp.clock, &p[P_HERO_TEXT]);
    paint_text(cp.date, &p[P_HERO_MUTED]);
    paint_text(cp.next, &p[P_HERO_MUTED]);
    paint_text(cp.count, &p[P_COUNT]);
    card_theme(&cp.card, th_combined);
}

/* Short clock for the small pages: "1:05 PM" / "13:05". */
static void short_clock(char *out, size_t len)
{
    snprintf(out, len, "%s%s%s", st.hm, st.ap[0] ? " " : "", st.ap);
}

static void notices_update(const cJSON *cfg)
{
    char buf[64];
    set_text(np.name, jstr(cfg, "mosque.name", ""));
    short_clock(buf, sizeof(buf));
    set_text(np.clock, buf);
    set_text(np.next, st.active >= 0 ? st.next_name : "");
    set_text(np.count, st.countdown);
    card_show(&np.card, cfg, notice_n ? notice_idx[notice_at % notice_n] : -1, 150);
}

static void combined_update(const cJSON *cfg)
{
    const paint_t *p = th_combined->p;
    char buf[64];
    short_clock(buf, sizeof(buf));
    set_text(cp.clock, buf);
    set_text(cp.date, st.hijri[0] ? st.hijri : st.date_long);
    set_text(cp.next, st.active >= 0 ? st.next_name : "");
    set_text(cp.count, st.countdown);
    for (int i = 0; i < PRAYER_N; i++) {
        bool on = i == st.active;
        paint_bg(cp.cell[i], on ? &p[P_ACCENT] : &p[P_PANEL]);
        paint_text(cp.cname[i], on ? &p[P_ON_ACCENT] : &p[P_PANEL_MUTED]);
        paint_text(cp.ctime[i], on ? &p[P_ON_ACCENT] : &p[P_PANEL_TEXT]);
        paint_text(cp.ciq[i], on ? &p[P_ON_ACCENT] : &p[P_PANEL_MUTED]);
        fmt_min(st.times.azan[i], h24, buf, sizeof(buf));
        char *sp = strchr(buf, ' ');
        if (sp) *sp = 0;                          /* "5:45" without AM/PM: no room */
        set_text(cp.ctime[i], buf);
        fmt_min(st.times.iqamah[i], h24, buf, sizeof(buf));
        sp = strchr(buf, ' ');
        if (sp) *sp = 0;
        set_text(cp.ciq[i], buf);
    }
    card_show(&cp.card, cfg, notice_n ? notice_idx[notice_at % notice_n] : -1, 70);
}

/* ---------------------------------------------------------------------------------------------- */
/* the admin screen and Wi-Fi setup                                                               */

static struct {
    lv_obj_t *root, *id, *pass, *info, *qr;
    int64_t until;
} ad;

static struct {
    lv_obj_t *root, *title, *status, *list, *form, *form_title, *pass, *kb, *back;
    char chosen[33];
    bool secure;
    int64_t list_at;
} wf;

static void admin_hide(lv_event_t *e) { lv_obj_add_flag(ad.root, LV_OBJ_FLAG_HIDDEN); }
static void wifi_open(lv_event_t *e);

static lv_obj_t *button(lv_obj_t *parent, const char *label, int x, int y, int w, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, 56);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_set_style_bg_color(b, col(0x0A4436), 0);
    lv_obj_set_style_bg_color(b, col(0x16624F), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(b, col(0xC19749), 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &serif_b18, 0);
    lv_obj_set_style_text_color(l, col(0xFFFBEF), 0);
    lv_label_set_text(l, label);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    return b;
}

static void admin_build(void)
{
    ad.root = box(scr, 0, 0, W, H);
    lv_obj_add_flag(ad.root, LV_OBJ_FLAG_CLICKABLE);       /* takes the taps, so the pages under it do not */
    lv_obj_set_style_bg_color(ad.root, col(0x0b0e13), 0);
    lv_obj_set_style_bg_opa(ad.root, 245, 0);
    lv_obj_t *l = text(ad.root, &font_small, 20, 16, 260, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_color(l, col(0xb8c0cc), 0);
    lv_label_set_text(l, "Screen ID");
    ad.id = text(ad.root, &font_title, 20, 36, 300, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_color(ad.id, col(0xffd479), 0);
    ad.qr = lv_qrcode_create(ad.root);
    lv_qrcode_set_size(ad.qr, 124);
    lv_qrcode_set_dark_color(ad.qr, lv_color_black());
    lv_qrcode_set_light_color(ad.qr, lv_color_white());
    lv_obj_set_pos(ad.qr, W - 124 - 22, 22);
    lv_obj_set_style_border_color(ad.qr, lv_color_white(), 0);
    lv_obj_set_style_border_width(ad.qr, 6, 0);
    ad.info = lv_label_create(ad.root);
    lv_obj_set_style_text_font(ad.info, &font_small, 0);
    lv_obj_set_style_text_color(ad.info, col(0xe8ecf2), 0);
    lv_obj_set_pos(ad.info, 20, 160);
    lv_obj_set_width(ad.info, W - 40);
    lv_label_set_long_mode(ad.info, LV_LABEL_LONG_WRAP);
    button(ad.root, "Wi-Fi", 20, H - 70, 210, wifi_open);
    button(ad.root, "Close", W - 230, H - 70, 210, admin_hide);
    lv_obj_add_flag(ad.root, LV_OBJ_FLAG_HIDDEN);
}

static void admin_refresh(void)
{
    char buf[512], url[96];
    set_text(ad.id, link_id());
    if (wifi_connected()) snprintf(url, sizeof(url), "http://%s/", wifi_ip()); else url[0] = 0;
    static char qr_for[96] = "\x01";
    if (strcmp(qr_for, url)) {
        snprintf(qr_for, sizeof(qr_for), "%s", url);
        lv_qrcode_update(ad.qr, url[0] ? url : "-", url[0] ? strlen(url) : 1);
        lv_obj_set_flag(ad.qr, LV_OBJ_FLAG_HIDDEN, !url[0]);
    }
    int64_t at;
    const char *msg = wifi_last_message(&at);
    snprintf(buf, sizeof(buf),
             "Settings on this Wi-Fi:  %s%s\n"
             "or  http://prayer-%s.local/\n"
             "\n"
             "Wi-Fi:  %s%s%s\n"
             "Server:  %s\n"
             "Version:  %s",
             url[0] ? url : "(not connected)", url[0] ? "" : "", link_id_short(),
             wifi_connected() ? wifi_ssid() : msg[0] ? msg : "Not connected", wifi_connected() ? "  \xc2\xb7  signal " : "",
             wifi_connected() ? (snprintf(url, sizeof(url), "%d%%", wifi_signal()), url) : "",
             link_status(), FW_VERSION);
    set_text(ad.info, buf);
}

static void admin_show(void)
{
    admin_refresh();
    ad.until = esp_timer_get_time() / 1000 + 90000;
    lv_obj_remove_flag(ad.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ad.root);
}

/* ---- Wi-Fi setup ---- */

static void wifi_list_fill(void);

static void wifi_close(lv_event_t *e)
{
    lv_obj_add_flag(wf.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wf.form, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_rescan(lv_event_t *e)
{
    wifi_scan_start();
    set_text(wf.status, "Looking for networks\xe2\x80\xa6");
    wf.list_at = esp_timer_get_time() / 1000 + 4000;
}

static void wifi_form_cancel(lv_event_t *e) { lv_obj_add_flag(wf.form, LV_OBJ_FLAG_HIDDEN); }

static void wifi_form_ok(lv_event_t *e)
{
    const char *pass = lv_textarea_get_text(wf.pass);
    if (wf.secure && strlen(pass) < 8) { set_text(wf.form_title, "The password has at least 8 characters"); return; }
    wifi_save(wf.chosen, wf.secure ? pass : "", true);
    lv_obj_add_flag(wf.form, LV_OBJ_FLAG_HIDDEN);
    char buf[80];
    snprintf(buf, sizeof(buf), "Joining \"%s\"\xe2\x80\xa6", wf.chosen);
    set_text(wf.status, buf);
}

static void wifi_kb_event(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_READY) wifi_form_ok(e);
    else if (lv_event_get_code(e) == LV_EVENT_CANCEL) wifi_form_cancel(e);
}

static void wifi_pick(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target(e);
    wifi_net_t *n = lv_obj_get_user_data(b);
    snprintf(wf.chosen, sizeof(wf.chosen), "%s", n->ssid);
    wf.secure = n->secure;
    if (!wf.secure) { wifi_form_ok(e); return; }
    char buf[80];
    snprintf(buf, sizeof(buf), "Password for \"%s\"", wf.chosen);
    set_text(wf.form_title, buf);
    lv_textarea_set_text(wf.pass, "");
    lv_obj_remove_flag(wf.form, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(wf.form);
}

static wifi_net_t listed[24];

static void wifi_list_fill(void)
{
    int n = wifi_scan_results(listed, 24);
    lv_obj_clean(wf.list);
    for (int i = 0; i < n; i++) {
        char label[64];
        int q = 2 * (listed[i].rssi + 100);
        q = q < 0 ? 0 : q > 100 ? 100 : q;
        snprintf(label, sizeof(label), "%s%s%s", listed[i].ssid, listed[i].secure ? "" : "  (open)", q > 60 ? "" : "  \xe2\x80\xa2 weak");
        lv_obj_t *b = lv_list_add_button(wf.list, NULL, label);
        lv_obj_set_style_text_font(b, &serif_b18, 0);
        lv_obj_set_style_bg_color(b, col(0x0A4436), 0);
        lv_obj_set_style_bg_color(b, col(0x16624F), LV_STATE_PRESSED);
        lv_obj_set_style_text_color(b, col(0xFFFBEF), 0);
        lv_obj_set_style_border_color(b, col(0xC19749), 0);
        lv_obj_set_style_border_opa(b, 120, 0);
        lv_obj_set_style_pad_ver(b, 16, 0);
        lv_obj_set_user_data(b, &listed[i]);
        lv_obj_add_event_cb(b, wifi_pick, LV_EVENT_CLICKED, NULL);
    }
    if (!n) set_text(wf.status, "No networks found. Tap Look again.");
}

static void wifi_build(void)
{
    wf.root = box(scr, 0, 0, W, H);
    lv_obj_add_flag(wf.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(wf.root, col(0x03251F), 0);
    lv_obj_set_style_bg_grad_color(wf.root, col(0x0B4A3B), 0);
    lv_obj_set_style_bg_grad_dir(wf.root, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(wf.root, 255, 0);
    wf.title = text(wf.root, &serif_b30, 80, 16, W - 160, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_color(wf.title, col(0xECCF99), 0);
    lv_label_set_text(wf.title, "Wi-Fi");
    wf.status = text(wf.root, &serif_r17, 16, 64, W - 32, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_color(wf.status, col(0xD3D3B6), 0);
    wf.list = lv_list_create(wf.root);
    lv_obj_set_pos(wf.list, 16, 94);
    lv_obj_set_size(wf.list, W - 32, H - 94 - 84);
    lv_obj_set_style_bg_color(wf.list, col(0x0A4436), 0);
    lv_obj_set_style_border_color(wf.list, col(0xC19749), 0);
    lv_obj_set_style_border_width(wf.list, 2, 0);
    lv_obj_set_style_radius(wf.list, 14, 0);
    lv_obj_set_style_pad_all(wf.list, 4, 0);
    button(wf.root, "Look again", 16, H - 72, 216, wifi_rescan);
    wf.back = button(wf.root, "Back", W - 232, H - 72, 216, wifi_close);

    wf.form = box(wf.root, 0, 0, W, H);
    lv_obj_add_flag(wf.form, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(wf.form, col(0x03251F), 0);
    lv_obj_set_style_bg_opa(wf.form, 255, 0);
    wf.form_title = text(wf.form, &serif_b18, 16, 14, W - 32, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_style_text_color(wf.form_title, col(0xECCF99), 0);
    wf.pass = lv_textarea_create(wf.form);
    lv_textarea_set_one_line(wf.pass, true);
    lv_textarea_set_max_length(wf.pass, 63);
    lv_obj_set_pos(wf.pass, 16, 46);
    lv_obj_set_size(wf.pass, W - 32, 52);
    lv_obj_set_style_text_font(wf.pass, &serif_b18, 0);
    lv_obj_set_style_border_color(wf.pass, col(0xC19749), 0);
    wf.kb = lv_keyboard_create(wf.form);
    lv_obj_set_size(wf.kb, W, H - 110);
    lv_obj_align(wf.kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(wf.kb, wf.pass);
    lv_obj_set_style_text_font(wf.kb, &font_text, LV_PART_ITEMS);
    lv_obj_add_event_cb(wf.kb, wifi_kb_event, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(wf.form, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wf.root, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_show(bool can_close)
{
    lv_obj_add_flag(ad.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(wf.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(wf.root);
    lv_obj_set_flag(wf.back, LV_OBJ_FLAG_HIDDEN, !can_close);
    wifi_rescan(NULL);
}

static void wifi_open(lv_event_t *e) { wifi_show(true); }

static void wifi_tick(void)
{
    if (lv_obj_has_flag(wf.root, LV_OBJ_FLAG_HIDDEN)) return;
    int64_t now = esp_timer_get_time() / 1000;
    if (wf.list_at && now > wf.list_at) {
        wf.list_at = 0;
        wifi_list_fill();
        set_text(wf.status, wifi_connected() ? "Connected. Tap a network to change." : "Tap your network.");
    }
    if (wifi_connected()) {
        char buf[96];
        snprintf(buf, sizeof(buf), "Connected to \"%s\"", wifi_ssid());
        set_text(wf.status, buf);
        lv_obj_remove_flag(wf.back, LV_OBJ_FLAG_HIDDEN);
    } else {
        int64_t at;
        const char *m = wifi_last_message(&at);
        if (m[0] && now - at < 30000 && !wf.list_at) set_text(wf.status, m);
    }
}

/* ---------------------------------------------------------------------------------------------- */
/* splash, rotation, the 4 Hz tick                                                                */

static lv_obj_t *splash;
static picture_t splash_pic;
static int64_t splash_until;

static void splash_build(void)
{
    splash = box(scr, 0, 0, W, H);
    lv_obj_set_style_bg_color(splash, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(splash, 255, 0);
    if (picture_load_file(&splash_pic, "/www/screen/logo.png", "splash")) {
        lv_obj_t *img = lv_image_create(splash);
        lv_image_set_src(img, &splash_pic.dsc);
        lv_obj_set_size(img, 300, 300);
        lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CONTAIN);
        lv_obj_center(img);
    } else {
        lv_obj_t *l = lv_label_create(splash);
        lv_obj_set_style_text_font(l, &font_title, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_label_set_text(l, "Prayer display");
        lv_obj_center(l);
    }
    splash_until = esp_timer_get_time() / 1000 + 3500;
}

static void read_settings(const cJSON *cfg)
{
    schedule_apply_zone(cfg);
    const char *tpl = template_resolve(jstr(cfg, "templateId", "sanctuary"));
    th = theme_for_template(cfg, tpl);
    th_notice = theme_for_design(cfg, "noticeDesign");
    th_combined = theme_for_design(cfg, "combinedDesign");
    h24 = jbool(cfg, "options.clock24", false);
    azan_set_volume((int)jnum(cfg, "options.volume", 100));
    prayer_theme();
    notices_theme();
    combined_theme();
    set_text(pp.name, jstr(cfg, "mosque.name", ""));

    const char *logo = jstr(cfg, "mosque.logo", "");
    if (*logo && picture_load_data_url(&logo_pic, logo)) {
        lv_image_set_src(pp.logo, &logo_pic.dsc);
        lv_obj_remove_flag(pp.logo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(pp.name, 60, 10);
        lv_obj_set_width(pp.name, W - 120);
    } else {
        lv_image_set_src(pp.logo, NULL);
        picture_free(&logo_pic);
        lv_obj_add_flag(pp.logo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(pp.name, 20, 10);
        lv_obj_set_width(pp.name, W - 40);
    }

    for (int i = 0; i < PAGE_N; i++) {
        char path[40];
        snprintf(path, sizeof(path), "pages.%s.show", PAGE_KEY[i]);
        bool show = jbool(cfg, path, i == PAGE_PRAYER);
        snprintf(path, sizeof(path), "pages.%s.seconds", PAGE_KEY[i]);
        int secs = (int)jnum(cfg, path, PAGE_DEFAULT_SECS[i]);
        page_secs[i] = show ? (secs < 3 ? 3 : secs) : 0;
    }

    message_n = 0;
    const char *m = jstr(cfg, "options.messages", "");
    while (*m && message_n < MAX_MSG) {
        const char *nl = strchr(m, '\n');
        size_t n = nl ? (size_t)(nl - m) : strlen(m);
        while (n && (m[n - 1] == '\r' || m[n - 1] == ' ')) n--;
        if (n) snprintf(messages[message_n++], sizeof(messages[0]), "%.*s", (int)n, m);
        if (!nl) break;
        m = nl + 1;
    }
    np.card.shown = cp.card.shown = -2;
}

/* Notices on today (core.js activeNotices): enabled, and inside their dates. */
static void read_notices(const cJSON *cfg)
{
    char today[12];
    snprintf(today, sizeof(today), "%04d-%02d-%02d", st.now.tm_year + 1900, st.now.tm_mon + 1, st.now.tm_mday);
    notice_n = 0;
    int i = 0;
    const cJSON *n;
    cJSON_ArrayForEach(n, jget(cfg, "notices")) {
        const char *from = jstr(n, "from", ""), *until = jstr(n, "until", "");
        if (jbool(n, "enabled", true) && (!*from || strcmp(from, today) <= 0) && (!*until || strcmp(until, today) >= 0) && notice_n < MAX_NOTICES)
            notice_idx[notice_n++] = i;
        i++;
    }
}

/* The home pages: prayer and weather, turning every 30 s as on the app; a tap turns the page. */
static int home_page = HOME_PRAYER;
static int64_t home_until, undim_until;

static void show_page(int p)
{
    page = p;
    home_show(home_page);
    lv_obj_add_flag(pp.root, LV_OBJ_FLAG_HIDDEN);       /* the mosque-style page is not used on a home screen */
    lv_obj_set_flag(np.root, LV_OBJ_FLAG_HIDDEN, p != PAGE_NOTICES);
    lv_obj_set_flag(cp.root, LV_OBJ_FLAG_HIDDEN, p != PAGE_COMBINED);
}

static void next_page(int64_t now)
{
    /* One page for now: the home prayer page. */
    if (page != PAGE_PRAYER) show_page(PAGE_PRAYER);
    page_until = now + 3600 * 1000LL;
    return;
    for (int k = 1; k <= PAGE_N; k++) {
        int p = (page + k + PAGE_N) % PAGE_N;
        if (!page_secs[p]) continue;
        if (p != PAGE_PRAYER && !notice_n) continue;       /* nothing to show on a notices page */
        if (p != page) show_page(p);
        page_until = now + page_secs[p] * 1000LL;
        return;
    }
    if (page != PAGE_PRAYER) show_page(PAGE_PRAYER);
    page_until = now + 3600 * 1000LL;
}

static int backlight = -1;

static void set_backlight(int percent)
{
    if (percent == backlight) return;
    backlight = percent;
    bsp_display_brightness_set(percent);
}

static void on_long_press(lv_event_t *e) { admin_show(); }
static void turn_page(int64_t now)
{
    weather_t w;
    weather_get(&w);
    home_page = home_page == HOME_PRAYER && w.valid ? HOME_WEATHER : HOME_PRAYER;
    home_show(home_page);
    home_until = now + 30000;
    last_sec = -1;                       /* redraw now */
}

/* A tap stops the Azan; on a dimmed screen it brightens it for a minute; otherwise it turns the page. */
static void on_tap(lv_event_t *e)
{
    int64_t now = esp_timer_get_time() / 1000;
    if (azan_playing()) azan_stop();
    else if (backlight < 100 && now >= undim_until) undim_until = now + 60000;
    else turn_page(now);
}

static void on_gear(lv_event_t *e) { settings_open(); }
static void wifi_from_settings(void) { wifi_show(true); }

void ui_close_overlays(void)
{
    lv_obj_add_flag(wf.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ad.root, LV_OBJ_FLAG_HIDDEN);
    settings_show(-1);
}

/* The Azan at each Azan time, once per prayer per day (AzanService.java). */
static void azan_check(void)
{
    static int fired = -1;
    if (!clock_is_set()) return;
    int now = st.now.tm_hour * 60 + st.now.tm_min;
    for (int i = 0; i < PRAYER_N; i++) {
        int key = st.now.tm_yday * 10 + i;
        if (st.times.azan[i] == now && fired != key) {
            fired = key;
            ESP_LOGI(TAG, "%s Azan", PRAYER_NAME[i]);
            azan_play(i);
        }
    }
}

static void tick(lv_timer_t *t)
{
    int64_t now = esp_timer_get_time() / 1000;
    if (splash) {
        if (now < splash_until) return;
        lv_obj_delete(splash);
        splash = NULL;
        picture_free(&splash_pic);
    }
    config_lock();
    const cJSON *cfg = config_get();
    bool changed = config_generation != seen_generation;
    if (changed) {
        seen_generation = config_generation;
        read_settings(cfg);
    }
    time_t secs = time(NULL);
    if (changed || secs != last_sec) {
        bool new_day = st.now.tm_mday != 0;
        int day = st.now.tm_mday;
        last_sec = secs;
        schedule_state(cfg, &st);
        new_day = new_day && day != st.now.tm_mday;
        if (changed || new_day || secs % 60 == 0) read_notices(cfg);
        if (page < 0 || now >= page_until || changed) {
            if (changed && page >= 0) { int keep = page; page = keep - 1; }
            next_page(now);
        }
        if (notice_n && (notice_at < 0 || now >= notice_until)) {
            notice_at++;
            const cJSON *n = cJSON_GetArrayItem(jget(cfg, "notices"), notice_idx[notice_at % notice_n]);
            int d = (int)jnum(n, "duration", 12);
            notice_until = now + (d < 3 ? 3 : d) * 1000LL;
        }
        if (secs % 6 == 0) message_at++;
        char foot[200];
        if (!clock_is_set()) snprintf(foot, sizeof(foot), "%s", wifi_connected() ? "Setting the clock\xe2\x80\xa6" : LV_SYMBOL_WARNING "  No Wi-Fi: the clock is not set. Hold the screen for Wi-Fi.");
        else footer_text(foot, sizeof(foot));
        set_text(pp.foot_text, foot);
        if (page == PAGE_PRAYER) {
            const char *city = jstr(cfg, "calc.city", "");
            if (!*city) city = jstr(cfg, "mosque.name", "");        /* no place chosen yet: the home's name */
            char playing[64];
            snprintf(playing, sizeof(playing), "%s Azan playing \xe2\x80\xa2 tap to stop", azan_prayer() >= 0 ? PRAYER_NAME[azan_prayer()] : "");
            const char *status = azan_playing() ? playing : !clock_is_set() ? (wifi_connected() ? "Setting the clock\xe2\x80\xa6" : "No Wi-Fi: the clock is not set. Tap the gear for Wi-Fi.") : "";
            if (now >= home_until) turn_page(now);
            weather_t wx;
            weather_get(&wx);
            home_update(home_page, &st, city, h24, status, &wx);
            settings_tick();
            azan_check();
        }
        else if (page == PAGE_NOTICES) notices_update(cfg);
        else combined_update(cfg);
        if (!lv_obj_has_flag(ad.root, LV_OBJ_FLAG_HIDDEN)) {
            admin_refresh();
            if (now > ad.until) lv_obj_add_flag(ad.root, LV_OBJ_FLAG_HIDDEN);
        }
    }
    float dim = st.dim;
    config_unlock();
    wifi_tick();
    /* Dimming (prayer curtain, night): the backlight, not a dark layer. Never fully off, and full while someone
     * is using the admin or Wi-Fi screens. */
    bool using = !lv_obj_has_flag(ad.root, LV_OBJ_FLAG_HIDDEN) || !lv_obj_has_flag(wf.root, LV_OBJ_FLAG_HIDDEN) || settings_visible()
                 || azan_playing() || esp_timer_get_time() / 1000 < undim_until;
    int pct = using ? 100 : (int)lroundf(100 * (1 - dim));
    set_backlight(pct < 1 ? 1 : pct);
}

void ui_start(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .double_buffer = true,
        .flags = {.buff_spiram = true},
    };
    cfg.lvgl_port_cfg.task_stack = 12288;
    cfg.lvgl_port_cfg.task_affinity = 1;
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    if (!disp) { ESP_LOGE(TAG, "no display"); return; }
    set_backlight(100);
    bsp_display_lock(0);
    scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_indev_t *touch = bsp_display_get_input_dev();
    if (touch) lv_indev_set_long_press_time(touch, 3000);
    lv_obj_add_event_cb(scr, on_long_press, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(scr, on_tap, LV_EVENT_SHORT_CLICKED, NULL);
    prayer_build();
    home_build(scr, on_gear);
    settings_build(scr, wifi_from_settings);
    notices_build();
    combined_build();
    admin_build();
    wifi_build();
    /* No start-up logo: it is the mosque display's. */
    show_page(PAGE_PRAYER);
    lv_timer_create(tick, 250, NULL);
    bsp_display_unlock();
}

bool ui_screenshot_send(bool (*write)(void *ctx, const void *data, size_t len), void *ctx)
{
    if (!bsp_display_lock(2000)) return false;
    lv_draw_buf_t *snap = lv_snapshot_take(scr, LV_COLOR_FORMAT_RGB565);
    bsp_display_unlock();
    if (!snap) return false;
    int w = snap->header.w, h = snap->header.h, stride = snap->header.stride;
    size_t row = w * 2, size = 66 + row * h;
    uint8_t b[66] = {0};
    b[0] = 'B'; b[1] = 'M';
    *(uint32_t *)(b + 2) = size;
    *(uint32_t *)(b + 10) = 66;
    *(uint32_t *)(b + 14) = 40;
    *(int32_t *)(b + 18) = w;
    *(int32_t *)(b + 22) = -h;          /* top-down */
    *(uint16_t *)(b + 26) = 1;
    *(uint16_t *)(b + 28) = 16;
    *(uint32_t *)(b + 30) = 3;          /* BI_BITFIELDS */
    *(uint32_t *)(b + 34) = row * h;
    *(uint32_t *)(b + 54) = 0xF800;
    *(uint32_t *)(b + 58) = 0x07E0;
    *(uint32_t *)(b + 62) = 0x001F;
    /* Straight from the snapshot, so a second copy of the screen is never needed. */
    bool ok = write(ctx, b, sizeof(b));
    for (int y = 0; ok && y < h; y += 16) {
        int n = h - y < 16 ? h - y : 16;
        if (stride == (int)row) ok = write(ctx, snap->data + y * stride, row * n);
        else for (int k = 0; ok && k < n; k++) ok = write(ctx, snap->data + (y + k) * stride, row);
    }
    lv_draw_buf_destroy(snap);
    return ok;
}
