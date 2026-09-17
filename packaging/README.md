# 패키징

**유일한 치환 지점 = [`render-manifests.sh`](render-manifests.sh)** — 릴리스 자산에서 sha256을 직접 계산해 아래 템플릿의 `@VERSION@`·`@SHA_*@`를 채운다(손으로 적은 해시 금지 · nexa-clip 선례).

| 채널 | 파일 | 워크플로 | 게이트 |
| --- | --- | --- | --- |
| GitHub Releases | `.github/workflows/release.yml` — `v*` 태그 → mac universal zip · Windows x64/x86 zip · Linux x64 tar.gz + `SHA256SUMS.txt` | release | — |
| Homebrew(Cask + Linux Formula) | `homebrew/nexa-coffee.rb` · `homebrew/nexa-coffee-portable.rb` → `kiros33/homebrew-tap` | `homebrew.yml`(release가 호출) | 시크릿 `TAP_TOKEN` |
| winget | `winget/*.yaml` → microsoft/winget-pkgs PR(`SosomLab.NexaCoffee` · 포터블 zip) | `publish-windows-packages.yml` | 변수 `WINGET_PUBLISH=true` + 시크릿 `WINGET_TOKEN` · 열린 PR 있으면 대기 · `channels`로 채널 선택 |
| Chocolatey | `choco/` → `choco push`(`nexa-coffee` · 포터블 zip · `.gui` shim) | 〃 | 변수 `CHOCO_PUSH=true` + 시크릿 `CHOCO_API_KEY` · 직전 버전 미공개면 대기 |
| macOS .app | `macos/Info.plist` + `macos/make-app.sh` — LSUIElement · ad-hoc 서명 · LSEnvironment(MallocSpaceEfficient) | release | — |
| Linux | `linux/nexa-coffee.desktop` + `install.sh`/`uninstall.sh`(~/.local) | release | — |

설치 명령: `brew install --cask kiros33/tap/nexa-coffee` · `brew install kiros33/tap/nexa-coffee-portable`(Linux) · `winget install SosomLab.NexaCoffee` · `choco install nexa-coffee`.

로컬 점검: 릴리스와 같은 이름의 자산을 한 폴더에 두고 `./packaging/render-manifests.sh 0.1.1 assets out` → `ruby -c out/homebrew/Casks/nexa-coffee.rb`. 수동 재제출은 각 워크플로의 `workflow_dispatch`(tag 입력).

## 검수 지적을 고쳐 다시 올릴 때

| 채널 | 절차 |
| --- | --- |
| choco | 같은 버전을 그대로 다시 push한다. `publish-windows-packages` 를 **`channels=choco force=true`** 로 dispatch(`force` 없이는 "직전 제출 검수 대기"로 건너뛰고, `channels` 없이는 winget PR이 중복으로 또 열린다). 검수 상태·모더레이터 코멘트는 **공개 API가 없고** 패키지 페이지 HTML에만 있다 — API 키로 할 수 있는 건 push뿐이다. |
| winget | PR은 **버전 단위**다. 지적이 매니페스트 문제면 그 PR 브랜치를 고쳐 push하고, 바이너리를 바꿔야 하면 **새 버전을 내고 옛 PR은 닫는다**. |

⚠️ **winget 자동 제출에는 PAT `workflow` 스코프가 필요하다** — `wingetcreate submit`은 포크 master가 upstream과 어긋나면 거부하는데(릴리스 사이에 수천 커밋 벌어진다), upstream 커밋이 `.github/workflows/`를 건드려 동기화에 그 스코프가 든다.
없을 때의 우회(09-17 0.1.1에 쓴 방법 — `manifests/…`만 건드리므로 `repo` 스코프면 된다):

```sh
gh api -X POST repos/<me>/winget-pkgs/git/refs -f ref=refs/heads/<ID>-<버전>        -f sha=$(gh api repos/<me>/winget-pkgs/git/ref/heads/master --jq .object.sha)
# 매니페스트 3개를 contents API로 PUT 후
gh pr create --repo microsoft/winget-pkgs --base master --head <me>:<ID>-<버전> ...
```

⚠️ **Windows 백신 오탐** — 09-17 실측: Defender가 **x86 zip만** `Trojan:Win32/Tecabans.STV!cl`(클라우드 ML)로 격리해 winget 설치 검증이 실패했다(x64는 통과 · 로컬 `MpCmdRun` 스캔은 깨끗). 재현:
`winget settings --enable LocalManifestFiles` 후 `winget install --manifest <폴더> --architecture x86` → Defender 이벤트 로그 ID 1116/1117. 완화책으로 PE 버전 리소스를 넣었고(`res/nexa-coffee.rc` · 숫자는 VERSION 단일 출처), 그래도 잡히면 [WDSI 오탐 신고](https://www.microsoft.com/en-us/wdsi/filesubmission). 근본 해결은 코드 서명.
브랜딩 자산은 [`branding/`](branding/README.md). ⚠️ v1은 코드 서명·공증이 없다(Cask가 설치 시 quarantine을 뗀다 · SmartScreen 경고).
