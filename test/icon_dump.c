/* icon_dump.c — 아이콘을 PAM(P7 RGBA)으로 덤프해 눈으로 확인한다(호스트 전용 · 배포물 아님).
 * 사용: icon_dump <out_dir>  → idle/active 각 크기 파일 생성 */
#include <stdio.h>
#include <stdlib.h>
#include "../src/core/coffee.h"

static void dump(const char *dir, const char *name, int size, const u8 *rgba)
{
    char path[512];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s-%d.pam", dir, name, size);
    f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n", size, size);
    fwrite(rgba, 1, (size_t)size * size * 4, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    static u8 buf[CF_ICON_BYTES];
    const char *dir = argc > 1 ? argv[1] : ".";
    static const int sizes[] = { 16, 22, 32, 36, 44 };
    CfColor gray = { 154, 154, 158, 255 };
    CfDisplay d;
    unsigned i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        int s = sizes[i];
        cf_icon_idle(buf, s, gray); dump(dir, "idle", s, buf);
        cf_display(11 * 3600 + 1800, 12 * 3600, &d); cf_icon_active(buf, s, &d); dump(dir, "hour11", s, buf);
        cf_display(3 * 3600, 12 * 3600, &d);         cf_icon_active(buf, s, &d); dump(dir, "hour3", s, buf);
        cf_display(45 * 60, 3600, &d);               cf_icon_active(buf, s, &d); dump(dir, "min45", s, buf);
        cf_display(5 * 60, 3600, &d);                cf_icon_active(buf, s, &d); dump(dir, "min5", s, buf);
        cf_display(30, 1800, &d);                    cf_icon_active(buf, s, &d); dump(dir, "sec30", s, buf);
        cf_display(3, 1800, &d);                     cf_icon_active(buf, s, &d); dump(dir, "sec3", s, buf);
        cf_display(2 * 86400 + 5, 3 * 86400, &d);    cf_icon_active(buf, s, &d); dump(dir, "day2", s, buf);
        cf_display(0, -1, &d);                       cf_icon_active(buf, s, &d); dump(dir, "inf", s, buf);
    }
    return 0;
}
