/*
 * Nexa Coffee — 코어 API (플랫폼 비종속 · libc 비의존).
 *
 * 규칙
 *  - 이 헤더 아래의 코어(src/core 아래 .c)는 **어떤 라이브러리도 호출하지 않는다**
 *    (Windows 빌드는 CRT를 링크하지 않는다 — nexa-shortcut 원칙 계승).
 *  - 부동소수점 금지. 아이콘 래스터라이저는 정수·고정소수점만 쓴다 → 3-OS 동일 픽셀.
 *  - 힙 할당 없음. 호출자가 버퍼를 준다.
 *
 * 구성: timer.c(잔여시간→표시) · draw.c(래스터 도우미) · font.c(3x5 숫자·∞)
 *       icon_idle.c(대기 아이콘) · icon_active.c(동작 아이콘) · app.c(상태·메뉴·설정·문구) · util.c
 */
#ifndef NEXA_COFFEE_H
#define NEXA_COFFEE_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef int            i32;
typedef long long      i64;

/* ───────────────────────── util.c ───────────────────────── */
void  cf_memset(void *dst, int c, u32 n);
void  cf_memcpy(void *dst, const void *src, u32 n);
u32   cf_strlen(const char *s);
/* 10진 정수 → 문자열. 반환 = 쓴 길이(NUL 제외). cap 부족 시 0. */
u32   cf_itoa(i64 v, char *out, u32 cap);
/* 문자열 이어붙이기(NUL 보장). 반환 = 새 길이. */
u32   cf_strcat(char *dst, u32 cap, const char *src);
/* 앞부분 10진수 파싱. *end = 다음 위치. 숫자 없으면 0 반환·*end=s. */
i64   cf_atoi(const char *s, const char **end);

/* ───────────────────────── timer.c ───────────────────────── */
enum { CF_UNIT_SEC = 0, CF_UNIT_MIN, CF_UNIT_HOUR, CF_UNIT_DAY, CF_UNIT_INF };

typedef struct {
    int unit;   /* CF_UNIT_* */
    int value;  /* 표시할 정수(단위별 내림). INF는 0 */
    int frac;   /* 잔여 비율 0..4096(4096 = 전체). INF는 4096 */
} CfDisplay;

#define CF_FRAC_ONE 4096

/* 잔여(초)·전체(초) → 표시. total_s <= 0 → 무제한. */
void cf_display(i64 remaining_s, i64 total_s, CfDisplay *out);
/* 표시가 바뀔 때까지의 ms(≥ 200). 무제한이면 큰 값. */
i64  cf_next_change_ms(i64 remaining_ms, i64 total_s);

/* ───────────────────────── draw.c ───────────────────────── */
#define CF_ICON_MAX   64                 /* 한 변 최대(px) */
#define CF_ICON_BYTES (CF_ICON_MAX * CF_ICON_MAX * 4)
#define CF_SS         4                  /* 슈퍼샘플 배율(한 변) */
#define CF_TURN       4096               /* 각도 단위: 1바퀴 = 4096 · 0 = 12시 · 시계방향 */

/* 픽셀 = straight-alpha RGBA(메모리 순서 R,G,B,A). 플랫폼이 자기 형식으로 바꾼다. */
typedef struct { u8 r, g, b, a; } CfColor;

/* 12시 기준 시계방향 각도(0..CF_TURN-1). 정수 atan2(LUT). */
u32  cf_angle(i32 dx, i32 dy);
/* 정수 제곱근(floor). */
u32  cf_isqrt(u32 v);
/* sin·cos × 4096 (angle: 12시 = 0 · 시계방향). */
void cf_sincos(u32 angle, i32 *s, i32 *c);
/* dst 픽셀 위에 색을 alpha(0..256)로 over-합성. */
void cf_px_over(u8 *dst, CfColor c, u32 a256);

/* ───────────────────────── font.c ───────────────────────── */
/* 3x5 숫자 글리프: 행별 3비트(상위 비트 = 왼쪽). */
extern const u8 CF_FONT_DIGIT[10][5];
/* ∞ 글리프 7x4: 행별 7비트. */
extern const u8 CF_FONT_INF[4];

/* ───────────────────────── icon_idle.c ───────────────────────── */
/* 대기(미동작) 아이콘 — 앱 아이콘 모티프(실선 1/3 · 실선 2/9 · 점 4/9)를 단색으로.
 * mac 템플릿 이미지용으로 색을 받는다(검정+alpha → 시스템이 메뉴바 명암에 맞춘다). */
void cf_icon_idle(u8 *rgba, int size, CfColor color);

/* ───────────────────────── icon_active.c ───────────────────────── */
/* 동작 아이콘 — 얇은 외곽선 + 굵은 잔여 호(꼬리가 그라데이션으로 사라짐) + 단위색 정수. */
void cf_icon_active(u8 *rgba, int size, const CfDisplay *d);

/* ───────────────────────── app.c ───────────────────────── */
enum { CF_LANG_EN = 0, CF_LANG_KO = 1 };

/* 선택지 — 메뉴 순서와 같다. */
enum {
    CF_SEL_OFF = 0, CF_SEL_INF, CF_SEL_12H, CF_SEL_6H, CF_SEL_2H, CF_SEL_1H, CF_SEL_30M,
    CF_SEL_CUSTOM, CF_SEL_COUNT
};

