/* config.c: settings in WEASTHER.CFG next to the program */
#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    char location[40];          /* empty: wttr.in finds us by IP address */
    char lang[8];
    int imperial;
    int refresh;                /* minutes, 0 = off */
    char host[64];              /* wttr.in, or your own instance */
    int win_x, win_y;           /* window position, -1 = centred */
} CONFIG;

extern CONFIG cfg;

void config_init(void);         /* defaults, remember the program folder */
int config_load(void);          /* 1 if the file was there */
int config_save(void);

#endif
