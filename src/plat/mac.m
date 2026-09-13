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
 * - 입력 창(일·시·분 + 시작/저장)·About은 **자식 프로세스**에서 띄운다(T-14 · 09-13 실측: 창 1회 후 AppKit
 *   텍스트 시스템 캐시 +5 MB가 프로세스 안에서는 안 돌아옴). 자기 자신을 `--dialog`/`--about`로 NSTask 실행,
 *   결과 "d h m"은 stdout 파이프로. 자식이 끝나면 캐시도 함께 사라져 부모는 대기 6 MB를 유지한다.
 *   Linux(zenity 자식 프로세스)·Windows(CreateProcess)와 같은 구조.
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
#include <stdio.h>
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
    NSTask *child;            /* 열려 있는 입력 창/About 자식 프로세스(없으면 nil) */
    NSTimer *menuTimer;       /* 메뉴가 열린 동안 1초마다 남은 시간 갱신 */
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
            activity = [NSTimer timerWithTimeInterval:ACTIVITY_SEC target:self
                                             selector:@selector(declareActivity) userInfo:nil repeats:YES];
            activity.tolerance = 5;
            [[NSRunLoop mainRunLoop] addTimer:activity forMode:NSRunLoopCommonModes];
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
    tick = [NSTimer timerWithTimeInterval:(NSTimeInterval)next / 1000.0 target:self
                                 selector:@selector(tickNow) userInfo:nil repeats:NO];
    tick.tolerance = 0.05;
    [[NSRunLoop mainRunLoop] addTimer:tick forMode:NSRunLoopCommonModes]; /* 모달 입력 창 중에도 갱신 */
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
- (void)refreshRemaining { app.remaining_s = deadline ? (deadline - now_ms() + 999) / 1000 : 0; }
- (void)menuNeedsUpdate:(NSMenu *)m { [self refreshRemaining]; [self fill:m parent:CF_ID_ROOT]; }
/* 메뉴가 열린 동안 맨 위 항목을 1초마다 갱신(사용자 요청 09-13) — 메뉴 추적 모드에서도 도는 common-modes 타이머 */
- (void)menuWillOpen:(NSMenu *)m
{
    if (m != menu) return;
    [menuTimer invalidate];
    menuTimer = [NSTimer timerWithTimeInterval:1.0 target:self selector:@selector(menuTick) userInfo:nil repeats:YES];
    [[NSRunLoop mainRunLoop] addTimer:menuTimer forMode:NSRunLoopCommonModes];
}
- (void)menuDidClose:(NSMenu *)m { if (m == menu) { [menuTimer invalidate]; menuTimer = nil; } }
- (void)menuTick
{
    char label[96];
    NSMenuItem *st = [menu itemWithTag:CF_ID_STATUS];
    if (!st) return;
    [self refreshRemaining];
    cf_remaining_label(&app, label, sizeof label);
    st.title = [NSString stringWithUTF8String:label];
}

- (void)act:(int)action
{
    switch (action) {
    case CF_ACT_START: [self confSave]; [self startJob]; break;
    case CF_ACT_STOP:  [self confSave]; [self stopJob];  break;
    case CF_ACT_MENU:  [self confSave]; break;
    case CF_ACT_LANG:  [self confSave]; if (app.running) [self tickNow]; else [self showIdle]; break; /* 툴팁 갱신 · 메뉴는 열 때 새로 만든다 */
    case CF_ACT_QUIT:  [self stopJob]; [NSApp terminate:nil]; break;
    case CF_ACT_DIALOG_CUSTOM: [self showDialog:CF_DLG_CUSTOM]; break;
    case CF_ACT_DIALOG_AUTO:   [self showDialog:CF_DLG_AUTO]; break;
    case CF_ACT_ABOUT: [self showAbout]; break;
    default: break;
    }
}
- (void)click:(NSMenuItem *)mi { [self act:cf_app_click(&app, (int)mi.tag)]; }
- (void)actBoxed:(NSNumber *)n { [self act:n.intValue]; }

/* ── 창은 자식 프로세스로(T-14) — 부모는 띄우고 결과만 받는다 ── */
- (void)showDialog:(int)mode
{
    int v[3];
    cf_dialog_values(&app, mode, &v[0], &v[1], &v[2]);
    [self spawnUI:@[ @"--dialog", mode == CF_DLG_AUTO ? @"auto" : @"custom",
                     @(v[0]).stringValue, @(v[1]).stringValue, @(v[2]).stringValue, @(app.lang).stringValue ] mode:mode];
}
- (void)showAbout { [self spawnUI:@[ @"--about", @(app.lang).stringValue ] mode:-1]; }

