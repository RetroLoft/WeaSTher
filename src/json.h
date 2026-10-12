/* json.c: just enough JSON to read wttr.in's j1 format, no allocation */
#ifndef JSON_H
#define JSON_H

/* value at a path below p, e.g. "weather/0/hourly/4/tempC"; NULL if absent */
const char *js_get(const char *p, const char *path);

/* copy a string (or bare number) value to out, UTF-8 converted to the
   Atari character set; returns the length, -1 if v is not a value */
int js_copy(const char *v, char *out, int max);

/* Atari character set <-> Unicode (Latin letters, degree sign, ...) */
int atari_from_unicode(long cp);        /* '?' if there is none */
long unicode_from_atari(unsigned char c);

#endif
