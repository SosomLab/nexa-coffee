/* dbus.c — 최소 D-Bus 클라이언트 구현. 명세: https://dbus.freedesktop.org/doc/dbus-specification.html */
#define _GNU_SOURCE
#include "dbus.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#ifndef MSG_CMSG_CLOEXEC
#define MSG_CMSG_CLOEXEC 0
#endif
#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0
#endif

/* ── 버퍼 ── */
static int grow(DbMsg *m, u32 need)
{
    if (m->err) return 0;
    if (m->len + need <= m->cap) return 1;
    {
        u32 nc = m->cap ? m->cap * 2 : 256;
        u8 *nb;
        while (nc < m->len + need) nc *= 2;
        nb = realloc(m->buf, nc);
        if (!nb) { m->err = 1; return 0; }
        m->buf = nb; m->cap = nc;
    }
    return 1;
}
static void put(DbMsg *m, const void *p, u32 n) { if (grow(m, n)) { memcpy(m->buf + m->len, p, n); m->len += n; } }
static void pad(DbMsg *m, u32 a) { while (m->len % a) db_w_byte(m, 0); }
static void put_u32_at(DbMsg *m, u32 pos, u32 v) { memcpy(m->buf + pos, &v, 4); }

void db_msg_init(DbMsg *m, u8 type, u8 flags)
{
    u8 hdr[12] = { 'l', type, flags, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
    memset(m, 0, sizeof *m);
    put(m, hdr, 12);
    m->serial_pos = 8;
    m->fields_pos = 12;
    db_w_u32(m, 0); /* 필드 배열 길이(나중에 패치) — 배열 원소(구조체)는 8 정렬 */
}
static void hdr_open(DbMsg *m, u8 code, char typ)
{
    char sig[2] = { typ, 0 };
    pad(m, 8);
    db_w_byte(m, code);
    db_w_sig(m, sig);
}
void db_hdr_str(DbMsg *m, u8 code, char typ, const char *s)
{
    hdr_open(m, code, typ);
    if (typ == 'g') db_w_sig(m, s); else db_w_str(m, s);
}
void db_hdr_u32(DbMsg *m, u8 code, u32 v) { hdr_open(m, code, 'u'); db_w_u32(m, v); }
void db_hdr_end(DbMsg *m)
{
    put_u32_at(m, m->fields_pos, m->len - (m->fields_pos + 4));
    pad(m, 8);
    m->body_off = m->len;
}
void db_w_byte(DbMsg *m, u8 v) { put(m, &v, 1); }
void db_w_bool(DbMsg *m, int v) { db_w_u32(m, v ? 1 : 0); }
void db_w_i32(DbMsg *m, i32 v) { pad(m, 4); put(m, &v, 4); }
void db_w_u32(DbMsg *m, u32 v) { pad(m, 4); put(m, &v, 4); }
void db_w_str(DbMsg *m, const char *s) { u32 n = (u32)strlen(s); db_w_u32(m, n); put(m, s, n + 1); }
void db_w_sig(DbMsg *m, const char *s) { u8 n = (u8)strlen(s); put(m, &n, 1); put(m, s, n + 1u); }
void db_w_variant(DbMsg *m, const char *sig) { db_w_sig(m, sig); }
void db_w_struct(DbMsg *m) { pad(m, 8); }
void db_w_arr_open(DbMsg *m, u32 elem_align, DbArr *a)
{
    pad(m, 4);
    a->len_pos = m->len;
    db_w_u32(m, 0);
    pad(m, elem_align);
    a->start = m->len;
}
void db_w_arr_close(DbMsg *m, const DbArr *a) { if (!m->err) put_u32_at(m, a->len_pos, m->len - a->start); }
void db_w_bytes(DbMsg *m, const u8 *p, u32 n) { put(m, p, n); }
void db_w_fd(DbMsg *m, int fd)
{
    if (m->nfds < 4) { m->fds[m->nfds] = fd; db_w_u32(m, m->nfds); m->nfds++; }
    else m->err = 1;
}
void db_msg_free(DbMsg *m) { free(m->buf); memset(m, 0, sizeof *m); }

void db_call_init(DbMsg *m, const char *dest, const char *path, const char *iface, const char *member, const char *sig)
{
    db_msg_init(m, DB_METHOD_CALL, 0);
    db_hdr_str(m, DB_HDR_PATH, 'o', path);
    if (iface) db_hdr_str(m, DB_HDR_IFACE, 's', iface);
    db_hdr_str(m, DB_HDR_MEMBER, 's', member);
    db_hdr_str(m, DB_HDR_DEST, 's', dest);
    if (sig && *sig) db_hdr_str(m, DB_HDR_SIG, 'g', sig);
    db_hdr_end(m);
}
void db_signal_init(DbMsg *m, const char *path, const char *iface, const char *member, const char *sig)
{
    db_msg_init(m, DB_SIGNAL, DB_FLAG_NO_REPLY);
    db_hdr_str(m, DB_HDR_PATH, 'o', path);
    db_hdr_str(m, DB_HDR_IFACE, 's', iface);
    db_hdr_str(m, DB_HDR_MEMBER, 's', member);
    if (sig && *sig) db_hdr_str(m, DB_HDR_SIG, 'g', sig);
    db_hdr_end(m);
}
void db_reply_init(DbMsg *m, const DbRead *req, const char *sig)
{
    db_msg_init(m, DB_METHOD_RETURN, DB_FLAG_NO_REPLY);
    db_hdr_u32(m, DB_HDR_REPLY_SERIAL, req->serial);
    if (req->sender) db_hdr_str(m, DB_HDR_DEST, 's', req->sender);
    if (sig && *sig) db_hdr_str(m, DB_HDR_SIG, 'g', sig);
    db_hdr_end(m);
}
void db_reply_error(DbConn *c, const DbRead *req, const char *name, const char *text)
{
    DbMsg m;
    if (req->flags & DB_FLAG_NO_REPLY) return;
    db_msg_init(&m, DB_ERROR, DB_FLAG_NO_REPLY);
    db_hdr_str(&m, DB_HDR_ERROR, 's', name);
    db_hdr_u32(&m, DB_HDR_REPLY_SERIAL, req->serial);
    if (req->sender) db_hdr_str(&m, DB_HDR_DEST, 's', req->sender);
    db_hdr_str(&m, DB_HDR_SIG, 'g', "s");
    db_hdr_end(&m);
    db_w_str(&m, text);
    db_send(c, &m);
}

/* ── 읽기 ── */
static void ralign(DbRead *r, u32 a) { while (r->pos % a) r->pos++; }
static int rneed(DbRead *r, u32 n) { if (r->pos + n > r->end) { r->pos = r->end + 1; return 0; } return 1; }
int  db_r_ok(const DbRead *r) { return r->pos <= r->end; }
u8   db_r_byte(DbRead *r) { if (!rneed(r, 1)) return 0; return r->buf[r->pos++]; }
u32  db_r_u32(DbRead *r) { u32 v; ralign(r, 4); if (!rneed(r, 4)) return 0; memcpy(&v, r->buf + r->pos, 4); r->pos += 4; return v; }
i32  db_r_i32(DbRead *r) { return (i32)db_r_u32(r); }
int  db_r_bool(DbRead *r) { return db_r_u32(r) != 0; }
const char *db_r_str(DbRead *r)
{
    u32 n = db_r_u32(r);
    const char *s;
    if (!rneed(r, n + 1)) return "";
    s = (const char *)r->buf + r->pos;
    r->pos += n + 1;
    return s;
}
const char *db_r_sig(DbRead *r)
{
    u8 n = db_r_byte(r);
    const char *s;
    if (!rneed(r, n + 1u)) return "";
    s = (const char *)r->buf + r->pos;
    r->pos += n + 1u;
    return s;
}
const char *db_r_variant(DbRead *r) { return db_r_sig(r); }
u32  db_r_arr_open(DbRead *r, u32 elem_align)
{
    u32 n = db_r_u32(r);
    ralign(r, elem_align);
    if (!rneed(r, n)) return r->pos;
    return r->pos + n;
}
void db_r_struct(DbRead *r) { ralign(r, 8); }

static u32 sig_align(char t)
{
    switch (t) {
    case 'y': case 'g': case 'v': return 1;
    case 'n': case 'q': return 2;
    case 'b': case 'i': case 'u': case 's': case 'o': case 'a': case 'h': return 4;
    default: return 8; /* x t d ( { */
    }
}
/* 시그니처의 완전한 형 하나 끝 위치 */
static const char *sig_end(const char *s)
{
    int depth = 0;
    do {
        char c = *s++;
        if (c == 'a') continue;          /* 원소형이 뒤따른다 */
        if (c == '(' || c == '{') depth++;
        else if (c == ')' || c == '}') depth--;
        else if (c == 0) return s - 1;
    } while (depth > 0 || *(s - 1) == 'a');
    return s;
}
int db_r_skip(DbRead *r, const char **sig)
{
    const char *s = *sig;
    char t = *s;
    if (!t) return 0;
    switch (t) {
    case 'y': db_r_byte(r); s++; break;
    case 'n': case 'q': ralign(r, 2); r->pos += 2; s++; break;
    case 'b': case 'i': case 'u': case 'h': db_r_u32(r); s++; break;
    case 'x': case 't': case 'd': ralign(r, 8); r->pos += 8; s++; break;
    case 's': case 'o': db_r_str(r); s++; break;
    case 'g': db_r_sig(r); s++; break;
    case 'v': { const char *vs = db_r_variant(r); db_r_skip(r, &vs); s++; break; }
    case 'a': {
        const char *es = s + 1, *ee = sig_end(es);
        u32 end = db_r_arr_open(r, sig_align(*es));
        r->pos = end; s = ee; break;
    }
    case '(': case '{': {
        s++;
        db_r_struct(r);
        while (*s && *s != ')' && *s != '}') if (!db_r_skip(r, &s)) return 0;
        if (*s) s++;
        break;
    }
    default: return 0;
    }
    *sig = s;
    return db_r_ok(r);
}

int db_parse(DbRead *r, const u8 *buf, u32 len, u32 *total)
{
    u32 body_len, fields_len, hdr_len;
    memset(r, 0, sizeof *r);
    if (len < 16) return 0;
    if (buf[0] != 'l') return -1;      /* 빅엔디언 송신자는 지원 안 함 */
    memcpy(&body_len, buf + 4, 4);
    memcpy(&fields_len, buf + 12, 4);
    hdr_len = (16 + fields_len + 7) & ~7u;
    *total = hdr_len + body_len;
    if (len < *total) return 0;
    r->buf = buf; r->len = *total; r->end = *total;
    r->type = buf[1]; r->flags = buf[2];
    memcpy(&r->serial, buf + 8, 4);
    r->pos = 16;
    while (r->pos < 16 + fields_len) {
        u8 code;
        const char *vs;
        db_r_struct(r);
        if (r->pos >= 16 + fields_len) break;
        code = db_r_byte(r);
        vs = db_r_variant(r);
        switch (code) {
        case DB_HDR_PATH:   r->path = db_r_str(r); break;
        case DB_HDR_IFACE:  r->iface = db_r_str(r); break;
        case DB_HDR_MEMBER: r->member = db_r_str(r); break;
        case DB_HDR_ERROR:  r->error = db_r_str(r); break;
        case DB_HDR_REPLY_SERIAL: r->reply_serial = db_r_u32(r); break;
        case DB_HDR_DEST:   r->dest = db_r_str(r); break;
        case DB_HDR_SENDER: r->sender = db_r_str(r); break;
        case DB_HDR_SIG:    r->sig = db_r_sig(r); break;
        case DB_HDR_FDS:    r->nfds = db_r_u32(r); break;
        default: if (!db_r_skip(r, &vs)) return -1; break;
        }
        if (!db_r_ok(r)) return -1;
    }
    r->pos = hdr_len;
    return 1;
}

/* ── 소켓 ── */
static int write_all(int fd, const void *p, u32 n, const int *fds, u32 nfds)
{
    const u8 *b = p;
    while (n) {
        struct msghdr mh; struct iovec iov; ssize_t w;
        char cbuf[CMSG_SPACE(sizeof(int) * 4)];
        memset(&mh, 0, sizeof mh);
        iov.iov_base = (void *)b; iov.iov_len = n;
        mh.msg_iov = &iov; mh.msg_iovlen = 1;
        if (nfds) {
            struct cmsghdr *cm;
            memset(cbuf, 0, sizeof cbuf);
            mh.msg_control = cbuf; mh.msg_controllen = CMSG_SPACE(sizeof(int) * nfds);
            cm = CMSG_FIRSTHDR(&mh);
            cm->cmsg_level = SOL_SOCKET; cm->cmsg_type = SCM_RIGHTS; cm->cmsg_len = CMSG_LEN(sizeof(int) * nfds);
            memcpy(CMSG_DATA(cm), fds, sizeof(int) * nfds);
        }
        w = sendmsg(fd, &mh, 0);
        if (w < 0) { if (errno == EINTR) continue; return -1; }
        b += w; n -= (u32)w; nfds = 0;
    }
    return 0;
}

static int read_line(int fd, char *out, u32 cap)
{
    u32 n = 0;
    while (n + 1 < cap) {
        char ch;
        ssize_t r = read(fd, &ch, 1);
        if (r <= 0) { if (r < 0 && errno == EINTR) continue; return -1; }
        if (ch == '\n') { if (n && out[n - 1] == '\r') n--; out[n] = 0; return (int)n; }
        out[n++] = ch;
    }
    return -1;
}

static int sasl(DbConn *c)
{
    char line[256], auth[96], uid[24];
    u32 i, n;
    cf_itoa((i64)getuid(), uid, sizeof uid);
    strcpy(auth, "AUTH EXTERNAL "); /* 선행 NUL 1바이트는 아래에서 따로 보낸다 */
    n = (u32)strlen(auth);
    for (i = 0; uid[i]; i++) { static const char hx[] = "0123456789abcdef"; auth[n++] = hx[(u8)uid[i] >> 4]; auth[n++] = hx[uid[i] & 15]; }
    auth[n++] = '\r'; auth[n++] = '\n'; auth[n] = 0;
    if (write_all(c->fd, "", 1, NULL, 0) < 0) return -1;
    if (write_all(c->fd, auth, n, NULL, 0) < 0) return -1;
    if (read_line(c->fd, line, sizeof line) < 0 || strncmp(line, "OK", 2) != 0) return -1;
    if (write_all(c->fd, "NEGOTIATE_UNIX_FD\r\n", 19, NULL, 0) < 0) return -1;
    if (read_line(c->fd, line, sizeof line) < 0) return -1; /* AGREE_UNIX_FD 또는 ERROR — 어느 쪽이든 진행 */
    if (write_all(c->fd, "BEGIN\r\n", 7, NULL, 0) < 0) return -1;
    return 0;
}

static int parse_addr(const char *addr, struct sockaddr_un *sa, socklen_t *slen)
{
    const char *p;
    memset(sa, 0, sizeof *sa);
    sa->sun_family = AF_UNIX;
    if (strncmp(addr, "unix:", 5) != 0) return -1;
    p = addr + 5;
    while (*p) {
        const char *eq = strchr(p, '='), *end;
        u32 n;
        if (!eq) return -1;
        end = strchr(eq, ','); if (!end) end = eq + strlen(eq);
        n = (u32)(end - eq - 1);
        if (n >= sizeof sa->sun_path - 1) return -1;
        if (strncmp(p, "path=", 5) == 0) {
            memcpy(sa->sun_path, eq + 1, n);
            *slen = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + n + 1);
            return 0;
        }
        if (strncmp(p, "abstract=", 9) == 0) {
            sa->sun_path[0] = 0; memcpy(sa->sun_path + 1, eq + 1, n);
            *slen = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + n + 1);
            return 0;
        }
        p = *end ? end + 1 : end;
    }
    return -1;
}

