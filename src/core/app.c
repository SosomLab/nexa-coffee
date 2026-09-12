/* app.c — 앱 상태 · 메뉴 모델(3-OS 공통 트리) · 클릭/입력 창 처리 · 설정 직렬화 · 문구(ko/en) · About.
 *
 * 메뉴(사용자 확정 09-12 2차):
 *   무제한 · 12시간 · 6시간 · 2시간 · 1시간 · 30분 · 사용자 지정… · 끄기
 *   ─ 실행 시 자동 시작 ▸ (끄기 / 사용자 지정…) ─ 정보 · 종료
 * "사용자 지정…"과 "자동 시작 ▸ 사용자 지정…"은 같은 입력 창(일·시·분 숫자 + 시작/저장 버튼)을 쓴다.
 * 창을 그리는 것은 플랫폼 몫 — 여기서는 초기값·검증·결과 반영만.
 */
#include "coffee.h"

#ifndef CF_VERSION
#define CF_VERSION "dev"
#endif

/* ── 문구 ─────────────────────────────────────────────────── */
enum {
    S_APP = 0, S_IDLE, S_QUIT, S_AUTO, S_INF, S_12H, S_6H, S_2H, S_1H, S_30M, S_CUSTOM, S_OFF,
    S_D, S_H, S_M, S_S, S_LEFT, S_KEEPING,
    S_DLG_CUSTOM, S_DLG_AUTO, S_DAYS, S_HOURS, S_MINUTES, S_START, S_SAVE, S_CANCEL, S_ABOUT,
    S_ABOUT_DESC, S_ABOUT_LICENSE,
    S_DAY1, S_DAYN, S_HOUR1, S_HOURN, S_MIN1, S_MINN, S_SEC1, S_SECN, S_COUNT
};
static const char *const STR[2][S_COUNT] = {
    { "Nexa Coffee", "Idle", "Quit", "Start automatically at launch",
      "Unlimited", "12 hours", "6 hours", "2 hours", "1 hour", "30 minutes", "Custom\xE2\x80\xA6", "Off",
      "d", "h", "m", "s", "left", "keeping awake",
      "Custom duration", "Auto-start duration", "day(s)", "hour(s)", "minute(s)", "Start", "Save", "Cancel",
      "About Nexa Coffee\xE2\x80\xA6",
      "Keeps the computer awake from the tray.", "\xC2\xA9 2026 SosomLab \xC2\xB7 MIT License",
      " day", " days", " hour", " hours", " minute", " minutes", " second", " seconds" },
    { "Nexa Coffee", "대기 중", "종료", "실행 시 자동 시작",
      "무제한", "12시간", "6시간", "2시간", "1시간", "30분", "사용자 지정…", "끄기",
      "일", "시간", "분", "초", "남음", "절전 방지 중",
      "사용자 지정 시간", "자동 시작 시간", "일", "시간", "분", "시작", "저장", "취소",
      "Nexa Coffee 정보…",
      "트레이에서 PC가 잠들지 않게 합니다.", "© 2026 SosomLab · MIT 라이선스",
      "일", "일", "시간", "시간", "분", "분", "초", "초" },
};
static const char *s_(int lang, int id) { return STR[lang == CF_LANG_KO ? 1 : 0][id]; }

const char *cf_str(int lang, int id)
{
    switch (id) {
    case CF_STR_APP:        return s_(lang, S_APP);
    case CF_STR_IDLE:       return s_(lang, S_IDLE);
    case CF_STR_QUIT:       return s_(lang, S_QUIT);
    case CF_STR_AUTO:       return s_(lang, S_AUTO);
    case CF_STR_DLG_CUSTOM: return s_(lang, S_DLG_CUSTOM);
    case CF_STR_DLG_AUTO:   return s_(lang, S_DLG_AUTO);
    case CF_STR_DAYS:       return s_(lang, S_DAYS);
    case CF_STR_HOURS:      return s_(lang, S_HOURS);
    case CF_STR_MINUTES:    return s_(lang, S_MINUTES);
    case CF_STR_START:      return s_(lang, S_START);
    case CF_STR_SAVE:       return s_(lang, S_SAVE);
    case CF_STR_CANCEL:     return s_(lang, S_CANCEL);
    case CF_STR_ABOUT:      return s_(lang, S_ABOUT);
    case CF_STR_VERSION:    return CF_VERSION;
    default: return "";
    }
}

/* ── 상태 ─────────────────────────────────────────────────── */
void cf_app_init(CfApp *a, int lang)
{
    cf_memset(a, 0, sizeof *a);
    a->lang = lang;
    a->sel = CF_SEL_OFF;
    a->cust_h = 1;
}

