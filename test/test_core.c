/* test_core.c — 코어 단위 테스트(호스트 cc · libc 사용 OK). `make test` */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/core/coffee.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

static void t_util(void)
{
    char b[24];
    const char *e;
    CHECK(cf_itoa(0, b, sizeof b) == 1 && !strcmp(b, "0"));
    CHECK(cf_itoa(-42, b, sizeof b) == 3 && !strcmp(b, "-42"));
    CHECK(cf_itoa(1234567890123LL, b, sizeof b) == 13);
    CHECK(cf_itoa(123, b, 3) == 0);
    CHECK(cf_atoi("12,3", &e) == 12 && *e == ',');
    CHECK(cf_atoi("x", &e) == 0 && *e == 'x');
    b[0] = 0; cf_strcat(b, 4, "abcdef"); CHECK(!strcmp(b, "abc"));
}

static void t_timer(void)
{
    CfDisplay d;
    cf_display(0, -1, &d);        CHECK(d.unit == CF_UNIT_INF && d.frac == CF_FRAC_ONE);
    cf_display(3600, 7200, &d);   CHECK(d.unit == CF_UNIT_HOUR && d.value == 1 && d.frac == CF_FRAC_ONE / 2);
    cf_display(7199, 7200, &d);   CHECK(d.unit == CF_UNIT_HOUR && d.value == 1);
    cf_display(3599, 7200, &d);   CHECK(d.unit == CF_UNIT_MIN && d.value == 59);
    cf_display(60, 7200, &d);     CHECK(d.unit == CF_UNIT_MIN && d.value == 1);
    cf_display(59, 7200, &d);     CHECK(d.unit == CF_UNIT_SEC && d.value == 59);
    cf_display(0, 7200, &d);      CHECK(d.unit == CF_UNIT_SEC && d.value == 0 && d.frac == 0);
    cf_display(86400 * 2 + 1, 86400 * 3, &d); CHECK(d.unit == CF_UNIT_DAY && d.value == 2);
    cf_display(7200, 7200, &d);   CHECK(d.frac == CF_FRAC_ONE);
    cf_display(999999, 7200, &d); CHECK(d.frac == CF_FRAC_ONE); /* 클램프 */
    /* 다음 변화: 초 단위는 1초 안에 · 시 단위는 시 경계 또는 비율 칸 */
    CHECK(cf_next_change_ms(30500, 1800) <= 1020 + 20 && cf_next_change_ms(30500, 1800) >= 200);
    CHECK(cf_next_change_ms(3600000 * 5, 43200) <= 43200000 / 256 + 20);
    CHECK(cf_next_change_ms(0, 7200) == 200);
    CHECK(cf_next_change_ms(1, -1) >= 3600000);
    CHECK(cf_next_change_ms(150, 60) == 200); /* 최소 200ms */
}

