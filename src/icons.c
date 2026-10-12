/*
 * Weather icons drawn with VDI calls, so they are sharp at any size, in
 * ST low (16 colours), medium (4 colours, tall pixels) and high (mono).
 *
 * Coordinates are in a 100 x 100 box scaled to the icon's w x h. Shapes
 * get a black outline by drawing them slightly bigger in black first.
 */
#include <gem.h>
#include "icons.h"

/* colour roles */
enum { R_BLACK, R_WHITE, R_SUN, R_DARK, R_RAIN, R_SNOW, R_BOLT, R_FOG, NROLES };

typedef struct { short color, interior, style; } PAINT;

/* VDI colours: 0 white, 1 black, 2 red, 4 blue, 6 yellow, 8 light grey,
   9 dark grey, 12 light blue (default TOS palette) */
static const PAINT paint16[NROLES] = {
    { 1, FIS_SOLID, 0 }, { 0, FIS_SOLID, 0 }, { 6, FIS_SOLID, 0 },
    { 8, FIS_SOLID, 0 }, { 4, FIS_SOLID, 0 }, { 12, FIS_SOLID, 0 },
    { 6, FIS_SOLID, 0 }, { 9, FIS_SOLID, 0 },
};
static const PAINT paint4[NROLES] = {           /* white, black, red, green */
    { 1, FIS_SOLID, 0 }, { 0, FIS_SOLID, 0 }, { 2, FIS_SOLID, 0 },
    { 1, FIS_PATTERN, 2 }, { 1, FIS_SOLID, 0 }, { 1, FIS_SOLID, 0 },
    { 2, FIS_SOLID, 0 }, { 1, FIS_SOLID, 0 },
};
static const PAINT paint2[NROLES] = {
    { 1, FIS_SOLID, 0 }, { 0, FIS_SOLID, 0 }, { 1, FIS_PATTERN, 2 },
    { 1, FIS_PATTERN, 3 }, { 1, FIS_SOLID, 0 }, { 1, FIS_SOLID, 0 },
    { 0, FIS_SOLID, 0 }, { 1, FIS_SOLID, 0 },
};

static short vh;
static const PAINT *paint = paint2;
static short ox, oy, sw, sh;        /* the icon box */
static short gx, gy;                /* outline thickness in x and y */
static short lw;                    /* line width (odd) */

#define X(u)  (ox + (short)((long)(u) * sw / 100))
#define Y(v)  (oy + (short)((long)(v) * sh / 100))
#define RX(r) ((short)((long)(r) * sw / 100))
#define RY(r) ((short)((long)(r) * sh / 100))

void icons_init(short vdi_handle, int ncolors)
{
    vh = vdi_handle;
    paint = ncolors >= 16 ? paint16 : ncolors >= 4 ? paint4 : paint2;
}

static void fill(int role)
{
    vsf_color(vh, paint[role].color);
    vsf_interior(vh, paint[role].interior);
    if (paint[role].interior == FIS_PATTERN) vsf_style(vh, paint[role].style);
}

static void line_paint(int role)
{
    vsl_color(vh, paint[role].color);
}

/* filled ellipse, grown by the outline thickness if grow */
static void disc(int u, int v, int r, int grow)
{
    short rx = RX(r) + (grow ? gx : 0), ry = RY(r) + (grow ? gy : 0);
    v_ellipse(vh, X(u), Y(v), rx > 0 ? rx : 1, ry > 0 ? ry : 1);
}

static void box(int u1, int v1, int u2, int v2, int grow)
{
    short p[4];
    p[0] = X(u1) - (grow ? gx : 0);
    p[1] = Y(v1) - (grow ? gy : 0);
    p[2] = X(u2) + (grow ? gx : 0);
    p[3] = Y(v2) + (grow ? gy : 0);
    v_bar(vh, p);
}

static void line(int u1, int v1, int u2, int v2)
{
    short p[4];
    p[0] = X(u1); p[1] = Y(v1); p[2] = X(u2); p[3] = Y(v2);
    v_pline(vh, 2, p);
}

/* cloud centred on u,v at k percent size: three bumps on a flat base */
static void cloud_shape(int u, int v, int k, int grow)
{
    disc(u - 22 * k / 100, v + 1 * k / 100, 14 * k / 100, grow);
    disc(u - 2 * k / 100, v - 8 * k / 100, 20 * k / 100, grow);
    disc(u + 20 * k / 100, v, 15 * k / 100, grow);
    box(u - 22 * k / 100, v, u + 20 * k / 100, v + 15 * k / 100, grow);
}

static void cloud(int u, int v, int k, int role)
{
    fill(R_BLACK);
    cloud_shape(u, v, k, 1);
    fill(role);
    cloud_shape(u, v, k, 0);
}

/* rays: unit vectors * 100 for 8 directions */
static const signed char ray_dx[8] = { 100, 71, 0, -71, -100, -71, 0, 71 };
static const signed char ray_dy[8] = { 0, 71, 100, 71, 0, -71, -100, -71 };

static void sun(int u, int v, int r, int face)
{
    int i, r1 = r * 130 / 100, r2 = r * 170 / 100;

    line_paint(R_BLACK);
    vsl_width(vh, lw);
    for (i = 0; i < 8; i++)
        line(u + ray_dx[i] * r1 / 100, v + ray_dy[i] * r1 / 100,
             u + ray_dx[i] * r2 / 100, v + ray_dy[i] * r2 / 100);
    vsl_width(vh, 1);
    fill(R_BLACK);
    disc(u, v, r, 1);
    fill(R_SUN);
    disc(u, v, r, 0);
    if (face) {
        fill(R_BLACK);
        disc(u - r * 35 / 100, v - r * 22 / 100, r / 9, 0);
        disc(u + r * 35 / 100, v - r * 22 / 100, r / 9, 0);
        vsl_width(vh, lw);
        v_ellarc(vh, X(u), Y(v), RX(r * 55 / 100), RY(r * 55 / 100), 2100, 3300);
        vsl_width(vh, 1);
    }
}

