/*
 * WeaSTher - a weather gadget for the Atari ST
 *
 * Shows the weather from wttr.in (via STinG) in a small GEM window, in ST
 * low, medium and high resolution. Close box, Esc or Q quits.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <osbind.h>
#include <gem.h>
#include "net.h"
#include "json.h"
#include "weather.h"
#include "icons.h"
#include "config.h"
#include "texts.h"

#define WKIND   (NAME | CLOSER | MOVER)

static short vh, win = -1;
static short cw, ch;                    /* character cell */
static short big_cw, big_ch;            /* big font cell (temperature) */
static short font_h, big_font_h;        /* vst_height values */
static short pix_w, pix_h;              /* pixel size in microns */
static int ncolors;
static short scr_w, scr_h;
static GRECT desk, work;

static const TEXTS *T;
static WEATHER wd;
static int have_data, busy, failed;
static char status[48], errtext[48];
static long last_fetch;
static int pos_dirty;

/* layout, relative to the work area */
static short W, H, P;
static short big_w, big_h, small_w, small_h, text_x, text_cols;
static short y_title, y_cur, cur_h, y_sep1, y_fc, fc_h, y_sep2, y_bot;
static short col_w, desc_lines, sun_line;
static GRECT b_refresh, b_options;      /* buttons, relative */

/* ---------------------------------------------------------------- utils */

static long get_ticks200(void) { return *(volatile long *)0x4baL; }
static long ticks(void) { return Supexec(get_ticks200); }

static short aspect(short w) { return (short)((long)w * pix_w / pix_h); }

static void set_font(int big)
{
    short a, b, c, d;
    vst_height(vh, big ? big_font_h : font_h, &a, &b, &c, &d);
}

static short text_width(const char *s)
{
    short ext[8];
    vqt_extent(vh, s, ext);
    return ext[2] - ext[0];
}

/* copy at most n characters */
static char *clip(char *dst, const char *src, int n)
{
    int len = (int)strlen(src);
    if (n < 0) n = 0;
    if (len > n) len = n;
    memmove(dst, src, len);
    dst[len] = 0;
    return dst;
}

static void text(short x, short y, const char *s, int effects)
{
    vst_effects(vh, effects);
    v_gtext(vh, work.g_x + x, work.g_y + y, s);
    vst_effects(vh, 0);
}

static void text_center(short x, short w, short y, const char *s, int effects)
{
    short tw;
    vst_effects(vh, effects);
    tw = text_width(s);
    v_gtext(vh, work.g_x + x + (w - tw) / 2, work.g_y + y, s);
    vst_effects(vh, 0);
}

static void hline(short y)
{
    short p[4];
    vsl_color(vh, ncolors >= 16 ? 9 : 1);
    vsl_width(vh, 1);
    p[0] = work.g_x + P / 2; p[2] = work.g_x + W - P / 2 - 1;
    p[1] = p[3] = work.g_y + y;
    v_pline(vh, 2, p);
}

/* n-th line (0-based) of s word-wrapped at cols; the last line is cut */
static void wrap_line(const char *s, int cols, int n, int last, char *out)
{
    int line = 0;
    for (;;) {
        int len = (int)strlen(s), cut = len;
        if (len > cols) {
            for (cut = cols; cut > 0 && s[cut] != ' '; cut--) ;
            if (cut == 0) cut = cols;
        }
        if (line == n) {
            clip(out, s, line == last ? (len < cols ? len : cols) : cut);
            return;
        }
        s += cut;
        while (*s == ' ') s++;
        if (!*s) { *out = 0; return; }
        line++;
    }
}

