/*
 * linux.c — Linux 플랫폼 계층. 트레이 = StatusNotifierItem(SNI · D-Bus) + com.canonical.dbusmenu,
 * 절전 방지 = org.freedesktop.ScreenSaver.Inhibit(세션) + org.freedesktop.login1 Inhibit(시스템 · fd 잠금).
 *
 * - 라이브러리: libc 뿐(자체 D-Bus 클라이언트 dbus.c). 정적 musl 빌드 → 배포판 비종속.
 * - 메뉴는 셸(KDE Plasma · GNOME AppIndicator 확장 · XFCE 등)이 dbusmenu 레이아웃을 읽어 그린다.
 *   좌·우클릭 모두 메뉴(ItemIsMenu = true).
 * - logind Inhibit "sleep:idle:handle-lid-switch"(block) → 유휴 절전·최대 절전·**덮개 닫힘 절전**까지 막는다
 *   (polkit 기본 정책은 활성 세션에 허용 · 거부되면 "sleep:idle"로 재시도). ScreenSaver.Inhibit → 화면보호기·화면 끄기.
 * - 작업 종료 시: 억제 해제(fd close · UnInhibit) · 동작 아이콘 버퍼 free · malloc_trim(glibc).
 * - 이식 참고: nexa-clip `nclip-plat/src/tray.rs::sni`(zbus 구현)의 속성·메뉴 계약을 C로 옮겼다.
 */
#define _GNU_SOURCE
#include "dbus.h"
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __GLIBC__
#include <malloc.h>
#endif

#define ITEM_PATH "/StatusNotifierItem"
#define MENU_PATH "/MenuBar"
#define SNI_IFACE "org.kde.StatusNotifierItem"
#define MENU_IFACE "com.canonical.dbusmenu"
#define WATCHER "org.kde.StatusNotifierWatcher"
#define PROPS_IFACE "org.freedesktop.DBus.Properties"

static const int ICON_SIZES[2] = { 22, 44 };

static DbConn ses, sysb;
static int has_sys;
static CfApp app;
static i64 deadline, total, next_tick;
static u8 *idle_px[2], *active_px[2];
static u32 ss_cookie, ss_serial;
static int login_fd = -1;
static u32 menu_rev = 1;
static char busname[64], conf_path[1024];
static volatile sig_atomic_t quit;

