#pragma once
#include <stdbool.h>
#include <time.h>

/* The weather for the place in the settings (src/weather.c). */
typedef struct {
    bool valid;
    time_t fetched;
    double lat, lng;
    double temp, feels, wind, humidity, high, low;     /* C, km/h, % */
    int code;                                          /* WMO weather code */
    int hours, hour[4], hour_code[4];                  /* the next hours (local hour of day) */
    double hour_temp[4];
} weather_t;

void weather_start(void);
void weather_wake(void);                 /* fetch now (the place changed) */
void weather_get(weather_t *out);
int weather_kind(int code);              /* icon: 0 clear, 1 partly cloudy, 2 cloudy/fog, 3 rain, 4 snow */
const char *weather_text(int code);
