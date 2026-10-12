/*
 * wttr.in j1 JSON -> WEATHER, and the weather code -> icon mapping
 * (the same grouping as wttr.in's own constants.py).
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "json.h"
#include "weather.h"
#include "icons.h"

static void get(const char *root, const char *path, char *out, int max)
{
    if (js_copy(js_get(root, path), out, max) < 0) strcpy(out, "-");
}

/* "lang_nl/0/value" if present, else the English "weatherDesc/0/value" */
static void get_desc(const char *obj, const char *lang, char *out, int max)
{
    char path[32];
    if (*lang && strcmp(lang, "en")) {
        sprintf(path, "lang_%s/0/value", lang);
        if (js_copy(js_get(obj, path), out, max) > 0) return;
    }
    get(obj, "weatherDesc/0/value", out, max);
}

/* "08:00 AM" -> "08:00", "06:53 PM" -> "18:53" */
static void time24(char *t)
{
    int h = atoi(t), m;
    char *c = strchr(t, ':');
    if (!c) return;
    m = atoi(c + 1);
    if (strstr(t, "PM") && h < 12) h += 12;
    if (strstr(t, "AM") && h == 12) h = 0;
    sprintf(t, "%02d:%02d", h, m);
}

int weather_parse(WEATHER *w, const char *json, int imperial, const char *lang)
{
    const char *cur, *d;
    char path[40], tmp[16];
    int i;

    memset(w, 0, sizeof *w);
    cur = js_get(json, "current_condition/0");
    if (!cur) return 0;

    get(cur, "weatherCode", tmp, sizeof tmp);
    w->code = atoi(tmp);
    get_desc(cur, lang, w->desc, sizeof w->desc);
    get(cur, imperial ? "temp_F" : "temp_C", w->temp, sizeof w->temp);
    get(cur, imperial ? "FeelsLikeF" : "FeelsLikeC", w->feels, sizeof w->feels);
    get(cur, "humidity", w->humidity, sizeof w->humidity);
    get(cur, "uvIndex", w->uv, sizeof w->uv);
    get(cur, imperial ? "pressureInches" : "pressure", w->pressure, sizeof w->pressure);
    get(cur, imperial ? "precipInches" : "precipMM", w->precip, sizeof w->precip);
    get(cur, "winddir16Point", w->winddir, sizeof w->winddir);
    get(cur, imperial ? "windspeedMiles" : "windspeedKmph", w->wind, sizeof w->wind);

    get(json, "nearest_area/0/areaName/0/value", w->area, sizeof w->area);
    get(json, "nearest_area/0/country/0/value", w->country, sizeof w->country);

    for (i = 0; i < NDAYS; i++) {
        DAY *day = &w->day[i];
        sprintf(path, "weather/%d", i);
        d = js_get(json, path);
        if (!d) break;
        get(d, "date", day->date, sizeof day->date);
        get(d, imperial ? "mintempF" : "mintempC", day->tmin, sizeof day->tmin);
        get(d, imperial ? "maxtempF" : "maxtempC", day->tmax, sizeof day->tmax);
        get(d, "astronomy/0/sunrise", day->sunrise, sizeof day->sunrise);
        get(d, "astronomy/0/sunset", day->sunset, sizeof day->sunset);
        time24(day->sunrise);
        time24(day->sunset);
        d = js_get(d, "hourly/4");          /* 12:00 */
        if (d) {
            get(d, "weatherCode", tmp, sizeof tmp);
            day->code = atoi(tmp);
            get_desc(d, lang, day->desc, sizeof day->desc);
        }
        w->ndays = i + 1;
    }
    return 1;
}

int weather_icon(int code)
{
    switch (code) {
    case 113: return IC_SUNNY;
    case 116: return IC_PARTLY_CLOUDY;
    case 119: return IC_CLOUDY;
    case 122: return IC_VERY_CLOUDY;
    case 143: case 248: case 260: return IC_FOG;
    case 176: case 263: case 353: return IC_LIGHT_SHOWERS;
    case 179: case 362: case 365: case 374: return IC_LIGHT_SLEET_SHOWERS;
    case 182: case 185: case 281: case 284: case 311: case 314: case 317:
    case 350: case 377: return IC_LIGHT_SLEET;
    case 200: case 386: return IC_THUNDERY_SHOWERS;
    case 227: case 320: return IC_LIGHT_SNOW;
    case 230: case 329: case 332: case 338: return IC_HEAVY_SNOW;
    case 266: case 293: case 296: return IC_LIGHT_RAIN;
    case 299: case 305: case 356: return IC_HEAVY_SHOWERS;
    case 302: case 308: case 359: return IC_HEAVY_RAIN;
    case 323: case 326: case 368: return IC_LIGHT_SNOW_SHOWERS;
    case 335: case 371: case 395: return IC_HEAVY_SNOW_SHOWERS;
    case 389: return IC_THUNDERY_HEAVY_RAIN;
    case 392: return IC_THUNDERY_SNOW_SHOWERS;
    }
    return IC_UNKNOWN;
}
