/* app.c — 앱 상태 · 메뉴 모델(3-OS 공통 트리) · 클릭 처리 · 설정 직렬화 · 문구(ko/en).
 *
 * 메뉴(사용자 확정 09-12): 무제한 · 12시간 · 6시간 · 2시간 · 1시간 · 30분 · 사용자 지정 ▸ · 끄기
 *                          ─ 실행 시 자동 시작 ─ 종료
 * 사용자 지정 ▸ : 시작(요약) ─ 일 ▸(0~7) · 시간 ▸(0~23) · 분 ▸(0~55 · 5분 단위)
 * 창을 띄우지 않는다 — 메뉴만으로 값을 정한다(극단적 최소화 · UI 코드 0).
 */
#include "coffee.h"

/* ── 문구 ─────────────────────────────────────────────────── */
enum {
    S_APP = 0, S_IDLE, S_QUIT, S_AUTO, S_INF, S_12H, S_6H, S_2H, S_1H, S_30M, S_CUSTOM, S_OFF,
    S_START, S_DAYS, S_HOURS, S_MINS, S_D, S_H, S_M, S_S, S_LEFT, S_KEEPING, S_COUNT
};
static const char *const STR[2][S_COUNT] = {
    { "Nexa Coffee", "Idle", "Quit", "Start automatically at launch",
      "Unlimited", "12 hours", "6 hours", "2 hours", "1 hour", "30 minutes", "Custom", "Off",
      "Start", "Days", "Hours", "Minutes", "d", "h", "m", "s", "left", "keeping awake" },
    { "Nexa Coffee", "대기 중", "종료", "실행 시 자동 시작",
      "무제한", "12시간", "6시간", "2시간", "1시간", "30분", "사용자 지정", "끄기",
      "시작", "일", "시간", "분", "일", "시간", "분", "초", "남음", "절전 방지 중" },
};
static const char *s_(int lang, int id) { return STR[lang == CF_LANG_KO ? 1 : 0][id]; }

const char *cf_str(int lang, int id)
{
    switch (id) {
    case CF_STR_APP:  return s_(lang, S_APP);
    case CF_STR_IDLE: return s_(lang, S_IDLE);
    case CF_STR_QUIT: return s_(lang, S_QUIT);
    case CF_STR_AUTO: return s_(lang, S_AUTO);
    default: return "";
    }
}

/* ── 상태 ─────────────────────────────────────────────────── */
void cf_app_init(CfApp *a, int lang)
{
    cf_memset(a, 0, sizeof *a);
    a->lang = lang;
    a->sel = CF_SEL_OFF;
    a->cust_d = 0; a->cust_h = 1; a->cust_m = 0;
}

static i64 custom_secs(const CfApp *a)
{
    return (i64)a->cust_d * 86400 + (i64)a->cust_h * 3600 + (i64)a->cust_m * 60;
}

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
    if (id == CF_ID_CUSTOM_START) {
        if (custom_secs(a) <= 0) return CF_ACT_NONE;
        a->sel = CF_SEL_CUSTOM; a->running = 1; return CF_ACT_START;
    }
    if (id == CF_ID_OFF)  { a->running = 0; a->sel = CF_SEL_OFF; return CF_ACT_STOP; }
    if (id == CF_ID_AUTO) { a->auto_start = !a->auto_start; return CF_ACT_MENU; }
    if (id == CF_ID_QUIT) return CF_ACT_QUIT;
    if (id >= CF_ID_DAY0 && id <= CF_ID_DAY0 + CF_MAX_DAYS) { a->cust_d = id - CF_ID_DAY0; return CF_ACT_MENU; }
    if (id >= CF_ID_HOUR0 && id < CF_ID_HOUR0 + 24)          { a->cust_h = id - CF_ID_HOUR0; return CF_ACT_MENU; }
    if (id >= CF_ID_MIN0 && id < CF_ID_MIN0 + 60 / CF_MIN_STEP) { a->cust_m = (id - CF_ID_MIN0) * CF_MIN_STEP; return CF_ACT_MENU; }
    return CF_ACT_NONE;
}

