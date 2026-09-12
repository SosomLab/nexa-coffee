/* icon_idle.c — 대기(미동작) 트레이 아이콘. 동작 아이콘과 **별도 모듈**(사용자 요청 09-12).
 *
 * 앱 아이콘(packaging/branding/icon.svg)과 같은 모티프를 단색으로:
 *   12시부터 시계방향으로 ① 1/3 바퀴 실선 ② 남은 길이의 1/3(= 2/9 바퀴) 옅은 실선
 *   ③ 나머지 4/9 바퀴는 완전한 원(점)들로 채운다.
 * 외곽선·글자 없음 → 동작 아이콘과 한눈에 구분된다.
 * mac은 검정+alpha 템플릿으로 넘겨 시스템이 메뉴바 명암에 맞춘다(색은 호출자가 준다).
 */
#include "coffee.h"

#define SEG1_END (CF_TURN / 3)              /* 1365 */
#define SEG2_END (CF_TURN * 5 / 9)          /* 2275 */

void cf_icon_idle(u8 *rgba, int size, CfColor color)
{
    const int N = size * CF_SS, C2 = N;
    const i32 r_o2 = 4 * size - 4;
    i32 T2 = size * 8 * 17 / 100; if (T2 < 16) T2 = 16;
    const i32 r_i2 = r_o2 - T2;
    const i32 r_c2 = r_o2 - T2 / 2;                          /* 중심선 반지름 */
    const u32 R_O = (u32)(r_o2 * r_o2), R_I = (u32)(r_i2 * r_i2);
    const u32 gap = (u32)(260 * T2 / r_c2);                  /* 이음새 ≈ 두께 × 0.4의 호 길이 */
    const i32 rd2 = T2 * 9 / 20;                             /* 점 반지름 = 두께 × 0.45 */
    const u32 RD = (u32)(rd2 * rd2);
    u32 span0 = SEG2_END + gap, span1 = CF_TURN - gap, span = span1 - span0;
    i32 ndots, k;
    i32 dcx[12], dcy[12];
    CfColor faint = color;
    int x, y, i, j;

    if (size < 8 || size > CF_ICON_MAX) return;
    cf_memset(rgba, 0, (u32)(size * size * 4));
    faint.a = (u8)((color.a * 55) / 100);

    /* 점 개수: 간격 ≈ 지름 × 1.6 */
    ndots = (i32)(((i64)span * r_c2 * 3927) / (4096000LL * T2));
    if (ndots < 2) ndots = 2;
    if (ndots > 12) ndots = 12;
    for (k = 0; k < ndots; k++) {
        u32 a = span0 + (u32)(((i64)span * (2 * k + 1)) / (2 * ndots));
        i32 s, c;
        cf_sincos(a, &s, &c);
        dcx[k] = C2 + (r_c2 * s) / 4096;
        dcy[k] = C2 - (r_c2 * c) / 4096;
    }

    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            u32 acc_solid = 0, acc_faint = 0;
            u8 *p = rgba + (y * size + x) * 4;
            for (j = 0; j < CF_SS; j++) {
                for (i = 0; i < CF_SS; i++) {
                    const int sx = x * CF_SS + i, sy = y * CF_SS + j;
                    const i32 dx = 2 * sx + 1 - C2, dy = 2 * sy + 1 - C2;
                    const u32 d2 = (u32)(dx * dx + dy * dy);
                    u32 th;
                    if (d2 > R_O || d2 < R_I) continue;
                    th = cf_angle(dx, dy);
                    if (th >= gap && th < SEG1_END - gap)            acc_solid += 256;
                    else if (th >= SEG1_END + gap && th < SEG2_END - gap) acc_faint += 256;
                    else if (th >= span0 && th < span1) {
                        for (k = 0; k < ndots; k++) {
                            const i32 ex = dx + C2 - dcx[k], ey = dy + C2 - dcy[k];
                            if ((u32)(ex * ex + ey * ey) <= RD) { acc_solid += 256; break; }
                        }
                    }
                }
            }
            cf_px_over(p, faint, acc_faint / (CF_SS * CF_SS));
            cf_px_over(p, color, acc_solid / (CF_SS * CF_SS));
        }
    }
}
