#pragma once
#include <stdint.h>

/* One paint from a theme: a colour, or a two-stop gradient (first and last stop of the CSS one). */
typedef struct {
    uint32_t c1, c2;
    uint8_t opa;     /* 0-255 */
    uint8_t grad;    /* 0 none, 1 vertical, 2 horizontal, 3 radial (drawn as vertical) */
} paint_t;

enum {
    P_BG, P_TEXT, P_MUTED, P_HEADING, P_CLOCK, P_PANEL, P_PANEL_TEXT, P_PANEL_MUTED, P_HERO, P_HERO_TEXT, P_HERO_MUTED,
    P_ACCENT, P_ON_ACCENT, P_COUNT, P_INK, P_PANEL_INK, P_LINE, P_PATTERN, P__N
};

typedef struct {
    const char *id;
    const char *name;
    paint_t p[P__N];
} theme_t;

typedef struct {
    const char *template_id;
    const char *theme_id;
} template_theme_t;

extern const theme_t THEMES[];
extern const int THEME_COUNT;
extern const template_theme_t TEMPLATE_THEMES[];
extern const int TEMPLATE_COUNT;
extern const template_theme_t TEMPLATE_ALIASES[];
extern const int TEMPLATE_ALIAS_COUNT;

const theme_t *theme_find(const char *id);
const char *template_resolve(const char *id);           /* follows aliases; unknown -> first template */
const char *template_default_theme(const char *template_id);
