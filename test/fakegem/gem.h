/* just enough of gem.h to compile src/icons.c on the host (iconview.c) */
#ifndef FAKE_GEM_H
#define FAKE_GEM_H

#define FIS_HOLLOW  0
#define FIS_SOLID   1
#define FIS_PATTERN 2
#define MD_REPLACE  1
#define MD_TRANS    2

short vsf_color(short h, short c);
short vsf_interior(short h, short i);
short vsf_style(short h, short s);
short vsf_perimeter(short h, short p);
short vsl_color(short h, short c);
short vsl_width(short h, short w);
short vsl_type(short h, short t);
void vsl_ends(short h, short b, short e);
short vswr_mode(short h, short m);
void v_ellipse(short h, short x, short y, short rx, short ry);
void v_ellarc(short h, short x, short y, short rx, short ry, short beg, short end);
void v_bar(short h, short *p);
void v_pline(short h, short n, short *p);
void v_fillarea(short h, short n, short *p);

#endif
