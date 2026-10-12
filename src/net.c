/*
 * STinG access and a small non-blocking HTTP/1.0 GET.
 *
 * The program is built without -mshort, STinG is Pure C with 16-bit ints:
 * every call goes through pc_call() (pccall.S) with the arguments packed
 * as Pure C pushes them.
 */
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <osbind.h>
#include "net.h"

#define BUF_SIZE    65000L      /* wttr.in j1 answers are 20-30 KB */
#define TIMEOUT     (30 * 200L) /* 200 Hz ticks: connect + transfer */

/* STinG error numbers used here (transprt.h) */
#define E_NORMAL    0
#define E_OBUFFULL  -1
#define E_NODATA    -2
#define E_EOF       -3
#define E_LISTEN    -10

long pc_call(void *fn, const void *args, long nbytes);

typedef struct {
    char magic[10];             /* "STiKmagic" */
    void *get_dftab;            /* DRV_HDR *cdecl get_dftab(char *) */
    void *etm_exec;
    void *cfg;
    void *basepage;
} DRV_LIST;

/* TPL as in transprt.h: three strings, then the function table */
typedef struct {
    char *module, *author, *version;
    void *KRmalloc, *KRfree, *KRgetfree, *KRrealloc, *get_err_text, *getvstr;
    void *carrier_detect, *TCP_open, *TCP_close, *TCP_send, *TCP_wait_state;
    void *TCP_ack_wait, *UDP_open, *UDP_close, *UDP_send, *CNkick;
    void *CNbyte_count, *CNget_char, *CNget_NDB, *CNget_block, *housekeep;
    void *resolve;
} TPL;

static TPL *tpl;

char *net_body;
long net_body_len;
int net_http_status;
int net_error_kind;
int net_error_code;

static char *buf;
static long len;
static int cn = -1;
static int state;               /* 0 idle, 1 connecting, 2 receiving */
static long t_start;
static char request[400];

/* call a STinG function: fmt has 'w' (16 bit) or 'l' (32 bit, pointer) per
   argument; all arguments are passed as long */
static long scall(void *fn, const char *fmt, ...)
{
    unsigned char a[32];
    int n = 0;
    va_list ap;

    va_start(ap, fmt);
    for (; *fmt; fmt++) {
        unsigned long v = va_arg(ap, unsigned long);
        if (*fmt == 'l') {
            a[n++] = (unsigned char)(v >> 24);
            a[n++] = (unsigned char)(v >> 16);
        }
        a[n++] = (unsigned char)(v >> 8);
        a[n++] = (unsigned char)v;
    }
    va_end(ap);
    return pc_call(fn, a, n);
}

/* int16 result, sign extended */
#define S16(x)  ((long)(short)(x))

static long ticks(void) { return *(volatile long *)0x4baL; }
static long get_ticks(void) { return Supexec(ticks); }

static long cookie_val;
static long find_stik(void)
{
    long *jar = *(long **)0x5a0L;
    cookie_val = 0;
    if (!jar) return 0;
    for (; jar[0]; jar += 2)
        if (jar[0] == 0x5354694bL) {    /* 'STiK' */
            cookie_val = jar[1];
            return 1;
        }
    return 0;
}

int net_init(void)
{
    DRV_LIST *drv;

    if (tpl) return 1;
    Supexec(find_stik);
    drv = (DRV_LIST *)cookie_val;
    if (!drv || memcmp(drv->magic, "STiKmagic", 10)) return 0;
    tpl = (TPL *)scall(drv->get_dftab, "l", (long)"TRANSPORT_TCPIP");
    if (!tpl || ((long)tpl & 1) || strcmp(tpl->module, "TRANSPORT_TCPIP")) {
        tpl = 0;
        return 0;
    }
    if (!buf) buf = malloc(BUF_SIZE + 1);
    return tpl != 0 && buf != 0;
}

static int fail(int kind, long code)
{
    net_abort();
    net_error_kind = kind;
    net_error_code = (int)code;
    return NET_FAILED;
}

int net_start(const char *host, const char *path)
{
    unsigned long ip = 0;
    long r;

    net_abort();
    net_error_kind = NE_NONE;
    net_error_code = 0;
    net_http_status = 0;
    if (!net_init()) return fail(tpl ? NE_NOMEM : NE_NOSTING, 0);

    /* resolve() blocks, but answers from STinG's cache are instant */
    r = S16(scall(tpl->resolve, "lllw", (long)host, 0L, (long)&ip, 1L));
    if (r <= 0) return fail(NE_RESOLVE, r);

    r = S16(scall(tpl->TCP_open, "lwww", (long)ip, 80L, 0L, 4096L));
    if (r < 0) return fail(NE_CONNECT, r);
    cn = (int)r;

    strcpy(request, "GET ");
    strcat(request, path);
    strcat(request, " HTTP/1.0\r\nHost: ");
    strcat(request, host);
    strcat(request, "\r\nUser-Agent: WeaSTher/1.0 (Atari ST; STinG)\r\n"
                    "Accept: */*\r\nConnection: close\r\n\r\n");
    len = 0;
    state = 1;
    t_start = get_ticks();
    return NET_BUSY;
}

/* status line and header end; body moved to the start of the buffer */
static int finish(void)
{
    char *p, *body;

    buf[len] = 0;
    if (len < 12 || memcmp(buf, "HTTP/", 5)) return fail(NE_HTTP, 0);
    p = strchr(buf, ' ');
    net_http_status = p ? atoi(p + 1) : 0;
    body = strstr(buf, "\r\n\r\n");
    if (!body) return fail(NE_HTTP, 0);
    body += 4;
    net_body_len = len - (body - buf);
    memmove(buf, body, net_body_len + 1);
    net_body = buf;
    net_abort();
    if (net_http_status != 200) {
        net_error_kind = NE_HTTP;
        net_error_code = net_http_status;
        return NET_FAILED;
    }
    return NET_DONE;
}

int net_poll(void)
{
    long r;
    int rounds;

    if (!state) return NET_FAILED;
    if (get_ticks() - t_start > TIMEOUT) return fail(NE_TIMEOUT, 0);

    if (state == 1) {
        /* TCP_send answers E_NODATA/E_LISTEN until the connection is up */
        r = S16(scall(tpl->TCP_send, "wlw", (long)cn, (long)request,
                      (long)strlen(request)));
        if (r == E_NODATA || r == E_LISTEN || r == E_OBUFFULL) return NET_BUSY;
        if (r != E_NORMAL) return fail(NE_CONNECT, r);
        state = 2;
        return NET_BUSY;
    }

    for (rounds = 0; rounds < 8; rounds++) {
        r = S16(scall(tpl->CNbyte_count, "w", (long)cn));
        if (r == E_NODATA) return NET_BUSY;
        if (r == E_EOF) return finish();
        if (r < 0) {
            /* a reset after a complete answer still counts */
            if (len > 0) return finish();
            return fail(NE_CONNECT, r);
        }
        if (r == 0) return NET_BUSY;
        if (len + r > BUF_SIZE) return fail(NE_TOOBIG, 0);
        r = S16(scall(tpl->CNget_block, "wlw", (long)cn, (long)(buf + len), r));
        if (r > 0) len += r;
        else if (r != E_NODATA) return fail(NE_CONNECT, r);
    }
    return NET_BUSY;
}

void net_abort(void)
{
    static short result;
    if (cn >= 0 && tpl)
        scall(tpl->TCP_close, "wwl", (long)cn, 0L, (long)&result);
    cn = -1;
    state = 0;
}
