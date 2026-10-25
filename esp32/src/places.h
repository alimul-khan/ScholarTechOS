#pragma once
#include <stddef.h>
typedef struct {
    const char *code;
    const char *name;
} country_t;
extern const country_t COUNTRIES[];
extern const int COUNTRY_COUNT;

const char *country_name(const char *code, size_t len);
/* Answers /api/places?q=&limit= as JSON {"results":[...]}, the same as the Android app. Online. Caller frees. */
char *places_search_json(const char *query, int limit);
