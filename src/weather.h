/* weather.c: wttr.in j1 JSON -> WEATHER */
#ifndef WEATHER_H
#define WEATHER_H

#define NDAYS 3

typedef struct {
    char date[12];              /* "2026-10-12" */
    int code;                   /* WWO weather code at 12:00 */
    char desc[48];
    char tmin[8], tmax[8];
    char sunrise[12], sunset[12]; /* 24 h, "08:00" */
} DAY;

typedef struct {
    char area[40], country[40]; /* nearest_area */
    int code;                   /* WWO weather code */
    char desc[48];
    char temp[8], feels[8];
    char humidity[8], uv[8];
    char pressure[8], precip[8];
    char winddir[8], wind[8];
    DAY day[NDAYS];
    int ndays;
} WEATHER;

/* returns 1 if the answer had the current weather; imperial picks the
   F/mph/inch fields, lang is the wttr.in language code */
int weather_parse(WEATHER *w, const char *json, int imperial, const char *lang);

/* WWO weather code -> icon number (icons.h) */
int weather_icon(int code);

#endif