static void t_app(void)
{
    CfApp a;
    CfMenuItem it;
    int ids[CF_MENU_MAX_CHILDREN], n, d, h, m;
    char buf[256];
    cf_app_init(&a, CF_LANG_KO);
    CHECK(a.sel == CF_SEL_OFF && !a.running && cf_auto_secs(&a) == 0);
    n = cf_menu_children(&a, CF_ID_ROOT, ids, CF_MENU_MAX_CHILDREN); CHECK(n == 17 && ids[0] == CF_ID_STATUS && ids[1] == CF_ID_SEP0 && ids[2] == CF_ID_INF && ids[11] == CF_ID_AUTO && ids[12] == CF_ID_LOGIN && ids[13] == CF_ID_LANG && ids[16] == CF_ID_QUIT);
    CHECK(cf_menu_item(&a, CF_ID_STATUS, &it) && !it.enabled && !strcmp(it.label, "대기 중"));
    n = cf_menu_children(&a, CF_ID_AUTO, ids, CF_MENU_MAX_CHILDREN); CHECK(n == 2 && ids[0] == CF_ID_AUTO_OFF);
    n = cf_menu_children(&a, CF_ID_CUSTOM, ids, CF_MENU_MAX_CHILDREN); CHECK(n == 0);
    CHECK(cf_menu_item(&a, CF_ID_OFF, &it) && it.kind == CF_KIND_RADIO && it.checked);
    CHECK(cf_menu_item(&a, CF_ID_12H, &it) && !it.checked && !strcmp(it.label, "12시간"));
    CHECK(cf_menu_item(&a, CF_ID_CUSTOM, &it) && it.kind == CF_KIND_RADIO && !it.checked && !strcmp(it.label, "사용자 지정…"));
    CHECK(cf_menu_item(&a, CF_ID_AUTO, &it) && it.kind == CF_KIND_SUBMENU && !it.checked && !strcmp(it.label, "실행 시 자동 시작"));
    CHECK(cf_menu_item(&a, CF_ID_AUTO_OFF, &it) && it.checked);
    CHECK(cf_menu_item(&a, CF_ID_ABOUT, &it) && it.kind == CF_KIND_NORMAL);
    CHECK(cf_app_click(&a, CF_ID_12H) == CF_ACT_START && a.running && cf_sel_secs(&a) == 43200);
    CHECK(cf_menu_item(&a, CF_ID_12H, &it) && it.checked);
    CHECK(cf_menu_item(&a, CF_ID_OFF, &it) && !it.checked);
    CHECK(cf_app_click(&a, CF_ID_INF) == CF_ACT_START && cf_sel_secs(&a) == -1);
    CHECK(cf_app_click(&a, CF_ID_OFF) == CF_ACT_STOP && !a.running && a.sel == CF_SEL_OFF);
    /* 입력 창 — 사용자 지정 */
    CHECK(cf_app_click(&a, CF_ID_CUSTOM) == CF_ACT_DIALOG_CUSTOM);
    cf_dialog_values(&a, CF_DLG_CUSTOM, &d, &h, &m); CHECK(d == 0 && h == 1 && m == 0);
    CHECK(cf_dialog_submit(&a, CF_DLG_CUSTOM, 0, 0, 0) == CF_ACT_NONE && !a.running);
    CHECK(cf_dialog_submit(&a, CF_DLG_CUSTOM, 0, 12, 50) == CF_ACT_START && a.running && a.sel == CF_SEL_CUSTOM);
    CHECK(cf_sel_secs(&a) == 12 * 3600 + 50 * 60);
    CHECK(cf_menu_item(&a, CF_ID_CUSTOM, &it) && it.checked && !strcmp(it.label, "사용자 지정… (12시간 50분)"));
    CHECK(cf_dialog_submit(&a, CF_DLG_CUSTOM, 500, 99, -3) == CF_ACT_START && a.cust_d == 99 && a.cust_h == 23 && a.cust_m == 0);
    /* 입력 창 — 자동 시작 */
    CHECK(cf_app_click(&a, CF_ID_AUTO_CUSTOM) == CF_ACT_DIALOG_AUTO);
    cf_dialog_values(&a, CF_DLG_AUTO, &d, &h, &m); CHECK(d == 99 && h == 23 && m == 0); /* 자동이 비면 사용자 지정 값 */
    CHECK(cf_dialog_submit(&a, CF_DLG_AUTO, 0, 12, 50) == CF_ACT_MENU && cf_auto_secs(&a) == 12 * 3600 + 50 * 60);
    cf_dialog_values(&a, CF_DLG_AUTO, &d, &h, &m); CHECK(d == 0 && h == 12 && m == 50);
    CHECK(cf_menu_item(&a, CF_ID_AUTO, &it) && it.checked && !strcmp(it.label, "실행 시 자동 시작 (12시간 50분)"));
    CHECK(cf_menu_item(&a, CF_ID_AUTO_CUSTOM, &it) && it.checked && !strcmp(it.label, "사용자 지정… (12시간 50분)"));
    CHECK(cf_menu_item(&a, CF_ID_AUTO_OFF, &it) && !it.checked);
    CHECK(cf_app_click(&a, CF_ID_AUTO_OFF) == CF_ACT_MENU && cf_auto_secs(&a) == 0);
    /* 로그인 시 실행(DR-15) — 진실 원천은 OS 상태: 플랫폼이 login_on을 채우고, 클릭은 뒤집기만 한다 · 설정 파일엔 안 들어간다 */
    CHECK(cf_menu_item(&a, CF_ID_LOGIN, &it) && it.kind == CF_KIND_CHECK && !it.checked && it.enabled && !strcmp(it.label, "로그인 시 실행"));
    CHECK(cf_app_click(&a, CF_ID_LOGIN) == CF_ACT_LOGIN && a.login_on == 1);
    CHECK(cf_menu_item(&a, CF_ID_LOGIN, &it) && it.checked);
    CHECK(cf_app_click(&a, CF_ID_LOGIN) == CF_ACT_LOGIN && a.login_on == 0);
    a.login_on = 1; cf_conf_format(&a, buf, sizeof buf); CHECK(!strstr(buf, "login")); a.login_on = 0;
    CHECK(!strcmp(cf_str(CF_LANG_EN, CF_STR_LOGIN), "Start at login") && !strcmp(cf_str(CF_LANG_JA, CF_STR_LOGIN), "ログイン時に起動") && !strcmp(cf_str(CF_LANG_ZH, CF_STR_LOGIN), "登录时启动"));
    CHECK(cf_app_click(&a, CF_ID_ABOUT) == CF_ACT_ABOUT);
    CHECK(cf_app_click(&a, CF_ID_QUIT) == CF_ACT_QUIT);
    CHECK(cf_app_click(&a, 999) == CF_ACT_NONE);
    CHECK(cf_menu_item(&a, 999, &it) == 0);
    /* 설정 왕복 */
    a.cust_d = 1; a.cust_h = 2; a.cust_m = 30; a.auto_d = 0; a.auto_h = 12; a.auto_m = 50;
    cf_conf_format(&a, buf, sizeof buf);
    CHECK(!strcmp(buf, "auto=0,12,50\ncustom=1,2,30\n"));
    {
        CfApp b; cf_app_init(&b, CF_LANG_EN);
        cf_conf_parse(&b, buf, (u32)strlen(buf));
        CHECK(b.auto_d == 0 && b.auto_h == 12 && b.auto_m == 50 && b.cust_d == 1 && b.cust_h == 2 && b.cust_m == 30);
        cf_conf_parse(&b, "auto=9,77,61\njunk\n", 17);
        CHECK(b.auto_d == 9 && b.auto_h == 23 && b.auto_m == 59); /* 클램프 */
        cf_conf_parse(&b, "auto=0\ncustom=2,3,7", 19); /* 마지막 줄 개행 없음 */
        CHECK(cf_auto_secs(&b) == 0 && b.cust_d == 2 && b.cust_h == 3 && b.cust_m == 7);
    }
    /* 툴팁 · About */
    {
        CfDisplay dd;
        a.running = 1;
        cf_display(2 * 3600 + 5, 7200 * 2, &dd);
        cf_tooltip(&a, &dd, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — 2시간 남음"));
        a.lang = CF_LANG_EN;
        cf_tooltip(&a, &dd, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — 2 hours left"));
        cf_display(3600, 7200, &dd);
        cf_tooltip(&a, &dd, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — 1 hour left"));
        cf_display(0, -1, &dd);
        cf_tooltip(&a, &dd, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — Unlimited · keeping awake"));
        a.running = 0;
        cf_tooltip(&a, NULL, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — Idle"));
        CHECK(cf_about(CF_LANG_EN, buf, sizeof buf) > 40 && strstr(buf, "MIT") && strstr(buf, "github.com/SosomLab/nexa-coffee"));
        CHECK(!strcmp(cf_str(CF_LANG_KO, CF_STR_START), "시작") && !strcmp(cf_str(CF_LANG_EN, CF_STR_SAVE), "Save"));
    }
    /* 자식 프로세스 결과 파싱 */
    {
        i64 d, h, m;
        CHECK(cf_parse_dhm("0 12 50\n", &d, &h, &m) == 3 && d == 0 && h == 12 && m == 50);
        CHECK(cf_parse_dhm("1.000000 2.000000 30.000000\n", &d, &h, &m) == 3 && d == 1 && h == 2 && m == 30);
        CHECK(cf_parse_dhm("0|1|5", &d, &h, &m) == 3 && h == 1 && m == 5);
        CHECK(cf_parse_dhm("7 x", &d, &h, &m) == 1);
        CHECK(cf_parse_dhm("", &d, &h, &m) == 0);
    }
    /* 남은 시간 라벨 */
    {
        a.running = 1; a.sel = CF_SEL_CUSTOM; a.lang = CF_LANG_EN;
        a.remaining_s = 0;
        cf_remaining_label(&a, buf, sizeof buf); CHECK(!strcmp(buf, "0 day 0 hour 0 minute 0 second"));
        a.remaining_s = 2 * 86400 + 3600 + 5 * 60 + 30;
        cf_remaining_label(&a, buf, sizeof buf); CHECK(!strcmp(buf, "2 days 1 hour 5 minutes 30 seconds"));
        a.remaining_s = 1;
        cf_remaining_label(&a, buf, sizeof buf); CHECK(!strcmp(buf, "0 day 0 hour 0 minute 1 second"));
        a.lang = CF_LANG_KO; a.remaining_s = 12 * 3600 + 50 * 60 + 3;
        cf_remaining_label(&a, buf, sizeof buf); CHECK(!strcmp(buf, "0일 12시간 50분 3초"));
        CHECK(cf_menu_item(&a, CF_ID_STATUS, &it) && !strcmp(it.label, "0일 12시간 50분 3초") && !it.enabled);
        a.sel = CF_SEL_INF;
        cf_remaining_label(&a, buf, sizeof buf); CHECK(!strcmp(buf, "무제한 · 절전 방지 중"));
        a.running = 0;
        cf_remaining_label(&a, buf, sizeof buf); CHECK(!strcmp(buf, "대기 중"));
    }
    /* 언어 메뉴 · i18n(en/ko/ja/zh) */
    {
        CfDisplay dd;
        int l, id;
        cf_app_init(&a, CF_LANG_KO);
        n = cf_menu_children(&a, CF_ID_LANG, ids, CF_MENU_MAX_CHILDREN); CHECK(n == 5 && ids[0] == CF_ID_LANG_SYS && ids[1] == CF_ID_LANG0 && ids[4] == CF_ID_LANG0 + CF_LANG_ZH);
        CHECK(cf_menu_item(&a, CF_ID_LANG, &it) && it.kind == CF_KIND_SUBMENU && !strcmp(it.label, "언어"));
        /* 고르기 전 = 시스템(OS 언어)이 체크 · 명시 항목은 OS 언어와 같아도 체크 안 됨 */
        CHECK(cf_menu_item(&a, CF_ID_LANG_SYS, &it) && it.kind == CF_KIND_RADIO && it.checked && !strcmp(it.label, "시스템 (한국어)"));
        CHECK(cf_menu_item(&a, CF_ID_LANG0 + CF_LANG_KO, &it) && it.kind == CF_KIND_RADIO && !it.checked && !strcmp(it.label, "한국어"));
        CHECK(cf_menu_item(&a, CF_ID_LANG0 + CF_LANG_EN, &it) && !it.checked && !strcmp(it.label, "English"));
        CHECK(cf_menu_item(&a, CF_ID_LANG0 + CF_LANG_JA, &it) && !strcmp(it.label, "日本語"));
        CHECK(cf_menu_item(&a, CF_ID_LANG0 + CF_LANG_ZH, &it) && !strcmp(it.label, "中文"));
        CHECK(!cf_menu_item(&a, CF_ID_LANG0 + CF_LANG_COUNT, &it));
        /* 고르기 전엔 설정에 lang 없음(OS 로케일 자동) · 고르면 저장 */
        cf_conf_format(&a, buf, sizeof buf); CHECK(!strstr(buf, "lang="));
        CHECK(cf_app_click(&a, CF_ID_LANG0 + CF_LANG_JA) == CF_ACT_LANG && a.lang == CF_LANG_JA && a.lang_set);
        CHECK(cf_menu_item(&a, CF_ID_QUIT, &it) && !strcmp(it.label, "終了"));
        CHECK(cf_menu_item(&a, CF_ID_LANG, &it) && !strcmp(it.label, "言語"));
        CHECK(cf_menu_item(&a, CF_ID_AUTO, &it) && !strcmp(it.label, "起動時に自動開始"));
        cf_conf_format(&a, buf, sizeof buf); CHECK(strstr(buf, "\nlang=ja\n"));
        CHECK(cf_menu_item(&a, CF_ID_LANG_SYS, &it) && !it.checked && !strcmp(it.label, "システム (한국어)")); /* 괄호는 늘 OS 언어 */
        CHECK(cf_menu_item(&a, CF_ID_LANG0 + CF_LANG_JA, &it) && it.checked);
        /* OS 언어가 바뀌어도 명시 선택 중엔 안 따라간다 · 시스템으로 돌아가면 OS 언어 + 설정에서 lang= 제거 */
        CHECK(cf_set_os_lang(&a, CF_LANG_ZH) == 0 && a.lang == CF_LANG_JA && a.os_lang == CF_LANG_ZH);
        CHECK(cf_menu_item(&a, CF_ID_LANG_SYS, &it) && !strcmp(it.label, "システム (中文)"));
        CHECK(cf_app_click(&a, CF_ID_LANG_SYS) == CF_ACT_LANG && a.lang == CF_LANG_ZH && !a.lang_set);
        cf_conf_format(&a, buf, sizeof buf); CHECK(!strstr(buf, "lang="));
        CHECK(cf_menu_item(&a, CF_ID_LANG_SYS, &it) && it.checked && !strcmp(it.label, "系统 (中文)"));
        CHECK(cf_set_os_lang(&a, CF_LANG_KO) == 1 && a.lang == CF_LANG_KO); /* 자동 모드면 즉시 */
        CHECK(cf_set_os_lang(&a, 99) == 1 && a.lang == CF_LANG_EN);        /* 모르는 값은 EN */
        cf_set_os_lang(&a, CF_LANG_KO);
        CHECK(cf_app_click(&a, CF_ID_LANG0 + CF_LANG_JA) == CF_ACT_LANG); /* 아래 검사들은 ja 상태·lang=ja 버퍼를 기대한다 */
        cf_conf_format(&a, buf, sizeof buf);
        {
            CfApp b; cf_app_init(&b, CF_LANG_EN);
            cf_conf_parse(&b, buf, (u32)strlen(buf)); CHECK(b.lang == CF_LANG_JA && b.lang_set);
            cf_conf_parse(&b, "lang=zh\n", 8); CHECK(b.lang == CF_LANG_ZH);
            cf_conf_parse(&b, "lang=xx\n", 8); CHECK(b.lang == CF_LANG_EN);
            cf_conf_parse(&b, "lang=\n", 6);   CHECK(b.lang == CF_LANG_EN);
        }
        CHECK(cf_lang_of("ko_KR.UTF-8") == CF_LANG_KO && cf_lang_of("ja-JP") == CF_LANG_JA && cf_lang_of("zh_CN") == CF_LANG_ZH);
        CHECK(cf_lang_of("en_US") == CF_LANG_EN && cf_lang_of("C") == CF_LANG_EN && cf_lang_of("") == CF_LANG_EN && cf_lang_of(0) == CF_LANG_EN);
        /* 툴팁 어순 · 남은 시간 라벨 */
        a.running = 1; cf_display(2 * 3600 + 5, 7200 * 2, &dd);
        a.lang = CF_LANG_JA; cf_tooltip(&a, &dd, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — 残り2時間"));
        a.lang = CF_LANG_ZH; cf_tooltip(&a, &dd, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — 剩余2小时"));
        a.lang = CF_LANG_KO; cf_tooltip(&a, &dd, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — 2시간 남음"));
        cf_display(5 * 60 + 3, 7200, &dd);
        a.lang = CF_LANG_EN; cf_tooltip(&a, &dd, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — 5 minutes left"));
        cf_display(1, 7200, &dd);
        cf_tooltip(&a, &dd, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — 1 second left"));
        a.sel = CF_SEL_CUSTOM; a.remaining_s = 12 * 3600 + 50 * 60 + 3;
        a.lang = CF_LANG_ZH; cf_remaining_label(&a, buf, sizeof buf); CHECK(!strcmp(buf, "0天 12小时 50分钟 3秒"));
        a.lang = CF_LANG_JA; cf_remaining_label(&a, buf, sizeof buf); CHECK(!strcmp(buf, "0日 12時間 50分 3秒"));
        a.sel = CF_SEL_INF; cf_remaining_label(&a, buf, sizeof buf); CHECK(!strcmp(buf, "無制限 · スリープ防止中"));
        /* 네 언어 모두 모든 고정 문구·메뉴 라벨이 비어 있지 않다(NUL 블록 개수 검증) */
        for (l = 0; l < CF_LANG_COUNT; l++) {
            a.lang = l; a.running = 0;
            for (id = 0; id < CF_STR_COUNT; id++) CHECK(cf_str(l, id)[0] != 0);
            n = cf_menu_children(&a, CF_ID_ROOT, ids, CF_MENU_MAX_CHILDREN);
            for (id = 0; id < n; id++) { CHECK(cf_menu_item(&a, ids[id], &it)); CHECK(it.kind == CF_KIND_SEPARATOR || it.label[0] != 0); }
            CHECK(cf_about(l, buf, sizeof buf) > 30 && strstr(buf, "SosomLab"));
        }
        a.lang = CF_LANG_KO; a.lang_set = 0;
    }
}

static void t_icon(void)
{
    static u8 a[CF_ICON_BYTES], b[CF_ICON_BYTES];
    CfDisplay d;
    CfColor c = {0, 0, 0, 255};
    int i, opaque = 0, sizes[] = { 16, 22, 44, 64 };
    for (i = 0; i < 4; i++) {
        int s = sizes[i], k, any = 0;
        cf_icon_idle(a, s, c);
        for (k = 0; k < s * s; k++) if (a[k * 4 + 3]) any++;
        CHECK(any > s);                                   /* 뭔가 그려졌다 */
        cf_display(0, -1, &d); cf_icon_active(a, s, &d);
        cf_display(1, 7200, &d); cf_icon_active(b, s, &d);
        CHECK(memcmp(a, b, (size_t)s * s * 4) != 0);       /* ∞와 1초 표시는 다르다 */
        cf_display(7200, 7200, &d); cf_icon_active(a, s, &d);
        cf_display(7200, 7200, &d); cf_icon_active(b, s, &d);
        CHECK(memcmp(a, b, (size_t)s * s * 4) == 0);       /* 결정적 */
    }
    /* 종료 직전엔 호가 없고 외곽선만 → 반투명 픽셀만 남는다 */
    cf_display(1, 86400, &d); cf_icon_active(a, 22, &d);
    for (i = 0; i < 22 * 22; i++) if (a[i * 4 + 3] == 255 && a[i * 4] == 255) opaque++;
    CHECK(opaque > 0); /* 빨간 글자는 불투명 */
    CHECK(cf_angle(0, -10) == 0 && cf_angle(10, 0) == 1024 && cf_angle(0, 10) == 2048 && cf_angle(-10, 0) == 3072);
    CHECK(cf_isqrt(0) == 0 && cf_isqrt(15) == 3 && cf_isqrt(16) == 4 && cf_isqrt(1u << 31) == 46340);
    { i32 s, cc; cf_sincos(1024, &s, &cc); CHECK(s == 4096 && cc == 0); cf_sincos(2048, &s, &cc); CHECK(s == 0 && cc == -4096); }
    /* 8보다 작거나 64보다 크면 아무것도 하지 않는다(버퍼 밖 쓰기 없음) */
    cf_icon_idle(a, 4, c); cf_icon_idle(a, 65, c);
}

/* 보안 경계(docs/19) — V-1 설정 버퍼 경계 · V-2 자릿수 포화 · V-3 INT64_MIN · 범위 밖 인덱스. `make test-san`에서는 ASan이 경계 밖 읽기를 잡는다 */
static void t_hardening(void)
{
    CfApp a; char b[32]; const char *e;
    /* V-1: NUL 없이 정확히 len 바이트인 힙 버퍼 — 파서가 len 밖을 읽으면 ASan 보고 */
    { const char src[] = "auto=1,2,3"; u32 n = (u32)strlen(src); char *h = malloc(n); memcpy(h, src, n);
      cf_app_init(&a, CF_LANG_EN); cf_conf_parse(&a, h, n); CHECK(a.auto_d == 1 && a.auto_h == 2 && a.auto_m == 3); free(h); }
    { const char src[] = "custom=5,6,7\nlang=ko"; u32 n = (u32)strlen(src); char *h = malloc(n); memcpy(h, src, n);
      cf_app_init(&a, CF_LANG_EN); cf_conf_parse(&a, h, n); CHECK(a.cust_d == 5 && a.cust_h == 6 && a.cust_m == 7 && a.lang == CF_LANG_KO); free(h); }
    /* 줄 끝이 곧 버퍼 끝이어도 다음 줄 숫자를 이어 읽지 않는다 */
    cf_app_init(&a, CF_LANG_EN); cf_conf_parse(&a, "auto=1,2\ncustom=9", 17); CHECK(a.auto_h == 2 && a.auto_m == 0 && a.cust_d == 9);
    cf_app_init(&a, CF_LANG_EN); cf_conf_parse(&a, "auto=", 5); CHECK(a.auto_d == 0 && a.auto_h == 0);
    cf_app_init(&a, CF_LANG_EN); cf_conf_parse(&a, "auto=1,", 7); CHECK(a.auto_d == 1 && a.auto_h == 0);
    /* V-2: 자릿수 초과 → 포화(UB 없음) · 끝 포인터는 숫자 끝 */
    CHECK(cf_atoi("99999999999999999999999999", &e) == 1000000000000000000LL && *e == 0);
    CHECK(cf_atoi("-99999999999999999999999999", &e) == -1000000000000000000LL);
    { const char *s = "12345"; CHECK(cf_atoi_n(s, s + 3, &e) == 123 && e == s + 3); }
    { const char *s = "-"; CHECK(cf_atoi_n(s, s + 1, &e) == 0 && e == s); CHECK(cf_atoi_n(s, s, &e) == 0 && e == s); }
    { i64 d, h, m; CHECK(cf_parse_dhm("99999999999999999999 99999999999999999999 -5", &d, &h, &m) == 3);
      cf_app_init(&a, CF_LANG_EN);
      CHECK(cf_dialog_submit(&a, CF_DLG_CUSTOM, d, h, m) == CF_ACT_START && a.cust_d == CF_MAX_DAYS && a.cust_h == 23 && a.cust_m == 0); }
    /* V-3: INT64_MIN 부정 */
    CHECK(cf_itoa((i64)(-9223372036854775807LL - 1), b, sizeof b) == 20 && !strcmp(b, "-9223372036854775808"));
    /* 범위 밖 언어·메뉴 ID — 표 밖을 읽지 않는다 */
    cf_app_init(&a, 12345); CHECK(a.lang == CF_LANG_EN && a.os_lang == CF_LANG_EN);
    cf_app_init(&a, -1);    CHECK(a.lang == CF_LANG_EN);
    CHECK(cf_str(-1, CF_STR_APP)[0] != 0 && cf_str(99, CF_STR_QUIT)[0] != 0 && cf_str(0, 9999)[0] == 0);
    { CfMenuItem it; CHECK(!cf_menu_item(&a, 123456, &it)); CHECK(!cf_menu_item(&a, -7, &it)); }
    { int ids[CF_MENU_MAX_CHILDREN]; CHECK(cf_menu_children(&a, 777, ids, CF_MENU_MAX_CHILDREN) == 0); CHECK(cf_menu_children(&a, CF_ID_ROOT, ids, 3) == 3); }
    CHECK(cf_app_click(&a, 1 << 30) == CF_ACT_NONE && cf_app_click(&a, -5) == CF_ACT_NONE);
    CHECK(cf_about(77, b, sizeof b) > 0); /* cap 32 → 잘림 · 경계 안 */
}

int main(void)
{
    t_hardening();
    t_util(); t_timer(); t_app(); t_icon();
    if (fails) { printf("%d failure(s)\n", fails); return 1; }
    printf("test_core: all green\n");
    return 0;
}
