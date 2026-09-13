# 18 · 빌드 · 테스트 (SSOT)

## 빌드

| 대상 | 명령 | 도구 | 산출물 |
| --- | --- | --- | --- |
| macOS(유니버설) | `make` · `make app` | Xcode CLT(clang) | `dist/nexa-coffee` · `dist/Nexa Coffee.app` |
| Linux(동적) | `make` | gcc | `dist/nexa-coffee` |
| Linux(정적 · 배포용) | `make static` | `musl-tools` | `dist/nexa-coffee`(libc 하나 · 배포판 비종속) |
| Windows(크로스) | `make win` | mingw-w64 | `dist/nexa-coffee-x64.exe` · `-x86.exe` |
| Windows(MSVC) | `build.bat` | VS x64 Native Tools | `dist\nexa-coffee-x64.exe` |

플래그 원칙(nexa-shortcut 계승): `-Os` · `-fno-ident` · `-fno-asynchronous-unwind-tables` · `-fno-stack-protector` · 섹션 GC · strip.
Windows는 `-nostdlib` + `-fno-builtin -fno-tree-loop-distribute-patterns`(코어의 바이트 루프가 memset 호출로 되접히는 것 방지) + 진입점 `start`.
x86은 64비트 나눗셈 헬퍼 때문에 `-lgcc`를 정적으로 붙인다.

## 테스트

| 단계 | 명령 | 검사 |
| --- | --- | --- |
| 코어 단위 | `make test` | util · 표시 규칙 · 다음 변화 시각 · 메뉴 트리/클릭 · 설정 왕복 · 툴팁 · 아이콘 결정성/경계 |
| 아이콘 육안 | `make test` → `build/icons/*.pam` | `magick x.pam -background '#1e1e1e' -flatten -filter point -resize 800% x.png` |
| Linux D-Bus 계약 | `sh scripts/linux-smoke.sh dist/nexa-coffee` | 세션 버스 위에서 SNI 속성 · dbusmenu 레이아웃/속성 · Event 클릭 → 작업/설정 · 종료 |
| macOS 실기 | `make app && open "dist/Nexa Coffee.app"` | 메뉴바 아이콘 · `pmset -g assertions \| grep Coffee` · `ps -o rss` · 창 확인은 `NEXA_COFFEE_SHOW=custom\|auto\|about dist/nexa-coffee` |
| Windows 실기 | exe 실행 | 트레이 · 우클릭 메뉴 · `powercfg /requests`(SYSTEM/DISPLAY에 nexa-coffee) |

`linux-smoke.sh`는 시스템 세션 버스를 쓰지 않고 `scripts/dbus-test.conf`로 전용 dbus-daemon을 띄운다(macOS launchd·CI 컨테이너 공통). macOS는 `brew install dbus`.
Docker가 있으면 `scripts/linux-docker.sh`가 Linux 컨테이너에서 빌드+정적+스모크를 한 번에 돌린다.

## 메모리 실측(macOS)
`footprint --pid <pid>`의 `phys_footprint`가 기준(RSS는 공유 캐시 포함). 실험 절차는 journal 09-13 2차. 아이콘·리소스 크기는 `ls -la res/ packaging/branding/`.

## CI(.github/workflows)

- `ci.yml`: ubuntu(테스트 · musl 정적 · D-Bus 스모크 · mingw 크로스 · **크기 예산** Windows ≤ 64 KB / Linux ≤ 128 KB) · macos(테스트 · 유니버설 · .app · ≤ 256 KB) · windows(MSVC).
- `release.yml`: `v*` 태그 → mac universal zip · Windows x64/x86 zip · Linux tar.gz + `SHA256SUMS.txt` → GitHub Release → `homebrew.yml`(탭) · `publish-windows-packages.yml`(winget PR · choco push). 태그 = `VERSION` 파일과 일치해야 한다.
- 릴리스 절차: `VERSION` 갱신 → main green → `git tag vX.Y.Z && git push origin vX.Y.Z`(공개 행위 · 사용자 승인) → `gh run watch` → 탭 커밋 · winget PR · choco 피드 확인(검수 며칠). 재제출은 각 워크플로 `workflow_dispatch`.
