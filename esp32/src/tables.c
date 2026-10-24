/* Lookups over the generated tables in gen/. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "places.h"
#include "themes.h"
#include "tzdb.h"

const theme_t *theme_find(const char *id)
{
    if (id) for (int i = 0; i < THEME_COUNT; i++) if (!strcmp(THEMES[i].id, id)) return &THEMES[i];
    return NULL;
}

const char *template_resolve(const char *id)
{
    if (!id) return TEMPLATE_THEMES[0].template_id;
    for (int i = 0; i < TEMPLATE_ALIAS_COUNT; i++)
        if (!strcmp(TEMPLATE_ALIASES[i].template_id, id)) { id = TEMPLATE_ALIASES[i].theme_id; break; }
    for (int i = 0; i < TEMPLATE_COUNT; i++) if (!strcmp(TEMPLATE_THEMES[i].template_id, id)) return TEMPLATE_THEMES[i].template_id;
    return TEMPLATE_THEMES[0].template_id;
}

const char *template_default_theme(const char *template_id)
{
    for (int i = 0; i < TEMPLATE_COUNT; i++)
        if (!strcmp(TEMPLATE_THEMES[i].template_id, template_id)) return TEMPLATE_THEMES[i].theme_id;
    return THEMES[0].id;
}

/* "UTC+05:45", "GMT-6", "UTC-06:00" -> minutes east of UTC; returns 0 and sets *ok false when it is not one. */
static int parse_offset(const char *s, bool *ok)
{
    *ok = false;
    while (*s == ' ') s++;
    if (strncasecmp(s, "UTC", 3) && strncasecmp(s, "GMT", 3)) return 0;
    s += 3;
    while (*s == ' ') s++;
    int sign;
    if (*s == '+') sign = 1;
    else if (*s == '-') sign = -1;
    else if (!strncmp(s, "\xe2\x88\x92", 3)) { sign = -1; s += 2; }      /* the minus sign U+2212 */
    else return 0;
    s++;
    while (*s == ' ') s++;
    if (!isdigit((unsigned char)*s)) return 0;
    int h = 0, m = 0, digits = 0;
    while (isdigit((unsigned char)*s) && digits < 2) { h = h * 10 + (*s++ - '0'); digits++; }
    if (*s == ':') s++;
    if (isdigit((unsigned char)*s)) {
        if (!isdigit((unsigned char)s[1])) return 0;
        m = (s[0] - '0') * 10 + (s[1] - '0');
        s += 2;
        if (m > 59) return 0;
    }
    while (*s == ' ') s++;
    if (*s) return 0;
    int minutes = sign * (h * 60 + m);
    if (minutes < -12 * 60 || minutes > 14 * 60) return 0;
    *ok = true;
    return minutes;
}

bool tz_posix(const char *zone, char *out, size_t len)
{
    snprintf(out, len, "UTC0");
    if (!zone || !*zone) return false;
    bool ok;
    int minutes = parse_offset(zone, &ok);
    if (ok) {
        /* POSIX counts west as positive: UTC+05:45 is "<+0545>-5:45". */
        int a = abs(minutes);
        snprintf(out, len, "<%c%02d%02d>%c%d:%02d", minutes < 0 ? '-' : '+', a / 60, a % 60, minutes < 0 ? '+' : '-', a / 60, a % 60);
        return true;
    }
    int lo = 0, hi = TZ_DB_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2, c = strcmp(TZ_DB[mid].name, zone);
        if (!c) { snprintf(out, len, "%s", TZ_DB[mid].posix); return true; }
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return false;
}

const char *country_name(const char *code, size_t len)
{
    for (int i = 0; i < COUNTRY_COUNT; i++)
        if (strlen(COUNTRIES[i].code) == len && !strncasecmp(COUNTRIES[i].code, code, len)) return COUNTRIES[i].name;
    return NULL;
}
