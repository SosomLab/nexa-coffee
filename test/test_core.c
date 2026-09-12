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
    CHECK(cf_streq("ab", "ab") && !cf_streq("ab", "abc"));
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
    int ids[CF_MENU_MAX_CHILDREN], n;
    char buf[256];
    cf_app_init(&a, CF_LANG_KO);
    CHECK(a.sel == CF_SEL_OFF && !a.running);
    n = cf_menu_children(&a, CF_ID_ROOT, ids, CF_MENU_MAX_CHILDREN); CHECK(n == 12 && ids[0] == CF_ID_INF && ids[11] == CF_ID_QUIT);
    n = cf_menu_children(&a, CF_ID_CUSTOM_HOURS, ids, CF_MENU_MAX_CHILDREN); CHECK(n == 24);
    n = cf_menu_children(&a, CF_ID_CUSTOM_MINS, ids, CF_MENU_MAX_CHILDREN); CHECK(n == 12);
    n = cf_menu_children(&a, CF_ID_CUSTOM_DAYS, ids, CF_MENU_MAX_CHILDREN); CHECK(n == 8);
    CHECK(cf_menu_item(&a, CF_ID_OFF, &it) && it.kind == CF_KIND_RADIO && it.checked);
    CHECK(cf_menu_item(&a, CF_ID_12H, &it) && !it.checked && !strcmp(it.label, "12시간"));
    CHECK(cf_app_click(&a, CF_ID_12H) == CF_ACT_START && a.running && cf_sel_secs(&a) == 43200);
    CHECK(cf_menu_item(&a, CF_ID_12H, &it) && it.checked);
    CHECK(cf_menu_item(&a, CF_ID_OFF, &it) && !it.checked);
    CHECK(cf_app_click(&a, CF_ID_INF) == CF_ACT_START && cf_sel_secs(&a) == -1);
    CHECK(cf_app_click(&a, CF_ID_OFF) == CF_ACT_STOP && !a.running && a.sel == CF_SEL_OFF);
    /* 사용자 지정 */
    CHECK(cf_app_click(&a, CF_ID_DAY0 + 1) == CF_ACT_MENU && a.cust_d == 1);
    CHECK(cf_app_click(&a, CF_ID_HOUR0 + 2) == CF_ACT_MENU && a.cust_h == 2);
    CHECK(cf_app_click(&a, CF_ID_MIN0 + 6) == CF_ACT_MENU && a.cust_m == 30);
    CHECK(cf_menu_item(&a, CF_ID_CUSTOM_START, &it) && it.enabled && !strcmp(it.label, "시작 — 1일 2시간 30분"));
    CHECK(cf_menu_item(&a, CF_ID_CUSTOM, &it) && it.kind == CF_KIND_SUBMENU && !strcmp(it.label, "사용자 지정 (1일 2시간 30분)"));
    CHECK(cf_app_click(&a, CF_ID_CUSTOM_START) == CF_ACT_START && cf_sel_secs(&a) == 86400 + 7200 + 1800);
    CHECK(cf_menu_item(&a, CF_ID_CUSTOM, &it) && it.checked);
    a.cust_d = a.cust_h = a.cust_m = 0;
    CHECK(cf_menu_item(&a, CF_ID_CUSTOM_START, &it) && !it.enabled && !strcmp(it.label, "시작 — 0분"));
    CHECK(cf_app_click(&a, CF_ID_CUSTOM_START) == CF_ACT_NONE);
    CHECK(cf_app_click(&a, CF_ID_AUTO) == CF_ACT_MENU && a.auto_start);
    CHECK(cf_app_click(&a, CF_ID_QUIT) == CF_ACT_QUIT);
    CHECK(cf_app_click(&a, 999) == CF_ACT_NONE);
    CHECK(cf_menu_item(&a, 999, &it) == 0);
    /* 설정 왕복 */
    a.cust_d = 1; a.cust_h = 2; a.cust_m = 30; a.sel = CF_SEL_CUSTOM;
    cf_conf_format(&a, buf, sizeof buf);
    CHECK(!strcmp(buf, "auto=1\nsel=7\ncustom=1,2,30\n"));
    {
        CfApp b; cf_app_init(&b, CF_LANG_EN);
        cf_conf_parse(&b, buf, (u32)strlen(buf));
        CHECK(b.auto_start == 1 && b.sel == CF_SEL_CUSTOM && b.cust_d == 1 && b.cust_h == 2 && b.cust_m == 30);
        cf_conf_parse(&b, "sel=99\ncustom=9,77,61\njunk\n", 27);
        CHECK(b.sel == CF_SEL_OFF && b.cust_d == 0 && b.cust_h == 0 && b.cust_m == 0);
        cf_conf_parse(&b, "auto=0\ncustom=2,3,7", 19); /* 마지막 줄 개행 없음 · 분은 5단위로 내림 */
        CHECK(b.auto_start == 0 && b.cust_d == 2 && b.cust_h == 3 && b.cust_m == 5);
    }
    /* 툴팁 */
    {
        CfDisplay d;
        a.running = 1;
        cf_display(2 * 3600 + 5, 7200 * 2, &d);
        cf_tooltip(&a, &d, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — 2시간 남음"));
        a.lang = CF_LANG_EN;
        cf_tooltip(&a, &d, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — 2 hours left"));
        cf_display(3600, 7200, &d);
        cf_tooltip(&a, &d, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — 1 hour left"));
        cf_display(0, -1, &d);
        cf_tooltip(&a, &d, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — Unlimited · keeping awake"));
        a.running = 0;
        cf_tooltip(&a, NULL, buf, sizeof buf); CHECK(!strcmp(buf, "Nexa Coffee — Idle"));
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

int main(void)
{
    t_util(); t_timer(); t_app(); t_icon();
    if (fails) { printf("%d failure(s)\n", fails); return 1; }
    printf("test_core: all green\n");
    return 0;
}