- (void)spawnUI:(NSArray<NSString *> *)args mode:(int)mode
{
    if (child) return; /* 이미 열려 있음 */
    NSString *exe = NSBundle.mainBundle.executablePath ?: NSProcessInfo.processInfo.arguments.firstObject;
    NSTask *t = [NSTask new];
    NSPipe *pipe = [NSPipe pipe];
    t.executableURL = [NSURL fileURLWithPath:exe];
    t.arguments = args;
    t.standardOutput = pipe;
    __weak Coffee *ws = self;
    t.terminationHandler = ^(NSTask *tk) {
        NSData *data = [pipe.fileHandleForReading readDataToEndOfFile];
        dispatch_async(dispatch_get_main_queue(), ^{ [ws childDone:tk data:data mode:mode]; });
    };
    NSError *err = nil;
    if ([t launchAndReturnError:&err]) child = t;
}
- (void)childDone:(NSTask *)t data:(NSData *)data mode:(int)mode
{
    if (t != child) return;
    child = nil;
    if (mode >= 0 && t.terminationStatus == 0 && data.length) {
        char buf[64];
        i64 d, h, m;
        NSUInteger n = data.length < sizeof buf - 1 ? data.length : sizeof buf - 1;
        memcpy(buf, data.bytes, n); buf[n] = 0;
        if (cf_parse_dhm(buf, &d, &h, &m) == 3) [self act:cf_dialog_submit(&app, mode, d, h, m)];
    }
    malloc_zone_pressure_relief(NULL, 0);
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    cf_app_init(&app, cf_lang_of([[[NSLocale preferredLanguages] firstObject] UTF8String]));
    [self confInit];
    [self confLoad];
    app.running = 0;

    item = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];
    menu = [[NSMenu alloc] initWithTitle:@"Nexa Coffee"];
    menu.autoenablesItems = NO;
    menu.delegate = self;
    item.menu = menu;
    [self showIdle];
    malloc_zone_pressure_relief(NULL, 0); /* 초기화가 남긴 여유 페이지 반납 */

    /* 디버그: NEXA_COFFEE_SHOW=custom|auto|about|menu 이면 해당 창을 바로 띄운다(스크린샷 검증용 · 비용 0) */
    const char *show = getenv("NEXA_COFFEE_SHOW");
    if (show) {
        if (!strcmp(show, "menu")) [item.button performSelector:@selector(performClick:) withObject:nil afterDelay:0.3];
        else {
            int a = !strcmp(show, "about") ? CF_ACT_ABOUT : !strcmp(show, "auto") ? CF_ACT_DIALOG_AUTO : CF_ACT_DIALOG_CUSTOM;
            [self performSelector:@selector(actBoxed:) withObject:@(a) afterDelay:0.3];
        }
    }
    /* 첫 실행 자동 시작(사용자 확정): 옵션 켜짐 + 시간 지정 */
    if (cf_auto_secs(&app) > 0) { app.cust_d = app.auto_d; app.cust_h = app.auto_h; app.cust_m = app.auto_m; app.sel = CF_SEL_CUSTOM; app.running = 1; [self startJob]; }
}
- (void)applicationWillTerminate:(NSNotification *)note { [self inhibit:NO]; }
@end

/* ══════════════════ 자식 프로세스 UI(`--dialog` · `--about`) — 부모와 상태를 공유하지 않는다 ══════════════════ */
static NSString *S(int lang, int id) { return [NSString stringWithUTF8String:cf_str(lang, id)]; }

