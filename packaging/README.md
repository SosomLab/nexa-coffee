# 패키징

| 채널 | 파일 | 상태 |
| --- | --- | --- |
| GitHub Releases | `.github/workflows/release.yml` — `v*` 태그 push 시 5 산출물(mac universal zip · Windows x64/x86 zip · Linux x64 tar.gz) + `SHA256SUMS.txt` | ✅ |
| macOS .app | `macos/Info.plist` + `macos/make-app.sh` — LSUIElement(메뉴바 전용) · ad-hoc 서명 | ✅ |
| Homebrew Cask | `homebrew/nexa-coffee.rb` — `kiros33/tap`에 복사(자동화는 후속) | 📐 템플릿 |
| winget | `winget/*.yaml` — 포터블(zip) · nexa-shortcut 선례 | 📐 템플릿 |
| Chocolatey | `choco/` — 포터블 zip · `.gui` shim | 📐 템플릿 |
| Linux | `linux/nexa-coffee.desktop` + `install.sh`/`uninstall.sh`(~/.local) | ✅ |

브랜딩 자산은 [`branding/`](branding/README.md). ⚠️ v1은 코드 서명·공증이 없다(Gatekeeper·SmartScreen 경고).
