/*
 * Just enough JSON to read wttr.in's j1 format: walk a path through
 * objects and arrays in the raw text, no tree, no allocation.
 */
#include <string.h>
#include <stdlib.h>
#include "json.h"

/* Atari ST character set: the non-ASCII characters we can show */
static const struct { unsigned short uc; unsigned char at; } charmap[] = {
    { 0x00c7, 0x80 }, { 0x00fc, 0x81 }, { 0x00e9, 0x82 }, { 0x00e2, 0x83 },
    { 0x00e4, 0x84 }, { 0x00e0, 0x85 }, { 0x00e5, 0x86 }, { 0x00e7, 0x87 },
    { 0x00ea, 0x88 }, { 0x00eb, 0x89 }, { 0x00e8, 0x8a }, { 0x00ef, 0x8b },
    { 0x00ee, 0x8c }, { 0x00ec, 0x8d }, { 0x00c4, 0x8e }, { 0x00c5, 0x8f },
    { 0x00c9, 0x90 }, { 0x00e6, 0x91 }, { 0x00c6, 0x92 }, { 0x00f4, 0x93 },
    { 0x00f6, 0x94 }, { 0x00f2, 0x95 }, { 0x00fb, 0x96 }, { 0x00f9, 0x97 },
    { 0x00ff, 0x98 }, { 0x00d6, 0x99 }, { 0x00dc, 0x9a }, { 0x00a2, 0x9b },
    { 0x00a3, 0x9c }, { 0x00a5, 0x9d }, { 0x00df, 0x9e }, { 0x00e1, 0xa0 },
    { 0x00ed, 0xa1 }, { 0x00f3, 0xa2 }, { 0x00fa, 0xa3 }, { 0x00f1, 0xa4 },
    { 0x00d1, 0xa5 }, { 0x00aa, 0xa6 }, { 0x00ba, 0xa7 }, { 0x00bf, 0xa8 },
    { 0x00ac, 0xaa }, { 0x00bd, 0xab }, { 0x00bc, 0xac }, { 0x00a1, 0xad },
    { 0x00ab, 0xae }, { 0x00bb, 0xaf }, { 0x00e3, 0xb0 }, { 0x00f5, 0xb1 },
    { 0x00d8, 0xb2 }, { 0x00f8, 0xb3 }, { 0x0153, 0xb4 }, { 0x0152, 0xb5 },
    { 0x00c0, 0xb6 }, { 0x00c3, 0xb7 }, { 0x00d5, 0xb8 }, { 0x00a7, 0xdd },
    { 0x00b1, 0xf1 }, { 0x00f7, 0xf6 }, { 0x00b0, 0xf8 }, { 0x00b2, 0xfd },
    { 0x00b3, 0xfe }, { 0x00b5, 0xe6 },
    /* close enough */
    { 0x00c1, 'A' }, { 0x00c2, 'A' }, { 0x00c8, 'E' }, { 0x00ca, 'E' },
    { 0x00cb, 'E' }, { 0x00cd, 'I' }, { 0x00d3, 'O' }, { 0x00d4, 'O' },
    { 0x00da, 'U' }, { 0x00fd, 'y' }, { 0x0107, 'c' }, { 0x010d, 'c' },
    { 0x0119, 'e' }, { 0x011b, 'e' }, { 0x0142, 'l' }, { 0x0144, 'n' },
    { 0x0159, 'r' }, { 0x015b, 's' }, { 0x0161, 's' }, { 0x017a, 'z' },
    { 0x017c, 'z' }, { 0x017e, 'z' }, { 0x0160, 'S' }, { 0x017d, 'Z' },
    { 0x0141, 'L' }, { 0x0131, 'i' }, { 0x011f, 'g' }, { 0x015f, 's' },
    { 0x2018, '\'' }, { 0x2019, '\'' }, { 0x201c, '"' }, { 0x201d, '"' },
    { 0x2013, '-' }, { 0x2014, '-' }, { 0x00a0, ' ' },
};
#define NCHARMAP (sizeof charmap / sizeof charmap[0])

int atari_from_unicode(long cp)
{
    unsigned i;
    if (cp < 0x80) return (int)cp;
    for (i = 0; i < NCHARMAP; i++)
        if (charmap[i].uc == cp) return charmap[i].at;
    return '?';
}