u32 db_send(DbConn *c, DbMsg *m)
{
    u32 serial = 0;
    if (!m->err && m->body_off) {
        serial = ++c->serial;
        put_u32_at(m, m->serial_pos, serial);
        put_u32_at(m, 4, m->len - m->body_off);
        if (m->nfds) {
            /* UNIX_FDS 헤더 필드는 init 시점에 없다 — fd를 쓰는 호출자는 db_hdr_u32(DB_HDR_FDS)를 직접 넣는다 */
        }
        if (write_all(c->fd, m->buf, m->len, m->fds, m->nfds) < 0) serial = 0;
    }
    db_msg_free(m);
    return serial;
}

static int fill(DbConn *c, int timeout_ms)
{
    struct pollfd p = { c->fd, POLLIN, 0 };
    struct msghdr mh; struct iovec iov; ssize_t n;
    char cbuf[CMSG_SPACE(sizeof(int) * 16)];
    int r = poll(&p, 1, timeout_ms);
    if (r <= 0) return r < 0 && errno != EINTR ? -1 : 0;
    if (c->rlen + 4096 > c->rcap) {
        u32 nc = c->rcap ? c->rcap * 2 : 8192;
        u8 *nb;
        while (nc < c->rlen + 4096) nc *= 2;
        nb = realloc(c->rbuf, nc);
        if (!nb) return -1;
        c->rbuf = nb; c->rcap = nc;
    }
    memset(&mh, 0, sizeof mh);
    iov.iov_base = c->rbuf + c->rlen; iov.iov_len = c->rcap - c->rlen;
    mh.msg_iov = &iov; mh.msg_iovlen = 1;
    mh.msg_control = cbuf; mh.msg_controllen = sizeof cbuf;
    n = recvmsg(c->fd, &mh, MSG_CMSG_CLOEXEC);
    if (n == 0) return -1;
    if (n < 0) return errno == EINTR || errno == EAGAIN ? 0 : -1;
    c->rlen += (u32)n;
    {
        struct cmsghdr *cm;
        for (cm = CMSG_FIRSTHDR(&mh); cm; cm = CMSG_NXTHDR(&mh, cm)) {
            if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_RIGHTS) {
                u32 k = (u32)((cm->cmsg_len - CMSG_LEN(0)) / sizeof(int)), i;
                const int *fds = (const int *)CMSG_DATA(cm);
                for (i = 0; i < k; i++) {
                    if (c->nfds < 16) c->fds[c->nfds++] = fds[i]; else close(fds[i]);
                }
            }
        }
    }
    return 1;
}