static void drop(int u, int v)
{
    line(u, v, u - 4, v + 10);
}

static void flake(int u, int v)
{
    line(u - 6, v, u + 6, v);
    line(u - 3, v - 5, u + 3, v + 5);
    line(u - 3, v + 5, u + 3, v - 5);
}

static void bolt(int u)
{
    static const signed char bx[7] = { 4, -8, 0, -6, 12, 4, 12 };
    static const signed char by[7] = { 50, 72, 72, 96, 66, 66, 50 };
    short p[16];
    int i;

    for (i = 0; i < 7; i++) {
        p[2 * i] = X(u + bx[i]);
        p[2 * i + 1] = Y(by[i]);
    }
    fill(R_BOLT);
    v_fillarea(vh, 7, p);
    p[14] = p[0]; p[15] = p[1];
    line_paint(R_BLACK);
    v_pline(vh, 8, p);
}

/* precipitation below the cloud: n drops or flakes (or mixed for sleet) */
static void precip(int n, int kind, int skip_middle)
{
    static const signed char px3[3] = { 32, 52, 72 };
    static const signed char px6[6] = { 26, 46, 66, 36, 56, 76 };
    int i;

    line_paint(kind == 1 ? R_SNOW : R_RAIN);
    vsl_width(vh, lw);
    for (i = 0; i < n; i++) {
        int u = n == 3 ? px3[i] : px6[i];
        int v = n == 3 ? 72 : i < 3 ? 66 : 82;
        if (skip_middle && u > 40 && u < 62) continue;     /* room for the bolt */
        if (kind == 1 || (kind == 2 && i & 1)) {
            line_paint(R_SNOW);
            vsl_width(vh, sh >= 64 ? lw : 1);   /* thick flakes only when big */
            flake(u, v + 5);
            vsl_width(vh, lw);
        } else {
            line_paint(R_RAIN);
            drop(u, v);
        }
    }
    vsl_width(vh, 1);
}

void icon_draw(int icon, short x, short y, short w, short h, int face)
{
    int has_sun = 0, dark = 0, n = 0, kind = 0, thunder = 0;

    ox = x; oy = y; sw = w; sh = h;
    gx = w / 28 > 0 ? w / 28 : 1;
    gy = (long)gx * h / w > 0 ? (short)((long)gx * h / w) : 1;
    lw = h >= 48 ? 3 : 1;           /* by height: medium res rows are tall */
    vswr_mode(vh, MD_REPLACE);
    vsf_perimeter(vh, 0);
    vsl_ends(vh, 2, 2);
    vsl_type(vh, 1);

    switch (icon) {
    case IC_SUNNY:
        sun(50, 50, 25, face);
        return;
    case IC_PARTLY_CLOUDY:
        sun(36, 36, 20, 0);
        cloud(58, 64, 90, R_WHITE);
        return;
    case IC_CLOUDY:
    case IC_VERY_CLOUDY:
        dark = icon == IC_VERY_CLOUDY;
        cloud(64, 38, 65, dark ? R_DARK : R_WHITE);
        cloud(46, 64, 100, dark ? R_DARK : R_WHITE);
        return;
    case IC_FOG:
        cloud(50, 40, 95, R_WHITE);
        line_paint(R_FOG);
        vsl_width(vh, lw);
        line(14, 70, 78, 70);
        line(24, 80, 88, 80);
        line(14, 90, 78, 90);
        vsl_width(vh, 1);
        return;
    case IC_LIGHT_SHOWERS:        has_sun = 1; n = 3; break;
    case IC_HEAVY_SHOWERS:        has_sun = 1; n = 6; dark = 1; break;
    case IC_LIGHT_RAIN:           n = 3; break;
    case IC_HEAVY_RAIN:           n = 6; dark = 1; break;
    case IC_LIGHT_SNOW:           n = 3; kind = 1; break;
    case IC_HEAVY_SNOW:           n = 6; kind = 1; dark = 1; break;
    case IC_LIGHT_SNOW_SHOWERS:   has_sun = 1; n = 3; kind = 1; break;
    case IC_HEAVY_SNOW_SHOWERS:   has_sun = 1; n = 6; kind = 1; dark = 1; break;
    case IC_LIGHT_SLEET:          n = 6; kind = 2; break;
    case IC_LIGHT_SLEET_SHOWERS:  has_sun = 1; n = 6; kind = 2; break;
    case IC_THUNDERY_SHOWERS:     has_sun = 1; n = 6; thunder = 1; dark = 1; break;
    case IC_THUNDERY_HEAVY_RAIN:  n = 6; thunder = 1; dark = 1; break;
    case IC_THUNDERY_SNOW_SHOWERS: has_sun = 1; n = 6; kind = 1; thunder = 1; dark = 1; break;
    default:                      /* unknown: just a cloud */
        cloud(50, 56, 100, R_WHITE);
        return;
    }
    if (has_sun) sun(30, 28, 17, 0);
    cloud(54, 42, 90, dark ? R_DARK : R_WHITE);
    precip(n, kind, thunder);
    if (thunder) bolt(52);
}
