# 패키징

**유일한 치환 지점 = [`render-manifests.sh`](render-manifests.sh)** — 릴리스 자산에서 sha256을 직접 계산해 아래 템플릿의 `@VERSION@`·`@SHA_*@`를 채운다(손으로 적은 해시 금지 · nexa-clip 선례).

| 채널 | 파일 | 워크플로 | 게이트 |
| --- | --- | --- | --- |
| GitHub Releases | `.github/workflows/release.yml` — `v*` 태그 → mac universal zip · Windows x64/x86 zip · Linux x64 tar.gz + `SHA256SUMS.txt` | release | — |
| Homebrew(Cask + Linux Formula) | `homebrew/nexa-coffee.rb` · `homebrew/nexa-coffee-portable.rb` → `kiros33/homebrew-tap` | `homebrew.yml`(release가 호출) | 시크릿 `TAP_TOKEN` |
| winget | `winget/*.yaml` → microsoft/winget-pkgs PR(`SosomLab.NexaCoffee` · 포터블 zip) | `publish-windows-packages.yml` | 변수 `WINGET_PUBLISH=true` + 시크릿 `WINGET_TOKEN` · 열린 PR 있으면 대기 |
| Chocolatey | `choco/` → `choco push`(`nexa-coffee` · 포터블 zip · `.gui` shim) | 〃 | 변수 `CHOCO_PUSH=true` + 시크릿 `CHOCO_API_KEY` · 직전 버전 미공개면 대기 |
| macOS .app | `macos/Info.plist` + `macos/make-app.sh` — LSUIElement · ad-hoc 서명 · LSEnvironment(MallocSpaceEfficient) | release | — |
| Linux | `linux/nexa-coffee.desktop` + `install.sh`/`uninstall.sh`(~/.local) | release | — |

설치 명령: `brew install --cask kiros33/tap/nexa-coffee` · `brew install kiros33/tap/nexa-coffee-portable`(Linux) · `winget install SosomLab.NexaCoffee` · `choco install nexa-coffee`.

로컬 점검: 릴리스와 같은 이름의 자산을 한 폴더에 두고 `./packaging/render-manifests.sh 0.1.0 assets out` → `ruby -c out/homebrew/Casks/nexa-coffee.rb`. 수동 재제출은 각 워크플로의 `workflow_dispatch`(tag 입력).
브랜딩 자산은 [`branding/`](branding/README.md). ⚠️ v1은 코드 서명·공증이 없다(Cask가 설치 시 quarantine을 뗀다 · SmartScreen 경고).
