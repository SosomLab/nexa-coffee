/*
 * win.c — Windows 플랫폼 계층. 순수 Win32 · CRT 미링크(nexa-shortcut nShiftSpace 원칙 계승).
 *
 * - 라이브러리: kernel32 · user32 · shell32 · gdi32 만. 진입점 start(링커 -e start / /ENTRY:start).
 * - 트레이 = Shell_NotifyIcon. 아이콘 = 코어 RGBA → 32bpp DIB(premultiplied BGRA) → CreateIconIndirect.
 *   크기 = SM_CXSMICON(DPI 반영 · Per-Monitor V2를 동적으로 켠다).
 * - 절전 방지 = SetThreadExecutionState(ES_CONTINUOUS|ES_SYSTEM_REQUIRED|ES_DISPLAY_REQUIRED)
 *   → 유휴 절전·최대 절전·화면 끄기·화면보호기 진입을 막는다.
 *   ⚠️ 덮개 닫힘 동작은 전원 정책(powercfg LIDACTION · 관리자)이라 앱이 바꾸지 않는다.
 * - 작업 종료 시: 실행 상태 해제 · 타이머 제거 · 동작 아이콘 파괴 · SetProcessWorkingSetSize(-1,-1)로
 *   작업 집합을 OS에 돌려준다(사용자 요청 09-12 "모든 메모리 회수").
 */
#include <windows.h>
#include <shellapi.h>
#include "../core/coffee.h"

#define WM_TRAYICON (WM_USER + 1)
#define TIMER_TICK  1
#define ICON_UID    1
#define IDI_APP     1 /* res/nexa-coffee.rc */

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE)-4)
#endif

/* CRT 대체 — 컴파일러가 구조체 초기화/복사에 memset·memcpy 호출을 만들 수 있다 */
#if defined(_MSC_VER)
#pragma function(memset, memcpy)
int _fltused = 0;
#endif
void *memset(void *d, int c, size_t n) { cf_memset(d, c, (u32)n); return d; }
void *memcpy(void *d, const void *s, size_t n) { cf_memcpy(d, s, (u32)n); return d; }

static NOTIFYICONDATAW g_nid;
static WNDCLASSW       g_wc;
static UINT            g_taskbar_created;
static HWND            g_hwnd;
static CfApp           g_app;
static i64             g_deadline; /* ms(GetTickCount64) · 0 = 무제한 */
static i64             g_total;
static HICON           g_icon;     /* 현재 트레이 아이콘 */
static HICON           g_idle;     /* 대기 아이콘(재사용) */
static int             g_idle_size;
static WCHAR           g_conf[MAX_PATH];

/* ── 메모리 ── */
static void *xalloc(u32 n) { return HeapAlloc(GetProcessHeap(), 0, n); }
static void  xfree(void *p) { if (p) HeapFree(GetProcessHeap(), 0, p); }