/* 0 = Sunday, for "YYYY-MM-DD" */
static int weekday(const char *date)
{
    static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    int y = atoi(date), m = atoi(date + 5), d = atoi(date + 8);
    if (m < 1 || m > 12) return 0;
    if (m < 3) y--;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

/* "SSW" -> "ZZW" in Dutch etc. */
static void compass(const char *dir, char *out)
{
    for (; *dir; dir++)
        *out++ = *dir == 'N' ? T->compass[0] : *dir == 'E' ? T->compass[1] :
                 *dir == 'S' ? T->compass[2] : *dir == 'W' ? T->compass[3] : *dir;
    *out = 0;
}

static const char *deg(void) { return cfg.imperial ? "\xf8" "F" : "\xf8" "C"; }

/* --------------------------------------------------------------- layout */

static void layout(void)
{
    short fc_room, max_h;
    GRECT b;
    int n;

    P = cw;
    big_w = scr_w / 6;
    big_h = aspect(big_w);
    small_w = big_w * 2 / 3;
    small_h = aspect(small_w);
    text_x = P + big_w + P;
    text_cols = 27;
    W = text_x + text_cols * cw + P;
    col_w = (W - 2 * P) / 3;
    desc_lines = ch >= 16 ? 2 : 1;
    sun_line = 1;

    /* drop the less important lines until the window fits on the screen */
    for (n = 0; n < 3; n++) {
        y_title = P / 2;
        y_cur = y_title + ch + P / 2;
        cur_h = big_ch + (5 + sun_line) * ch;
        if (cur_h < big_h) cur_h = big_h;
        y_sep1 = y_cur + cur_h + P / 2;
        y_fc = y_sep1 + P / 2;
        fc_room = ch + 2 + small_h + 2 + ch + desc_lines * ch;
        fc_h = fc_room;
        y_sep2 = y_fc + fc_h + P / 2;
        y_bot = y_sep2 + P / 2;
        H = y_bot + ch + 4 + P / 2;
        wind_calc(WC_BORDER, WKIND, 0, 0, W, H, &b.g_x, &b.g_y, &b.g_w, &b.g_h);
        max_h = desk.g_h;
        if (b.g_h <= max_h) break;
        if (desc_lines) desc_lines--;
        else sun_line = 0;
    }
}

/* the two buttons, right aligned in the bottom row */
static void layout_buttons(void)
{
    short wo = ((short)strlen(T->options) + 2) * cw;
    short wr = ((short)strlen(T->refresh) + 2) * cw;
    b_options.g_x = W - P - wo;
    b_options.g_w = wo;
    b_refresh.g_x = b_options.g_x - cw - wr;
    b_refresh.g_w = wr;
    b_options.g_y = b_refresh.g_y = y_bot;
    b_options.g_h = b_refresh.g_h = ch + 4;
}

/* -------------------------------------------------------------- drawing */

static void button(const GRECT *r, const char *label)
{
    short p[10];
    p[0] = p[6] = p[8] = work.g_x + r->g_x;
    p[1] = p[3] = p[9] = work.g_y + r->g_y;
    p[2] = p[4] = work.g_x + r->g_x + r->g_w - 1;
    p[5] = p[7] = work.g_y + r->g_y + r->g_h - 1;
    vsl_color(vh, 1);
    v_pline(vh, 5, p);
    text_center(r->g_x, r->g_w, r->g_y + 2, label, 0);
}

static void draw_current(void)
{
    char buf[64], dir[8];
    short y = y_cur;
    int tcol = 1, t;

    icon_draw(have_data ? weather_icon(wd.code) : IC_UNKNOWN,
              work.g_x + P, work.g_y + y_cur + (cur_h - big_h) / 2, big_w, big_h, 1);

    if (!have_data) {
        set_font(1);
        text(text_x, y, busy ? "..." : failed ? T->failed : "", 1);
        set_font(0);
        text(text_x, y + big_ch, clip(buf, busy ? T->fetching : errtext, text_cols), 0);
        return;
    }

    /* the temperature, red when warm and blue when freezing */
    t = atoi(wd.temp);
    if (cfg.imperial) t = (t - 32) * 5 / 9;
    if (t >= 25 && ncolors >= 4) tcol = 2;
    if (t <= 0 && ncolors >= 16) tcol = 4;
    set_font(1);
    vst_color(vh, tcol);
    sprintf(buf, "%s%s", wd.temp, deg());
    text(text_x, y, buf, 1);
    vst_color(vh, 1);
    set_font(0);
    y += big_ch;

    text(text_x, y, clip(buf, wd.desc, text_cols), 0);
    y += ch;
    sprintf(buf, "%s %s%s", T->feels, wd.feels, deg());
    text(text_x, y, clip(buf, buf, text_cols), 0);
    y += ch;
    compass(wd.winddir, dir);
    sprintf(buf, "%s %s %s %s", T->wind, dir, wd.wind, cfg.imperial ? "mph" : "km/h");
    text(text_x, y, clip(buf, buf, text_cols), 0);
    y += ch;
    sprintf(buf, "%s %s%%  %s %s", T->humidity, wd.humidity, wd.pressure,
            cfg.imperial ? "inHg" : "hPa");
    text(text_x, y, clip(buf, buf, text_cols), 0);
    y += ch;
    sprintf(buf, "%s %s %s  UV %s", T->precip, wd.precip, cfg.imperial ? "in" : "mm", wd.uv);
    text(text_x, y, clip(buf, buf, text_cols), 0);
    y += ch;
    if (sun_line && wd.ndays > 0) {
        sprintf(buf, "%s %s - %s", T->sun, wd.day[0].sunrise, wd.day[0].sunset);
        text(text_x, y, clip(buf, buf, text_cols), 0);
    }
}

static void draw_forecast(void)
{
    char buf[64];
    int i, l, cols = col_w / cw - 1;

    for (i = 0; i < wd.ndays && i < 3; i++) {
        const DAY *d = &wd.day[i];
        short x = P + i * col_w, y = y_fc;
        const char *name = i == 0 ? T->today : i == 1 ? T->tomorrow :
                           T->weekday[weekday(d->date)];

        text_center(x, col_w, y, clip(buf, name, cols), 1);
        y += ch + 2;
        icon_draw(weather_icon(d->code), work.g_x + x + (col_w - small_w) / 2,
                  work.g_y + y, small_w, small_h, 0);
        y += small_h + 2;
        sprintf(buf, "%s / %s%s", d->tmin, d->tmax, deg());
        text_center(x, col_w, y, clip(buf, buf, cols), 0);
        y += ch;
        for (l = 0; l < desc_lines; l++, y += ch) {
            wrap_line(d->desc, cols, l, desc_lines - 1, buf);
            text_center(x, col_w, y, buf, 0);
        }
    }
}

static void draw_bottom(void)
{
    char buf[48];
    int room = (b_refresh.g_x - P) / cw - 1;

    text(P, y_bot + 2, clip(buf, status, room), 0);
    button(&b_refresh, T->refresh);
    button(&b_options, T->options);
}

static void draw_all(void)
{
    char buf[80];
    short p[4];

    vswr_mode(vh, MD_REPLACE);
    vsf_interior(vh, FIS_SOLID);
    vsf_color(vh, 0);
    p[0] = work.g_x; p[1] = work.g_y;
    p[2] = work.g_x + work.g_w - 1; p[3] = work.g_y + work.g_h - 1;
    vr_recfl(vh, p);

    set_font(0);
    vst_color(vh, 1);
    vst_alignment(vh, 0, 5, p, p + 1);         /* left, top */

    if (cfg.location[0])
        strcpy(buf, cfg.location);
    else if (have_data)
        sprintf(buf, "%s, %s", wd.area, wd.country);
    else
        strcpy(buf, cfg.host);
    vswr_mode(vh, MD_TRANS);
    text(P, y_title, clip(buf, buf, (W - 2 * P) / cw), 1);

    draw_current();
    vswr_mode(vh, MD_TRANS);
    hline(y_sep1);
    if (have_data) draw_forecast();
    vswr_mode(vh, MD_TRANS);
    hline(y_sep2);
    draw_bottom();
}

static int intersect(const GRECT *a, GRECT *b)
{
    short x1 = a->g_x > b->g_x ? a->g_x : b->g_x;
    short y1 = a->g_y > b->g_y ? a->g_y : b->g_y;
    short x2 = a->g_x + a->g_w < b->g_x + b->g_w ? a->g_x + a->g_w : b->g_x + b->g_w;
    short y2 = a->g_y + a->g_h < b->g_y + b->g_h ? a->g_y + a->g_h : b->g_y + b->g_h;
    b->g_x = x1; b->g_y = y1; b->g_w = x2 - x1; b->g_h = y2 - y1;
    return x2 > x1 && y2 > y1;
}

/* redraw the part of the window (screen coordinates) */
static void redraw(GRECT *area)
{
    GRECT r;
    short clipxy[4];

    if (win < 0) return;
    wind_update(BEG_UPDATE);
    graf_mouse(M_OFF, NULL);
    wind_get(win, WF_FIRSTXYWH, &r.g_x, &r.g_y, &r.g_w, &r.g_h);
    while (r.g_w && r.g_h) {
        if (intersect(area, &r) && intersect(&work, &r)) {
            clipxy[0] = r.g_x; clipxy[1] = r.g_y;
            clipxy[2] = r.g_x + r.g_w - 1; clipxy[3] = r.g_y + r.g_h - 1;
            vs_clip(vh, 1, clipxy);
            draw_all();
        }
        wind_get(win, WF_NEXTXYWH, &r.g_x, &r.g_y, &r.g_w, &r.g_h);
    }
    vs_clip(vh, 0, clipxy);
    graf_mouse(M_ON, NULL);
    wind_update(END_UPDATE);
}

static void redraw_all(void) { GRECT r = work; redraw(&r); }

/* ------------------------------------------------------------- fetching */

static void url_encode(const char *s, char *out, int max)
{
    static const char hex[] = "0123456789ABCDEF";
    int n = 0;

    for (; *s && n < max - 13; s++) {
        unsigned char c = (unsigned char)*s;
        long u;
        if (isalnum(c) || strchr("-._~,@:", c)) { out[n++] = c; continue; }
        if (c == ' ') { out[n++] = '+'; continue; }
        u = unicode_from_atari(c);      /* Atari -> UTF-8, percent-encoded */
        if (u < 0x80) {
            out[n++] = '%'; out[n++] = hex[u >> 4]; out[n++] = hex[u & 15];
        } else {
            unsigned char b[3];
            int i, k;
            if (u < 0x800) { b[0] = 0xc0 | u >> 6; b[1] = 0x80 | (u & 0x3f); k = 2; }
            else { b[0] = 0xe0 | u >> 12; b[1] = 0x80 | (u >> 6 & 0x3f); b[2] = 0x80 | (u & 0x3f); k = 3; }
            for (i = 0; i < k; i++) {
                out[n++] = '%'; out[n++] = hex[b[i] >> 4]; out[n++] = hex[b[i] & 15];
            }
        }
    }
    out[n] = 0;
}

static void set_error(const char *msg)
{
    failed = 1;
    clip(errtext, msg, sizeof errtext - 1);
    sprintf(status, "%s: %s", T->failed, errtext);
}

static void start_fetch(void)
{
    char path[200], loc[150];

    url_encode(cfg.location, loc, sizeof loc);
    sprintf(path, "/%s?format=j1&lang=%s", loc, cfg.lang);
    last_fetch = ticks();
    failed = 0;
    busy = 1;
    strcpy(status, T->fetching);
    redraw_all();                       /* before resolve(), which may block */
    if (net_start(cfg.host, path) != NET_BUSY) {
        busy = 0;
        set_error(T->err[net_error_kind]);
        if (net_error_code) sprintf(errtext + strlen(errtext), " (%d)", net_error_code);
        redraw_all();
    }
}

static void poll_fetch(void)
{
    static WEATHER nw;
    int r = net_poll();
    unsigned t;

    if (r == NET_BUSY) return;
    busy = 0;
    if (r == NET_DONE && weather_parse(&nw, net_body, cfg.imperial, cfg.lang)) {
        wd = nw;
        have_data = 1;
        t = Tgettime();
        sprintf(status, "%s %02u:%02u", T->updated, t >> 11, t >> 5 & 63);
        if ((short)strlen(status) * cw > b_refresh.g_x - 2 * P)
            sprintf(status, "%02u:%02u", t >> 11, t >> 5 & 63);
    } else if (r == NET_DONE) {
        set_error(T->bad_answer);
    } else if (net_error_kind == NE_HTTP && net_body && strstr(net_body, "not found")) {
        set_error(T->unknown_place);
    } else {
        set_error(T->err[net_error_kind]);
        if (net_error_code) sprintf(errtext + strlen(errtext), " (%d)", net_error_code);
    }
    redraw_all();
}

/* ------------------------------------------------------------- settings */

enum {
    D_ROOT, D_TITLE, D_LPLACE, D_PLACE, D_HPLACE, D_LLANG, D_LANG, D_HLANG,
    D_LUNITS, D_UNITS, D_METRIC, D_IMPERIAL, D_LREFRESH, D_REFRESH, D_HREFRESH,
    D_OK, D_CANCEL, D_COUNT
};

static OBJECT dlg[D_COUNT];
static TEDINFO ted[3];
static char t_place[33], t_lang[6], t_refresh[4];
static char m_place[33], m_lang[6], m_refresh[4];
static char v_place[33], v_lang[6], v_refresh[4];

static void obj(int i, int next, int head, int tail, int type, int flags,
                int state, long spec, int x, int y, int w, int h)
{
    OBJECT *o = &dlg[i];
    o->ob_next = next; o->ob_head = head; o->ob_tail = tail;
    o->ob_type = type; o->ob_flags = flags; o->ob_state = state;
    o->ob_spec.index = spec;
    o->ob_x = x; o->ob_y = y; o->ob_width = w; o->ob_height = h;
}

static void field(int i, int t, char *txt, char *tmpl, char *valid, int len,
                  char vchar, int next, int x, int y)
{
    memset(tmpl, '_', len); tmpl[len] = 0;
    memset(valid, vchar, len); valid[len] = 0;
    ted[t].te_ptext = txt; ted[t].te_ptmplt = tmpl; ted[t].te_pvalid = valid;
    ted[t].te_font = 3; ted[t].te_just = 0; ted[t].te_color = 0x1180;
    ted[t].te_thickness = 0; ted[t].te_txtlen = len + 1; ted[t].te_tmplen = len + 1;
    obj(i, next, -1, -1, G_FTEXT, OF_EDITABLE, 0, (long)&ted[t], x, y, len, 1);
}

static void trim(char *s)
{
    char *e = s + strlen(s);
    while (e > s && e[-1] == ' ') *--e = 0;
    while (*s == ' ') memmove(s, s + 1, strlen(s));
}

/* returns 1 when OK was clicked (cfg changed and saved) */
static int settings(void)
{
    short x, y, w, h, exit;
    int i, tl = (int)strlen(T->dlg_title);

    clip(t_place, cfg.location, 32);
    clip(t_lang, cfg.lang, 5);
    sprintf(t_refresh, "%d", cfg.refresh > 999 ? 999 : cfg.refresh);

    obj(D_ROOT, -1, D_TITLE, D_CANCEL, G_BOX, 0, OS_OUTLINED, 0x00021100L, 0, 0, 36, 16);
    obj(D_TITLE, D_LPLACE, -1, -1, G_STRING, 0, 0, (long)T->dlg_title, (36 - tl) / 2, 1, tl, 1);
    obj(D_LPLACE, D_PLACE, -1, -1, G_STRING, 0, 0, (long)T->dlg_place, 2, 3, 10, 1);
    field(D_PLACE, 0, t_place, m_place, v_place, 32, 'X', D_HPLACE, 2, 4);
    obj(D_HPLACE, D_LLANG, -1, -1, G_STRING, 0, 0, (long)T->dlg_place_hint, 2, 5, 22, 1);
    obj(D_LLANG, D_LANG, -1, -1, G_STRING, 0, 0, (long)T->dlg_lang, 2, 7, 10, 1);
    field(D_LANG, 1, t_lang, m_lang, v_lang, 5, 'X', D_HLANG, 13, 7);
    obj(D_HLANG, D_LUNITS, -1, -1, G_STRING, 0, 0, (long)T->dlg_lang_hint, 13, 8, 23, 1);
    obj(D_LUNITS, D_UNITS, -1, -1, G_STRING, 0, 0, (long)T->dlg_units, 2, 10, 10, 1);
    obj(D_UNITS, D_LREFRESH, D_METRIC, D_IMPERIAL, G_IBOX, 0, 0, 0L, 13, 10, 21, 1);
    obj(D_METRIC, D_IMPERIAL, -1, -1, G_BUTTON, OF_SELECTABLE | OF_RBUTTON,
        cfg.imperial ? 0 : OS_SELECTED, (long)T->dlg_metric, 0, 0, 10, 1);
    obj(D_IMPERIAL, D_UNITS, -1, -1, G_BUTTON, OF_SELECTABLE | OF_RBUTTON,
        cfg.imperial ? OS_SELECTED : 0, (long)T->dlg_imperial, 11, 0, 10, 1);
    obj(D_LREFRESH, D_REFRESH, -1, -1, G_STRING, 0, 0, (long)T->dlg_refresh, 2, 12, 11, 1);
    field(D_REFRESH, 2, t_refresh, m_refresh, v_refresh, 3, '9', D_HREFRESH, 13, 12);
    obj(D_HREFRESH, D_OK, -1, -1, G_STRING, 0, 0, (long)T->dlg_refresh_hint, 17, 12, 16, 1);
    obj(D_OK, D_CANCEL, -1, -1, G_BUTTON, OF_SELECTABLE | OF_DEFAULT | OF_EXIT, 0,
        (long)T->dlg_ok, 13, 14, 9, 1);
    obj(D_CANCEL, D_ROOT, -1, -1, G_BUTTON, OF_SELECTABLE | OF_EXIT | OF_LASTOB, 0,
        (long)T->dlg_cancel, 24, 14, 10, 1);
    for (i = 0; i < D_COUNT; i++) rsrc_obfix(dlg, i);

    form_center(dlg, &x, &y, &w, &h);
    form_dial(FMD_START, 0, 0, 0, 0, x, y, w, h);
    objc_draw(dlg, D_ROOT, 8, x, y, w, h);
    exit = form_do(dlg, D_PLACE) & 0x7fff;
    form_dial(FMD_FINISH, 0, 0, 0, 0, x, y, w, h);
    if (exit != D_OK) return 0;

    trim(t_place);
    trim(t_lang);
    for (i = 0; t_lang[i]; i++) t_lang[i] = (char)tolower((unsigned char)t_lang[i]);
    strcpy(cfg.location, t_place);
    strcpy(cfg.lang, t_lang[0] ? t_lang : "en");
    cfg.imperial = (dlg[D_IMPERIAL].ob_state & OS_SELECTED) != 0;
    cfg.refresh = atoi(t_refresh);
    config_save();
    T = texts_for(cfg.lang);
    layout_buttons();
    return 1;
}

/* ----------------------------------------------------------------- main */

static void open_window(void)
{
    GRECT b;

    wind_calc(WC_BORDER, WKIND, 0, 0, W, H, &b.g_x, &b.g_y, &b.g_w, &b.g_h);
    if (cfg.win_x >= 0 && cfg.win_y >= desk.g_y &&
        cfg.win_x + b.g_w <= desk.g_x + desk.g_w &&
        cfg.win_y + b.g_h <= desk.g_y + desk.g_h) {
        b.g_x = cfg.win_x;
        b.g_y = cfg.win_y;
    } else {
        b.g_x = desk.g_x + (desk.g_w - b.g_w) / 2;
        b.g_y = desk.g_y + (desk.g_h - b.g_h) / 2;
    }
    win = wind_create(WKIND, desk.g_x, desk.g_y, desk.g_w, desk.g_h);
    if (win < 0) return;
    wind_set_str(win, WF_NAME, " WeaSTher ");
    wind_open(win, b.g_x, b.g_y, b.g_w, b.g_h);
    wind_get(win, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w, &work.g_h);
}

static int in(const GRECT *r, short mx, short my)
{
    mx -= work.g_x; my -= work.g_y;
    return mx >= r->g_x && mx < r->g_x + r->g_w && my >= r->g_y && my < r->g_y + r->g_h;
}

static void do_settings(void)
{
    if (busy) { net_abort(); busy = 0; }
    if (settings()) start_fetch();
}

int main(void)
{
    short work_in[11], work_out[57], msg[8];
    short mx, my, mb, ks, key, clicks, dummy, ev;
    int i, first_run, quit = 0;

    if (appl_init() < 0) return 1;
    graf_mouse(ARROW, NULL);
    vh = graf_handle(&cw, &ch, &dummy, &dummy);
    for (i = 0; i < 10; i++) work_in[i] = 1;
    work_in[10] = 2;
    v_opnvwk(work_in, &vh, work_out);
    if (!vh) { appl_exit(); return 1; }
    scr_w = work_out[0] + 1;
    scr_h = work_out[1] + 1;
    pix_w = work_out[3];
    pix_h = work_out[4];
    ncolors = work_out[13];
    icons_init(vh, ncolors);

    font_h = ch >= 16 ? 13 : 6;
    big_font_h = font_h * 2;
    vst_height(vh, big_font_h, &dummy, &dummy, &big_cw, &big_ch);
    set_font(0);

    wind_get(0, WF_WORKXYWH, &desk.g_x, &desk.g_y, &desk.g_w, &desk.g_h);
    config_init();
    first_run = !config_load();
    T = texts_for(cfg.lang);
    layout();
    layout_buttons();
    open_window();
    if (win < 0) {
        form_alert(1, "[3][WeaSTher:|No window available.][ OK ]");
        v_clsvwk(vh);
        appl_exit();
        return 1;
    }
    if (first_run) settings();
    start_fetch();

    while (!quit) {
        ev = evnt_multi(MU_MESAG | MU_BUTTON | MU_KEYBD | MU_TIMER,
                        1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                        msg, busy ? 20 : 1000, &mx, &my, &mb, &ks, &key, &clicks);

        if (ev & MU_MESAG) {
            switch (msg[0]) {
            case WM_REDRAW:
                if (msg[3] == win) redraw((GRECT *)&msg[4]);
                break;
            case WM_TOPPED:
                if (msg[3] == win) wind_set(win, WF_TOP, 0, 0, 0, 0);
                break;
            case WM_MOVED:
                if (msg[3] == win) {
                    wind_set(win, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
                    wind_get(win, WF_WORKXYWH, &work.g_x, &work.g_y, &work.g_w, &work.g_h);
                    cfg.win_x = msg[4];
                    cfg.win_y = msg[5];
                    pos_dirty = 1;
                }
                break;
            case WM_CLOSED:
            case AP_TERM:
                quit = 1;
                break;
            }
        }
        if (ev & MU_KEYBD) {
            switch (key & 0xff) {
            case 27: case 'q': case 'Q': case 17:      /* Esc, Q, ^Q */
                quit = 1;
                break;
            case 'r': case 'R':
                if (!busy) start_fetch();
                break;
            case 'o': case 'O': case 's': case 'S':
                do_settings();
                break;
            }
        }
        if (ev & MU_BUTTON)                     /* wait for the release */
            evnt_button(1, 1, 0, &dummy, &dummy, &dummy, &dummy);
        if ((ev & MU_BUTTON) && wind_find(mx, my) == win) {
            if (in(&b_refresh, mx, my)) {
                if (!busy) start_fetch();
            } else if (in(&b_options, mx, my)) {
                do_settings();
            }
        }
        if (busy) {
            poll_fetch();
        } else if (cfg.refresh > 0 && ticks() - last_fetch > cfg.refresh * 60L * 200L) {
            start_fetch();
        }
    }

    net_abort();
    if (pos_dirty) config_save();
    wind_close(win);
    wind_delete(win);
    v_clsvwk(vh);
    appl_exit();
    return 0;
}