static i64 dhm_secs(int d, int h, int m) { return (i64)d * 86400 + (i64)h * 3600 + (i64)m * 60; }
static i64 custom_secs(const CfApp *a) { return dhm_secs(a->cust_d, a->cust_h, a->cust_m); }
i64 cf_auto_secs(const CfApp *a) { return dhm_secs(a->auto_d, a->auto_h, a->auto_m); }

i64 cf_sel_secs(const CfApp *a)
{
    switch (a->sel) {
    case CF_SEL_INF:    return -1;
    case CF_SEL_12H:    return 12 * 3600;
    case CF_SEL_6H:     return 6 * 3600;
    case CF_SEL_2H:     return 2 * 3600;
    case CF_SEL_1H:     return 3600;
    case CF_SEL_30M:    return 1800;
    case CF_SEL_CUSTOM: return custom_secs(a);
    default:            return 0;
    }
}

static int sel_of_id(int id)
{
    switch (id) {
    case CF_ID_INF: return CF_SEL_INF;   case CF_ID_12H: return CF_SEL_12H;
    case CF_ID_6H:  return CF_SEL_6H;    case CF_ID_2H:  return CF_SEL_2H;
    case CF_ID_1H:  return CF_SEL_1H;    case CF_ID_30M: return CF_SEL_30M;
    default: return -1;
    }
}

int cf_app_click(CfApp *a, int id)
{
    int s = sel_of_id(id);
    if (s >= 0) { a->sel = s; a->running = 1; return CF_ACT_START; }
    switch (id) {
    case CF_ID_CUSTOM:      return CF_ACT_DIALOG_CUSTOM;
    case CF_ID_OFF:         a->running = 0; a->sel = CF_SEL_OFF; return CF_ACT_STOP;
    case CF_ID_AUTO_OFF:    a->auto_d = a->auto_h = a->auto_m = 0; return CF_ACT_MENU;
    case CF_ID_AUTO_CUSTOM: return CF_ACT_DIALOG_AUTO;
    case CF_ID_ABOUT:       return CF_ACT_ABOUT;
    case CF_ID_QUIT:        return CF_ACT_QUIT;
    default:                return CF_ACT_NONE;
    }
}

void cf_dialog_values(const CfApp *a, int mode, int *d, int *h, int *m)
{
    if (mode == CF_DLG_AUTO && cf_auto_secs(a) > 0) { *d = a->auto_d; *h = a->auto_h; *m = a->auto_m; }
    else { *d = a->cust_d; *h = a->cust_h; *m = a->cust_m; }
}

static int clampi(i64 v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : (int)v; }

int cf_dialog_submit(CfApp *a, int mode, i64 d, i64 h, i64 m)
{
    int cd = clampi(d, 0, CF_MAX_DAYS), ch = clampi(h, 0, 23), cm = clampi(m, 0, 59);
    if (mode == CF_DLG_AUTO) {
        a->auto_d = cd; a->auto_h = ch; a->auto_m = cm;
        return CF_ACT_MENU;
    }
    if (dhm_secs(cd, ch, cm) <= 0) return CF_ACT_NONE;
    a->cust_d = cd; a->cust_h = ch; a->cust_m = cm;
    a->sel = CF_SEL_CUSTOM; a->running = 1;
    return CF_ACT_START;
}

/* ── 메뉴 트리 ─────────────────────────────────────────────── */
int cf_menu_children(const CfApp *a, int parent, int *ids, int max)
{
    static const int root[] = { CF_ID_STATUS, CF_ID_SEP0, CF_ID_INF, CF_ID_12H, CF_ID_6H, CF_ID_2H, CF_ID_1H, CF_ID_30M,
                                CF_ID_CUSTOM, CF_ID_OFF, CF_ID_SEP1, CF_ID_AUTO, CF_ID_SEP2, CF_ID_ABOUT, CF_ID_QUIT };
    static const int autom[] = { CF_ID_AUTO_OFF, CF_ID_AUTO_CUSTOM };
    const int *src = 0;
    int n = 0, count = 0, i;
    (void)a;
    if (parent == CF_ID_ROOT) { src = root;  count = (int)(sizeof root / sizeof root[0]); }
    if (parent == CF_ID_AUTO) { src = autom; count = (int)(sizeof autom / sizeof autom[0]); }
    for (i = 0; i < count && n < max; i++) ids[n++] = src[i];
    return n;
}

