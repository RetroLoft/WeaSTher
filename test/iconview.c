/*
 * host preview of the icons: src/icons.c drawn by a tiny fake VDI into a
 * PPM, in ST low (16 colours), medium (4 colours, tall pixels) and high
 * resolution (mono). Medium is shown with doubled rows, as on a monitor.
 *
 *   iconview out.ppm
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gem.h"
#include "icons.h"

#define IW 1400
#define IH 760

static unsigned char img[IH][IW];         /* VDI colour index */
static int f_color, f_interior, f_style, l_color, l_width = 1;
static int ncol;

/* default TOS palettes by VDI index */
static const unsigned char pal16[16][3] = {
    {255,255,255},{0,0,0},{255,0,0},{0,255,0},{0,0,255},{0,255,255},{255,255,0},
    {255,0,255},{182,182,182},{109,109,109},{255,109,109},{109,255,109},
    {109,109,255},{109,255,255},{255,255,109},{255,109,255}};

short vsf_color(short h, short c) { (void)h; return f_color = c; }
short vsf_interior(short h, short i) { (void)h; return f_interior = i; }
short vsf_style(short h, short s) { (void)h; return f_style = s; }
short vsf_perimeter(short h, short p) { (void)h; return p; }
short vsl_color(short h, short c) { (void)h; return l_color = c; }
short vsl_width(short h, short w) { (void)h; return l_width = w; }
short vsl_type(short h, short t) { (void)h; return t; }
void vsl_ends(short h, short b, short e) { (void)h; (void)b; (void)e; }
short vswr_mode(short h, short m) { (void)h; return m; }

static int clip_y0, clip_y1;

static void pset(int x, int y, int c)
{
    if (x >= 0 && x < IW && y >= clip_y0 && y < clip_y1) img[y][x] = (unsigned char)c;
}

/* fill pixel: solid colour or a GEM-like grey dither */
static void fset(int x, int y)
{
    int on = 1;
    if (f_interior == FIS_PATTERN)
        switch (f_style) {
        case 1: on = !(x & 3) && !(y & 3); break;
        case 2: on = !(x & 1) && !(y & 1); break;
        case 3: on = (x ^ y) & 1; break;
        default: on = !((x & 1) && (y & 1)); break;
        }
    pset(x, y, on ? f_color : 0);
}

void v_ellipse(short h, short cx, short cy, short rx, short ry)
{
    int y;
    (void)h;
    for (y = -ry; y <= ry; y++) {
        double t = 1.0 - (double)y * y / ((double)ry * ry);
        int x, w = (int)(rx * sqrt(t > 0 ? t : 0) + 0.5);
        for (x = -w; x <= w; x++) fset(cx + x, cy + y);
    }
}

void v_bar(short h, short *p)
{
    int x, y;
    (void)h;
    for (y = p[1]; y <= p[3]; y++)
        for (x = p[0]; x <= p[2]; x++) fset(x, y);
}

static void brush(int x, int y)
{
    int r = l_width / 2, i, j;
    for (j = -r; j <= r; j++)
        for (i = -r; i <= r; i++)
            if (i * i + j * j <= r * r + r) pset(x + i, y + j, l_color);
}

static void seg(int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        int e2 = 2 * err;
        brush(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void v_pline(short h, short n, short *p)
{
    int i;
    (void)h;
    for (i = 0; i < n - 1; i++) seg(p[2 * i], p[2 * i + 1], p[2 * i + 2], p[2 * i + 3]);
}

void v_ellarc(short h, short cx, short cy, short rx, short ry, short beg, short end)
{
    int a, px = 0, py = 0;
    (void)h;
    for (a = beg; a <= end; a += 25) {
        double r = a / 1800.0 * M_PI;
        int x = cx + (int)(rx * cos(r)), y = cy - (int)(ry * sin(r));
        if (a != beg) seg(px, py, x, y);
        px = x; py = y;
    }
}

void v_fillarea(short h, short n, short *p)
{
    int y, ymin = 32767, ymax = -32768, i;
    (void)h;
    for (i = 0; i < n; i++) {
        if (p[2 * i + 1] < ymin) ymin = p[2 * i + 1];
        if (p[2 * i + 1] > ymax) ymax = p[2 * i + 1];
    }
    for (y = ymin; y <= ymax; y++) {
        double xs[32];
        int k = 0, a, b;
        for (i = 0; i < n; i++) {
            int x0 = p[2 * i], y0 = p[2 * i + 1];
            int x1 = p[2 * ((i + 1) % n)], y1 = p[2 * ((i + 1) % n) + 1];
            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y))
                xs[k++] = x0 + (double)(y - y0) * (x1 - x0) / (y1 - y0);
        }
        for (a = 0; a < k; a++)
            for (b = a + 1; b < k; b++)
                if (xs[b] < xs[a]) { double t = xs[a]; xs[a] = xs[b]; xs[b] = t; }
        for (a = 0; a + 1 < k; a += 2) {
            int x;
            for (x = (int)(xs[a] + 0.5); x <= (int)(xs[a + 1] + 0.5); x++) fset(x, y);
        }
    }
}

/* one resolution: a row of all icons, big and small */
static void row(int y, int colors, int w, int pw, int ph, int rowscale)
{
    int i, x = 4;
    int h = w * pw / ph, sw = w * 2 / 3, sh = sw * pw / ph;
    (void)rowscale;
    ncol = colors;
    icons_init(1, colors);
    clip_y0 = 0; clip_y1 = IH;
    for (i = 0; i < IC_COUNT; i++) {
        icon_draw(i, x, y, w, h, 1);
        icon_draw(i, x + (w - sw) / 2, y + h + 6, sw, sh, 0);
        x += w + 8;
    }
}

int main(int argc, char **argv)
{
    FILE *f;
    int x, y, r;
    static int rowmode[IH];             /* 1: medium-res row (shown doubled) */

    if (argc < 2) return 2;
    memset(img, 0, sizeof img);
    /* low: 320 px wide screen -> big 53, pixels 338x372 um */
    row(10, 16, 53, 338, 372, 1);
    /* medium: 640 wide -> big 106, pixels 169x372 um */
    row(130, 4, 106 / 2 * 2, 169, 372, 2);
    for (y = 130; y < 230; y++) rowmode[y] = 1;
    /* high: big 106, square pixels */
    row(240, 2, 106, 372, 372, 1);

    f = fopen(argv[1], "wb");
    fprintf(f, "P6 %d %d 255\n", IW, IH);
    for (y = 0; y < IH; y++) {
        /* medium-res rows are drawn twice to show the real aspect */
        int src = y;
        if (y >= 130 && y < 330) {
            if (y < 330) src = 130 + (y - 130) / 2;
        } else if (y >= 330) {
            src = y - 100;
        }
        if (src >= IH) src = IH - 1;
        (void)rowmode;
        for (x = 0; x < IW; x++) {
            int c = img[src][x];
            const unsigned char *rgb = pal16[c & 15];
            if (src >= 240 && c > 1) rgb = pal16[1];
            for (r = 0; r < 3; r++) fputc(rgb[r], f);
        }
    }
    fclose(f);
    return 0;
}
