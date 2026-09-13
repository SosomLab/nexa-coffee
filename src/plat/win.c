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
 * - 입력 창(일·시·분 + 시작/저장) = 리소스 DIALOGEX(IDD_DHM) · DialogBoxParamW. About = MessageBoxW.
 */
#include <windows.h>
#include <shellapi.h>
#include "../core/coffee.h"
#include "../../res/resource.h"

#define WM_TRAYICON (WM_USER + 1)
#define TIMER_TICK  1
#define TIMER_MENU  2 /* 메뉴가 열린 동안 남은 시간 1초 갱신 */
#define ICON_UID    1

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
static HMENU           g_menu;     /* 열려 있는 팝업 메뉴(없으면 NULL) */

static void to_wide(const char *utf8, WCHAR *out, int cap);

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
        u8 *buf = xalloc((u32)(s * s * 4));
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
    buf = xalloc((u32)(s * s * 4));
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
        to_wide(it.label, w, 64);
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

static void refresh_remaining(void)
{
    g_app.remaining_s = g_deadline ? (g_deadline - (i64)GetTickCount64() + 999) / 1000 : 0;
}

/* 열린 메뉴의 맨 위 항목 글자를 바꾸고 팝업 창(#32768)을 다시 그린다 */
static void menu_tick(void)
{
    char label[96];
    WCHAR w[96];
    MENUITEMINFOW mii;
    HWND popup;
    if (!g_menu) return;
    refresh_remaining();
    cf_remaining_label(&g_app, label, sizeof label);
    to_wide(label, w, 96);
    cf_memset(&mii, 0, sizeof mii);
    mii.cbSize = sizeof mii;
    mii.fMask = MIIM_STRING;
    mii.dwTypeData = w;
    SetMenuItemInfoW(g_menu, CF_ID_STATUS, FALSE, &mii);
    popup = FindWindowW(L"#32768", NULL);
    if (popup) RedrawWindow(popup, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
}

static void show_menu(void)
{
    POINT pt;
    HMENU m;
    refresh_remaining();
    m = build_menu(CF_ID_ROOT);
    g_menu = m;
    SetTimer(g_hwnd, TIMER_MENU, 1000, NULL);
    GetCursorPos(&pt);
    SetForegroundWindow(g_hwnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_hwnd, NULL);
    PostMessageW(g_hwnd, WM_NULL, 0, 0);
    KillTimer(g_hwnd, TIMER_MENU);
    g_menu = NULL;
    DestroyMenu(m);
}

static void to_wide(const char *utf8, WCHAR *out, int cap) { MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, cap); }

static void act(int action);

/* ── 입력 창 ── */
static int g_dlg_mode;
static i64 g_dlg_v[3];