/* ── 메뉴 트리 ─────────────────────────────────────────────── */
int cf_menu_children(const CfApp *a, int parent, int *ids, int max)
{
    static const int root[] = { CF_ID_INF, CF_ID_12H, CF_ID_6H, CF_ID_2H, CF_ID_1H, CF_ID_30M,
                                CF_ID_CUSTOM, CF_ID_OFF, CF_ID_SEP1, CF_ID_AUTO, CF_ID_SEP2, CF_ID_QUIT };
    static const int cust[] = { CF_ID_CUSTOM_START, CF_ID_CUSTOM_SEP, CF_ID_CUSTOM_DAYS,
                                CF_ID_CUSTOM_HOURS, CF_ID_CUSTOM_MINS };
    int n = 0, i, base = 0, count = 0;
    (void)a;
    if (parent == CF_ID_ROOT) {
        for (i = 0; i < (int)(sizeof root / sizeof root[0]) && n < max; i++) ids[n++] = root[i];
        return n;
    }
    if (parent == CF_ID_CUSTOM) {
        for (i = 0; i < (int)(sizeof cust / sizeof cust[0]) && n < max; i++) ids[n++] = cust[i];
        return n;
    }
    if (parent == CF_ID_CUSTOM_DAYS)  { base = CF_ID_DAY0;  count = CF_MAX_DAYS + 1; }
    if (parent == CF_ID_CUSTOM_HOURS) { base = CF_ID_HOUR0; count = 24; }
    if (parent == CF_ID_CUSTOM_MINS)  { base = CF_ID_MIN0;  count = 60 / CF_MIN_STEP; }
    for (i = 0; i < count && n < max; i++) ids[n++] = base + i;
    return n;
}

/* "1일 2시간 30분" / "1d 2h 30m" — 0인 단위는 생략(전부 0이면 "0분") */
static void custom_summary(const CfApp *a, char *out, u32 cap)
{
    const int ko = a->lang == CF_LANG_KO;
    char num[8];
    int any = 0;
    out[0] = 0;
    if (a->cust_d) { cf_itoa(a->cust_d, num, sizeof num); cf_strcat(out, cap, num); cf_strcat(out, cap, s_(a->lang, S_D)); any = 1; }
    if (a->cust_h) { if (any) cf_strcat(out, cap, " "); cf_itoa(a->cust_h, num, sizeof num); cf_strcat(out, cap, num); cf_strcat(out, cap, s_(a->lang, S_H)); any = 1; }
    if (a->cust_m || !any) { if (any) cf_strcat(out, cap, " "); cf_itoa(a->cust_m, num, sizeof num); cf_strcat(out, cap, num); cf_strcat(out, cap, s_(a->lang, S_M)); }
    (void)ko;
}

static void set(CfMenuItem *o, int kind, int checked, const char *label)
{
    o->kind = kind; o->checked = checked; o->enabled = 1;
    o->label[0] = 0;
    cf_strcat(o->label, sizeof o->label, label);
}