@interface UiChild : NSObject <NSWindowDelegate>
@end
@implementation UiChild {
    NSTextField *field[3];
}
/* [ 0 ] day(s) [ 12 ] hour(s) [ 50 ] minute(s)   [Cancel] [Start] → stdout "d h m" · 종료 코드 0(확정)/1(취소) */
- (int)runDialog:(int)mode lang:(int)lang values:(const int *)v
{
    static const int labelId[3] = { CF_STR_DAYS, CF_STR_HOURS, CF_STR_MINUTES };
    NSPanel *p = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 360, 92)
                                            styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                                              backing:NSBackingStoreBuffered defer:NO];
    p.title = S(lang, mode == CF_DLG_AUTO ? CF_STR_DLG_AUTO : CF_STR_DLG_CUSTOM);
    p.delegate = self;
    NSView *cv = p.contentView;
    CGFloat x = 18;
    for (int i = 0; i < 3; i++) {
        NSTextField *tf = [NSTextField textFieldWithString:[NSString stringWithFormat:@"%d", v[i]]];
        tf.frame = NSMakeRect(x, 50, 46, 24);
        tf.alignment = NSTextAlignmentRight;
        NSNumberFormatter *nf = [NSNumberFormatter new];
        nf.numberStyle = NSNumberFormatterNoStyle;
        nf.minimum = @0; nf.maximum = @(i == 0 ? CF_MAX_DAYS : i == 1 ? 23 : 59);
        tf.formatter = nf;
        [cv addSubview:tf];
        field[i] = tf;
        x += 52;
        NSTextField *lb = [NSTextField labelWithString:S(lang, labelId[i])];
        [lb sizeToFit];
        lb.frame = NSMakeRect(x, 54, lb.frame.size.width, lb.frame.size.height);
        [cv addSubview:lb];
        x += lb.frame.size.width + 14;
    }
    for (int i = 0; i < 3; i++) field[i].nextKeyView = field[(i + 1) % 3];
    CGFloat width = x + 4 > 360 ? x + 4 : 360;
    NSButton *ok = [NSButton buttonWithTitle:S(lang, mode == CF_DLG_AUTO ? CF_STR_SAVE : CF_STR_START) target:self action:@selector(ok:)];
    ok.keyEquivalent = @"\r";
    [ok sizeToFit];
    CGFloat okw = ok.frame.size.width < 84 ? 84 : ok.frame.size.width;
    ok.frame = NSMakeRect(width - 14 - okw, 10, okw, 30);
    NSButton *cancel = [NSButton buttonWithTitle:S(lang, CF_STR_CANCEL) target:self action:@selector(cancel:)];
    cancel.keyEquivalent = @"\033";
    [cancel sizeToFit];
    CGFloat cw = cancel.frame.size.width < 84 ? 84 : cancel.frame.size.width;
    cancel.frame = NSMakeRect(ok.frame.origin.x - 6 - cw, 10, cw, 30);
    [cv addSubview:cancel];
    [cv addSubview:ok];
    [p setContentSize:NSMakeSize(width, 92)];
    [p center];
    [p makeFirstResponder:field[0]];
    [NSApp activateIgnoringOtherApps:YES];
    NSModalResponse r = [NSApp runModalForWindow:p];
    [p orderOut:nil];
    if (r != NSModalResponseOK) return 1;
    printf("%ld %ld %ld\n", (long)field[0].integerValue, (long)field[1].integerValue, (long)field[2].integerValue);
    fflush(stdout);
    return 0;
}
- (void)ok:(id)sender     { [NSApp stopModalWithCode:NSModalResponseOK]; }
- (void)cancel:(id)sender { [NSApp stopModalWithCode:NSModalResponseCancel]; }
- (void)windowWillClose:(NSNotification *)note
{
    if ([NSApp modalWindow] == note.object) [NSApp stopModalWithCode:NSModalResponseCancel];
    else exit(0); /* About 패널 닫힘 → 자식 종료 */
}
- (void)runAbout:(int)lang
{
    char body[512];
    cf_about(lang, body, sizeof body);
    NSString *text = [NSString stringWithUTF8String:body];
    NSRange nl = [text rangeOfString:@"\n"];
    NSString *credits = nl.location == NSNotFound ? text : [text substringFromIndex:nl.location + 1];
    NSMutableParagraphStyle *ps = [NSMutableParagraphStyle new];
    ps.alignment = NSTextAlignmentCenter;
    NSAttributedString *cr = [[NSAttributedString alloc] initWithString:credits attributes:@{
        NSFontAttributeName: [NSFont systemFontOfSize:11], NSParagraphStyleAttributeName: ps,
        NSForegroundColorAttributeName: [NSColor labelColor] }];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp orderFrontStandardAboutPanelWithOptions:@{
        NSAboutPanelOptionApplicationName: @"Nexa Coffee",
        NSAboutPanelOptionApplicationVersion: [NSString stringWithUTF8String:cf_str(lang, CF_STR_VERSION)],
        NSAboutPanelOptionVersion: @"",
        NSAboutPanelOptionCredits: cr }];
    for (NSWindow *w in NSApp.windows) w.delegate = self; /* 닫히면 windowWillClose → exit */
    [NSApp run];
}
@end

static int child_main(int argc, char **argv)
{
    @autoreleasepool {
        NSApplication *nsapp = [NSApplication sharedApplication];
        [nsapp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        [nsapp finishLaunching];
        UiChild *ui = [UiChild new];
        if (!strcmp(argv[1], "--dialog") && argc >= 7) {
            int v[3] = { atoi(argv[3]), atoi(argv[4]), atoi(argv[5]) };
            return [ui runDialog:(!strcmp(argv[2], "auto") ? CF_DLG_AUTO : CF_DLG_CUSTOM) lang:atoi(argv[6]) values:v];
        }
        if (!strcmp(argv[1], "--about") && argc >= 3) { [ui runAbout:atoi(argv[2])]; return 0; }
    }
    return 2;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && argv[1][0] == '-' && argv[1][1] == '-') return child_main(argc, argv);
    /* 상주 메모리 최소화(09-13 실측): libmalloc의 MallocSpaceEfficient=1이 고유 메모리를 7.4 → 5.9 MB로 줄인다.
     * 환경변수는 프로세스 시작 전에 읽히므로, 없으면 세팅하고 자신을 한 번 다시 실행한다(번들은 Info.plist
     * LSEnvironment로도 넣어 두어 보통은 재실행이 일어나지 않는다). */
    if (!getenv("MallocSpaceEfficient") && argc > 0) {
        setenv("MallocSpaceEfficient", "1", 1);
        execv(argv[0], argv); /* 실패하면 그냥 계속 */
    }
    @autoreleasepool {
        NSApplication *nsapp = [NSApplication sharedApplication];
        [nsapp setActivationPolicy:NSApplicationActivationPolicyAccessory];
        Coffee *c = [Coffee new];
        nsapp.delegate = c;
        [nsapp run];
    }
    return 0;
}
