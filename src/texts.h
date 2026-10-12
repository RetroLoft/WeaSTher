/* texts.c: user interface texts (Atari character set) */
#ifndef TEXTS_H
#define TEXTS_H

typedef struct {
    const char *code;
    const char *today, *tomorrow, *weekday[7];
    const char *feels, *wind, *humidity, *precip, *sun;
    const char *refresh, *options;
    const char *fetching, *updated, *failed;
    const char *err[8];                 /* by net.h NE_* */
    const char *unknown_place, *bad_answer;
    const char *dlg_title, *dlg_place, *dlg_place_hint, *dlg_lang, *dlg_lang_hint;
    const char *dlg_units, *dlg_metric, *dlg_imperial;
    const char *dlg_refresh, *dlg_refresh_hint, *dlg_ok, *dlg_cancel;
    const char *compass;                /* N E S W in this language */
} TEXTS;

/* texts for a wttr.in language code; English if we have no table */
const TEXTS *texts_for(const char *lang);

#endif
