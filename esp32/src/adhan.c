/* A line-by-line port of adhan.js 4.x (vendor/adhan.js in the display app), without the polar-circle options
 * the display does not use. Keep it in step with that file: test/adhan_test.c compares the two. */
#include <math.h>
#include <string.h>
#include "adhan.h"

#define DEG(x) ((x) * 180.0 / M_PI)
#define RAD(x) ((x) * M_PI / 180.0)

static double unwind(double v, double m) { return v - m * floor(v / m); }
static double unwind360(double v) { return unwind(v, 360); }
static double closest_angle(double a) { return a >= -180 && a <= 180 ? a : a - 360 * round(a / 360); }
static bool leap(int y) { return !(y % 4 != 0 || (y % 100 == 0 && y % 400 != 0)); }

static double mean_solar_longitude(double t) { return unwind360(280.4664567 + 36000.76983 * t + 0.0003032 * t * t); }
static double mean_lunar_longitude(double t) { return unwind360(218.3165 + 481267.8813 * t); }
static double ascending_node(double t) { return unwind360(125.04452 - 1934.136261 * t + 0.0020708 * t * t + t * t * t / 450000); }
static double mean_solar_anomaly(double t) { return unwind360(357.52911 + 35999.05029 * t - 0.0001537 * t * t); }
static double equation_of_center(double t, double m)
{
    double r = RAD(m);
    return (1.914602 - 0.004817 * t - 0.000014 * t * t) * sin(r) + (0.019993 - 0.000101 * t) * sin(2 * r) + 0.000289 * sin(3 * r);
}
static double apparent_solar_longitude(double t, double l0)
{
    double l = l0 + equation_of_center(t, mean_solar_anomaly(t));
    double o = 125.04 - 1934.136 * t;
    return unwind360(l - 0.00569 - 0.00478 * sin(RAD(o)));
}
static double mean_obliquity(double t) { return 23.439291 - 0.013004167 * t - 0.0000001639 * t * t + 0.0000005036 * t * t * t; }
static double apparent_obliquity(double t, double e0) { return e0 + 0.00256 * cos(RAD(125.04 - 1934.136 * t)); }
static double mean_sidereal_time(double t)
{
    double jd = t * 36525 + 2451545;
    return unwind360(280.46061837 + 360.98564736629 * (jd - 2451545) + 0.000387933 * t * t - t * t * t / 38710000);
}
static double nutation_longitude(double l0, double lp, double om)
{
    return (-17.2 / 3600) * sin(RAD(om)) - (1.32 / 3600) * sin(2 * RAD(l0)) - (0.23 / 3600) * sin(2 * RAD(lp)) + (0.21 / 3600) * sin(2 * RAD(om));
}
static double nutation_obliquity(double l0, double lp, double om)
{
    return (9.2 / 3600) * cos(RAD(om)) + (0.57 / 3600) * cos(2 * RAD(l0)) + (0.1 / 3600) * cos(2 * RAD(lp)) - (0.09 / 3600) * cos(2 * RAD(om));
}
static double altitude(double phi, double delta, double h)
{
    return DEG(asin(sin(RAD(phi)) * sin(RAD(delta)) + cos(RAD(phi)) * cos(RAD(delta)) * cos(RAD(h))));
}
static double approximate_transit(double l, double theta0, double alpha2)
{
    double lw = -l;
    double a = unwind((alpha2 + lw - theta0) / 360, 1);
    double b = unwind((12 - l / 15) / 24, 1);
    return a - b > 0.5 ? a - 1 : b - a > 0.5 ? a + 1 : a;
}
static double interpolate(double y2, double y1, double y3, double n)
{
    double a = y2 - y1, b = y3 - y2, c = b - a;
    return y2 + (n / 2) * (a + b + n * c);
}
static double interpolate_angles(double y2, double y1, double y3, double n)
{
    double a = unwind360(y2 - y1), b = unwind360(y3 - y2), c = b - a;
    return y2 + (n / 2) * (a + b + n * c);
}
static double corrected_transit(double m0, double l, double theta0, double a2, double a1, double a3)
{
    double lw = -l;
    double th = unwind360(theta0 + 360.985647 * m0);
    double a = unwind360(interpolate_angles(a2, a1, a3, m0));
    double h = closest_angle(th - lw - a);
    return (m0 + h / -360) * 24;
}
static double corrected_hour_angle(double m0, double h0, double lat, double lng, bool after, double theta0,
                                   double a2, double a1, double a3, double d2, double d1, double d3)
{
    double lw = -lng;
    double term1 = sin(RAD(h0)) - sin(RAD(lat)) * sin(RAD(d2));
    double term2 = cos(RAD(lat)) * cos(RAD(d2));
    double H0 = DEG(acos(term1 / term2));
    double m = after ? m0 + H0 / 360 : m0 - H0 / 360;
    double th = unwind360(theta0 + 360.985647 * m);
    double a = unwind360(interpolate_angles(a2, a1, a3, m));
    double d = interpolate(d2, d1, d3, m);
    double H = th - lw - a;
    double h = altitude(lat, d, H);
    double dm = (h - h0) / (360 * cos(RAD(d)) * cos(RAD(lat)) * sin(RAD(H)));
    return (m + dm) * 24;
}
static double julian_day(int year, int month, int day)
{
    int y = month > 2 ? year : year - 1;
    int m = month > 2 ? month : month + 12;
    int a = (int)trunc(y / 100.0);
    int b = (int)trunc(2 - a + trunc(a / 4.0));
    int c = (int)trunc(365.25 * (y + 4716));
    int e = (int)trunc(30.6001 * (m + 1));
    return c + e + day + b - 1524.5;
}

