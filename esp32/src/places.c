/* The offline city list behind the settings page's place search (GET /api/places), as in the Android app's
 * Places.java: the "places" partition holds places.tsv as built by tools/build_files.mjs, mapped only during a search.
 * Columns: name, folded name, country code, region, latitude, longitude, time zone, population, district flag. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "places.h"
#include "tzdb.h"

static const char *TAG = "places";
static const char *data;
static size_t size;
static const esp_partition_t *part;
static esp_partition_mmap_handle_t map_handle;

/* The city list is mapped only while a search runs. While any flash page is mapped, IDF must turn the cache off
 * for every flash write, and the screen's bounce-buffer interrupt then reads the PSRAM frame buffer with the
 * cache off and crashes the board. With nothing mapped (and XIP from PSRAM) the cache stays on during writes. */
static bool map_list(void)
{
    const void *map;
    if (!part || esp_partition_mmap(part, 0, part->size, ESP_PARTITION_MMAP_DATA, &map, &map_handle) != ESP_OK) return false;
    data = (const char *)map + 17;
    return true;
}

static void unmap_list(void)
{
    esp_partition_munmap(map_handle);
    data = NULL;
}

void places_init(void)
{
    const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "places");
    char head[18] = {0};
    if (!p || esp_partition_read(p, 0, head, 17) != ESP_OK) return;
    if (memcmp(head, "PLACES1\n", 8)) { ESP_LOGW(TAG, "no city list written (tools/flash_files.sh)"); return; }
    size = strtoul(head + 8, NULL, 16);
    if (size == 0 || size > p->size - 17) return;
    part = p;
    ESP_LOGI(TAG, "city list: %u KB", (unsigned)(size / 1024));
}

/* Lower case without accents, like Places.fold() for the common Latin letters. */
static void fold(const char *in, char *out, size_t len)
{
    size_t k = 0;
    const unsigned char *s = (const unsigned char *)in;
    bool space = true;
    while (*s && k + 3 < len) {
        unsigned c = *s++;
        char a = 0, b = 0;
        if (c < 0x80) a = (char)tolower(c);
        else if (c == 0xC3 && *s) {
            unsigned d = *s++ | 0x20;                 /* fold capital into small: À (80) -> à (A0) */
            if (d >= 0xA0 && d <= 0xA5) a = 'a';
            else if (d == 0xA6) { a = 'a'; b = 'e'; }
            else if (d == 0xA7) a = 'c';
            else if (d >= 0xA8 && d <= 0xAB) a = 'e';
            else if (d >= 0xAC && d <= 0xAF) a = 'i';
            else if (d == 0xB1) a = 'n';
            else if ((d >= 0xB2 && d <= 0xB6) || d == 0xB8) a = 'o';
            else if (d >= 0xB9 && d <= 0xBC) a = 'u';
            else if (d == 0xBD || d == 0xBF) a = 'y';
            else if (*(s - 1) == 0x9F) { a = 's'; b = 's'; }
        } else if (c >= 0xC4 && c <= 0xC5 && *s) {
            /* Latin Extended-A: pairs of capital/small; map the usual ones by their first byte and range. */
            unsigned d = *s++, code = ((c & 0x1F) << 6) | (d & 0x3F);
            static const char ext[] = "aaaaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiiiiiiiiijjkkkllllllllllnnnnnnnnnoooooooorrrrrrssssssssttttttuuuuuuuuuuuuwwyyyzzzzzzs";   /* U+0100..U+017F without accents */
            if (code >= 0x100 && code < 0x100 + sizeof(ext) - 1) a = ext[code - 0x100];
        } else {
            while ((*s & 0xC0) == 0x80) s++;         /* any other letter: skipped */
            continue;
        }
        if (!a) continue;
        if (a == ' ' || a == '\t' || a == '\n') { if (!space) out[k++] = ' '; space = true; continue; }
        out[k++] = a;
        space = false;
        if (b) out[k++] = b;
    }
    while (k && out[k - 1] == ' ') k--;
    out[k] = 0;
}

typedef struct {
    const char *line;
    int score;
} hit_t;

static const char *field(const char *line, int column, size_t *len)
{
    const char *p = line, *end = data + size;
    for (int t = 0; t < column && p < end; t++) {
        while (p < end && *p != '\t' && *p != '\n') p++;
        if (p < end && *p == '\t') p++;
    }
    const char *q = p;
    while (q < end && *q != '\t' && *q != '\n') q++;
    *len = q - p;
    return p;
}

