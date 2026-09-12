/* draw.c — 정수 전용 래스터 도우미. 부동소수점·libc 없음(3-OS 동일 픽셀 · CRT 불요). */
#include "coffee.h"

/* atan(i/64) · 단위 = 1바퀴 4096(π/4 = 512). tools/gen-tables.py */
static const u16 ATAN_LUT[65] = {
    0, 10, 20, 31, 41, 51, 61, 71, 81, 91, 101, 111, 121,
    131, 140, 150, 160, 169, 179, 188, 197, 207, 216, 225, 234, 243,
    252, 260, 269, 277, 286, 294, 302, 310, 318, 326, 334, 342, 349,
    357, 364, 371, 379, 386, 393, 399, 406, 413, 419, 426, 432, 439,
    445, 451, 457, 463, 469, 474, 480, 486, 491, 496, 502, 507, 512,
};

/* 0..512 범위의 atan(y/x) (0 ≤ y ≤ x, x > 0) — LUT 선형보간 */
static u32 atan_oct(u32 y, u32 x)
{
    u32 t, i, f;
    if (x == 0) return 0;
    t = (y << 12) / x;          /* y/x · 4096 */
    i = t >> 6; f = t & 63;     /* 64단계 */
    if (i >= 64) return 512;
    return ATAN_LUT[i] + (((u32)(ATAN_LUT[i + 1] - ATAN_LUT[i]) * f) >> 6);
}

u32 cf_angle(i32 dx, i32 dy)
{
    /* θ = atan2(dx, -dy): 12시 = 0 · 시계방향 */
    i32 yy = dx, xx = -dy;
    u32 ax = xx < 0 ? (u32)(-xx) : (u32)xx;
    u32 ay = yy < 0 ? (u32)(-yy) : (u32)yy;
    u32 a;
    if (ax == 0 && ay == 0) return 0;
    a = (ay <= ax) ? atan_oct(ay, ax) : 1024 - atan_oct(ax, ay); /* 0..1024 (사분면 내) */
    if (xx < 0) a = 2048 - a;
    if (yy < 0) a = (CF_TURN - a) & (CF_TURN - 1);
    return a & (CF_TURN - 1);
}

u32 cf_isqrt(u32 v)
{
    u32 r = 0, bit = 1u << 30;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return r;
}

void cf_px_over(u8 *dst, CfColor c, u32 a256)
{
    u32 sa, da, oa, inv;
    if (a256 == 0) return;
    if (a256 > 256) a256 = 256;
    sa = (c.a * a256) >> 8;           /* 0..255 */
    if (sa == 0) return;
    da = dst[3];
    inv = 255 - sa;
    oa = sa + ((da * inv) + 127) / 255; /* 결과 alpha */
    if (oa == 0) { dst[0] = dst[1] = dst[2] = dst[3] = 0; return; }
    /* straight alpha over: (src*sa + dst*da*(1-sa)) / oa */
    dst[0] = (u8)((c.r * sa + (dst[0] * da * inv) / 255) / oa);
    dst[1] = (u8)((c.g * sa + (dst[1] * da * inv) / 255) / oa);
    dst[2] = (u8)((c.b * sa + (dst[2] * da * inv) / 255) / oa);
    dst[3] = (u8)oa;
}

/* sin(i/64 · π/2) · 4096 — tools/gen-tables.py */
static const u16 SIN_LUT[65] = {
    0, 101, 201, 301, 401, 501, 601, 700, 799, 897, 995, 1092, 1189,
    1285, 1380, 1474, 1567, 1660, 1751, 1842, 1931, 2019, 2106, 2191, 2276, 2359,
    2440, 2520, 2598, 2675, 2751, 2824, 2896, 2967, 3035, 3102, 3166, 3229, 3290,
    3349, 3406, 3461, 3513, 3564, 3612, 3659, 3703, 3745, 3784, 3822, 3857, 3889,
    3920, 3948, 3973, 3996, 4017, 4036, 4052, 4065, 4076, 4085, 4091, 4095, 4096,
};

/* 0..1024 (사분면 내 각) → sin · 4096 */
static i32 sin_q(u32 a)
{
    u32 i = a >> 4, f = a & 15; /* 1024/64 = 16 */
    if (i >= 64) return 4096;
    return (i32)(SIN_LUT[i] + (((u32)(SIN_LUT[i + 1] - SIN_LUT[i]) * f) >> 4));
}

void cf_sincos(u32 angle, i32 *s, i32 *c)
{
    /* angle: 12시 = 0 · 시계방향. 화면 좌표(y 아래)로 x = sin, y = -cos */
    u32 a = angle & (CF_TURN - 1);
    u32 q = a >> 10, r = a & 1023;
    i32 sv, cv;
    switch (q) {
    case 0:  sv = sin_q(r);         cv = sin_q(1024 - r);  break;
    case 1:  sv = sin_q(1024 - r);  cv = -sin_q(r);        break;
    case 2:  sv = -sin_q(r);        cv = -sin_q(1024 - r); break;
    default: sv = -sin_q(1024 - r); cv = sin_q(r);         break;
    }
    *s = sv; *c = cv;
}