/* 메뉴 항목 id(3-OS 공통). */
enum {
    CF_ID_ROOT = 0,
    CF_ID_INF = 1, CF_ID_12H, CF_ID_6H, CF_ID_2H, CF_ID_1H, CF_ID_30M,
    CF_ID_CUSTOM,            /* "Custom…" → 입력 창 */
    CF_ID_OFF,
    CF_ID_SEP1,
    CF_ID_AUTO,              /* 서브메뉴: Off / Custom… */
    CF_ID_SEP2, CF_ID_ABOUT, CF_ID_QUIT,
    CF_ID_STATUS,            /* 맨 위: 남은 시간(비활성 · 1초 갱신) */
    CF_ID_SEP0,
    CF_ID_AUTO_OFF = 20, CF_ID_AUTO_CUSTOM
};
#define CF_MAX_DAYS 99
#define CF_MENU_MAX_CHILDREN 16

enum { CF_KIND_NORMAL = 0, CF_KIND_SEPARATOR, CF_KIND_RADIO, CF_KIND_CHECK, CF_KIND_SUBMENU };

typedef struct {
    int  kind;      /* CF_KIND_* */
    int  checked;   /* RADIO/CHECK */
    int  enabled;
    char label[64]; /* UTF-8 */
} CfMenuItem;

typedef struct {
    int lang;        /* CF_LANG_* */
    int sel;         /* 현재 선택(CF_SEL_*) — running이 0이면 OFF */
    int running;     /* 작업 진행 중 */
    int cust_d, cust_h, cust_m; /* 사용자 지정 일·시·분(마지막 입력) */
    int auto_d, auto_h, auto_m; /* 실행 시 자동 시작 시간(전부 0 = 끔) */
    i64 remaining_s; /* 플랫폼이 메뉴를 만들기 전에 채운다(초 · 올림) */
} CfApp;

/* 클릭 결과 — 플랫폼이 수행. */
enum {
    CF_ACT_NONE = 0,
    CF_ACT_START,         /* app->sel로 시작 · 초 = cf_sel_secs(app) (≤0 = 무제한) */
    CF_ACT_STOP,
    CF_ACT_QUIT,
    CF_ACT_MENU,          /* 설정만 바뀜(메뉴 갱신 + 저장) */
    CF_ACT_DIALOG_CUSTOM, /* 입력 창(CF_DLG_CUSTOM) 열기 */
    CF_ACT_DIALOG_AUTO,   /* 입력 창(CF_DLG_AUTO) 열기 */
    CF_ACT_ABOUT          /* About 화면 */
};
/* 입력 창 모드 */
enum { CF_DLG_CUSTOM = 0, CF_DLG_AUTO = 1 };

void cf_app_init(CfApp *a, int lang);
/* 선택의 초. INF = -1 · OFF = 0. */
i64  cf_sel_secs(const CfApp *a);
/* 자동 시작 초(0 = 끔). */
i64  cf_auto_secs(const CfApp *a);
/* 메뉴 클릭 → 상태 갱신 + 행동 반환. START/STOP/MENU는 저장 대상. */
int  cf_app_click(CfApp *a, int id);
/* 입력 창 초기값. */
void cf_dialog_values(const CfApp *a, int mode, int *d, int *h, int *m);
/* 입력 창 확정 — 범위로 클램프. CUSTOM: START(전부 0이면 NONE) · AUTO: MENU(저장). */
int  cf_dialog_submit(CfApp *a, int mode, i64 d, i64 h, i64 m);
/* parent의 자식 id 목록. 반환 = 개수. */
int  cf_menu_children(const CfApp *a, int parent, int *ids, int max);
/* 항목 속성. 반환 0 = 없는 id. */
int  cf_menu_item(const CfApp *a, int id, CfMenuItem *out);

/* 설정 직렬화 — "auto=0,12,50\ncustom=1,2,30\n". 반환 = 길이. */
u32  cf_conf_format(const CfApp *a, char *out, u32 cap);
/* 설정 파싱(관대함 — 모르는 줄 무시). */
void cf_conf_parse(CfApp *a, const char *buf, u32 len);

/* 툴팁 문구 — "Nexa Coffee — 2시간 남음" 등. 반환 = 길이. */
u32  cf_tooltip(const CfApp *a, const CfDisplay *d, char *out, u32 cap);
/* 메뉴 맨 위 남은 시간 — "0 days 12 hours 50 minutes 3 seconds"(1보다 크면 복수형) / "0일 12시간 50분 3초".
 * 대기 중이면 "Idle", 무제한이면 "Unlimited · keeping awake". 반환 = 길이. */
u32  cf_remaining_label(const CfApp *a, char *out, u32 cap);
/* About 본문(여러 줄). 반환 = 길이. */
u32  cf_about(int lang, char *out, u32 cap);
/* 고정 문구. */
enum {
    CF_STR_APP = 0, CF_STR_IDLE, CF_STR_QUIT, CF_STR_AUTO,
    CF_STR_DLG_CUSTOM, CF_STR_DLG_AUTO, CF_STR_DAYS, CF_STR_HOURS, CF_STR_MINUTES,
    CF_STR_START, CF_STR_SAVE, CF_STR_CANCEL, CF_STR_ABOUT, CF_STR_VERSION, CF_STR_COUNT
};
const char *cf_str(int lang, int id);

#endif /* NEXA_COFFEE_H */
