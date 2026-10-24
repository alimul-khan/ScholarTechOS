/* Prints calculated times for test/adhan_cases; test/adhan_check.sh compares them with adhan.js. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/adhan.h"

int main(void)
{
    const char *methods[] = {"NorthAmerica", "MuslimWorldLeague", "Egyptian", "Karachi", "UmmAlQura", "Dubai",
                             "MoonsightingCommittee", "Kuwait", "Qatar", "Singapore", "Tehran", "Turkey"};
    double places[][2] = {{50.4452, -104.6189}, {21.4225, 39.8262}, {51.5072, -0.1276}, {-33.8688, 151.2093},
                          {64.1466, -21.9426}, {59.9139, 10.7522}, {-54.8019, -68.303}, {1.3521, 103.8198}};
    for (int mi = 0; mi < 12; mi++)
        for (int pi = 0; pi < 8; pi++)
            for (int hl = 0; hl < 4; hl++)
                for (int day = 0; day < 366; day += 9) {
                    int y = 2026, m = 1, d = 1 + day;
                    static const int len[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                    while (d > len[m - 1]) { d -= len[m - 1]; m++; }
                    for (int hanafi = 0; hanafi < 2; hanafi++) {
                        adhan_times_t t;
                        adhan_calculate(places[pi][0], places[pi][1], y, m, d, methods[mi], hanafi, (adhan_highlat_t)hl, &t);
                        printf("%s %d %d %d-%02d-%02d %d %lld %lld %lld %lld %lld %lld\n", methods[mi], pi, hl, y, m, d, hanafi,
                               (long long)t.fajr, (long long)t.sunrise, (long long)t.dhuhr, (long long)t.asr,
                               (long long)t.maghrib, (long long)t.isha);
                    }
                }
    return 0;
}
