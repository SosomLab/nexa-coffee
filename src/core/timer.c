/* timer.c — 잔여시간 → 표시(단위·정수·비율) · 다음 변화 시각.
 *
 * 규칙(사용자 확정): 정수만. 24h 이상 = 일 · 1h 이상 = 시 · 59분~1분 = 분 · 59초 이하 = 초.
 * 값은 단위별 **내림**(1h59m → "1"). 호(arc)는 잔여/전체 비율 — 시작 시 꽉 참.
 */
#include "coffee.h"

#define FRAC_STEPS 256 /* 호 갱신 해상도 — 12h면 169초마다, 30분이면 7초마다 다시 그린다 */

void cf_display(i64 remaining_s, i64 total_s, CfDisplay *out)
{
    if (total_s <= 0) {
        out->unit = CF_UNIT_INF; out->value = 0; out->frac = CF_FRAC_ONE;
        return;
    }
    if (remaining_s < 0) remaining_s = 0;
    if (remaining_s > total_s) remaining_s = total_s;
    if (remaining_s >= 86400)     { out->unit = CF_UNIT_DAY;  out->value = (int)(remaining_s / 86400); }
    else if (remaining_s >= 3600) { out->unit = CF_UNIT_HOUR; out->value = (int)(remaining_s / 3600); }
    else if (remaining_s >= 60)   { out->unit = CF_UNIT_MIN;  out->value = (int)(remaining_s / 60); }
    else                          { out->unit = CF_UNIT_SEC;  out->value = (int)remaining_s; }
    if (out->value > 99) out->value = 99; /* 글자 2자리 */
    /* 비율은 FRAC_STEPS 단위로 양자화 — 다시 그릴 시점을 예측 가능하게 */
    {
        i64 step = (remaining_s * FRAC_STEPS) / total_s; /* 0..FRAC_STEPS */
        out->frac = (int)((step * CF_FRAC_ONE) / FRAC_STEPS);
    }
}

i64 cf_next_change_ms(i64 remaining_ms, i64 total_s)
{
    i64 unit_ms, to_unit, to_frac, q, next;
    if (total_s <= 0) return 3600LL * 1000;
    if (remaining_ms <= 0) return 200;
    /* 단위 경계 */
    if (remaining_ms >= 86400000LL)     unit_ms = 86400000LL;
    else if (remaining_ms >= 3600000LL) unit_ms = 3600000LL;
    else if (remaining_ms >= 60000LL)   unit_ms = 60000LL;
    else                                unit_ms = 1000LL;
    to_unit = remaining_ms % unit_ms;
    if (to_unit == 0) to_unit = unit_ms;
    /* 비율 한 칸 */
    q = (total_s * 1000) / FRAC_STEPS;
    if (q < 1000) q = 1000;
    to_frac = remaining_ms % q;
    if (to_frac == 0) to_frac = q;
    next = to_unit < to_frac ? to_unit : to_frac;
    if (next > remaining_ms) next = remaining_ms;
    /* 경계 직후에 깨어나도록 소량 더함 · 최소 200ms */
    next += 20;
    if (next < 200) next = 200;
    return next;
}
