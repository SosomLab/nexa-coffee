/* util.c — libc 없는 최소 문자열/메모리 도우미(코어 전용). */
#include "coffee.h"

void cf_memset(void *dst, int c, u32 n)
{
    volatile u8 *d = (volatile u8 *)dst; /* volatile: 컴파일러가 memset 호출로 되접지 못하게 */
    while (n--) *d++ = (u8)c;
}

void cf_memcpy(void *dst, const void *src, u32 n)
{
    volatile u8 *d = (volatile u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n--) *d++ = *s++;
}

u32 cf_strlen(const char *s)
{
    u32 n = 0;
    while (s[n]) n++;
    return n;
}

u32 cf_itoa(i64 v, char *out, u32 cap)
{
    char tmp[24];
    u32 n = 0, i, neg = v < 0;
    unsigned long long u = neg ? 0ull - (unsigned long long)v : (unsigned long long)v; /* -INT64_MIN은 UB → 부호 없는 산술(V-3) */
    if (cap == 0) return 0;
    do { tmp[n++] = (char)('0' + (u % 10)); u /= 10; } while (u && n < 20);
    if (neg) tmp[n++] = '-';
    if (n + 1 > cap) { out[0] = 0; return 0; }
    for (i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = 0;
    return n;
}

u32 cf_strcat(char *dst, u32 cap, const char *src)
{
    u32 n = cf_strlen(dst);
    while (*src && n + 1 < cap) dst[n++] = *src++;
    dst[n] = 0;
    return n;
}

/* 숫자 파싱 — [s, end) 안에서만 읽는다(end가 NULL이면 NUL 종단). 자릿수가 넘치면 CF_ATOI_MAX로 포화(오버플로 UB 없음 · V-2).
 * 숫자가 없으면 0을 돌려주고 *e = s. 외부 입력(설정 파일 · 자식 stdout)은 반드시 이 함수로 읽는다(docs/19). */
#define CF_ATOI_MAX 1000000000000000000LL /* 1e18 — ×10+9 해도 i64 안 */
i64 cf_atoi_n(const char *s, const char *end, const char **e)
{
    i64 v = 0;
    const char *p = s;
    int neg = 0;
#define AT_END() (end ? p >= end : 0)
    if (!AT_END() && *p == '-') { neg = 1; p++; }
    if (AT_END() || *p < '0' || *p > '9') { if (e) *e = s; return 0; }
    while (!AT_END() && *p >= '0' && *p <= '9') { v = v > (CF_ATOI_MAX - 9) / 10 ? CF_ATOI_MAX : v * 10 + (*p - '0'); p++; } /* 포화 뒤에도 숫자는 끝까지 소비 */
#undef AT_END
    if (e) *e = p;
    return neg ? -v : v;
}
i64 cf_atoi(const char *s, const char **end) { return cf_atoi_n(s, 0, end); }