long unicode_from_atari(unsigned char c)
{
    unsigned i;
    if (c < 0x80) return c;
    for (i = 0; i < NCHARMAP; i++)
        if (charmap[i].at == c) return charmap[i].uc;
    return '?';
}

static const char *ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

/* p at the opening quote; returns the position after the closing quote */
static const char *skip_string(const char *p)
{
    for (p++; *p && *p != '"'; p++)
        if (*p == '\\' && p[1]) p++;
    return *p ? p + 1 : p;
}

/* skip one value (any type); returns the position after it */
static const char *skip_value(const char *p)
{
    int depth = 0;

    p = ws(p);
    do {
        switch (*p) {
        case 0:
            return p;
        case '"':
            p = skip_string(p);
            continue;
        case '{': case '[':
            depth++;
            break;
        case '}': case ']':
            depth--;
            break;
        default:
            if (!depth) {           /* number, true, false, null */
                while (*p && *p != ',' && *p != '}' && *p != ']') p++;
                return p;
            }
        }
        p++;
    } while (depth > 0);
    return p;
}

static const char *member(const char *p, const char *key, int keylen)
{
    p = ws(p);
    if (*p != '{') return NULL;
    p = ws(p + 1);
    while (*p == '"') {
        const char *k = p + 1, *v;
        p = skip_string(p);
        v = ws(ws(p) + 1);                  /* skip the ':' */
        if (p - k - 1 == keylen && !memcmp(k, key, keylen)) return v;
        p = ws(skip_value(v));
        if (*p != ',') break;
        p = ws(p + 1);
    }
    return NULL;
}

static const char *element(const char *p, int idx)
{
    p = ws(p);
    if (*p != '[') return NULL;
    p = ws(p + 1);
    if (*p == ']') return NULL;
    while (idx-- > 0) {
        p = ws(skip_value(p));
        if (*p != ',') return NULL;
        p = ws(p + 1);
    }
    return p;
}

const char *js_get(const char *p, const char *path)
{
    while (p && *path) {
        const char *e = strchr(path, '/');
        int n = e ? (int)(e - path) : (int)strlen(path);
        if (*path >= '0' && *path <= '9')
            p = element(p, atoi(path));
        else
            p = member(p, path, n);
        path += n;
        if (*path == '/') path++;
    }
    return p;
}

static int hex4(const char *p, long *cp)
{
    int i;
    *cp = 0;
    for (i = 0; i < 4; i++) {
        char c = p[i];
        int d = c >= '0' && c <= '9' ? c - '0' :
                c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return 0;
        *cp = *cp << 4 | d;
    }
    return 1;
}

int js_copy(const char *v, char *out, int max)
{
    int n = 0;
    const unsigned char *p;

    if (!v) return -1;
    v = ws(v);
    if (*v != '"') {                        /* bare number */
        while (*v && *v != ',' && *v != '}' && *v != ']' && *v != ' ' && n < max - 1)
            out[n++] = *v++;
        out[n] = 0;
        return n;
    }
    p = (const unsigned char *)v + 1;
    while (*p && *p != '"' && n < max - 1) {
        long cp;
        if (*p == '\\') {
            p++;
            switch (*p) {
            case 'n': case 'r': case 't': cp = ' '; p++; break;
            case 'u':
                if (!hex4((const char *)p + 1, &cp)) cp = '?';
                p += 5;
                break;
            case 0: cp = 0; break;
            default: cp = *p++;
            }
            if (!cp) break;
        } else if (*p < 0x80) {
            cp = *p++;
        } else if ((*p & 0xe0) == 0xc0 && p[1]) {
            cp = (long)(*p & 0x1f) << 6 | (p[1] & 0x3f);
            p += 2;
        } else if ((*p & 0xf0) == 0xe0 && p[1] && p[2]) {
            cp = (long)(*p & 0x0f) << 12 | (long)(p[1] & 0x3f) << 6 | (p[2] & 0x3f);
            p += 3;
        } else {
            p++;
            while ((*p & 0xc0) == 0x80) p++;
            cp = '?';
        }
        out[n++] = (char)atari_from_unicode(cp);
    }
    /* wttr.in pads some descriptions with a space */
    while (n > 0 && out[n - 1] == ' ') n--;
    out[n] = 0;
    return n;
}
