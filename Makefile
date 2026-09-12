# Nexa Coffee — 빌드 (SSOT: docs/18-build-and-test.md)
#
#   make            # 호스트 OS(macOS/Linux) 네이티브 빌드 → dist/nexa-coffee
#   make win        # Windows x64 + x86 크로스 빌드(mingw-w64) → dist/nexa-coffee-x64.exe · -x86.exe
#   make app        # macOS .app 번들 → dist/Nexa Coffee.app
#   make static     # Linux 정적(musl-gcc) 빌드 → dist/nexa-coffee (배포판 비종속)
#   make test       # 코어 단위 테스트 + 아이콘 덤프(build/icons)
#   make clean

UNAME := $(shell uname -s)
CORE  := $(wildcard src/core/*.c)
VERSION ?= $(shell cat VERSION)

# 공통: 크기 최적화 · 심볼 제거 · 식별 문자열 제거
CFLAGS_MIN := -Os -fno-ident -fno-asynchronous-unwind-tables -fno-stack-protector -ffunction-sections -fdata-sections
WARN := -Wall -Wextra -Wno-unused-parameter

.PHONY: all win app static test clean icons

all: dist/nexa-coffee

dist build build/icons:
	mkdir -p $@

# ── macOS ──
ifeq ($(UNAME),Darwin)
MAC_ARCH ?= -arch arm64 -arch x86_64
dist/nexa-coffee: src/plat/mac.m $(CORE) src/core/coffee.h | dist
	clang -std=gnu99 -fobjc-arc $(CFLAGS_MIN) $(WARN) $(MAC_ARCH) -DCF_VERSION=\"$(VERSION)\" \
	  -o $@ src/plat/mac.m $(CORE) -framework AppKit -framework IOKit -Wl,-dead_strip
	strip $@
	@ls -la $@

app: dist/nexa-coffee packaging/macos/Info.plist packaging/branding/nexa-coffee.icns
	VERSION=$(VERSION) sh packaging/macos/make-app.sh dist/nexa-coffee "dist/Nexa Coffee.app"
endif

# ── Linux ──
ifeq ($(UNAME),Linux)
CC ?= gcc
dist/nexa-coffee: src/plat/linux.c src/plat/dbus.c $(CORE) src/core/coffee.h src/plat/dbus.h | dist
	$(CC) -std=gnu99 $(CFLAGS_MIN) $(WARN) -DCF_VERSION=\"$(VERSION)\" \
	  -o $@ src/plat/linux.c src/plat/dbus.c $(CORE) -Wl,--gc-sections -s
	@ls -la $@

static: | dist
	musl-gcc -std=gnu99 $(CFLAGS_MIN) $(WARN) -static -DCF_VERSION=\"$(VERSION)\" \
	  -o dist/nexa-coffee src/plat/linux.c src/plat/dbus.c $(CORE) -Wl,--gc-sections -s
	@ls -la dist/nexa-coffee
endif

# ── Windows (크로스) — nexa-shortcut Makefile 계승: CRT 미링크 · 진입점 start ──
CC64 := x86_64-w64-mingw32-gcc
CC32 := i686-w64-mingw32-gcc
RES64 := x86_64-w64-mingw32-windres
RES32 := i686-w64-mingw32-windres
WCFLAGS := -std=c99 $(CFLAGS_MIN) -s -mwindows -nostdlib -DUNICODE -D_UNICODE \
           -fno-stack-check -fno-builtin -fno-tree-loop-distribute-patterns -finput-charset=UTF-8 $(WARN)
WLIBS := -lkernel32 -luser32 -lshell32 -lgdi32
WSRC := src/plat/win.c $(CORE)

win: dist/nexa-coffee-x64.exe dist/nexa-coffee-x86.exe

build/rsrc-x64.o: res/nexa-coffee.rc res/nexa-coffee.ico | build
	$(RES64) --include-dir res $< -O coff -o $@
build/rsrc-x86.o: res/nexa-coffee.rc res/nexa-coffee.ico | build
	$(RES32) --include-dir res $< -O coff -o $@
dist/nexa-coffee-x64.exe: $(WSRC) build/rsrc-x64.o | dist
	$(CC64) $(WCFLAGS) -Wl,-e,start -Wl,--gc-sections $(WSRC) build/rsrc-x64.o -o $@ $(WLIBS)
	@ls -la $@
dist/nexa-coffee-x86.exe: $(WSRC) build/rsrc-x86.o | dist
	$(CC32) $(WCFLAGS) -Wl,-e,_start -Wl,--gc-sections $(WSRC) build/rsrc-x86.o -o $@ $(WLIBS) -lgcc
	@ls -la $@

# ── 테스트(호스트) ──
test: | build build/icons
	cc -std=c99 -Wall -Wextra -O1 -o build/test_core test/test_core.c $(CORE)
	./build/test_core
	cc -std=c99 -Wall -Wextra -O1 -o build/icon_dump test/icon_dump.c $(CORE)
	./build/icon_dump build/icons

clean:
	rm -rf build dist