typedef struct {
    double declination, right_ascension, apparent_sidereal_time;
} solar_coord_t;

static void solar_coordinates(double jd, solar_coord_t *s)
{
    double t = (jd - 2451545) / 36525;
    double l0 = mean_solar_longitude(t), lp = mean_lunar_longitude(t), om = ascending_node(t);
    double lambda = RAD(apparent_solar_longitude(t, l0));
    double theta0 = mean_sidereal_time(t);
    double dpsi = nutation_longitude(l0, lp, om), deps = nutation_obliquity(l0, lp, om);
    double e0 = mean_obliquity(t), eapp = RAD(apparent_obliquity(t, e0));
    s->declination = DEG(asin(sin(eapp) * sin(lambda)));
    s->right_ascension = unwind360(DEG(atan2(cos(eapp) * sin(lambda), cos(lambda))));
    s->apparent_sidereal_time = theta0 + (dpsi * 3600 * cos(RAD(e0 + deps))) / 3600;
}

typedef struct {
    double lat, lng;
    solar_coord_t solar, prev, next;
    double approx_transit, transit, sunrise, sunset;
} solar_time_t;

static double st_hour_angle(const solar_time_t *s, double angle, bool after)
{
    return corrected_hour_angle(s->approx_transit, angle, s->lat, s->lng, after, s->solar.apparent_sidereal_time,
                                s->solar.right_ascension, s->prev.right_ascension, s->next.right_ascension,
                                s->solar.declination, s->prev.declination, s->next.declination);
}

static void solar_time(int y, int m, int d, double lat, double lng, solar_time_t *s)
{
    double jd = julian_day(y, m, d);
    s->lat = lat;
    s->lng = lng;
    solar_coordinates(jd, &s->solar);
    solar_coordinates(jd - 1, &s->prev);
    solar_coordinates(jd + 1, &s->next);
    s->approx_transit = approximate_transit(lng, s->solar.apparent_sidereal_time, s->solar.right_ascension);
    s->transit = corrected_transit(s->approx_transit, lng, s->solar.apparent_sidereal_time, s->solar.right_ascension,
                                   s->prev.right_ascension, s->next.right_ascension);
    s->sunrise = st_hour_angle(s, -50.0 / 60, false);
    s->sunset = st_hour_angle(s, -50.0 / 60, true);
}

static double st_afternoon(const solar_time_t *s, double shadow)
{
    double tangent = fabs(s->lat - s->solar.declination);
    double inverse = shadow + tan(RAD(tangent));
    return st_hour_angle(s, DEG(atan(1 / inverse)), true);
}

