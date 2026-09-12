/*
 * dbus.h — 최소 D-Bus 클라이언트(라이브러리 없음 · POSIX 소켓만).
 *
 * libdbus/zbus를 쓰지 않는 이유: 산출물을 libc 하나에만 묶기 위해(nexa-shortcut 원칙 —
 * 정적 musl 빌드로 어느 배포판에서든 실행). 지원 범위는 이 앱이 쓰는 것만:
 *  - 유닉스 소켓(path/abstract) · SASL EXTERNAL · UNIX_FD 협상(logind Inhibit의 fd 수신)
 *  - 리틀엔디언 메시지 쓰기/읽기 · 기본형 · s/o/g · 배열 · 구조체 · 딕셔너리 · 변형(variant) · h(fd)
 *  - 동기 호출(사이에 온 메시지는 핸들러로) · 신호 송신 · 회신/오류 회신
 */
#ifndef CF_DBUS_H
#define CF_DBUS_H
#include "../core/coffee.h"

enum { DB_METHOD_CALL = 1, DB_METHOD_RETURN = 2, DB_ERROR = 3, DB_SIGNAL = 4 };
enum { DB_FLAG_NO_REPLY = 1 };
enum { DB_HDR_PATH = 1, DB_HDR_IFACE, DB_HDR_MEMBER, DB_HDR_ERROR, DB_HDR_REPLY_SERIAL,
       DB_HDR_DEST, DB_HDR_SENDER, DB_HDR_SIG, DB_HDR_FDS };

/* ── 쓰기 ── */
typedef struct {
    u8 *buf; u32 len, cap;
    u32 body_off;    /* 본문 시작(0 = 아직 헤더) */
    u32 fields_pos;  /* 헤더 필드 배열 길이 위치 */
    u32 serial_pos;
    int err;
    int fds[4]; u32 nfds;
} DbMsg;
typedef struct { u32 len_pos, start; } DbArr;

void db_msg_init(DbMsg *m, u8 type, u8 flags);
void db_hdr_str(DbMsg *m, u8 code, char typ, const char *s);  /* typ = 's' 'o' 'g' */
void db_hdr_u32(DbMsg *m, u8 code, u32 v);
void db_hdr_end(DbMsg *m);
void db_w_byte(DbMsg *m, u8 v);
void db_w_bool(DbMsg *m, int v);
void db_w_i32(DbMsg *m, i32 v);
void db_w_u32(DbMsg *m, u32 v);
void db_w_str(DbMsg *m, const char *s);      /* s · o */
void db_w_sig(DbMsg *m, const char *s);      /* g */
void db_w_variant(DbMsg *m, const char *sig);/* 시그니처만 — 내용은 이어서 쓴다 */
void db_w_struct(DbMsg *m);                  /* 8 정렬 */
void db_w_arr_open(DbMsg *m, u32 elem_align, DbArr *a);
void db_w_arr_close(DbMsg *m, const DbArr *a);
void db_w_bytes(DbMsg *m, const u8 *p, u32 n); /* ay 원소들(길이 포함 안 함) */
void db_w_fd(DbMsg *m, int fd);
void db_msg_free(DbMsg *m);

/* 편의: 자주 쓰는 헤더 묶음 */
void db_call_init(DbMsg *m, const char *dest, const char *path, const char *iface, const char *member, const char *sig);
void db_signal_init(DbMsg *m, const char *path, const char *iface, const char *member, const char *sig);

/* ── 읽기 ── */
typedef struct {
    const u8 *buf; u32 len, pos, end;
    u8 type, flags; u32 serial, reply_serial, nfds;
    const char *path, *iface, *member, *error, *dest, *sender, *sig;
    int fds[4]; u32 nfds_got;
} DbRead;
int         db_parse(DbRead *r, const u8 *buf, u32 len, u32 *total);
u8          db_r_byte(DbRead *r);
int         db_r_bool(DbRead *r);
i32         db_r_i32(DbRead *r);
u32         db_r_u32(DbRead *r);
const char *db_r_str(DbRead *r);
const char *db_r_sig(DbRead *r);
const char *db_r_variant(DbRead *r);         /* 시그니처 반환 · 내용은 이어서 읽는다 */
u32         db_r_arr_open(DbRead *r, u32 elem_align); /* 배열 끝 위치 반환 */
void        db_r_struct(DbRead *r);
int         db_r_skip(DbRead *r, const char **sig);   /* 완전한 형 하나 건너뜀 · *sig 전진 */
int         db_r_ok(const DbRead *r);

/* ── 연결 ── */
typedef struct {
    int fd;
    u32 serial;
    u8 *rbuf; u32 rlen, rcap;
    int fds[16]; u32 nfds;   /* 수신한 SCM_RIGHTS 대기열 */
    char unique[64];
} DbConn;
typedef void (*DbHandler)(DbConn *c, DbRead *msg, void *ud);

int  db_connect(DbConn *c, const char *addr);   /* 0 = ok · Hello까지 */
int  db_connect_session(DbConn *c);
int  db_connect_system(DbConn *c);
void db_close(DbConn *c);
u32  db_send(DbConn *c, DbMsg *m);              /* 직렬 반환(0 = 실패) · 메시지 해제 */
/* 수신: 완전한 메시지 1 · 없음 0 · 끊김 -1. out은 내부 버퍼 — 처리 후 db_consume */
int  db_recv(DbConn *c, DbRead *out, int timeout_ms);
void db_consume(DbConn *c);
/* 동기 호출. 회신(RETURN/ERROR)이 reply에. 사이 메시지는 other로. 1 = 회신 있음 */
int  db_call(DbConn *c, DbMsg *m, DbRead *reply, int timeout_ms, DbHandler other, void *ud);
/* 회신 도우미 */
void db_reply_init(DbMsg *m, const DbRead *req, const char *sig);
void db_reply_error(DbConn *c, const DbRead *req, const char *name, const char *text);

#endif