/* ── 설정 ── */
static void conf_init(void)
{
    WCHAR dir[MAX_PATH];
    if (!GetEnvironmentVariableW(L"APPDATA", dir, MAX_PATH)) return;
    lstrcatW(dir, L"\\nexa-coffee");
    CreateDirectoryW(dir, NULL);
    lstrcpyW(g_conf, dir);
    lstrcatW(g_conf, L"\\config");
}
static void conf_load(void)
{
    char buf[256];
    DWORD n = 0;
    HANDLE h = CreateFileW(g_conf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    if (ReadFile(h, buf, sizeof buf - 1, &n, NULL) && n > 0) cf_conf_parse(&g_app, buf, n);
    CloseHandle(h);
}
static void conf_save(void)
{
    char buf[256];
    DWORD w;
    u32 n = cf_conf_format(&g_app, buf, sizeof buf);
    HANDLE h = CreateFileW(g_conf, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, buf, n, &w, NULL);
    CloseHandle(h);
}

/* ── 아이콘 ── */
static int icon_size(void)
{
    int s = GetSystemMetrics(SM_CXSMICON);
    if (s < 16) s = 16;
    if (s > CF_ICON_MAX) s = CF_ICON_MAX;
    return s;
}

static HICON icon_from_rgba(const u8 *rgba, int size)
{
    BITMAPV5HEADER bi;
    void *bits = NULL;
    HDC dc;
    HBITMAP color, mask;
    ICONINFO ii;
    HICON h;
    u32 i, n = (u32)(size * size);

    cf_memset(&bi, 0, sizeof bi);
    bi.bV5Size = sizeof bi;
    bi.bV5Width = size;
    bi.bV5Height = -size; /* top-down */
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000; bi.bV5GreenMask = 0x0000FF00; bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;
    dc = GetDC(NULL);
    color = CreateDIBSection(dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, dc);
    if (!color || !bits) return NULL;
    for (i = 0; i < n; i++) {
        const u8 *p = rgba + i * 4;
        u32 a = p[3];
        ((u32 *)bits)[i] = (a << 24) | ((p[0] * a / 255) << 16) | ((p[1] * a / 255) << 8) | (p[2] * a / 255);
    }
    mask = CreateBitmap(size, size, 1, 1, NULL);
    ii.fIcon = TRUE; ii.xHotspot = 0; ii.yHotspot = 0; ii.hbmMask = mask; ii.hbmColor = color;
    h = CreateIconIndirect(&ii);
    DeleteObject(color);
    DeleteObject(mask);
    return h;
}

static void tray_set(HICON icon, const char *tip_utf8, BOOL add)
{
    HICON old = g_icon;
    g_icon = icon;
    g_nid.cbSize = sizeof g_nid;
    g_nid.hWnd = g_hwnd;
    g_nid.uID = ICON_UID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = g_icon;
    MultiByteToWideChar(CP_UTF8, 0, tip_utf8, -1, g_nid.szTip, sizeof g_nid.szTip / sizeof g_nid.szTip[0]);
    Shell_NotifyIconW(add ? NIM_ADD : NIM_MODIFY, &g_nid);
    if (old && old != g_icon && old != g_idle) DestroyIcon(old);
}

static void show_idle(BOOL add)
{
    char tip[128];
    int s = icon_size();
    if (!g_idle || g_idle_size != s) {
        CfColor gray = {154, 154, 158, 255};
        u8 *buf = xalloc(CF_ICON_BYTES);
        if (g_idle) DestroyIcon(g_idle);
        cf_icon_idle(buf, s, gray);
        g_idle = icon_from_rgba(buf, s);
        g_idle_size = s;
        xfree(buf);
    }
    cf_tooltip(&g_app, NULL, tip, sizeof tip);
    tray_set(g_idle, tip, add);
}

/* ── 작업 ── */
static void job_stop(void)
{
    KillTimer(g_hwnd, TIMER_TICK);
    SetThreadExecutionState(ES_CONTINUOUS);
    g_app.running = 0;
    show_idle(FALSE);
    /* 작업이 쓴 메모리 회수 — 동작 아이콘은 tray_set에서 파괴됐다 · 작업 집합을 OS에 반납 */
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

static void job_tick(void)
{
    CfDisplay d;
    char tip[128];
    u8 *buf;
    int s = icon_size();
    i64 rem_ms = g_deadline ? g_deadline - (i64)GetTickCount64() : 0;
    if (g_deadline && rem_ms <= 0) { job_stop(); return; }
    cf_display(g_deadline ? (rem_ms + 999) / 1000 : 0, g_total, &d);
    buf = xalloc(CF_ICON_BYTES);
    cf_icon_active(buf, s, &d);
    cf_tooltip(&g_app, &d, tip, sizeof tip);
    tray_set(icon_from_rgba(buf, s), tip, FALSE);
    xfree(buf);
    SetTimer(g_hwnd, TIMER_TICK, (UINT)cf_next_change_ms(rem_ms, g_total), NULL);
}

static void job_start(void)
{
    i64 secs = cf_sel_secs(&g_app);
    g_total = secs;
    g_deadline = secs > 0 ? (i64)GetTickCount64() + secs * 1000 : 0;
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
    job_tick();
}

/* ── 메뉴(코어 트리 → HMENU · 열 때마다 새로 만든다) ── */
static HMENU build_menu(int parent)
{
    int ids[CF_MENU_MAX_CHILDREN];
    int n = cf_menu_children(&g_app, parent, ids, CF_MENU_MAX_CHILDREN), i;
    HMENU m = CreatePopupMenu();
    for (i = 0; i < n; i++) {
        CfMenuItem it;
        WCHAR w[64];
        UINT fl;
        if (!cf_menu_item(&g_app, ids[i], &it)) continue;
        if (it.kind == CF_KIND_SEPARATOR) { AppendMenuW(m, MF_SEPARATOR, 0, NULL); continue; }
        MultiByteToWideChar(CP_UTF8, 0, it.label, -1, w, 64);
        fl = MF_STRING | (it.checked ? MF_CHECKED : 0) | (it.enabled ? 0 : MF_GRAYED);
        if (it.kind == CF_KIND_SUBMENU) AppendMenuW(m, fl | MF_POPUP, (UINT_PTR)build_menu(ids[i]), w);
        else {
            AppendMenuW(m, fl, (UINT_PTR)ids[i], w);
            if (it.kind == CF_KIND_RADIO && it.checked) /* 라디오 점 표시 */
                CheckMenuRadioItem(m, (UINT)ids[i], (UINT)ids[i], (UINT)ids[i], MF_BYCOMMAND);
        }
    }
    return m;
}

static void show_menu(void)
{
    POINT pt;
    HMENU m = build_menu(CF_ID_ROOT);
    GetCursorPos(&pt);
    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_hwnd, NULL);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    DestroyMenu(m);
}

static void on_command(int id)
{
    switch (cf_app_click(&g_app, id)) {
    case CF_ACT_START: conf_save(); job_start(); break;
    case CF_ACT_STOP:  conf_save(); job_stop();  break;
    case CF_ACT_MENU:  conf_save(); break;
    case CF_ACT_QUIT:  DestroyWindow(g_hwnd); break;
    default: break;
    }
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == g_taskbar_created) { /* 탐색기 재시작 → 트레이 복구 */
        if (g_app.running) { tray_set(g_icon, "", TRUE); job_tick(); }
        else show_idle(TRUE);
        return 0;
    }
    switch (msg) {
    case WM_TRAYICON:
        if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_LBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU)
            show_menu();
        return 0;
    case WM_TIMER:
        if (wp == TIMER_TICK && g_app.running) job_tick();
        return 0;
    case WM_COMMAND:
        on_command((int)LOWORD(wp));
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_TICK);
        SetThreadExecutionState(ES_CONTINUOUS);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