/* Days since 1 Jan 1970 for a civil date (any month/day overflow is allowed, like Date.UTC). */
static long days_from_civil(int y, int m, int d)
{
    y += (m - 1) / 12;
    m = (m - 1) % 12 + 1;
    if (m <= 0) { m += 12; y--; }
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468 + d;
}

/* adhan's TimeComponents(hours).utcDate(y, m, d): NAN in gives NAN out. */
static double utc_date(double hours, int y, int m, int d)
{
    if (!isfinite(hours)) return NAN;
    double h = floor(hours);
    double mi = floor((hours - h) * 60);
    double s = floor((hours - (h + mi / 60)) * 3600);
    return (double)days_from_civil(y, m, d) * 86400 + h * 3600 + mi * 60 + s;
}

static int day_of_year(int y, int m, int d)
{
    static const int len[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int n = 0;
    for (int i = 0; i < m - 1; i++) n += len[i] + (i == 1 && leap(y));
    return n + d;
}

static int days_since_solstice(int doy, int year, double lat)
{
    int north = leap(year) ? 173 : 172, days = leap(year) ? 366 : 365, s;
    if (lat >= 0) { s = doy + 10; if (s >= days) s -= days; }
    else { s = doy - north; if (s < 0) s += days; }
    return s;
}

static double season_blend(int d, double a, double b, double c, double e)
{
    return d < 91 ? a + (b - a) / 91 * d
         : d < 137 ? b + (c - b) / 46 * (d - 91)
         : d < 183 ? c + (e - c) / 46 * (d - 137)
         : d < 229 ? e + (c - e) / 46 * (d - 183)
         : d < 275 ? c + (b - c) / 46 * (d - 229)
         : b + (a - b) / 91 * (d - 275);
}

static double season_morning(double lat, int doy, int year, double sunrise)
{
    double a = 75 + 28.65 / 55 * fabs(lat), b = 75 + 19.44 / 55 * fabs(lat), c = 75 + 32.74 / 55 * fabs(lat), e = 75 + 48.1 / 55 * fabs(lat);
    return sunrise + round(season_blend(days_since_solstice(doy, year, lat), a, b, c, e) * -60);
}

static double season_evening(double lat, int doy, int year, double sunset)
{
    /* Shafaq "general" (adhan's default). */
    double a = 75 + 25.6 / 55 * fabs(lat), b = 75 + 2.05 / 55 * fabs(lat), c = 75 - 9.21 / 55 * fabs(lat), e = 75 + 6.14 / 55 * fabs(lat);
    return sunset + round(season_blend(days_since_solstice(doy, year, lat), a, b, c, e) * 60);
}

typedef struct {
    const char *name;
    double fajr, isha, maghrib;
    int isha_interval;
    int adj_fajr, adj_sunrise, adj_dhuhr, adj_asr, adj_maghrib, adj_isha;
    bool round_up;
} method_t;

static const method_t METHODS[] = {
    {"NorthAmerica", 15, 15, 0, 0, 0, 0, 1, 0, 0, 0, false},
    {"MuslimWorldLeague", 18, 17, 0, 0, 0, 0, 1, 0, 0, 0, false},
    {"Egyptian", 19.5, 17.5, 0, 0, 0, 0, 1, 0, 0, 0, false},
    {"Karachi", 18, 18, 0, 0, 0, 0, 1, 0, 0, 0, false},
    {"UmmAlQura", 18.5, 0, 0, 90, 0, 0, 0, 0, 0, 0, false},
    {"Dubai", 18.2, 18.2, 0, 0, 0, -3, 3, 3, 3, 0, false},
    {"MoonsightingCommittee", 18, 18, 0, 0, 0, 0, 5, 0, 3, 0, false},
    {"Kuwait", 18, 17.5, 0, 0, 0, 0, 0, 0, 0, 0, false},
    {"Qatar", 18, 0, 0, 90, 0, 0, 0, 0, 0, 0, false},
    {"Singapore", 20, 18, 0, 0, 0, 0, 1, 0, 0, 0, true},
    {"Tehran", 17.7, 14, 4.5, 0, 0, 0, 0, 0, 0, 0, false},
    {"Turkey", 18, 17, 0, 0, 0, -7, 5, 4, 7, 0, false},
};

/* Rounds to the minute like adhan: nearest (30 s and up goes up), or up for Singapore. */
static time_t to_minute(double t, bool up)
{
    if (!isfinite(t)) return (time_t)-1;
    long long s = (long long)floor(t);
    int sec = (int)(((s % 60) + 60) % 60);
    long long adj = up ? 60 - sec : (sec >= 30 ? 60 - sec : -sec);
    return (time_t)(s + adj);
}

bool adhan_calculate(double lat, double lng, int y, int mo, int d, const char *method_name, bool hanafi,
                     adhan_highlat_t highlat, adhan_times_t *out)
{
    const method_t *p = &METHODS[0];
    for (size_t i = 0; method_name && i < sizeof(METHODS) / sizeof(METHODS[0]); i++)
        if (!strcmp(METHODS[i].name, method_name)) p = &METHODS[i];
    bool moonsighting = !strcmp(p->name, "MoonsightingCommittee");
    if (highlat == HL_AUTO) highlat = lat > 48 ? HL_SEVENTH : HL_MIDDLE;

    solar_time_t st, tomorrow;
    solar_time(y, mo, d, lat, lng, &st);
    long next_day = days_from_civil(y, mo, d) + 1;
    /* The next civil date: back from the day count. */
    time_t nd = (time_t)next_day * 86400;
    struct tm tmn;
    gmtime_r(&nd, &tmn);
    int ty = tmn.tm_year + 1900, tm_ = tmn.tm_mon + 1, td = tmn.tm_mday;
    solar_time(ty, tm_, td, lat, lng, &tomorrow);

    double dhuhr = utc_date(st.transit, y, mo, d);
    double sunrise = utc_date(st.sunrise, y, mo, d);
    double sunset = utc_date(st.sunset, y, mo, d);
    double asr = utc_date(st_afternoon(&st, hanafi ? 2 : 1), y, mo, d);
    double tomorrow_sunrise = utc_date(tomorrow.sunrise, ty, tm_, td);
    double night = tomorrow_sunrise - sunset;

    double fajr = utc_date(st_hour_angle(&st, -p->fajr, false), y, mo, d);
    if (moonsighting && lat >= 55) fajr = sunrise - night / 7;
    double safe_fajr;
    if (moonsighting) safe_fajr = season_morning(lat, day_of_year(y, mo, d), y, sunrise);
    else {
        double portion = highlat == HL_MIDDLE ? 0.5 : highlat == HL_SEVENTH ? 1.0 / 7 : p->fajr / 60;
        safe_fajr = sunrise - portion * night;
    }
    if (isnan(fajr) || safe_fajr > fajr) fajr = safe_fajr;

    double isha;
    if (p->isha_interval > 0) {
        isha = sunset + p->isha_interval * 60;
    } else {
        isha = utc_date(st_hour_angle(&st, -p->isha, true), y, mo, d);
        if (moonsighting && lat >= 55) isha = sunset + night / 7;
        double safe_isha;
        if (moonsighting) safe_isha = season_evening(lat, day_of_year(y, mo, d), y, sunset);
        else {
            double portion = highlat == HL_MIDDLE ? 0.5 : highlat == HL_SEVENTH ? 1.0 / 7 : p->isha / 60;
            safe_isha = sunset + portion * night;
        }
        if (isnan(isha) || safe_isha < isha) isha = safe_isha;
    }

    double maghrib = sunset;
    if (p->maghrib > 0) {
        double angle = utc_date(st_hour_angle(&st, -p->maghrib, true), y, mo, d);
        if (sunset < angle && isha > angle) maghrib = angle;
    }

    bool up = p->round_up;
    out->fajr = to_minute(fajr + p->adj_fajr * 60, up);
    out->sunrise = to_minute(sunrise + p->adj_sunrise * 60, up);
    out->dhuhr = to_minute(dhuhr + p->adj_dhuhr * 60, up);
    out->asr = to_minute(asr + p->adj_asr * 60, up);
    out->sunset = to_minute(sunset, up);
    out->maghrib = to_minute(maghrib + p->adj_maghrib * 60, up);
    out->isha = to_minute(isha + p->adj_isha * 60, up);
    return out->dhuhr != (time_t)-1;
}
