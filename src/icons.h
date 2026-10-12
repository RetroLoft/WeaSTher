/* icons.c: weather icons drawn with VDI, any size, aspect and colour depth */
#ifndef ICONS_H
#define ICONS_H

enum {
    IC_UNKNOWN, IC_SUNNY, IC_PARTLY_CLOUDY, IC_CLOUDY, IC_VERY_CLOUDY, IC_FOG,
    IC_LIGHT_SHOWERS, IC_HEAVY_SHOWERS, IC_LIGHT_RAIN, IC_HEAVY_RAIN,
    IC_LIGHT_SNOW, IC_HEAVY_SNOW, IC_LIGHT_SNOW_SHOWERS, IC_HEAVY_SNOW_SHOWERS,
    IC_LIGHT_SLEET, IC_LIGHT_SLEET_SHOWERS, IC_THUNDERY_SHOWERS,
    IC_THUNDERY_HEAVY_RAIN, IC_THUNDERY_SNOW_SHOWERS, IC_COUNT
};

void icons_init(short vdi_handle, int ncolors);

/* draw in the box x,y,w,h (h already corrected for the pixel aspect);
   face: the sun smiles (big icon) */
void icon_draw(int icon, short x, short y, short w, short h, int face);

#endif