/* "1일 2시간 30분" / "1d 2h 30m" — 0인 단위는 생략(전부 0이면 "0분") */
static void dhm_summary(int lang, int d, int h, int m, char *out, u32 cap)
{
    char num[8];
    int any = 0;
    out[0] = 0;
    if (d) { cf_itoa(d, num, sizeof num); cf_strcat(out, cap, num); cf_strcat(out, cap, s_(lang, S_D)); any = 1; }
    if (h) { if (any) cf_strcat(out, cap, " "); cf_itoa(h, num, sizeof num); cf_strcat(out, cap, num); cf_strcat(out, cap, s_(lang, S_H)); any = 1; }
    if (m || !any) { if (any) cf_strcat(out, cap, " "); cf_itoa(m, num, sizeof num); cf_strcat(out, cap, num); cf_strcat(out, cap, s_(lang, S_M)); }
}

static void set(CfMenuItem *o, int kind, int checked, const char *label)
{
    o->kind = kind; o->checked = checked; o->enabled = 1;
    o->label[0] = 0;
    cf_strcat(o->label, sizeof o->label, label);
}
static void paren(CfMenuItem *o, const char *inner)
{
    cf_strcat(o->label, sizeof o->label, " (");
    cf_strcat(o->label, sizeof o->label, inner);
    cf_strcat(o->label, sizeof o->label, ")");
}

int cf_menu_item(const CfApp *a, int id, CfMenuItem *out)
{
    const int L = a->lang;
    char tmp[48];
    int s = sel_of_id(id);
    cf_memset(out, 0, sizeof *out);
    out->enabled = 1;
    if (s >= 0) {
        static const int sid[] = { 0, S_INF, S_12H, S_6H, S_2H, S_1H, S_30M };
        set(out, CF_KIND_RADIO, a->running && a->sel == s, s_(L, sid[s]));
        return 1;
    }
    switch (id) {
    case CF_ID_CUSTOM:
        set(out, CF_KIND_RADIO, a->running && a->sel == CF_SEL_CUSTOM, s_(L, S_CUSTOM));
        if (a->running && a->sel == CF_SEL_CUSTOM) { dhm_summary(L, a->cust_d, a->cust_h, a->cust_m, tmp, sizeof tmp); paren(out, tmp); }
        return 1;
    case CF_ID_OFF:  set(out, CF_KIND_RADIO, !a->running, s_(L, S_OFF)); return 1;
    case CF_ID_SEP0: case CF_ID_SEP1: case CF_ID_SEP2: set(out, CF_KIND_SEPARATOR, 0, ""); return 1;
    case CF_ID_STATUS:
        set(out, CF_KIND_NORMAL, 0, "");
        cf_remaining_label(a, out->label, sizeof out->label);
        out->enabled = 0;
        return 1;
    case CF_ID_AUTO:
        set(out, CF_KIND_SUBMENU, cf_auto_secs(a) > 0, s_(L, S_AUTO));
        if (cf_auto_secs(a) > 0) { dhm_summary(L, a->auto_d, a->auto_h, a->auto_m, tmp, sizeof tmp); paren(out, tmp); }
        return 1;
    case CF_ID_AUTO_OFF:    set(out, CF_KIND_RADIO, cf_auto_secs(a) <= 0, s_(L, S_OFF)); return 1;
    case CF_ID_AUTO_CUSTOM:
        set(out, CF_KIND_RADIO, cf_auto_secs(a) > 0, s_(L, S_CUSTOM));
        if (cf_auto_secs(a) > 0) { dhm_summary(L, a->auto_d, a->auto_h, a->auto_m, tmp, sizeof tmp); paren(out, tmp); }
        return 1;
    case CF_ID_ABOUT: set(out, CF_KIND_NORMAL, 0, s_(L, S_ABOUT)); return 1;
    case CF_ID_QUIT:  set(out, CF_KIND_NORMAL, 0, s_(L, S_QUIT)); return 1;
    default: return 0;
    }
}

/* ── 설정 ─────────────────────────────────────────────────── */
static void put_dhm(char *out, u32 cap, int d, int h, int m)
{
    char num[8];
    cf_itoa(d, num, sizeof num); cf_strcat(out, cap, num); cf_strcat(out, cap, ",");
    cf_itoa(h, num, sizeof num); cf_strcat(out, cap, num); cf_strcat(out, cap, ",");
    cf_itoa(m, num, sizeof num); cf_strcat(out, cap, num);
}
u32 cf_conf_format(const CfApp *a, char *out, u32 cap)
{
    out[0] = 0;
    cf_strcat(out, cap, "auto=");   put_dhm(out, cap, a->auto_d, a->auto_h, a->auto_m);
    cf_strcat(out, cap, "\ncustom="); put_dhm(out, cap, a->cust_d, a->cust_h, a->cust_m);
    return cf_strcat(out, cap, "\n");
}

