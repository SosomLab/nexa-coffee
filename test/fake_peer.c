/*
 * fake_peer.c — 스모크용 가짜 D-Bus 상대(시험 전용 · 배포물에 안 들어간다). scripts/linux-smoke.sh
 *
 *   fake_peer watcher   세션 버스에서 org.kde.StatusNotifierWatcher 흉내.
 *                       RegisterStatusNotifierItem을 받으면 **응답하기 전에** 항목에 GetAll을 보낸다
 *                       — GNOME AppIndicator 확장의 순서(09-13 4차 등록 경쟁)를 그대로 재현한다.
 *   fake_peer login1 [deny-lid|deny-all]
 *                       DBUS_SYSTEM_BUS_ADDRESS 버스에서 org.freedesktop.login1 흉내. Inhibit에 파이프 쓰기 끝을
 *                       fd로 돌려주고 읽기 끝의 EOF로 해제를 감지한다. deny-lid = 덮개 포함 요청만 거부(polkit 거부 재현).
 *
 * 결과는 한 줄씩 stdout(스크립트가 grep): READY · REGISTER <svc> · GETALL ok pixmaps=16,22,44 status=Active ·
 * INHIBIT <what> · DENY <what> · RELEASE <what>. 앱과 같은 dbus.c를 그대로 포함해 static 도우미를 쓴다.
 */
#include "../src/plat/dbus.c"
#include <signal.h>

static void out(const char *fmt, const char *a, const char *b)
{
    printf(fmt, a, b);
    putchar('\n');
    fflush(stdout);
}