int cf_menu_item(const CfApp *a, int id, CfMenuItem *out)
{
    const int L = a->lang;
    char num[8], tmp[48];
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
        custom_summary(a, tmp, sizeof tmp);
        set(out, CF_KIND_SUBMENU, a->running && a->sel == CF_SEL_CUSTOM, s_(L, S_CUSTOM));
        cf_strcat(out->label, sizeof out->label, " (");
        cf_strcat(out->label, sizeof out->label, tmp);
        cf_strcat(out->label, sizeof out->label, ")");
        return 1;
    case CF_ID_OFF:  set(out, CF_KIND_RADIO, !a->running, s_(L, S_OFF)); return 1;
    case CF_ID_SEP1: case CF_ID_SEP2: case CF_ID_CUSTOM_SEP:
        set(out, CF_KIND_SEPARATOR, 0, ""); return 1;
    case CF_ID_AUTO: set(out, CF_KIND_CHECK, a->auto_start, s_(L, S_AUTO)); return 1;
    case CF_ID_QUIT: set(out, CF_KIND_NORMAL, 0, s_(L, S_QUIT)); return 1;
    case CF_ID_CUSTOM_START:
        custom_summary(a, tmp, sizeof tmp);
        set(out, CF_KIND_NORMAL, 0, s_(L, S_START));
        cf_strcat(out->label, sizeof out->label, " — ");
        cf_strcat(out->label, sizeof out->label, tmp);
        out->enabled = custom_secs(a) > 0;
        return 1;
    case CF_ID_CUSTOM_DAYS:
        cf_itoa(a->cust_d, num, sizeof num);
        set(out, CF_KIND_SUBMENU, 0, s_(L, S_DAYS));
        cf_strcat(out->label, sizeof out->label, " ("); cf_strcat(out->label, sizeof out->label, num); cf_strcat(out->label, sizeof out->label, ")");
        return 1;
    case CF_ID_CUSTOM_HOURS:
        cf_itoa(a->cust_h, num, sizeof num);
        set(out, CF_KIND_SUBMENU, 0, s_(L, S_HOURS));
        cf_strcat(out->label, sizeof out->label, " ("); cf_strcat(out->label, sizeof out->label, num); cf_strcat(out->label, sizeof out->label, ")");
        return 1;
    case CF_ID_CUSTOM_MINS:
        cf_itoa(a->cust_m, num, sizeof num);
        set(out, CF_KIND_SUBMENU, 0, s_(L, S_MINS));
        cf_strcat(out->label, sizeof out->label, " ("); cf_strcat(out->label, sizeof out->label, num); cf_strcat(out->label, sizeof out->label, ")");
        return 1;
    default: break;
    }
    if (id >= CF_ID_DAY0 && id <= CF_ID_DAY0 + CF_MAX_DAYS) {
        int d = id - CF_ID_DAY0;
        cf_itoa(d, num, sizeof num);
        set(out, CF_KIND_RADIO, a->cust_d == d, num);
        cf_strcat(out->label, sizeof out->label, L == CF_LANG_KO ? "일" : " d");
        return 1;
    }
    if (id >= CF_ID_HOUR0 && id < CF_ID_HOUR0 + 24) {
        int h = id - CF_ID_HOUR0;
        cf_itoa(h, num, sizeof num);
        set(out, CF_KIND_RADIO, a->cust_h == h, num);
        cf_strcat(out->label, sizeof out->label, L == CF_LANG_KO ? "시간" : " h");
        return 1;
    }
    if (id >= CF_ID_MIN0 && id < CF_ID_MIN0 + 60 / CF_MIN_STEP) {
        int m = (id - CF_ID_MIN0) * CF_MIN_STEP;
        cf_itoa(m, num, sizeof num);
        set(out, CF_KIND_RADIO, a->cust_m == m, num);
        cf_strcat(out->label, sizeof out->label, L == CF_LANG_KO ? "분" : " min");
        return 1;
    }
    return 0;
}

/* ── 설정 ─────────────────────────────────────────────────── */
u32 cf_conf_format(const CfApp *a, char *out, u32 cap)
{
    char num[8];
    out[0] = 0;
    cf_strcat(out, cap, "auto=");   cf_itoa(a->auto_start, num, sizeof num); cf_strcat(out, cap, num);
    cf_strcat(out, cap, "\nsel=");  cf_itoa(a->sel, num, sizeof num);        cf_strcat(out, cap, num);
    cf_strcat(out, cap, "\ncustom="); cf_itoa(a->cust_d, num, sizeof num);   cf_strcat(out, cap, num);
    cf_strcat(out, cap, ",");       cf_itoa(a->cust_h, num, sizeof num);     cf_strcat(out, cap, num);
    cf_strcat(out, cap, ",");       cf_itoa(a->cust_m, num, sizeof num);     cf_strcat(out, cap, num);
    return cf_strcat(out, cap, "\n");
}

static int starts(const char *p, const char *end, const char *key, const char **rest)
{
    while (*key) { if (p >= end || *p != *key) return 0; p++; key++; }
    *rest = p;
    return 1;
}

void cf_conf_parse(CfApp *a, const char *buf, u32 len)
{
    const char *p = buf, *end = buf + len, *v, *e;
    while (p < end) {
        const char *nl = p;
        while (nl < end && *nl != '\n') nl++;
        if (starts(p, nl, "auto=", &v))      a->auto_start = cf_atoi(v, 0) ? 1 : 0;
        else if (starts(p, nl, "sel=", &v)) {
            i64 s = cf_atoi(v, 0);
            a->sel = (s >= 0 && s < CF_SEL_COUNT) ? (int)s : CF_SEL_OFF;
        } else if (starts(p, nl, "custom=", &v)) {
            i64 d = cf_atoi(v, &e), h = 0, m = 0;
            if (*e == ',') { h = cf_atoi(e + 1, &e); if (*e == ',') m = cf_atoi(e + 1, &e); }
            if (d < 0 || d > CF_MAX_DAYS) d = 0;
            if (h < 0 || h > 23) h = 0;
            if (m < 0 || m > 59) m = 0;
            a->cust_d = (int)d; a->cust_h = (int)h; a->cust_m = (int)(m / CF_MIN_STEP) * CF_MIN_STEP;
        }
        p = nl + 1;
    }
}

/* ── 툴팁 ─────────────────────────────────────────────────── */
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
