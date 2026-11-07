/* test_dbus.c — 미니 D-Bus 수신 파서 보안 시험(docs/19 V-7~V-9). 호스트 cc · 소켓 없이 db_parse/db_r_* 만.
 * 와이어가 고른 길이·시그니처로 경계 밖 읽기 · 랩어라운드 · 스택 고갈이 나지 않는지 — `make test-san`(ASan+UBSan)에서 효과가 난다. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/plat/dbus.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

/* 본문 길이·serial을 채운 완성 메시지(db_send가 소켓에 쓰기 직전 모양) */
static void finish(DbMsg *m)
{
    u32 body = m->len - m->body_off, serial = 7;
    memcpy(m->buf + 4, &body, 4);
    memcpy(m->buf + m->serial_pos, &serial, 4);
}
/* 정확한 길이의 힙 사본 — 1바이트라도 밖을 읽으면 ASan이 잡는다 */
static u8 *exact(const u8 *p, u32 n) { u8 *h = malloc(n ? n : 1); memcpy(h, p, n); return h; }

static void skip_all(DbRead *r)
{
    const char *sig = r->sig;
    int guard = 4096;
    while (sig && *sig && guard-- && db_r_skip(r, &sig)) {}
}
static int parse_exact(const u8 *buf, u32 len, DbRead *r)
{
    u8 *h = exact(buf, len); u32 total = 0;
    int st = db_parse(r, h, len, &total);
    if (st == 1) skip_all(r);
    /* 반환값을 쓰는 쪽은 메시지 안의 포인터를 들고 있을 수 있어 여기선 복사본을 유지한 채 끝낸다 */
    free(h);
    return st;
}

static void t_valid(void)
{
    DbMsg m; DbRead r; u32 total = 0;
    db_call_init(&m, "org.example.Dest", "/org/example/Path", "org.example.Iface", "Method", "sai(ib)v");
    db_w_str(&m, "hello");
    { DbArr a; db_w_arr_open(&m, 4, &a); db_w_i32(&m, 1); db_w_i32(&m, -2); db_w_arr_close(&m, &a); }
    db_w_struct(&m); db_w_i32(&m, 3); db_w_bool(&m, 1);
    db_w_variant(&m, "s"); db_w_str(&m, "var");
    finish(&m);
    CHECK(db_parse(&r, m.buf, m.len, &total) == 1 && total == m.len);
    CHECK(!strcmp(r.path, "/org/example/Path") && !strcmp(r.member, "Method") && !strcmp(r.sig, "sai(ib)v") && r.serial == 7);
    CHECK(!strcmp(db_r_str(&r), "hello"));
    { u32 end = db_r_arr_open(&r, 4); CHECK(db_r_i32(&r) == 1 && db_r_i32(&r) == -2 && r.pos == end); }
    db_r_struct(&r); CHECK(db_r_i32(&r) == 3 && db_r_u32(&r) == 1);
    { const char *vs = db_r_variant(&r); CHECK(!strcmp(vs, "s") && !strcmp(db_r_str(&r), "var")); }
    CHECK(db_r_ok(&r) && r.pos == r.end);
    /* 조각난 입력: 머리만 → 0(더 필요) */
    CHECK(db_parse(&r, m.buf, 15, &total) == 0 && db_parse(&r, m.buf, m.len - 1, &total) == 0);
    db_msg_free(&m);
}

/* 헤더 필드 문자열 길이를 와이어가 조작 */
static void t_header_lengths(void)
{
    DbMsg m; DbRead r;
    u32 bad[] = { 0xFFFFFFFFu, 0xFFFFFFF0u, 0x7FFFFFFFu, 1000, 0 };
    unsigned i;
    db_call_init(&m, "d", "/p", "i.f", "M", NULL); finish(&m);
    /* PATH 필드: 12(필드 배열 길이) 뒤 8정렬 → code(1) sig(3 'o') → 길이 u32 at 20 */
    CHECK(m.buf[16] == DB_HDR_PATH && m.buf[18] == 'o');
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        u8 *c = exact(m.buf, m.len); u32 total = 0;
        memcpy(c + 20, &bad[i], 4);
        CHECK(db_parse(&r, c, m.len, &total) != 1 || r.path[0] == 0);
        free(c);
    }
    /* NUL 종단 제거 → 폐기(-1) */
    { u8 *c = exact(m.buf, m.len); u32 total = 0; c[24 + 2] = 'x'; CHECK(db_parse(&r, c, m.len, &total) == -1); free(c); }
    /* 필드 배열 길이·본문 길이 거대 → 폐기 · total=0(u32 랩어라운드 금지) */
    { u8 *c = exact(m.buf, m.len); u32 total = 1, huge = 0xFFFFFFF0u; memcpy(c + 12, &huge, 4); CHECK(db_parse(&r, c, m.len, &total) == -1 && total == 0); free(c); }
    { u8 *c = exact(m.buf, m.len); u32 total = 1, huge = 0xFFFFFFF0u; memcpy(c + 4, &huge, 4); CHECK(db_parse(&r, c, m.len, &total) == -1 && total == 0); free(c); }
    /* 필드 배열 길이를 실제보다 조금 크게 → 0(더 필요) 또는 -1, 절대 1 아님 */
    { u8 *c = exact(m.buf, m.len); u32 total = 0, fl; memcpy(&fl, c + 12, 4); fl += 24; memcpy(c + 12, &fl, 4); CHECK(db_parse(&r, c, m.len, &total) != 1); free(c); }
    db_msg_free(&m);
}

