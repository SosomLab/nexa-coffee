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

int cf_streq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

u32 cf_itoa(i64 v, char *out, u32 cap)
{
    char tmp[24];
    u32 n = 0, i, neg = 0;
    if (cap == 0) return 0;
    if (v < 0) { neg = 1; v = -v; }
    do { tmp[n++] = (char)('0' + (v % 10)); v /= 10; } while (v && n < 20);
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

i64 cf_atoi(const char *s, const char **end)
{
    i64 v = 0;
    const char *p = s;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    if (*p < '0' || *p > '9') { if (end) *end = s; return 0; }
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
    if (end) *end = p;
    return neg ? -v : v;
}