static int request_name(DbConn *c, const char *name)
{
    DbMsg m; DbRead r;
    int ok = 0;
    db_call_init(&m, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName", "su");
    db_w_str(&m, name); db_w_u32(&m, 4); /* DO_NOT_QUEUE */
    if (db_call(c, &m, &r, 3000, NULL, NULL)) { ok = r.type == DB_METHOD_RETURN && db_r_u32(&r) == 1; db_consume(c); }
    return ok;
}

/* ── watcher ── */
static void item_getall(DbConn *c, const char *dest, const char *path)
{
    DbMsg m; DbRead r;
    char sizes[64] = "", status[32] = "?";
    db_call_init(&m, dest, path, "org.freedesktop.DBus.Properties", "GetAll", "s");
    db_w_str(&m, "org.kde.StatusNotifierItem");
    if (!db_call(c, &m, &r, 3000, NULL, NULL)) { out("GETALL timeout", "", ""); return; }
    if (r.type != DB_METHOD_RETURN) { out("GETALL error %s%s", r.error ? r.error : "?", ""); db_consume(c); return; }
    {
        u32 end = db_r_arr_open(&r, 8);
        while (r.pos < end && db_r_ok(&r)) {
            const char *key, *sig;
            db_r_struct(&r);
            key = db_r_str(&r);
            sig = db_r_variant(&r);
            if (!strcmp(key, "IconPixmap")) {
                u32 e2 = db_r_arr_open(&r, 8);
                while (r.pos < e2 && db_r_ok(&r)) {
                    i32 w, h; u32 e3; char n[16];
                    db_r_struct(&r);
                    w = db_r_i32(&r); h = db_r_i32(&r);
                    e3 = db_r_arr_open(&r, 1);
                    if ((u32)(w * h * 4) != e3 - r.pos) { out("GETALL bad-pixmap-length", "", ""); db_consume(c); return; }
                    r.pos = e3;
                    snprintf(n, sizeof n, "%s%d", *sizes ? "," : "", (int)w);
                    strncat(sizes, n, sizeof sizes - strlen(sizes) - 1);
                    (void)h;
                }
            } else if (!strcmp(key, "Status")) {
                strncpy(status, db_r_str(&r), sizeof status - 1);
            } else {
                db_r_skip(&r, &sig);
            }
        }
    }
    printf("GETALL ok pixmaps=%s status=%s\n", sizes, status);
    fflush(stdout);
    db_consume(c);
}

static void run_watcher(DbConn *c)
{
    DbRead r;
    for (;;) {
        int st = db_recv(c, &r, -1);
        if (st < 0) return;
        if (st == 0) continue;
        if (r.type == DB_METHOD_CALL && r.member && !strcmp(r.member, "RegisterStatusNotifierItem")) {
            DbRead req = r;
            char svc[128], sender[64];
            DbMsg m;
            strncpy(svc, db_r_str(&req), sizeof svc - 1); svc[sizeof svc - 1] = 0;
            strncpy(sender, req.sender ? req.sender : "", sizeof sender - 1); sender[sizeof sender - 1] = 0;
            out("REGISTER %s%s", svc, "");
            /* 요청 버퍼는 다음 수신에 덮이므로 회신에 필요한 값(직렬·송신자)만 따로 들고 간다 */
            {
                u32 serial = req.serial;
                db_consume(c);
                item_getall(c, svc[0] == '/' ? sender : svc, svc[0] == '/' ? svc : "/StatusNotifierItem");
                db_msg_init(&m, DB_METHOD_RETURN, DB_FLAG_NO_REPLY);
                db_hdr_u32(&m, DB_HDR_REPLY_SERIAL, serial);
                db_hdr_str(&m, DB_HDR_DEST, 's', sender);
                db_hdr_end(&m);
                db_send(c, &m);
            }
            continue;
        }
        if (r.type == DB_METHOD_CALL && !(r.flags & DB_FLAG_NO_REPLY))
            db_reply_error(c, &r, "org.freedesktop.DBus.Error.UnknownMethod", "fake watcher");
        db_consume(c);
    }
}

/* ── login1 ── */
#define MAXI 8
static int rd[MAXI];
static char whats[MAXI][64];

static void reply_fd(DbConn *c, const DbRead *req, int fd)
{
    DbMsg m;
    db_msg_init(&m, DB_METHOD_RETURN, DB_FLAG_NO_REPLY);
    db_hdr_u32(&m, DB_HDR_REPLY_SERIAL, req->serial);
    if (req->sender) db_hdr_str(&m, DB_HDR_DEST, 's', req->sender);
    db_hdr_str(&m, DB_HDR_SIG, 'g', "h");
    db_hdr_u32(&m, DB_HDR_FDS, 1);
    db_hdr_end(&m);
    db_w_u32(&m, 0); /* fd 색인 */
    /* db_send는 fd를 보내지 않는다(앱은 받기만 한다) — 같은 마무리를 하고 SCM_RIGHTS로 쓴다 */
    put_u32_at(&m, m.serial_pos, ++c->serial);
    put_u32_at(&m, 4, m.len - m.body_off);
    write_all(c->fd, m.buf, m.len, &fd, 1);
    db_msg_free(&m);
}

static void run_login1(DbConn *c, const char *mode)
{
    int i;
    for (i = 0; i < MAXI; i++) rd[i] = -1;
    for (;;) {
        struct pollfd p[1 + MAXI];
        DbRead r;
        int st;
        /* dbus.c의 수신 버퍼에 남은 메시지가 있을 수 있어 먼저 비운다(poll 목록은 그 뒤에 — 새 파이프 포함) */
        while ((st = db_recv(c, &r, 0)) == 1) {
            if (r.type == DB_METHOD_CALL && r.member && !strcmp(r.member, "Inhibit")) {
                const char *what = db_r_str(&r);
                int deny = !strcmp(mode, "deny-all") || (!strcmp(mode, "deny-lid") && strstr(what, "handle-lid-switch"));
                if (deny) {
                    out("DENY %s%s", what, "");
                    db_reply_error(c, &r, "org.freedesktop.DBus.Error.AccessDenied", "fake polkit says no");
                } else {
                    int pf[2];
                    for (i = 0; i < MAXI && rd[i] >= 0; i++) {}
                    if (i == MAXI || pipe(pf) < 0) { db_reply_error(c, &r, "org.freedesktop.DBus.Error.Failed", "full"); }
                    else {
                        rd[i] = pf[0];
                        strncpy(whats[i], what, sizeof whats[i] - 1);
                        out("INHIBIT %s%s", what, "");
                        reply_fd(c, &r, pf[1]);
                        close(pf[1]); /* 이제 쓰기 끝은 앱만 가진다 — 앱이 닫으면 EOF */
                    }
                }
            } else if (r.type == DB_METHOD_CALL && !(r.flags & DB_FLAG_NO_REPLY)) {
                db_reply_error(c, &r, "org.freedesktop.DBus.Error.UnknownMethod", "fake login1");
            }
            db_consume(c);
        }
        if (st < 0) return;
        p[0].fd = c->fd; p[0].events = POLLIN; p[0].revents = 0;
        for (i = 0; i < MAXI; i++) { p[1 + i].fd = rd[i]; p[1 + i].events = POLLIN; p[1 + i].revents = 0; }
        if (poll(p, 1 + MAXI, -1) < 0) continue;
        for (i = 0; i < MAXI; i++) {
            if (rd[i] >= 0 && p[1 + i].revents) {
                char b[8];
                if (read(rd[i], b, sizeof b) <= 0) { out("RELEASE %s%s", whats[i], ""); close(rd[i]); rd[i] = -1; }
            }
        }
    }
}

int main(int argc, char **argv)
{
    DbConn c;
    const char *addr;
    signal(SIGPIPE, SIG_IGN);
    if (argc < 2) { fprintf(stderr, "usage: fake_peer watcher | login1 [deny-lid|deny-all]\n"); return 2; }
    if (!strcmp(argv[1], "watcher")) {
        if (db_connect_session(&c) != 0 || !request_name(&c, "org.kde.StatusNotifierWatcher")) { fprintf(stderr, "fake_peer: watcher name\n"); return 1; }
        out("READY%s%s", "", "");
        run_watcher(&c);
    } else if (!strcmp(argv[1], "login1")) {
        addr = getenv("DBUS_SYSTEM_BUS_ADDRESS");
        if (!addr || db_connect(&c, addr) != 0 || !request_name(&c, "org.freedesktop.login1")) { fprintf(stderr, "fake_peer: login1 name\n"); return 1; }
        out("READY%s%s", "", "");
        run_login1(&c, argc > 2 ? argv[2] : "allow");
    } else {
        return 2;
    }
    return 0;
}
