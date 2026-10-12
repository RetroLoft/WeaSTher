/*
 * Settings in WEASTHER.CFG, a plain "key=value" text file in the folder
 * the program was started from (works on any drive, also TNFS).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <osbind.h>
#include "config.h"

CONFIG cfg;
static char cfg_path[160];

/* language from the TOS country: _AKP cookie, else the OS header */
static long cookie_akp = -1;
static long find_akp(void)
{
    long *jar = *(long **)0x5a0L;
    if (jar)
        for (; jar[0]; jar += 2)
            if (jar[0] == 0x5f414b50L) {        /* '_AKP' */
                cookie_akp = jar[1] >> 8 & 0xff;
                return 0;
            }
    /* os_conf >> 1 in the ROM header (_sysbase -> os_beg) */
    cookie_akp = (*(unsigned short **)0x4f2L)[0x1c / 2] >> 1;
    return 0;
}

static const char *tos_language(void)
{
    Supexec(find_akp);
    switch ((int)cookie_akp) {
    case 1: case 8: return "de";
    case 2: case 7: return "fr";
    case 4: return "es";
    case 5: return "it";
    case 6: return "sv";
    case 10: return "fi";
    case 11: return "nb";
    case 12: return "da";
    case 14: return "nl";
    }
    return "en";
}

void config_init(void)
{
    int drv = Dgetdrv();
    char *p;

    memset(&cfg, 0, sizeof cfg);
    strcpy(cfg.lang, tos_language());
    cfg.refresh = 30;
    strcpy(cfg.host, "wttr.in");
    cfg.win_x = cfg.win_y = -1;

    cfg_path[0] = (char)('A' + drv);
    cfg_path[1] = ':';
    if (Dgetpath(cfg_path + 2, drv + 1) < 0) cfg_path[2] = 0;
    p = cfg_path + strlen(cfg_path);
    if (p[-1] != '\\') *p++ = '\\';
    strcpy(p, "WEASTHER.CFG");
}

static void copy(char *dst, const char *src, int max)
{
    strncpy(dst, src, max - 1);
    dst[max - 1] = 0;
}

int config_load(void)
{
    char line[128];
    FILE *f = fopen(cfg_path, "r");

    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        char *v = strchr(line, '='), *e;
        if (line[0] == '#' || !v) continue;
        *v++ = 0;
        for (e = v + strlen(v); e > v && (e[-1] == '\n' || e[-1] == '\r'); ) *--e = 0;
        if (!strcmp(line, "location")) copy(cfg.location, v, sizeof cfg.location);
        else if (!strcmp(line, "lang")) copy(cfg.lang, v, sizeof cfg.lang);
        else if (!strcmp(line, "units")) cfg.imperial = *v == 'i';
        else if (!strcmp(line, "refresh")) cfg.refresh = atoi(v);
        else if (!strcmp(line, "host") && *v) copy(cfg.host, v, sizeof cfg.host);
        else if (!strcmp(line, "window") && strchr(v, ',')) {
            cfg.win_x = atoi(v);
            cfg.win_y = atoi(strchr(v, ',') + 1);
        }
    }
    fclose(f);
    return 1;
}

int config_save(void)
{
    FILE *f = fopen(cfg_path, "w");

    if (!f) return 0;
    fprintf(f, "# WeaSTher settings\n");
    fprintf(f, "location=%s\n", cfg.location);
    fprintf(f, "lang=%s\n", cfg.lang);
    fprintf(f, "units=%s\n", cfg.imperial ? "imperial" : "metric");
    fprintf(f, "refresh=%d\n", cfg.refresh);
    fprintf(f, "host=%s\n", cfg.host);
    fprintf(f, "window=%d,%d\n", cfg.win_x, cfg.win_y);
    fclose(f);
    return 1;
}