static bool starts_with_folded(const char *field_text, size_t len, const char *want)
{
    char buf[96], copy[96];
    size_t n = len < sizeof(copy) - 1 ? len : sizeof(copy) - 1;
    memcpy(copy, field_text, n);
    copy[n] = 0;
    fold(copy, buf, sizeof(buf));
    return !strncmp(buf, want, strlen(want));
}

static bool matches_filters(const char *line, char filters[][64], int nf)
{
    for (int i = 0; i < nf; i++) {
        if (!filters[i][0]) continue;
        size_t cl, rl;
        const char *code = field(line, 2, &cl), *region = field(line, 3, &rl);
        const char *country = country_name(code, cl);
        bool ok = (country && starts_with_folded(country, strlen(country), filters[i])) || starts_with_folded(region, rl, filters[i]) ||
                  (strlen(filters[i]) == cl && !strncasecmp(code, filters[i], cl));
        if (!ok) return false;
    }
    return true;
}

static int score(const char *key, size_t len, const char *want, size_t n)
{
    if (len < n) return 0;
    if (!memcmp(key, want, n)) return len == n ? 3 : 2;
    for (size_t i = 1; i + n <= len; i++)
        if ((key[i - 1] == ' ' || key[i - 1] == '-') && !memcmp(key + i, want, n)) return 1;
    return 0;
}

static int match(const char *city, char filters[][64], int nf, hit_t *best, int limit)
{
    size_t n = strlen(city);
    int found = 0;
    if (!n) return 0;
    for (const char *line = data, *end = data + size; line < end;) {
        const char *nl = memchr(line, '\n', end - line);
        if (!nl) nl = end;
        const char *key = memchr(line, '\t', nl - line);
        if (key) {
            key++;
            const char *ke = memchr(key, '\t', nl - key);
            int sc = ke ? score(key, ke - key, city, n) : 0;
            if (sc && matches_filters(line, filters, nf)) {
                int at = found;
                while (at > 0 && best[at - 1].score < sc) at--;
                if (at < limit) {
                    if (found < limit) found++;
                    memmove(best + at + 1, best + at, sizeof(hit_t) * (found - at - 1));
                    best[at] = (hit_t){line, sc};
                }
            }
        }
        line = nl + 1;
    }
    return found;
}

/* ---- the offset a zone has today, for "timezone" in the reply (GMT-06:00, as the settings page writes it) ---- */

static const char *posix_offset(const char *p, int *seconds)
{
    int sign = 1, h = 0, m = 0, s = 0;
    if (*p == '+' || *p == '-') sign = *p++ == '-' ? -1 : 1;
    while (isdigit((unsigned char)*p)) h = h * 10 + (*p++ - '0');
    if (*p == ':') { p++; while (isdigit((unsigned char)*p)) m = m * 10 + (*p++ - '0'); }
    if (*p == ':') { p++; while (isdigit((unsigned char)*p)) s = s * 10 + (*p++ - '0'); }
    *seconds = -sign * (h * 3600 + m * 60 + s);     /* POSIX counts west as positive; we want east */
    return p;
}

static const char *posix_name(const char *p)
{
    if (*p == '<') { while (*p && *p != '>') p++; return *p ? p + 1 : p; }
    while (isalpha((unsigned char)*p)) p++;
    return p;
}

