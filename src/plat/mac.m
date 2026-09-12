/*
 * mac.m — macOS 플랫폼 계층. NSStatusItem(메뉴바) + NSMenu + IOKit 전원 어설션.
 *
 * - 프레임워크: AppKit · IOKit 만. 창 없음(LSUIElement / Accessory 정책).
 * - 아이콘은 코어가 그린 RGBA를 CGImage로 감싼다(18pt · 1x/2x 두 벌).
 *   대기 아이콘은 **템플릿**(검정+alpha) → 시스템이 메뉴바 명암(라이트/다크)에 맞춘다.
 * - 절전 방지 = PreventUserIdleSystemSleep + PreventUserIdleDisplaySleep 어설션 +
 *   30초마다 IOPMAssertionDeclareUserActivity(화면보호기 유휴 타이머 재설정).
 *   ⚠️ 덮개 닫힘 절전은 OS 강제 절전이라 어설션으로 막을 수 없다(root `pmset disablesleep` 영역).
 * - 작업 종료 시: 어설션 해제 · 타이머 무효화 · 동작 아이콘 해제 · malloc_zone_pressure_relief.
 */
#import <AppKit/AppKit.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <malloc/malloc.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "../core/coffee.h"

#define ICON_PT 18
#define ACTIVITY_SEC 30

static i64 now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (i64)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

@interface Coffee : NSObject <NSApplicationDelegate, NSMenuDelegate>
@end

@implementation Coffee {
    CfApp app;
    NSStatusItem *item;
    NSMenu *menu;
    NSImage *idleImage;
    NSTimer *tick;       /* 다음 표시 변화 시각에 1회 */
    NSTimer *activity;   /* 30초 주기 사용자 활동 선언 */
    i64 deadline;        /* 0 = 무제한 */
    i64 total;
    IOPMAssertionID assertSys, assertDisp;
    char confPath[1024];
    int lockFd;
}

/* ── 설정 파일 ── */
- (void)confInit
{
    NSString *base = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES) firstObject];
    NSString *dir = [base stringByAppendingPathComponent:@"nexa-coffee"];
    mkdir([dir fileSystemRepresentation], 0700);
    snprintf(confPath, sizeof confPath, "%s/config", [dir fileSystemRepresentation]);
    /* 중복 실행 방지 — 잠금 파일 */
    char lockPath[1024];
    snprintf(lockPath, sizeof lockPath, "%s/lock", [dir fileSystemRepresentation]);
    lockFd = open(lockPath, O_RDWR | O_CREAT, 0600);
    if (lockFd >= 0 && flock(lockFd, LOCK_EX | LOCK_NB) != 0) exit(0);
}
- (void)confLoad
{
    char buf[256];
    int fd = open(confPath, O_RDONLY);
    if (fd < 0) return;
    ssize_t n = read(fd, buf, sizeof buf - 1);
    close(fd);
    if (n > 0) cf_conf_parse(&app, buf, (u32)n);
}
- (void)confSave
{
    char buf[256];
    u32 n = cf_conf_format(&app, buf, sizeof buf);
    int fd = open(confPath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return;
    (void)!write(fd, buf, n);
    close(fd);
}

/* ── 아이콘 ── */
static NSBitmapImageRep *repFromRGBA(const u8 *rgba, int px, CGFloat pt)
{
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CFDataRef data = CFDataCreate(NULL, rgba, (CFIndex)px * px * 4);   /* 복사 — 원본 버퍼는 곧 해제 */
    CGDataProviderRef dp = CGDataProviderCreateWithCFData(data);
    CGImageRef cg = CGImageCreate((size_t)px, (size_t)px, 8, 32, (size_t)px * 4, cs,
                                  kCGBitmapByteOrder32Big | kCGImageAlphaLast, dp, NULL, false,
                                  kCGRenderingIntentDefault);
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:cg];
    rep.size = NSMakeSize(pt, pt);
    CGImageRelease(cg);
    CGDataProviderRelease(dp);
    CFRelease(data);
    CGColorSpaceRelease(cs);
    return rep;
}

- (NSImage *)imageIdle:(BOOL)isIdle display:(const CfDisplay *)d
{
    u8 *buf = malloc(CF_ICON_BYTES);
    NSImage *img = [[NSImage alloc] initWithSize:NSMakeSize(ICON_PT, ICON_PT)];
    CfColor black = {0, 0, 0, 255};
    int sizes[2] = { ICON_PT, ICON_PT * 2 };
    for (int i = 0; i < 2; i++) {
        if (isIdle) cf_icon_idle(buf, sizes[i], black);
        else        cf_icon_active(buf, sizes[i], d);
        [img addRepresentation:repFromRGBA(buf, sizes[i], ICON_PT)];
    }
    free(buf);
    img.template = isIdle;
    return img;
}

- (void)showIdle
{
    if (!idleImage) idleImage = [self imageIdle:YES display:NULL];
    item.button.image = idleImage;
    char tip[128];
    cf_tooltip(&app, NULL, tip, sizeof tip);
    item.button.toolTip = [NSString stringWithUTF8String:tip];
}

