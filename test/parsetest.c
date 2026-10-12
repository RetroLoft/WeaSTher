/* host test: parsetest file.json lang [imperial] - prints the parsed weather
   (Atari characters above 127 shown as <xx>) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "weather.h"

static void show(const char *label, const char *s)
{
    printf("  %-10s ", label);
    for (; *s; s++)
        if ((unsigned char)*s >= 0x80) printf("<%02x>", (unsigned char)*s);
        else putchar(*s);
    putchar('\n');
}

int main(int argc, char **argv)
{
    static char buf[200000];
    WEATHER w;
    FILE *f;
    long n;
    int i;

    if (argc < 3 || !(f = fopen(argv[1], "rb"))) {
        fprintf(stderr, "parsetest file.json lang [imperial]\n");
        return 2;
    }
    n = (long)fread(buf, 1, sizeof buf - 1, f);
    buf[n] = 0;
    fclose(f);
    if (!weather_parse(&w, buf, argc > 3, argv[2])) {
        printf("parse failed\n");
        return 1;
    }
    show("area", w.area);
    show("country", w.country);
    printf("  code       %d (icon %d)\n", w.code, weather_icon(w.code));
    show("desc", w.desc);
    show("temp", w.temp);
    show("feels", w.feels);
    show("humidity", w.humidity);
    show("pressure", w.pressure);
    show("precip", w.precip);
    show("wind", w.wind);
    show("winddir", w.winddir);
    show("uv", w.uv);
    for (i = 0; i < w.ndays; i++) {
        printf(" day %d: %s code %d (icon %d)\n", i, w.day[i].date, w.day[i].code,
               weather_icon(w.day[i].code));
        show("desc", w.day[i].desc);
        show("min/max", w.day[i].tmin);
        show("", w.day[i].tmax);
        show("sun", w.day[i].sunrise);
        show("", w.day[i].sunset);
    }
    return 0;
}