static int starts(const char *p, const char *end, const char *key, const char **rest)
{
    while (*key) { if (p >= end || *p != *key) return 0; p++; key++; }
    *rest = p;
    return 1;
}
static void parse_dhm(const char *v, int *d, int *h, int *m)
{
    const char *e;
    i64 dd = cf_atoi(v, &e), hh = 0, mm = 0;
    if (*e == ',') { hh = cf_atoi(e + 1, &e); if (*e == ',') mm = cf_atoi(e + 1, &e); }
    *d = clampi(dd, 0, CF_MAX_DAYS); *h = clampi(hh, 0, 23); *m = clampi(mm, 0, 59);
}

void cf_conf_parse(CfApp *a, const char *buf, u32 len)
{
    const char *p = buf, *end = buf + len, *v;
    while (p < end) {
        const char *nl = p;
        while (nl < end && *nl != '\n') nl++;
        if (starts(p, nl, "auto=", &v))        parse_dhm(v, &a->auto_d, &a->auto_h, &a->auto_m);
        else if (starts(p, nl, "custom=", &v)) parse_dhm(v, &a->cust_d, &a->cust_h, &a->cust_m);
        p = nl + 1;
    }
}

/* ── 툴팁 · About ─────────────────────────────────────────── */
u32 cf_tooltip(const CfApp *a, const CfDisplay *d, char *out, u32 cap)
{
    const int L = a->lang;
    char num[8];
    out[0] = 0;
    cf_strcat(out, cap, s_(L, S_APP));
    cf_strcat(out, cap, " — ");
    if (!a->running || !d) return cf_strcat(out, cap, s_(L, S_IDLE));
    if (d->unit == CF_UNIT_INF) {
        cf_strcat(out, cap, s_(L, S_INF));
        cf_strcat(out, cap, " · ");
        return cf_strcat(out, cap, s_(L, S_KEEPING));
    }
    cf_itoa(d->value, num, sizeof num);
    cf_strcat(out, cap, num);
    if (L == CF_LANG_KO) {
        static const int u[] = { S_S, S_M, S_H, S_D };
        cf_strcat(out, cap, s_(L, u[d->unit]));
        cf_strcat(out, cap, " ");
    } else {
        static const char *const u[] = { " sec ", " min ", " hour", " day" };
        cf_strcat(out, cap, u[d->unit]);
        if (d->unit >= CF_UNIT_HOUR) cf_strcat(out, cap, d->value == 1 ? " " : "s ");
    }
    return cf_strcat(out, cap, s_(L, S_LEFT));
}

static void unit(char *out, u32 cap, int lang, i64 v, int one, int many)
{
    char num[24];
    cf_itoa(v, num, sizeof num);
    cf_strcat(out, cap, num);
    cf_strcat(out, cap, s_(lang, v > 1 ? many : one)); /* 1보다 크면 복수형(사용자 확정) */
}

u32 cf_remaining_label(const CfApp *a, char *out, u32 cap)
{
    const int L = a->lang;
    i64 r = a->remaining_s < 0 ? 0 : a->remaining_s;
    out[0] = 0;
    if (!a->running) return cf_strcat(out, cap, s_(L, S_IDLE));
    if (a->sel == CF_SEL_INF) {
        cf_strcat(out, cap, s_(L, S_INF));
        cf_strcat(out, cap, " · ");
        return cf_strcat(out, cap, s_(L, S_KEEPING));
    }
    unit(out, cap, L, r / 86400, S_DAY1, S_DAYN);          cf_strcat(out, cap, " ");
    unit(out, cap, L, (r / 3600) % 24, S_HOUR1, S_HOURN);  cf_strcat(out, cap, " ");
    unit(out, cap, L, (r / 60) % 60, S_MIN1, S_MINN);      cf_strcat(out, cap, " ");
    unit(out, cap, L, r % 60, S_SEC1, S_SECN);
    return cf_strlen(out);
}

u32 cf_about(int lang, char *out, u32 cap)
{
    out[0] = 0;
    cf_strcat(out, cap, s_(lang, S_APP));
    cf_strcat(out, cap, " v");
    cf_strcat(out, cap, CF_VERSION);
    cf_strcat(out, cap, "\n");
    cf_strcat(out, cap, s_(lang, S_ABOUT_DESC));
    cf_strcat(out, cap, "\n\n");
    cf_strcat(out, cap, s_(lang, S_ABOUT_LICENSE));
    cf_strcat(out, cap, "\nhttps://github.com/SosomLab/nexa-coffee");
    return cf_strlen(out);
}