/* ── 절전 방지 ── */
- (void)inhibit:(BOOL)on
{
    if (on) {
        if (!assertSys)
            IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleSystemSleep, kIOPMAssertionLevelOn,
                                        CFSTR("Nexa Coffee"), &assertSys);
        if (!assertDisp)
            IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn,
                                        CFSTR("Nexa Coffee"), &assertDisp);
        if (!activity) {
            activity = [NSTimer scheduledTimerWithTimeInterval:ACTIVITY_SEC target:self
                                                      selector:@selector(declareActivity) userInfo:nil repeats:YES];
            activity.tolerance = 5;
            [self declareActivity];
        }
    } else {
        if (assertSys)  { IOPMAssertionRelease(assertSys);  assertSys = 0; }
        if (assertDisp) { IOPMAssertionRelease(assertDisp); assertDisp = 0; }
        [activity invalidate]; activity = nil;
    }
}
- (void)declareActivity
{
    IOPMAssertionID id = 0;
    IOPMAssertionDeclareUserActivity(CFSTR("Nexa Coffee"), kIOPMUserActiveLocal, &id);
}

/* ── 작업 ── */
- (void)startJob
{
    i64 secs = cf_sel_secs(&app);
    total = secs;
    deadline = secs > 0 ? now_ms() + secs * 1000 : 0;
    [self inhibit:YES];
    [self tickNow];
}
- (void)stopJob
{
    [tick invalidate]; tick = nil;
    [self inhibit:NO];
    app.running = 0;
    [self showIdle];
    /* 타이머 작업이 쓴 메모리 회수(사용자 요청 09-12) — 동작 아이콘은 이미 교체돼 해제됐다 */
    malloc_zone_pressure_relief(NULL, 0);
}
- (void)tickNow
{
    CfDisplay d;
    i64 rem_ms = deadline ? deadline - now_ms() : 0;
    if (deadline && rem_ms <= 0) { [self stopJob]; return; }
    cf_display(deadline ? (rem_ms + 999) / 1000 : 0, total, &d);
    item.button.image = [self imageIdle:NO display:&d];
    char tip[128];
    cf_tooltip(&app, &d, tip, sizeof tip);
    item.button.toolTip = [NSString stringWithUTF8String:tip];
    [tick invalidate];
    i64 next = cf_next_change_ms(rem_ms, total);
    tick = [NSTimer scheduledTimerWithTimeInterval:(NSTimeInterval)next / 1000.0 target:self
                                          selector:@selector(tickNow) userInfo:nil repeats:NO];
    tick.tolerance = 0.05;
}

/* ── 메뉴(코어 트리를 그대로 옮긴다 · 열릴 때마다 새로 만든다) ── */
- (void)fill:(NSMenu *)m parent:(int)parent
{
    int ids[CF_MENU_MAX_CHILDREN];
    int n = cf_menu_children(&app, parent, ids, CF_MENU_MAX_CHILDREN);
    [m removeAllItems];
    for (int i = 0; i < n; i++) {
        CfMenuItem it;
        if (!cf_menu_item(&app, ids[i], &it)) continue;
        if (it.kind == CF_KIND_SEPARATOR) { [m addItem:[NSMenuItem separatorItem]]; continue; }
        NSMenuItem *mi = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:it.label]
                                                    action:@selector(click:) keyEquivalent:@""];
        mi.tag = ids[i];
        mi.target = self;
        mi.enabled = it.enabled != 0;
        mi.state = it.checked ? NSControlStateValueOn : NSControlStateValueOff;
        if (it.kind == CF_KIND_SUBMENU) {
            NSMenu *sub = [[NSMenu alloc] initWithTitle:mi.title];
            sub.autoenablesItems = NO;
            [self fill:sub parent:ids[i]];
            mi.submenu = sub;
            mi.action = nil;
        }
        [m addItem:mi];
    }
}
- (void)menuNeedsUpdate:(NSMenu *)m { [self fill:m parent:CF_ID_ROOT]; }

- (void)click:(NSMenuItem *)mi
{
    switch (cf_app_click(&app, (int)mi.tag)) {
    case CF_ACT_START: [self confSave]; [self startJob]; break;
    case CF_ACT_STOP:  [self confSave]; [self stopJob];  break;
    case CF_ACT_MENU:  [self confSave]; break;
    case CF_ACT_QUIT:  [self stopJob]; [NSApp terminate:nil]; break;
    default: break;
    }
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSString *lang = [[NSLocale preferredLanguages] firstObject];
    cf_app_init(&app, [lang hasPrefix:@"ko"] ? CF_LANG_KO : CF_LANG_EN);
    [self confInit];
    [self confLoad];
    app.running = 0;

    item = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];
    menu = [[NSMenu alloc] initWithTitle:@"Nexa Coffee"];
    menu.autoenablesItems = NO;
    menu.delegate = self;
    item.menu = menu;
    [self showIdle];

    /* 첫 실행 자동 시작(사용자 확정): 옵션 켜짐 + 시간 지정 */
    if (app.auto_start && app.sel != CF_SEL_OFF) { app.running = 1; [self startJob]; }
}
- (void)applicationWillTerminate:(NSNotification *)note { [self inhibit:NO]; }
@end

int main(void)
{
    @autoreleasepool {
        NSApplication *nsapp = [NSApplication sharedApplication];
        [nsapp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        Coffee *c = [Coffee new];
        nsapp.delegate = c;
        [nsapp run];
    }
    return 0;
}