int db_recv(DbConn *c, DbRead *out, int timeout_ms)
{
    for (;;) {
        u32 total = 0;
        int st = db_parse(out, c->rbuf, c->rlen, &total);
        if (st == 1) {
            u32 i;
            out->nfds_got = 0;
            for (i = 0; i < out->nfds && i < 4 && c->nfds; i++) {
                out->fds[out->nfds_got++] = c->fds[0];
                memmove(c->fds, c->fds + 1, sizeof(int) * --c->nfds);
            }
            return 1;
        }
        if (st < 0) { /* 못 읽는 메시지 — 버린다 */
            if (total && total <= c->rlen) { memmove(c->rbuf, c->rbuf + total, c->rlen - total); c->rlen -= total; continue; }
            return -1;
        }
        st = fill(c, timeout_ms);
        if (st <= 0) return st;
        timeout_ms = 0; /* 이미 뭔가 왔으면 남은 조각은 즉시 시도 */
        if (c->rlen < 16) timeout_ms = 200;
    }
}

void db_consume(DbConn *c)
{
    u32 total = 0;
    DbRead r;
    if (db_parse(&r, c->rbuf, c->rlen, &total) == 1 || (total && total <= c->rlen)) {
        memmove(c->rbuf, c->rbuf + total, c->rlen - total);
        c->rlen -= total;
    }
}