/* 본문: 배열 길이 랩어라운드 · 깊은 variant 중첩 · 잘린 고정폭 */
static void t_body(void)
{
    DbMsg m; DbRead r; u32 total = 0;
    /* ai 길이 0xFFFFFFF0 */
    db_call_init(&m, "d", "/p", "i.f", "M", "ai");
    { u32 n = 0xFFFFFFF0u; db_w_u32(&m, n); db_w_i32(&m, 1); }
    finish(&m);
    { u8 *c = exact(m.buf, m.len); CHECK(db_parse(&r, c, m.len, &total) == 1); { u32 end = db_r_arr_open(&r, 4); CHECK(!db_r_ok(&r) && end > r.end); } CHECK(db_r_i32(&r) == 0 && !db_r_ok(&r)); free(c); }
    db_msg_free(&m);
    /* v 안에 v 안에 v … 200겹 → 스택 고갈 없이 실패 */
    db_call_init(&m, "d", "/p", "i.f", "M", "v");
    { int i; for (i = 0; i < 200; i++) db_w_variant(&m, "v"); db_w_variant(&m, "y"); db_w_byte(&m, 1); }
    finish(&m);
    { u8 *c = exact(m.buf, m.len); const char *sig; CHECK(db_parse(&r, c, m.len, &total) == 1); sig = r.sig; CHECK(!db_r_skip(&r, &sig) && !db_r_ok(&r)); free(c); }
    db_msg_free(&m);
    /* 시그니처가 'x'(8바이트)인데 본문 1바이트 */
    db_call_init(&m, "d", "/p", "i.f", "M", "x"); db_w_byte(&m, 9); finish(&m);
    { u8 *c = exact(m.buf, m.len); const char *sig; CHECK(db_parse(&r, c, m.len, &total) == 1); sig = r.sig; CHECK(!db_r_skip(&r, &sig)); free(c); }
    db_msg_free(&m);
    /* 구조체·사전 중첩 시그니처 '(((((' 끝없는 깊이 — sig 문자열은 메시지 안 */
    db_call_init(&m, "d", "/p", "i.f", "M", "a{sv}"); 
    { DbArr a; db_w_arr_open(&m, 8, &a); db_w_struct(&m); db_w_str(&m, "k"); db_w_variant(&m, "(((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((i"); db_w_arr_close(&m, &a); }
    finish(&m);
    { u8 *c = exact(m.buf, m.len); CHECK(db_parse(&r, c, m.len, &total) == 1); skip_all(&r); free(c); }
    db_msg_free(&m);
}

/* 무작위 변형 — 어떤 바이트열에도 크래시·경계 밖 접근이 없어야 한다(결과값은 묻지 않음) */
static void t_fuzz(void)
{
    DbMsg m; DbRead r;
    unsigned it, seed = 12345;
    db_call_init(&m, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher", "org.freedesktop.DBus.Properties", "GetAll", "sa{sv}(ias)");
    db_w_str(&m, "org.kde.StatusNotifierItem");
    { DbArr a, d; db_w_arr_open(&m, 8, &a);
      db_w_struct(&m); db_w_str(&m, "IconPixmap"); db_w_variant(&m, "a(iiay)"); db_w_arr_open(&m, 8, &d); db_w_struct(&m); db_w_i32(&m, 2); db_w_i32(&m, 2);
      { DbArr y; u8 px[16] = {0}; db_w_arr_open(&m, 1, &y); db_w_bytes(&m, px, 16); db_w_arr_close(&m, &y); } db_w_arr_close(&m, &d);
      db_w_struct(&m); db_w_str(&m, "Title"); db_w_variant(&m, "s"); db_w_str(&m, "Nexa Coffee");
      db_w_arr_close(&m, &a); }
    db_w_struct(&m); db_w_i32(&m, 5); { DbArr a; db_w_arr_open(&m, 4, &a); db_w_str(&m, "x"); db_w_str(&m, "yy"); db_w_arr_close(&m, &a); }
    finish(&m);
    for (it = 0; it < 30000; it++) {
        u32 len = m.len, k, flips;
        u8 *c;
        seed = seed * 1103515245u + 12345u;
        if ((seed >> 16) % 8 == 0) len = (seed >> 8) % (m.len + 8); /* 잘린·늘어난 길이 */
        c = malloc(len ? len : 1); for (k = 0; k < len; k++) c[k] = k < m.len ? m.buf[k] : (u8)(seed >> (k % 24));
        flips = 1 + ((seed >> 20) % 4);
        for (k = 0; k < flips && len; k++) { seed = seed * 1103515245u + 12345u; c[(seed >> 8) % len] = (u8)(seed >> 24); }
        if (it % 7 == 0 && len > 20) { seed = seed * 1103515245u + 12345u; u32 v = (seed % 3 == 0) ? 0xFFFFFFFFu : (seed % 3 == 1) ? 0x7FFFFFF0u : seed; u32 at = 16 + ((seed >> 8) % (len - 16)) & ~3u; if (at + 4 <= len) memcpy(c + at, &v, 4); }
        parse_exact(c, len, &r);
        free(c);
    }
    db_msg_free(&m);
    CHECK(1);
}

int main(void)
{
    t_valid(); t_header_lengths(); t_body(); t_fuzz();
    if (fails) { printf("test_dbus: %d FAIL\n", fails); return 1; }
    printf("test_dbus: all green\n");
    return 0;
}
