/* net.c: STinG access and a small non-blocking HTTP/1.0 GET */
#ifndef NET_H
#define NET_H

/* results of net_poll() and error codes (net_error) */
#define NET_BUSY     0
#define NET_DONE     1
#define NET_FAILED  -1

/* what went wrong (net_error_kind) */
enum {
    NE_NONE, NE_NOSTING, NE_RESOLVE, NE_CONNECT, NE_TIMEOUT,
    NE_TOOBIG, NE_HTTP, NE_NOMEM
};

int net_init(void);             /* 1: STinG found */
int net_start(const char *host, const char *path);   /* NET_BUSY / NET_FAILED */
int net_poll(void);             /* call often while busy */
void net_abort(void);

extern char *net_body;          /* response body (0-terminated) after NET_DONE */
extern long net_body_len;
extern int net_http_status;
extern int net_error_kind;
extern int net_error_code;      /* STinG error number, 0 if none */

#endif