static INT_PTR CALLBACK dlg_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    WCHAR w[64];
    int v[3];
    switch (msg) {
    case WM_INITDIALOG:
        g_dlg_mode = (int)lp;
        cf_dialog_values(&g_app, g_dlg_mode, &v[0], &v[1], &v[2]);
        to_wide(cf_str(g_app.lang, g_dlg_mode == CF_DLG_AUTO ? CF_STR_DLG_AUTO : CF_STR_DLG_CUSTOM), w, 64); SetWindowTextW(h, w);
        to_wide(cf_str(g_app.lang, CF_STR_DAYS), w, 64);    SetDlgItemTextW(h, IDC_LD, w);
        to_wide(cf_str(g_app.lang, CF_STR_HOURS), w, 64);   SetDlgItemTextW(h, IDC_LH, w);
        to_wide(cf_str(g_app.lang, CF_STR_MINUTES), w, 64); SetDlgItemTextW(h, IDC_LM, w);
        to_wide(cf_str(g_app.lang, g_dlg_mode == CF_DLG_AUTO ? CF_STR_SAVE : CF_STR_START), w, 64); SetDlgItemTextW(h, IDOK, w);
        to_wide(cf_str(g_app.lang, CF_STR_CANCEL), w, 64);  SetDlgItemTextW(h, IDCANCEL, w);
        SetDlgItemInt(h, IDC_D, (UINT)v[0], FALSE);
        SetDlgItemInt(h, IDC_H, (UINT)v[1], FALSE);
        SetDlgItemInt(h, IDC_M, (UINT)v[2], FALSE);
        SendDlgItemMessageW(h, IDC_D, EM_LIMITTEXT, 3, 0);
        SendDlgItemMessageW(h, IDC_H, EM_LIMITTEXT, 2, 0);
        SendDlgItemMessageW(h, IDC_M, EM_LIMITTEXT, 2, 0);
        SendMessageW(h, WM_SETICON, ICON_SMALL, (LPARAM)g_wc.hIcon);
        SetForegroundWindow(h);
        return TRUE; /* 첫 필드에 포커스 */
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            g_dlg_v[0] = GetDlgItemInt(h, IDC_D, NULL, FALSE);
            g_dlg_v[1] = GetDlgItemInt(h, IDC_H, NULL, FALSE);
            g_dlg_v[2] = GetDlgItemInt(h, IDC_M, NULL, FALSE);
            EndDialog(h, 1);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) { EndDialog(h, 0); return TRUE; }
        return FALSE;
    case WM_CLOSE:
        EndDialog(h, 0);
        return TRUE;
    default:
        return FALSE;
    }
}

static void show_dialog(int mode)
{
    if (DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_DHM), g_hwnd, dlg_proc, (LPARAM)mode) == 1)
        act(cf_dialog_submit(&g_app, mode, g_dlg_v[0], g_dlg_v[1], g_dlg_v[2]));
}

static void show_about(void)
{
    char body[512];
    WCHAR wb[512], wt[64];
    cf_about(g_app.lang, body, sizeof body);
    to_wide(body, wb, 512);
    to_wide(cf_str(g_app.lang, CF_STR_APP), wt, 64);
    MessageBoxW(g_hwnd, wb, wt, MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
}

static void act(int action)
{
    switch (action) {
    case CF_ACT_START: conf_save(); job_start(); break;
    case CF_ACT_STOP:  conf_save(); job_stop();  break;
    case CF_ACT_MENU:  conf_save(); break;
    case CF_ACT_LANG:  conf_save(); if (g_app.running) job_tick(); else show_idle(FALSE); break; /* 툴팁 문구 갱신 · 메뉴는 열 때 새로 만든다 */
    case CF_ACT_QUIT:  DestroyWindow(g_hwnd); break;
    case CF_ACT_DIALOG_CUSTOM: show_dialog(CF_DLG_CUSTOM); break;
    case CF_ACT_DIALOG_AUTO:   show_dialog(CF_DLG_AUTO); break;
    case CF_ACT_ABOUT: show_about(); break;
    default: break;
    }
}

static void on_command(int id) { act(cf_app_click(&g_app, id)); }

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
        else if (wp == TIMER_MENU) menu_tick();
        return 0;
    case WM_COMMAND:
        on_command((int)LOWORD(wp));
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_TICK);
        KillTimer(hwnd, TIMER_MENU);
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
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_KOREAN:   lang = CF_LANG_KO; break;
    case LANG_JAPANESE: lang = CF_LANG_JA; break;
    case LANG_CHINESE:  lang = CF_LANG_ZH; break;
    default:            lang = CF_LANG_EN; break;
    }
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
    /* 초기화가 끌어온 페이지를 OS에 돌려준다 — 대기 상태의 작업 집합 최소화 */
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    /* 첫 실행 자동 시작(사용자 확정): 옵션 켜짐 + 시간 지정 */
    if (cf_auto_secs(&g_app) > 0) {
        g_app.cust_d = g_app.auto_d; g_app.cust_h = g_app.auto_h; g_app.cust_m = g_app.auto_m;
        g_app.sel = CF_SEL_CUSTOM; g_app.running = 1; job_start();
    }

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    ExitProcess(0);
}