int db_call(DbConn *c, DbMsg *m, DbRead *reply, int timeout_ms, DbHandler other, void *ud)
{
    u32 serial = db_send(c, m);
    int tries = 64;
    if (!serial) return 0;
    while (tries--) {
        int st = db_recv(c, reply, timeout_ms);
        if (st <= 0) return 0;
        if ((reply->type == DB_METHOD_RETURN || reply->type == DB_ERROR) && reply->reply_serial == serial) return 1;
        if (other) other(c, reply, ud);
        db_consume(c);
    }
    return 0;
}

int db_connect(DbConn *c, const char *addr)
{
    struct sockaddr_un sa; socklen_t slen = 0;
    DbMsg m; DbRead r;
    memset(c, 0, sizeof *c);
    c->fd = -1;
    if (!addr || parse_addr(addr, &sa, &slen) < 0) return -1;
    c->fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (c->fd < 0) return -1;
    fcntl(c->fd, F_SETFD, FD_CLOEXEC);
    if (connect(c->fd, (struct sockaddr *)&sa, slen) < 0 || sasl(c) < 0) { db_close(c); return -1; }
    db_call_init(&m, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "Hello", NULL);
    if (!db_call(c, &m, &r, 5000, NULL, NULL) || r.type != DB_METHOD_RETURN) { db_close(c); return -1; }
    strncpy(c->unique, db_r_str(&r), sizeof c->unique - 1);
    db_consume(c);
    return 0;
}
int db_connect_session(DbConn *c)
{
    const char *a = getenv("DBUS_SESSION_BUS_ADDRESS");
    char fallback[256];
    if (!a) {
        const char *rt = getenv("XDG_RUNTIME_DIR");
        if (!rt) return -1;
        snprintf(fallback, sizeof fallback, "unix:path=%s/bus", rt);
        a = fallback;
    }
    return db_connect(c, a);
}
int db_connect_system(DbConn *c)
{
    const char *a = getenv("DBUS_SYSTEM_BUS_ADDRESS");
    if (db_connect(c, a ? a : "unix:path=/run/dbus/system_bus_socket") == 0) return 0;
    return db_connect(c, "unix:path=/var/run/dbus/system_bus_socket");
}
void db_close(DbConn *c)
{
    u32 i;
    if (c->fd >= 0) close(c->fd);
    for (i = 0; i < c->nfds; i++) close(c->fds[i]);
    free(c->rbuf);
    memset(c, 0, sizeof *c);
    c->fd = -1;
}