static void enable_dpi(void)
{
    HMODULE u = GetModuleHandleW(L"user32.dll");
    typedef BOOL (WINAPI *SetCtx)(HANDLE);
    typedef BOOL (WINAPI *SetAware)(void);
    SetCtx f = u ? (SetCtx)(void *)GetProcAddress(u, "SetProcessDpiAwarenessContext") : NULL;
    if (f) { f(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); return; }
    { SetAware g = u ? (SetAware)(void *)GetProcAddress(u, "SetProcessDPIAware") : NULL; if (g) g(); }
}

/* CRT 없는 진입점 */
void start(void)
{
    HINSTANCE hinst = GetModuleHandleW(NULL);
    MSG msg;
    int lang;

    CreateMutexW(NULL, TRUE, L"nexa-coffee-single-instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) ExitProcess(0);

    enable_dpi();
    lang = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_KOREAN ? CF_LANG_KO : CF_LANG_EN;
    cf_app_init(&g_app, lang);
    conf_init();
    conf_load();
    g_app.running = 0;

    g_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
    g_wc.lpfnWndProc = wnd_proc;
    g_wc.hInstance = hinst;
    g_wc.lpszClassName = L"NexaCoffee";
    g_wc.hIcon = LoadIconW(hinst, MAKEINTRESOURCEW(IDI_APP));
    RegisterClassW(&g_wc);
    g_hwnd = CreateWindowExW(0, L"NexaCoffee", L"Nexa Coffee", 0, 0, 0, 0, 0, NULL, NULL, hinst, NULL);

    show_idle(TRUE);
    /* 첫 실행 자동 시작(사용자 확정): 옵션 켜짐 + 시간 지정 */
    if (g_app.auto_start && g_app.sel != CF_SEL_OFF) { g_app.running = 1; job_start(); }

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    ExitProcess(0);
}
