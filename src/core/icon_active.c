/* icon_active.c — 동작(타이머 진행) 트레이 아이콘.
 *
 * 구성(사용자 확정 · 09-12)
 *  - 얇은 외곽선 원(회색) — 호가 다 사라져도 남는다.
 *  - 굵은 호: 12시에서 시계방향으로 잔여 비율만큼. 꼬리 쪽 ~17%가 그라데이션으로 흐려지며
 *    시간이 줄수록 호가 짧아진다(종료 = 외곽선만).
 *  - 가운데 정수(1~2자리) 또는 ∞. 색은 단위별(일=보라 · 시=초록 · 분=주황 · 초=빨강 · ∞=파랑).
 *
 * 좌표: 슈퍼샘플 N = size·CF_SS. 샘플 중심은 2배 단위(X2 = 2·sx + 1) → 반지름도 2배 단위(px × 8).
 */
#include "coffee.h"

#define FADE (CF_TURN * 17 / 100)

CfColor cf_unit_color(int unit)
{
    CfColor c = {10, 132, 255, 255};           /* INF · 파랑 */
    switch (unit) {
    case CF_UNIT_DAY:  c.r = 191; c.g = 90;  c.b = 242; break; /* 보라 */
    case CF_UNIT_HOUR: c.r = 48;  c.g = 209; c.b = 88;  break; /* 초록 */
    case CF_UNIT_MIN:  c.r = 255; c.g = 159; c.b = 10;  break; /* 주황 */
    case CF_UNIT_SEC:  c.r = 255; c.g = 69;  c.b = 58;  break; /* 빨강 */
    default: break;
    }
    return c;
}

/* 호 가중치(0..256): θ < frac 안쪽은 256, 꼬리 FADE 구간은 선형 감쇠 */
static u32 arc_w(u32 theta, u32 frac)
{
    u32 tail;
    if (frac >= CF_TURN) return 256;
    if (theta >= frac) return 0;
    tail = frac - theta;
    return tail >= FADE ? 256 : (tail * 256) / FADE;
}

/* 글리프 배치 */
typedef struct {
    int cols, rows, k;   /* 폰트 격자(가로칸·세로칸) · 배율(서브픽셀/칸) */
    int x0, y0;          /* 좌상단(서브픽셀) */
    int n;               /* 자릿수(∞는 0) */
    int digit[2];
} Glyphs;

static void layout_text(const CfDisplay *d, int N, int r_in2, Glyphs *g)
{
    u32 diam_sub = (u32)r_in2;               /* 안지름(서브픽셀) = 2·r_in2/2 */
    u32 diag, budget;
    if (d->unit == CF_UNIT_INF) { g->n = 0; g->cols = 7; g->rows = 4; }
    else {
        g->n = d->value >= 10 ? 2 : 1;
        g->cols = 3 * g->n + (g->n - 1);
        g->rows = 5;
        g->digit[0] = g->n == 2 ? d->value / 10 : d->value;
        g->digit[1] = d->value % 10;
    }
    diag = cf_isqrt((u32)(g->cols * g->cols + g->rows * g->rows) * 256) ; /* ×16 */
    budget = diam_sub * 92 / 100 * 16;
    g->k = (int)(budget / diag);
    if (g->k < 1) g->k = 1;
    /* 작은 아이콘(글리프 칸 < 2px)은 정확히 1px 칸으로 — 흐려지지 않게 */
    if (g->k >= CF_SS && g->k < 2 * CF_SS) g->k = CF_SS;
    /* 원점은 픽셀 격자에 맞춘다(칸이 정수 px일 때 완전히 선명) */
    g->x0 = ((N - g->cols * g->k) / 2) / CF_SS * CF_SS;
    g->y0 = ((N - g->rows * g->k) / 2) / CF_SS * CF_SS;
}

static int text_hit(const Glyphs *g, int sx, int sy)
{
    int col, row, cell;
    if (sx < g->x0 || sy < g->y0) return 0;
    col = (sx - g->x0) / g->k; row = (sy - g->y0) / g->k;
    if (col >= g->cols || row >= g->rows) return 0;
    if (g->n == 0) return (CF_FONT_INF[row] >> (6 - col)) & 1;
    cell = col & 3;                 /* 0..2 = 글리프 · 3 = 간격 */
    if (cell == 3) return 0;
    return (CF_FONT_DIGIT[g->digit[col >> 2]][row] >> (2 - cell)) & 1;
}

void cf_icon_active(u8 *rgba, int size, const CfDisplay *d)
{
    const int N = size * CF_SS, C2 = N;              /* 중심(2배 단위) */
    const i32 r_o2 = 4 * size - 4;                   /* 바깥 반지름: size/2 − 0.5px */
    i32 T2 = size * 8 * 17 / 100; if (T2 < 16) T2 = 16;   /* 호 두께 ≥ 2px */
    i32 t2 = size * 8 / 22;      if (t2 < 6)  t2 = 6;     /* 외곽선 ≥ 0.75px */
    const i32 r_i2 = r_o2 - T2;                      /* 호 안쪽 */
    const i32 r_l2 = r_o2 - t2;                      /* 외곽선 안쪽 */
    const u32 R_O = (u32)(r_o2 * r_o2), R_I = (u32)(r_i2 * r_i2), R_L = (u32)(r_l2 * r_l2);
    const CfColor col = cf_unit_color(d->unit);
    const CfColor outline = {142, 142, 147, 150};
    const u32 frac = d->frac < 0 ? 0 : (u32)d->frac;
    Glyphs g;
    int x, y, i, j;

    if (size < 8 || size > CF_ICON_MAX) return;
    cf_memset(rgba, 0, (u32)(size * size * 4));
    layout_text(d, N, r_i2, &g);

    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            u32 acc_out = 0, acc_arc = 0, acc_txt = 0;
            u8 *p = rgba + (y * size + x) * 4;
            for (j = 0; j < CF_SS; j++) {
                for (i = 0; i < CF_SS; i++) {
                    const int sx = x * CF_SS + i, sy = y * CF_SS + j;
                    const i32 dx = 2 * sx + 1 - C2, dy = 2 * sy + 1 - C2;
                    const u32 d2 = (u32)(dx * dx + dy * dy);
                    if (d2 <= R_O) {
                        if (d2 >= R_I) {
                            u32 w = arc_w(cf_angle(dx, dy), frac);
                            acc_arc += w;
                            if (d2 >= R_L) acc_out += 256;
                        } else if (text_hit(&g, sx, sy)) {
                            acc_txt += 256;
                        }
                    }
                }
            }
            cf_px_over(p, outline, acc_out / (CF_SS * CF_SS));
            cf_px_over(p, col, acc_arc / (CF_SS * CF_SS));
            cf_px_over(p, col, acc_txt / (CF_SS * CF_SS));
        }
    }
}