static i64 now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (i64)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* ── 설정 ── */
static void conf_init(void)
{
    const char *x = getenv("XDG_CONFIG_HOME"), *h = getenv("HOME");
    char dir[900], lock[1024];
    int fd;
    if (x && *x) { snprintf(dir, sizeof dir, "%s", x); mkdir(dir, 0700); }
    else if (h) { snprintf(dir, sizeof dir, "%s/.config", h); mkdir(dir, 0700); }
    else snprintf(dir, sizeof dir, "/tmp");
    strncat(dir, "/nexa-coffee", sizeof dir - strlen(dir) - 1);
    mkdir(dir, 0700);
    snprintf(conf_path, sizeof conf_path, "%s/config", dir);
    snprintf(lock, sizeof lock, "%s/lock", dir);
    fd = open(lock, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd >= 0 && flock(fd, LOCK_EX | LOCK_NB) != 0) exit(0); /* 이미 실행 중 */
}
static void conf_load(void)
{
    char buf[256];
    int fd = open(conf_path, O_RDONLY);
    ssize_t n;
    if (fd < 0) return;
    n = read(fd, buf, sizeof buf - 1);
    close(fd);
    if (n > 0) cf_conf_parse(&app, buf, (u32)n);
}
static void conf_save(void)
{
    char buf[256];
    u32 n = cf_conf_format(&app, buf, sizeof buf);
    int fd = open(conf_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return;
    (void)!write(fd, buf, n);
    close(fd);
}

/* ── 아이콘 ── */
static void render_idle(void)
{
    CfColor gray = {154, 154, 158, 255};
    int i;
    for (i = 0; i < 2; i++) {
        if (!idle_px[i]) idle_px[i] = malloc(CF_ICON_BYTES);
        cf_icon_idle(idle_px[i], ICON_SIZES[i], gray);
    }
}
static void render_active(const CfDisplay *d)
{
    int i;
    for (i = 0; i < 2; i++) {
        if (!active_px[i]) active_px[i] = malloc(CF_ICON_BYTES);
        cf_icon_active(active_px[i], ICON_SIZES[i], d);
    }
}
/* a(iiay) — ARGB32 네트워크 바이트 순서 [A,R,G,B] */
static void write_pixmaps(DbMsg *m)
{
    DbArr a, b;
    int i;
    db_w_arr_open(m, 8, &a);
    for (i = 0; i < 2; i++) {
        const u8 *px = app.running && active_px[i] ? active_px[i] : idle_px[i];
        int s = ICON_SIZES[i], k;
        if (!px) continue;
        db_w_struct(m);
        db_w_i32(m, s); db_w_i32(m, s);
        db_w_arr_open(m, 1, &b);
        for (k = 0; k < s * s; k++) {
            u8 q[4] = { px[k * 4 + 3], px[k * 4], px[k * 4 + 1], px[k * 4 + 2] };
            db_w_bytes(m, q, 4);
        }
        db_w_arr_close(m, &b);
    }
    db_w_arr_close(m, &a);
}

/* ── 신호 ── */
static void emit0(const char *path, const char *iface, const char *member)
{
    DbMsg m;
    db_signal_init(&m, path, iface, member, NULL);
    db_send(&ses, &m);
}
static void emit_layout_updated(void)
{
    DbMsg m;
    menu_rev++;
    db_signal_init(&m, MENU_PATH, MENU_IFACE, "LayoutUpdated", "ui");
    db_w_u32(&m, menu_rev); db_w_i32(&m, 0);
    db_send(&ses, &m);
}
static void emit_icon(void)
{
    emit0(ITEM_PATH, SNI_IFACE, "NewIcon");
    emit0(ITEM_PATH, SNI_IFACE, "NewToolTip");
}

/* ── 절전 억제 ── */
static int login1_inhibit(const char *what)
{
    DbMsg m; DbRead r;
    int fd = -1;
    db_call_init(&m, "org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "Inhibit", "ssss");
    db_w_str(&m, what); db_w_str(&m, "Nexa Coffee");
    db_w_str(&m, app.lang == CF_LANG_KO ? "사용자가 절전 방지를 켰습니다" : "User asked to keep the system awake");
    db_w_str(&m, "block");
    if (db_call(&sysb, &m, &r, 3000, NULL, NULL)) {
        if (r.type == DB_METHOD_RETURN && r.nfds_got > 0) { fd = r.fds[0]; r.nfds_got = 0; }
        {
            u32 i;
            for (i = 0; i < r.nfds_got; i++) close(r.fds[i]);
        }
        db_consume(&sysb);
    }
    return fd;
}
static void inhibit(int on)
{
    DbMsg m;
    if (on) {
        if (!ss_cookie && !ss_serial) {
            db_call_init(&m, "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver", "org.freedesktop.ScreenSaver", "Inhibit", "ss");
            db_w_str(&m, "nexa-coffee");
            db_w_str(&m, app.lang == CF_LANG_KO ? "절전 방지" : "Keeping awake");
            ss_serial = db_send(&ses, &m);
        }
        if (login_fd < 0 && has_sys) {
            login_fd = login1_inhibit("sleep:idle:handle-lid-switch");
            if (login_fd < 0) login_fd = login1_inhibit("sleep:idle");
        }
    } else {
        if (ss_cookie) {
            db_call_init(&m, "org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver", "org.freedesktop.ScreenSaver", "UnInhibit", "u");
            m.buf[2] |= DB_FLAG_NO_REPLY;
            db_w_u32(&m, ss_cookie);
            db_send(&ses, &m);
            ss_cookie = 0;
        }
        ss_serial = 0;
        if (login_fd >= 0) { close(login_fd); login_fd = -1; }
    }
}

/* ── 작업 ── */
static void job_stop(void)
{
    int i;
    inhibit(0);
    app.running = 0;
    next_tick = 0;
    for (i = 0; i < 2; i++) { free(active_px[i]); active_px[i] = NULL; }
    emit_icon();
    emit_layout_updated();
#ifdef __GLIBC__
    malloc_trim(0); /* 작업이 쓴 힙을 OS에 반납(사용자 요청 09-12) */
#endif
}
static void job_tick(void)
{
    CfDisplay d;
    i64 rem = deadline ? deadline - now_ms() : 0;
    if (deadline && rem <= 0) { job_stop(); return; }
    cf_display(deadline ? (rem + 999) / 1000 : 0, total, &d);
    render_active(&d);
    emit_icon();
    next_tick = now_ms() + cf_next_change_ms(rem, total);
}
static void job_start(void)
{
    i64 secs = cf_sel_secs(&app);
    total = secs;
    deadline = secs > 0 ? now_ms() + secs * 1000 : 0;
    inhibit(1);
    job_tick();
    emit_layout_updated();
}

/* ── 메뉴 항목 → a{sv} ── */
static void dict_s(DbMsg *m, const char *k, const char *v) { db_w_struct(m); db_w_str(m, k); db_w_variant(m, "s"); db_w_str(m, v); }
static void dict_b(DbMsg *m, const char *k, int v)         { db_w_struct(m); db_w_str(m, k); db_w_variant(m, "b"); db_w_bool(m, v); }
static void dict_i(DbMsg *m, const char *k, i32 v)         { db_w_struct(m); db_w_str(m, k); db_w_variant(m, "i"); db_w_i32(m, v); }

static void write_item_props(DbMsg *m, int id)
{
    DbArr a;
    CfMenuItem it;
    char label[130];
    u32 i, n = 0;
    db_w_arr_open(m, 8, &a);
    if (id == CF_ID_ROOT) { dict_s(m, "children-display", "submenu"); db_w_arr_close(m, &a); return; }
    if (!cf_menu_item(&app, id, &it)) { db_w_arr_close(m, &a); return; }
    if (it.kind == CF_KIND_SEPARATOR) { dict_s(m, "type", "separator"); db_w_arr_close(m, &a); return; }
    for (i = 0; it.label[i] && n < sizeof label - 3; i++) { /* dbusmenu는 '_'를 니모닉으로 먹는다 */
        if (it.label[i] == '_') label[n++] = '_';
        label[n++] = it.label[i];
    }
    label[n] = 0;
    dict_s(m, "label", label);
    dict_b(m, "enabled", it.enabled);
    dict_b(m, "visible", 1);
    if (it.kind == CF_KIND_RADIO || it.kind == CF_KIND_CHECK) {
        dict_s(m, "toggle-type", it.kind == CF_KIND_RADIO ? "radio" : "checkmark");
        dict_i(m, "toggle-state", it.checked ? 1 : 0);
    }
    if (it.kind == CF_KIND_SUBMENU) dict_s(m, "children-display", "submenu");
    db_w_arr_close(m, &a);
}
/* (ia{sv}av) 재귀 */
static void write_node(DbMsg *m, int id, int depth)
{
    DbArr a;
    int ids[CF_MENU_MAX_CHILDREN], n = 0, i;
    db_w_struct(m);
    db_w_i32(m, id);
    write_item_props(m, id);
    if (depth != 0) n = cf_menu_children(&app, id, ids, CF_MENU_MAX_CHILDREN);
    db_w_arr_open(m, 1, &a);
    for (i = 0; i < n; i++) { db_w_variant(m, "(ia{sv}av)"); write_node(m, ids[i], depth > 0 ? depth - 1 : depth); }
    db_w_arr_close(m, &a);
}

/* ── SNI 속성(variant) ── */
static const char *const SNI_PROPS[] = {
    "Category", "Id", "Title", "Status", "IconName", "IconThemePath", "IconPixmap", "OverlayIconName",
    "OverlayIconPixmap", "AttentionIconName", "AttentionIconPixmap", "AttentionMovieName", "ToolTip",
    "ItemIsMenu", "Menu", "WindowId", NULL
};
static int write_sni_prop(DbMsg *m, const char *name)
{
    DbArr a;
    char tip[128];
    if (!strcmp(name, "Category"))      { db_w_variant(m, "s"); db_w_str(m, "ApplicationStatus"); return 1; }
    if (!strcmp(name, "Id"))            { db_w_variant(m, "s"); db_w_str(m, "nexa-coffee"); return 1; }
    if (!strcmp(name, "Title"))         { db_w_variant(m, "s"); db_w_str(m, cf_str(app.lang, CF_STR_APP)); return 1; }
    if (!strcmp(name, "Status"))        { db_w_variant(m, "s"); db_w_str(m, "Active"); return 1; }
    if (!strcmp(name, "IconName") || !strcmp(name, "IconThemePath") || !strcmp(name, "OverlayIconName") ||
        !strcmp(name, "AttentionIconName") || !strcmp(name, "AttentionMovieName")) { db_w_variant(m, "s"); db_w_str(m, ""); return 1; }
    if (!strcmp(name, "IconPixmap"))    { db_w_variant(m, "a(iiay)"); write_pixmaps(m); return 1; }
    if (!strcmp(name, "OverlayIconPixmap") || !strcmp(name, "AttentionIconPixmap")) {
        db_w_variant(m, "a(iiay)"); db_w_arr_open(m, 8, &a); db_w_arr_close(m, &a); return 1;
    }
    if (!strcmp(name, "ToolTip")) {
        CfDisplay d;
        i64 rem = deadline ? deadline - now_ms() : 0;
        if (app.running) cf_display(deadline ? (rem + 999) / 1000 : 0, total, &d);
        cf_tooltip(&app, app.running ? &d : NULL, tip, sizeof tip);
        db_w_variant(m, "(sa(iiay)ss)");
        db_w_struct(m); db_w_str(m, ""); db_w_arr_open(m, 8, &a); db_w_arr_close(m, &a);
        db_w_str(m, tip); db_w_str(m, "");
        return 1;
    }
    if (!strcmp(name, "ItemIsMenu"))    { db_w_variant(m, "b"); db_w_bool(m, 1); return 1; }
    if (!strcmp(name, "Menu"))          { db_w_variant(m, "o"); db_w_str(m, MENU_PATH); return 1; }
    if (!strcmp(name, "WindowId"))      { db_w_variant(m, "i"); db_w_i32(m, 0); return 1; }
    return 0;
}
static const char *const MENU_PROPS[] = { "Version", "Status", "TextDirection", "IconThemePath", NULL };
static int write_menu_prop(DbMsg *m, const char *name)
{
    DbArr a;
    if (!strcmp(name, "Version"))       { db_w_variant(m, "u"); db_w_u32(m, 3); return 1; }
    if (!strcmp(name, "Status"))        { db_w_variant(m, "s"); db_w_str(m, "normal"); return 1; }
    if (!strcmp(name, "TextDirection")) { db_w_variant(m, "s"); db_w_str(m, "ltr"); return 1; }
    if (!strcmp(name, "IconThemePath")) { db_w_variant(m, "as"); db_w_arr_open(m, 4, &a); db_w_arr_close(m, &a); return 1; }
    return 0;
}

static const char INTROSPECT_ITEM[] =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" "
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n<node>"
    "<interface name=\"org.kde.StatusNotifierItem\">"
    "<property name=\"Category\" type=\"s\" access=\"read\"/><property name=\"Id\" type=\"s\" access=\"read\"/>"
    "<property name=\"Title\" type=\"s\" access=\"read\"/><property name=\"Status\" type=\"s\" access=\"read\"/>"
    "<property name=\"WindowId\" type=\"i\" access=\"read\"/><property name=\"IconName\" type=\"s\" access=\"read\"/>"
    "<property name=\"IconThemePath\" type=\"s\" access=\"read\"/><property name=\"IconPixmap\" type=\"a(iiay)\" access=\"read\"/>"
    "<property name=\"OverlayIconName\" type=\"s\" access=\"read\"/><property name=\"OverlayIconPixmap\" type=\"a(iiay)\" access=\"read\"/>"
    "<property name=\"AttentionIconName\" type=\"s\" access=\"read\"/><property name=\"AttentionIconPixmap\" type=\"a(iiay)\" access=\"read\"/>"
    "<property name=\"AttentionMovieName\" type=\"s\" access=\"read\"/><property name=\"ToolTip\" type=\"(sa(iiay)ss)\" access=\"read\"/>"
    "<property name=\"ItemIsMenu\" type=\"b\" access=\"read\"/><property name=\"Menu\" type=\"o\" access=\"read\"/>"
    "<method name=\"ContextMenu\"><arg name=\"x\" type=\"i\" direction=\"in\"/><arg name=\"y\" type=\"i\" direction=\"in\"/></method>"
    "<method name=\"Activate\"><arg name=\"x\" type=\"i\" direction=\"in\"/><arg name=\"y\" type=\"i\" direction=\"in\"/></method>"
    "<method name=\"SecondaryActivate\"><arg name=\"x\" type=\"i\" direction=\"in\"/><arg name=\"y\" type=\"i\" direction=\"in\"/></method>"
    "<method name=\"Scroll\"><arg name=\"delta\" type=\"i\" direction=\"in\"/><arg name=\"orientation\" type=\"s\" direction=\"in\"/></method>"
    "<method name=\"ProvideXdgActivationToken\"><arg name=\"token\" type=\"s\" direction=\"in\"/></method>"
    "<signal name=\"NewTitle\"/><signal name=\"NewIcon\"/><signal name=\"NewAttentionIcon\"/><signal name=\"NewOverlayIcon\"/>"
    "<signal name=\"NewToolTip\"/><signal name=\"NewStatus\"><arg name=\"status\" type=\"s\"/></signal>"
    "</interface>"
    "<interface name=\"org.freedesktop.DBus.Properties\">"
    "<method name=\"Get\"><arg type=\"s\" direction=\"in\"/><arg type=\"s\" direction=\"in\"/><arg type=\"v\" direction=\"out\"/></method>"
    "<method name=\"GetAll\"><arg type=\"s\" direction=\"in\"/><arg type=\"a{sv}\" direction=\"out\"/></method>"
    "</interface>"
    "<interface name=\"org.freedesktop.DBus.Introspectable\"><method name=\"Introspect\"><arg type=\"s\" direction=\"out\"/></method></interface>"
    "</node>";
static const char INTROSPECT_MENU[] =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" "
    "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n<node>"
    "<interface name=\"com.canonical.dbusmenu\">"
    "<property name=\"Version\" type=\"u\" access=\"read\"/><property name=\"TextDirection\" type=\"s\" access=\"read\"/>"
    "<property name=\"Status\" type=\"s\" access=\"read\"/><property name=\"IconThemePath\" type=\"as\" access=\"read\"/>"
    "<method name=\"GetLayout\"><arg type=\"i\" name=\"parentId\" direction=\"in\"/><arg type=\"i\" name=\"recursionDepth\" direction=\"in\"/>"
    "<arg type=\"as\" name=\"propertyNames\" direction=\"in\"/><arg type=\"u\" name=\"revision\" direction=\"out\"/>"
    "<arg type=\"(ia{sv}av)\" name=\"layout\" direction=\"out\"/></method>"
    "<method name=\"GetGroupProperties\"><arg type=\"ai\" name=\"ids\" direction=\"in\"/><arg type=\"as\" name=\"propertyNames\" direction=\"in\"/>"
    "<arg type=\"a(ia{sv})\" name=\"properties\" direction=\"out\"/></method>"
    "<method name=\"GetProperty\"><arg type=\"i\" name=\"id\" direction=\"in\"/><arg type=\"s\" name=\"name\" direction=\"in\"/><arg type=\"v\" name=\"value\" direction=\"out\"/></method>"
    "<method name=\"Event\"><arg type=\"i\" name=\"id\" direction=\"in\"/><arg type=\"s\" name=\"eventId\" direction=\"in\"/>"
    "<arg type=\"v\" name=\"data\" direction=\"in\"/><arg type=\"u\" name=\"timestamp\" direction=\"in\"/></method>"
    "<method name=\"EventGroup\"><arg type=\"a(isvu)\" name=\"events\" direction=\"in\"/><arg type=\"ai\" name=\"idErrors\" direction=\"out\"/></method>"
    "<method name=\"AboutToShow\"><arg type=\"i\" name=\"id\" direction=\"in\"/><arg type=\"b\" name=\"needUpdate\" direction=\"out\"/></method>"
    "<method name=\"AboutToShowGroup\"><arg type=\"ai\" name=\"ids\" direction=\"in\"/><arg type=\"ai\" name=\"updatesNeeded\" direction=\"out\"/><arg type=\"ai\" name=\"idErrors\" direction=\"out\"/></method>"
    "<signal name=\"ItemsPropertiesUpdated\"><arg type=\"a(ia{sv})\"/><arg type=\"a(ias)\"/></signal>"
    "<signal name=\"LayoutUpdated\"><arg type=\"u\" name=\"revision\"/><arg type=\"i\" name=\"parent\"/></signal>"
    "<signal name=\"ItemActivationRequested\"><arg type=\"i\"/><arg type=\"u\"/></signal>"
    "</interface>"
    "<interface name=\"org.freedesktop.DBus.Properties\">"
    "<method name=\"Get\"><arg type=\"s\" direction=\"in\"/><arg type=\"s\" direction=\"in\"/><arg type=\"v\" direction=\"out\"/></method>"
    "<method name=\"GetAll\"><arg type=\"s\" direction=\"in\"/><arg type=\"a{sv}\" direction=\"out\"/></method>"
    "</interface>"
    "<interface name=\"org.freedesktop.DBus.Introspectable\"><method name=\"Introspect\"><arg type=\"s\" direction=\"out\"/></method></interface>"
    "</node>";

/* ── 클릭 ── */
static void on_click(int id)
{
    switch (cf_app_click(&app, id)) {
    case CF_ACT_START: conf_save(); job_start(); break;
    case CF_ACT_STOP:  conf_save(); job_stop();  break;
    case CF_ACT_MENU:  conf_save(); emit_layout_updated(); break;
    case CF_ACT_QUIT:  quit = 1; break;
    default: break;
    }
}

/* ── 메서드 디스패치 ── */
static void reply_empty(DbConn *c, const DbRead *req)
{
    DbMsg m;
    if (req->flags & DB_FLAG_NO_REPLY) return;
    db_reply_init(&m, req, NULL);
    db_send(c, &m);
}
static void handle_properties(DbConn *c, DbRead *r)
{
    const char *iface = db_r_str(r);
    int is_sni = !strcmp(iface, SNI_IFACE), is_menu = !strcmp(iface, MENU_IFACE);
    DbMsg m;
    if (!strcmp(r->member, "Get")) {
        const char *name = db_r_str(r);
        db_reply_init(&m, r, "v");
        if ((is_sni && write_sni_prop(&m, name)) || (is_menu && write_menu_prop(&m, name))) { db_send(c, &m); return; }
        db_msg_free(&m);
        db_reply_error(c, r, "org.freedesktop.DBus.Error.InvalidArgs", "No such property");
        return;
    }
    if (!strcmp(r->member, "GetAll")) {
        DbArr a;
        const char *const *names = is_sni ? SNI_PROPS : is_menu ? MENU_PROPS : NULL;
        db_reply_init(&m, r, "a{sv}");
        db_w_arr_open(&m, 8, &a);
        for (; names && *names; names++) {
            db_w_struct(&m); db_w_str(&m, *names);
            if (is_sni) write_sni_prop(&m, *names); else write_menu_prop(&m, *names);
        }
        db_w_arr_close(&m, &a);
        db_send(c, &m);
        return;
    }
    db_reply_error(c, r, "org.freedesktop.DBus.Error.PropertyReadOnly", "Read-only");
}
static void handle_menu(DbConn *c, DbRead *r)
{
    DbMsg m;
    if (!strcmp(r->member, "GetLayout")) {
        i32 parent = db_r_i32(r), depth = db_r_i32(r);
        db_reply_init(&m, r, "u(ia{sv}av)");
        db_w_u32(&m, menu_rev);
        write_node(&m, parent, depth);
        db_send(c, &m);
    } else if (!strcmp(r->member, "GetGroupProperties")) {
        DbArr a;
        u32 end = db_r_arr_open(r, 4);
        int ids[64], n = 0;
        while (r->pos < end && n < 64) ids[n++] = db_r_i32(r);
        db_reply_init(&m, r, "a(ia{sv})");
        db_w_arr_open(&m, 8, &a);
        for (int i = 0; i < n; i++) { db_w_struct(&m); db_w_i32(&m, ids[i]); write_item_props(&m, ids[i]); }
        db_w_arr_close(&m, &a);
        db_send(c, &m);
    } else if (!strcmp(r->member, "GetProperty")) {
        i32 id = db_r_i32(r);
        const char *name = db_r_str(r);
        CfMenuItem it;
        if (!cf_menu_item(&app, id, &it)) { db_reply_error(c, r, "org.freedesktop.DBus.Error.InvalidArgs", "No such item"); return; }
        db_reply_init(&m, r, "v");
        if (!strcmp(name, "label"))             { db_w_variant(&m, "s"); db_w_str(&m, it.label); }
        else if (!strcmp(name, "enabled"))      { db_w_variant(&m, "b"); db_w_bool(&m, it.enabled); }
        else if (!strcmp(name, "visible"))      { db_w_variant(&m, "b"); db_w_bool(&m, 1); }
        else if (!strcmp(name, "toggle-state")) { db_w_variant(&m, "i"); db_w_i32(&m, it.checked); }
        else if (!strcmp(name, "toggle-type"))  { db_w_variant(&m, "s"); db_w_str(&m, it.kind == CF_KIND_RADIO ? "radio" : it.kind == CF_KIND_CHECK ? "checkmark" : ""); }
        else if (!strcmp(name, "type"))         { db_w_variant(&m, "s"); db_w_str(&m, it.kind == CF_KIND_SEPARATOR ? "separator" : "standard"); }
        else if (!strcmp(name, "children-display")) { db_w_variant(&m, "s"); db_w_str(&m, it.kind == CF_KIND_SUBMENU ? "submenu" : ""); }
        else { db_msg_free(&m); db_reply_error(c, r, "org.freedesktop.DBus.Error.InvalidArgs", "No such property"); return; }
        db_send(c, &m);
    } else if (!strcmp(r->member, "Event")) {
        i32 id = db_r_i32(r);
        const char *ev = db_r_str(r);
        reply_empty(c, r);
        if (!strcmp(ev, "clicked")) on_click(id);
    } else if (!strcmp(r->member, "EventGroup")) {
        DbArr a;
        u32 end = db_r_arr_open(r, 8);
        int clicked[16], n = 0;
        while (r->pos < end && db_r_ok(r)) {
            i32 id; const char *ev, *vs;
            db_r_struct(r);
            id = db_r_i32(r); ev = db_r_str(r); vs = db_r_variant(r); db_r_skip(r, &vs); db_r_u32(r);
            if (!strcmp(ev, "clicked") && n < 16) clicked[n++] = id;
        }
        db_reply_init(&m, r, "ai");
        db_w_arr_open(&m, 4, &a); db_w_arr_close(&m, &a);
        db_send(c, &m);
        for (int i = 0; i < n; i++) on_click(clicked[i]);
    } else if (!strcmp(r->member, "AboutToShow")) {
        db_reply_init(&m, r, "b"); db_w_bool(&m, 0); db_send(c, &m);
    } else if (!strcmp(r->member, "AboutToShowGroup")) {
        DbArr a;
        db_reply_init(&m, r, "aiai");
        db_w_arr_open(&m, 4, &a); db_w_arr_close(&m, &a);
        db_w_arr_open(&m, 4, &a); db_w_arr_close(&m, &a);
        db_send(c, &m);
    } else {
        db_reply_error(c, r, "org.freedesktop.DBus.Error.UnknownMethod", "Unknown method");
    }
}

static void register_watcher(void)
{
    DbMsg m; DbRead r;
    db_call_init(&m, WATCHER, "/StatusNotifierWatcher", WATCHER, "RegisterStatusNotifierItem", "s");
    db_w_str(&m, busname);
    if (db_call(&ses, &m, &r, 3000, NULL, NULL)) {
        if (r.type == DB_ERROR) fprintf(stderr, "nexa-coffee: no StatusNotifierWatcher (%s) — tray hidden until a host appears\n", r.error ? r.error : "?");
        db_consume(&ses);
    }
}

static void handle(DbConn *c, DbRead *r, void *ud)
{
    (void)ud;
    if (r->type == DB_METHOD_RETURN) {
        if (ss_serial && r->reply_serial == ss_serial) {
            ss_cookie = db_r_u32(r); ss_serial = 0;
            if (!app.running) inhibit(0); /* 이미 멈췄으면 즉시 해제 */
        }
        return;
    }
    if (r->type == DB_ERROR) { if (ss_serial && r->reply_serial == ss_serial) ss_serial = 0; return; }
    if (r->type == DB_SIGNAL) {
        if (r->member && r->iface && !strcmp(r->iface, "org.freedesktop.DBus") && !strcmp(r->member, "NameOwnerChanged")) {
            const char *name = db_r_str(r), *old = db_r_str(r), *neu = db_r_str(r);
            (void)old;
            if (!strcmp(name, WATCHER) && *neu) register_watcher();
        }
        return;
    }
    if (r->type != DB_METHOD_CALL || !r->member) return;
    if (r->iface && !strcmp(r->iface, "org.freedesktop.DBus.Introspectable")) {
        DbMsg m;
        db_reply_init(&m, r, "s");
        db_w_str(&m, r->path && !strcmp(r->path, MENU_PATH) ? INTROSPECT_MENU : INTROSPECT_ITEM);
        db_send(c, &m);
        return;
    }
    if (r->iface && !strcmp(r->iface, "org.freedesktop.DBus.Peer")) { reply_empty(c, r); return; }
    if (r->iface && !strcmp(r->iface, PROPS_IFACE)) { handle_properties(c, r); return; }
    if (r->path && !strcmp(r->path, MENU_PATH)) { handle_menu(c, r); return; }
    if (r->iface && !strcmp(r->iface, SNI_IFACE)) { reply_empty(c, r); return; } /* Activate 등 — 메뉴는 셸이 그린다 */
    db_reply_error(c, r, "org.freedesktop.DBus.Error.UnknownMethod", "Unknown method");
}

static void on_signal(int s) { (void)s; quit = 1; }

int main(void)
{
    const char *lc = getenv("LC_ALL");
    DbMsg m; DbRead r;
    struct sigaction sa;
    if (!lc || !*lc) lc = getenv("LC_MESSAGES");
    if (!lc || !*lc) lc = getenv("LANG");
    cf_app_init(&app, lc && !strncmp(lc, "ko", 2) ? CF_LANG_KO : CF_LANG_EN);
    conf_init();
    conf_load();
    app.running = 0;

    memset(&sa, 0, sizeof sa); sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    if (db_connect_session(&ses) != 0) { fprintf(stderr, "nexa-coffee: cannot connect to the session bus\n"); return 1; }
    has_sys = db_connect_system(&sysb) == 0;
    render_idle();

    /* 이름 등록(규격: org.kde.StatusNotifierItem-<pid>-1) · 워처 등록 · 워처 재등장 감시 */
    snprintf(busname, sizeof busname, "org.kde.StatusNotifierItem-%d-1", (int)getpid());
    db_call_init(&m, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName", "su");
    db_w_str(&m, busname); db_w_u32(&m, 0);
    if (db_call(&ses, &m, &r, 3000, handle, NULL)) { if (r.type == DB_ERROR) strcpy(busname, ses.unique); db_consume(&ses); }
    db_call_init(&m, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "AddMatch", "s");
    db_w_str(&m, "type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='" WATCHER "'");
    m.buf[2] |= DB_FLAG_NO_REPLY;
    db_send(&ses, &m);
    register_watcher();

    if (app.auto_start && app.sel != CF_SEL_OFF) { app.running = 1; job_start(); }

    while (!quit) {
        struct pollfd p[2] = { { ses.fd, POLLIN, 0 }, { has_sys ? sysb.fd : -1, POLLIN, 0 } };
        int timeout = -1, st;
        if (app.running && next_tick) {
            i64 d = next_tick - now_ms();
            timeout = d < 0 ? 0 : (d > 3600000 ? 3600000 : (int)d);
        }
        st = poll(p, 2, timeout);
        if (st < 0) continue; /* EINTR */
        if (p[0].revents) {
            int got;
            while ((got = db_recv(&ses, &r, 0)) == 1) { handle(&ses, &r, NULL); db_consume(&ses); }
            if (got < 0) { fprintf(stderr, "nexa-coffee: session bus disconnected\n"); break; }
        }
        if (has_sys && p[1].revents) {
            int got = db_recv(&sysb, &r, 0);
            if (got == 1) db_consume(&sysb);
            else if (got < 0) { db_close(&sysb); has_sys = 0; login_fd = -1; }
        }
        if (app.running && next_tick && now_ms() >= next_tick) job_tick();
    }
    inhibit(0);
    db_close(&ses);
    if (has_sys) db_close(&sysb);
    return 0;
}
