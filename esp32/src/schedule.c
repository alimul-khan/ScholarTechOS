#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "adhan.h"
#include "config.h"
#include "schedule.h"
#include "tzdb.h"

const char *const PRAYER_ID[PRAYER_N] = {"fajr", "dhuhr", "asr", "maghrib", "isha"};
const char *const PRAYER_NAME[PRAYER_N] = {"Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};
static const char *const MONTHS[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September",
                                     "October", "November", "December"};
static const char *const DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *const HIJRI_MONTHS[] = {"Muharram", "Safar", "Rabi\xe2\x80\x98 I", "Rabi\xe2\x80\x98 II", "Jumada I", "Jumada II", "Rajab",
                                           "Sha\xe2\x80\x98" "ban", "Ramadan", "Shawwal", "Dhu\xe2\x80\x98l-Qi\xe2\x80\x98" "dah", "Dhu\xe2\x80\x98l-Hijjah"};

extern const int HIJRI_FIRST_YEAR, HIJRI_FIRST_MONTH, HIJRI_MONTHS_COUNT;
extern const uint32_t HIJRI_MONTH_START[];

static char zone_applied[64] = "\x01";

bool clock_is_set(void)
{
    return time(NULL) > 1767225600;     /* after 1 Jan 2026: SNTP has answered */
}

void schedule_apply_zone(const cJSON *cfg)
{
    const char *zone = jstr(cfg, "options.timeZone", "");
    if (!strcmp(zone, zone_applied)) return;
    char posix[64];
    tz_posix(zone, posix, sizeof(posix));
    setenv("TZ", posix, 1);
    tzset();
    snprintf(zone_applied, sizeof(zone_applied), "%s", zone);
}

static int to_min(const char *s)
{
    if (!s) return -1;
    int h = 0, m = 0, n = 0;
    while (*s >= '0' && *s <= '9' && n < 2) { h = h * 10 + (*s++ - '0'); n++; }
    if (!n || *s++ != ':') return -1;
    if (!(s[0] >= '0' && s[0] <= '9' && s[1] >= '0' && s[1] <= '9')) return -1;
    m = (s[0] - '0') * 10 + (s[1] - '0');
    return h * 60 + m;
}

void fmt_min(int min, bool h24, char *out, size_t len)
{
    if (min < 0) { snprintf(out, len, "--:--"); return; }
    int h = (min / 60) % 24, m = min % 60;
    if (h24) snprintf(out, len, "%02d:%02d", h, m);
    else snprintf(out, len, "%d:%02d %s", (h % 12) ? h % 12 : 12, m, h < 12 ? "AM" : "PM");
}

/* An instant as minutes after midnight on the zone's wall clock. */
static int wall_min(time_t t)
{
    if (t == (time_t)-1) return -1;
    struct tm tm;
    localtime_r(&t, &tm);
    return tm.tm_hour * 60 + tm.tm_min;
}

static bool calculate(const cJSON *cfg, int y, int m, int d, int out[7])
{
    const cJSON *lat = jget(cfg, "calc.lat"), *lng = jget(cfg, "calc.lng");
    if ((cJSON_IsString(lat) && !*lat->valuestring) || (cJSON_IsString(lng) && !*lng->valuestring) || !lat || !lng) return false;
    double la = jnum(cfg, "calc.lat", NAN), lo = jnum(cfg, "calc.lng", NAN);
    if (!isfinite(la) || !isfinite(lo) || fabs(la) > 90 || fabs(lo) > 180) return false;
    const char *hl = jstr(cfg, "calc.highLat", "auto");
    adhan_highlat_t rule = !strcmp(hl, "middleofthenight") ? HL_MIDDLE : !strcmp(hl, "seventhofthenight") ? HL_SEVENTH
                         : !strcmp(hl, "twilightangle") ? HL_TWILIGHT : HL_AUTO;
    adhan_times_t t;
    adhan_calculate(la, lo, y, m, d, jstr(cfg, "calc.method", "NorthAmerica"), !strcmp(jstr(cfg, "calc.madhab", "shafi"), "hanafi"), rule, &t);
    out[0] = wall_min(t.fajr);
    out[1] = wall_min(t.dhuhr);
    out[2] = wall_min(t.asr);
    out[3] = wall_min(t.maghrib);
    out[4] = wall_min(t.isha);
    out[5] = wall_min(t.sunrise);
    out[6] = wall_min(t.maghrib);       /* the display's "sunset" is the maghrib time, as in prayertimes.js */
    return true;
}

void schedule_resolve(const cJSON *cfg, int y, int mo, int d, day_times_t *out)
{
    memset(out, 0, sizeof(*out));
    for (int p = 0; p < PRAYER_N; p++) {
        char path[32];
        snprintf(path, sizeof(path), "times.%s.azan", PRAYER_ID[p]);
        out->azan[p] = to_min(jstr(cfg, path, ""));
        snprintf(path, sizeof(path), "times.%s.iqamah", PRAYER_ID[p]);
        out->iqamah[p] = to_min(jstr(cfg, path, ""));
    }
    out->sunrise = to_min(jstr(cfg, "times.sunrise", ""));
    out->sunset = to_min(jstr(cfg, "times.sunset", ""));
    out->jumua1 = to_min(jstr(cfg, "times.jumua1", ""));
    out->jumua2 = to_min(jstr(cfg, "times.jumua2", ""));
    snprintf(out->source, sizeof(out->source), "manual");

    const char *source = jstr(cfg, "prayerSource", "manual");
    if (!strcmp(source, "manual")) return;

    const cJSON *row = NULL;
    if (!strcmp(source, "calendar")) {
        char key[12];
        snprintf(key, sizeof(key), "%04d-%02d-%02d", y, mo, d);
        row = cJSON_GetObjectItemCaseSensitive(jget(cfg, "calendar"), key);
        if (!cJSON_IsObject(row)) row = NULL;
    }
    int azan[7];
    bool have;
    if (row) {
        for (int p = 0; p < PRAYER_N; p++) azan[p] = to_min(jstr(row, PRAYER_ID[p], ""));
        azan[5] = to_min(jstr(row, "sunrise", ""));
        azan[6] = to_min(jstr(row, "sunset", ""));
        if (azan[6] < 0) azan[6] = azan[3];
        have = true;
        snprintf(out->source, sizeof(out->source), "calendar");
    } else {
        have = calculate(cfg, y, mo, d, azan);
        if (have) snprintf(out->source, sizeof(out->source), "calculated");
    }
    if (!have) return;

    for (int p = 0; p < PRAYER_N; p++) {
        if (azan[p] >= 0) out->azan[p] = azan[p];
        char key[24];
        snprintf(key, sizeof(key), "%sIqamah", PRAYER_ID[p]);
        int given = row ? to_min(jstr(row, key, "")) : -1;
        char path[40];
        snprintf(path, sizeof(path), "iqamahRules.%s.mode", PRAYER_ID[p]);
        bool fixed = !strcmp(jstr(cfg, path, "offset"), "fixed");
        snprintf(path, sizeof(path), "iqamahRules.%s.time", PRAYER_ID[p]);
        int fixed_at = to_min(jstr(cfg, path, ""));
        snprintf(path, sizeof(path), "iqamahRules.%s.minutes", PRAYER_ID[p]);
        int offset = (int)jnum(cfg, path, 0);      /* the defaults are merged in, so a blank means 0, as in JS */
        if (given >= 0) out->iqamah[p] = given;
        else if (fixed && fixed_at >= 0) out->iqamah[p] = fixed_at;
        else if (out->azan[p] >= 0) out->iqamah[p] = ((out->azan[p] + offset) % 1440 + 1440) % 1440;
    }
    if (azan[5] >= 0) out->sunrise = azan[5];
    if (azan[6] >= 0) out->sunset = azan[6];
}

/* Julian day number of a civil date. */
static long jdn(int y, int m, int d)
{
    long a = (14 - m) / 12, yy = y + 4800 - a, mm = m + 12 * a - 3;
    return d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
}

static void hijri_string(long day, char *out, size_t len)
{
    out[0] = 0;
    if (day < (long)HIJRI_MONTH_START[0]) return;
    int lo = 0, hi = HIJRI_MONTHS_COUNT - 1;
    while (lo < hi) {                       /* the last month that starts on or before the day */
        int mid = (lo + hi + 1) / 2;
        if ((long)HIJRI_MONTH_START[mid] <= day) lo = mid; else hi = mid - 1;
    }
    if (lo == HIJRI_MONTHS_COUNT - 1) return;    /* past the table */
    int index = HIJRI_FIRST_MONTH - 1 + lo;
    int year = HIJRI_FIRST_YEAR + index / 12, month = index % 12;
    snprintf(out, len, "%ld %s %d AH", day - (long)HIJRI_MONTH_START[lo] + 1, HIJRI_MONTHS[month], year);
}

/* Minutes since midnight (local) of an instant on a given day offset; a congregation slot in js/curtain.js. */
static bool in_slot(int now_abs, int day_abs, int start_min, int minutes)
{
    if (start_min < 0 || minutes <= 0) return false;
    int start = day_abs + start_min;
    return now_abs >= start && now_abs < start + minutes;
}

static float dim_level(const cJSON *cfg, const struct tm *now)
{
    static char seen_test[40] = "\x01";
    static time_t test_until;
    float level = 0;
    const char *test = jstr(cfg, "prayerCurtain.test", "");
    if (seen_test[0] == 1) snprintf(seen_test, sizeof(seen_test), "%s", test);     /* the value at start-up is old */
    else if (*test && strcmp(test, seen_test)) {
        snprintf(seen_test, sizeof(seen_test), "%s", test);
        test_until = time(NULL) + 15;
    }

    if (jbool(cfg, "prayerCurtain.enabled", true) || time(NULL) < test_until) {
        bool on = time(NULL) < test_until;
        int now_abs = 1440 + now->tm_hour * 60 + now->tm_min;      /* today, in minutes from yesterday's midnight */
        for (int back = 1; back >= 0 && !on && jbool(cfg, "prayerCurtain.enabled", true); back--) {
            struct tm day = *now;
            day.tm_mday -= back;
            day.tm_hour = 12;
            mktime(&day);
            day_times_t t;
            schedule_resolve(cfg, day.tm_year + 1900, day.tm_mon + 1, day.tm_mday, &t);
            bool friday = day.tm_wday == 5, jumua = t.jumua1 >= 0 || t.jumua2 >= 0;
            int day_abs = (1 - back) * 1440;
            for (int p = 0; p < PRAYER_N && !on; p++) {
                if (friday && p == DHUHR && jumua) continue;
                char path[40];
                snprintf(path, sizeof(path), "prayerCurtain.minutes.%s", PRAYER_ID[p]);
                on = in_slot(now_abs, day_abs, t.iqamah[p], (int)jnum(cfg, path, 0));
            }
            if (friday && !on) {
                int mins = (int)jnum(cfg, "prayerCurtain.minutes.jumua", 0);
                on = in_slot(now_abs, day_abs, t.jumua1, mins) || in_slot(now_abs, day_abs, t.jumua2, mins);
            }
        }
        if (on) {
            double dark = jnum(cfg, "prayerCurtain.darkness", 70);
            if (!(dark > 0)) dark = 70;
            level = fmaxf(level, fminf(99, fmaxf(20, dark)) / 100.0f);
        }
    }
    if (jbool(cfg, "nightDim.enabled", false)) {
        int s = to_min(jstr(cfg, "nightDim.start", "")), e = to_min(jstr(cfg, "nightDim.end", ""));
        int m = now->tm_hour * 60 + now->tm_min;
        if (s >= 0 && e >= 0 && s != e && (s < e ? (m >= s && m < e) : (m >= s || m < e))) {
            double dark = jnum(cfg, "nightDim.darkness", 90);
            if (!(dark > 0)) dark = 90;
            level = fmaxf(level, fminf(99, fmaxf(20, dark)) / 100.0f);
        }
    }
    return level;
}

void schedule_state(const cJSON *cfg, display_state_t *s)
{
    memset(s, 0, sizeof(*s));
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t t = tv.tv_sec;
    localtime_r(&t, &s->now);
    const struct tm *n = &s->now;
    schedule_resolve(cfg, n->tm_year + 1900, n->tm_mon + 1, n->tm_mday, &s->times);
    bool h24 = jbool(cfg, "options.clock24", false);
    int h = n->tm_hour;
    if (h24) snprintf(s->hm, sizeof(s->hm), "%02d:%02d", h, n->tm_min);
    else snprintf(s->hm, sizeof(s->hm), "%d:%02d", (h % 12) ? h % 12 : 12, n->tm_min);
    if (jbool(cfg, "options.seconds", true)) snprintf(s->sec, sizeof(s->sec), ":%02d", n->tm_sec);
    if (!h24) snprintf(s->ap, sizeof(s->ap), "%s", h < 12 ? "AM" : "PM");
    snprintf(s->date_long, sizeof(s->date_long), "%s, %d %s %d", DAYS[n->tm_wday], n->tm_mday, MONTHS[n->tm_mon], n->tm_year + 1900);
    hijri_string(jdn(n->tm_year + 1900, n->tm_mon + 1, n->tm_mday) + (long)jnum(cfg, "options.hijriOffset", 0), s->hijri, sizeof(s->hijri));
    s->friday = n->tm_wday == 5;

    /* The next azan or iqamah (core.js computeState); after Isha, tomorrow's first. */
    double now_min = h * 60 + n->tm_min + n->tm_sec / 60.0;
    int best = -1, best_kind = 0, best_min = 0, first = -1, first_kind = 0, first_min = 1 << 30;
    for (int p = 0; p < PRAYER_N; p++)
        for (int k = 0; k < 2; k++) {
            int m = k ? s->times.iqamah[p] : s->times.azan[p];
            if (m < 0) continue;
            if (m < first_min) { first = p; first_kind = k; first_min = m; }
            if (m > now_min && (best < 0 || m < best_min)) { best = p; best_kind = k; best_min = m; }
        }
    int day_offset = 0;
    if (best < 0 && first >= 0) { best = first; best_kind = first_kind; best_min = first_min; day_offset = 1; }
    s->active = best;
    snprintf(s->countdown, sizeof(s->countdown), "--:--:--");
    if (best >= 0) {
        long left = lround((best_min + day_offset * 1440 - now_min) * 60);
        if (left < 0) left = 0;
        s->next_kind = best_kind;
        snprintf(s->next_name, sizeof(s->next_name), "%s %s", PRAYER_NAME[best], best_kind ? "Iqamah" : "Azan");
        char at[12];
        fmt_min(best_min, h24, at, sizeof(at));
        snprintf(s->next_at, sizeof(s->next_at), "%s at %s", best_kind ? "Iqamah" : "Azan", at);
        snprintf(s->countdown, sizeof(s->countdown), "%02ld:%02ld:%02ld", left / 3600, (left / 60) % 60, left % 60);
    }
    s->dim = dim_level(cfg, n);
}