/* The day of the month for "Mm.w.d" in a year. */
static int rule_day(int year, int m, int w, int d)
{
    struct tm t = {.tm_year = year - 1900, .tm_mon = m - 1, .tm_mday = 1, .tm_hour = 12};
    time_t x = timegm(&t);
    gmtime_r(&x, &t);
    int first = 1 + (d - t.tm_wday + 7) % 7;
    int day = first + (w - 1) * 7;
    static const int len[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int ml = len[m - 1] + (m == 2 && ((year % 4 == 0 && year % 100) || year % 400 == 0));
    while (day > ml) day -= 7;
    return day;
}

static const char *posix_rule(const char *p, int year, int offset, time_t *at)
{
    int m = 3, w = 2, d = 0, secs = 7200;
    if (*p == 'M') {
        p++;
        m = strtol(p, (char **)&p, 10);
        if (*p == '.') w = strtol(p + 1, (char **)&p, 10);
        if (*p == '.') d = strtol(p + 1, (char **)&p, 10);
    }
    if (*p == '/') { int s; p = posix_offset(p + 1, &s); secs = -s; }
    struct tm t = {.tm_year = year - 1900, .tm_mon = m - 1, .tm_mday = rule_day(year, m, w, d)};
    *at = timegm(&t) + secs - offset;
    return p;
}

static void zone_now(const char *zone, size_t zl, int *minutes, bool *dst, bool *known)
{
    char name[64], posix[64];
    snprintf(name, sizeof(name), "%.*s", (int)zl, zone);
    *known = tz_posix(name, posix, sizeof(posix));
    *dst = false;
    *minutes = 0;
    if (!*known) return;
    int std, dsto;
    const char *p = posix_offset(posix_name(posix), &std);
    *minutes = std / 60;
    if (!*p || *p == ',') return;
    p = posix_name(p);
    *dst = true;
    dsto = std + 3600;
    if (*p && *p != ',') p = posix_offset(p, &dsto);
    if (*p != ',') return;
    time_t now = time(NULL), start, end;
    struct tm t;
    gmtime_r(&now, &t);
    p = posix_rule(p + 1, t.tm_year + 1900, std, &start);
    if (*p != ',') return;
    posix_rule(p + 1, t.tm_year + 1900, dsto, &end);
    bool in = start < end ? (now >= start && now < end) : (now >= start || now < end);
    if (in) *minutes = dsto / 60;
}

char *places_search_json(const char *query, int limit)
{
    cJSON *o = cJSON_CreateObject();
    cJSON *results = cJSON_AddArrayToObject(o, "results");
    if (limit < 1) limit = 8;
    if (limit > 40) limit = 40;
    char folded[160];
    fold(query ? query : "", folded, sizeof(folded));
    if (strlen(query) <= 64 && map_list()) {
        char parts[4][64] = {{0}};
        int np = 0;
        for (char *s = folded; np < 4;) {
            char *c = strchr(s, ',');
            size_t n = c ? (size_t)(c - s) : strlen(s);
            while (n && s[n - 1] == ' ') n--;
            snprintf(parts[np++], 64, "%.*s", (int)n, s);
            if (!c) break;
            s = c + 1;
            while (*s == ' ') s++;
        }
        hit_t *best = heap_caps_calloc(limit + 1, sizeof(hit_t), MALLOC_CAP_SPIRAM);
        int found = 0;
        if (strlen(folded) >= 2 || (np > 1 && parts[0][0])) found = match(parts[0], parts + 1, np - 1, best, limit);
        /* "regina canada": with no comma, try the last words as the country or region. */
        for (char *sp = strrchr(folded, ' '); !found && np == 1 && sp && sp > folded + 1;) {
            char city[64], filter[1][64];
            snprintf(city, sizeof(city), "%.*s", (int)(sp - folded), folded);
            snprintf(filter[0], 64, "%s", sp + 1);
            found = match(city, filter, 1, best, limit);
            char *prev = sp - 1;
            while (prev > folded && *prev != ' ') prev--;
            sp = prev > folded ? prev : NULL;
        }
        for (int i = 0; i < found; i++) {
            size_t l;
            const char *f;
            cJSON *r = cJSON_CreateObject();
            char buf[96];
            f = field(best[i].line, 0, &l); snprintf(buf, sizeof(buf), "%.*s", (int)l, f); cJSON_AddStringToObject(r, "name", buf);
            f = field(best[i].line, 3, &l); snprintf(buf, sizeof(buf), "%.*s", (int)l, f); cJSON_AddStringToObject(r, "region", buf);
            f = field(best[i].line, 2, &l);
            const char *country = country_name(f, l);
            snprintf(buf, sizeof(buf), "%.*s", (int)l, f);
            cJSON_AddStringToObject(r, "country", country ? country : buf);
            cJSON_AddStringToObject(r, "country_code", buf);
            f = field(best[i].line, 4, &l); snprintf(buf, sizeof(buf), "%.4f", strtod(f, NULL)); cJSON_AddStringToObject(r, "latitude", buf);
            f = field(best[i].line, 5, &l); snprintf(buf, sizeof(buf), "%.4f", strtod(f, NULL)); cJSON_AddStringToObject(r, "longitude", buf);
            f = field(best[i].line, 6, &l);
            snprintf(buf, sizeof(buf), "%.*s", (int)l, f);
            cJSON_AddStringToObject(r, "zone", buf);
            int minutes;
            bool dst, known;
            zone_now(f, l, &minutes, &dst, &known);
            if (known) {
                minutes = (int)(lround(minutes / 15.0) * 15);
                int a = abs(minutes);
                snprintf(buf, sizeof(buf), "GMT%c%02d:%02d", minutes < 0 ? '-' : '+', a / 60, a % 60);
            } else buf[0] = 0;
            cJSON_AddStringToObject(r, "timezone", buf);
            cJSON_AddBoolToObject(r, "dst", dst);
            cJSON_AddItemToArray(results, r);
        }
        free(best);
        unmap_list();
    }
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return s;
}
