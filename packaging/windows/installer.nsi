; Nexa Coffee — Windows 설치본(NSIS · DR-16). 이식 원본: nexa-clip packaging/windows/installer.nsi.
;
; @VERSION@ · @VIVERSION@ 은 release.yml이 치환한다(VIVERSION = "x.y.z.0" — 개발 빌드의 "-dev.<sha>"는 VI에 못 들어간다).
; 로컬: build/nsis/ 에 exe·ico·LICENSE와 치환한 이 파일을 모아 `makensis -INPUTCHARSET UTF8 build/nsis/installer.nsi`
; (docs/18). 산출물 = 스크립트 옆 nexa-coffee-<v>-windows-setup.exe.
;
; 설계
; - **사용자 단위 설치**(HKCU · $LOCALAPPDATA\Programs\NexaCoffee) — 관리자 권한을 요구하지 않는다.
;   `/S` 무인 설치 지원(winget·choco가 설치본을 쓰게 될 때 그대로 통과).
; - x64/x86 exe를 **둘 다 담고** 설치할 때 OS 비트에 맞는 하나를 nexa-coffee.exe로 놓는다 → 릴리스 자산은 setup.exe 하나.
; - **"로그인 시 실행" 섹션(기본 해제)** = HKCU\Software\Microsoft\Windows\CurrentVersion\Run 값 "Nexa Coffee" =
;   "<설치 경로>\nexa-coffee.exe". ★ 값 이름은 src/plat/win.c(RUN_VALUE)와 같다 — 앱의 트레이 메뉴 토글이 같은 값을 읽고 쓴다(DR-15).
;   업그레이드 때는 값이 이미 있으면 체크된 채로 뜬다(진실 원천 = 레지스트리). 무인 설치에서 켜기: `/AUTOSTART=1`.
; - 업그레이드·제거 전에 실행 중 인스턴스를 끝낸다: 숨은 창 클래스 "NexaCoffee"(win.c)에 WM_CLOSE → 앱이 트레이를 지우고 종료.
; - 제거 정보를 레지스트리에 남긴다(설정 > 앱 목록 · winget이 설치 여부·버전을 읽는다). 설정(%APPDATA%\nexa-coffee)은 지우지 않는다.

Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"

!define APPNAME   "Nexa Coffee"
!define COMPANY   "SosomLab"
!define VERSION   "@VERSION@"
!define EXENAME   "nexa-coffee.exe"
!define WNDCLASS  "NexaCoffee"   ; win.c CreateWindowExW 의 클래스·제목 — 둘 다 넘겨 찾는다(제목 생략은 빈 제목과 매칭될 수 있다)
!define WNDTITLE  "Nexa Coffee"
!define APPKEY    "Software\${COMPANY}\NexaCoffee"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\NexaCoffee"
!define RUNKEY    "Software\Microsoft\Windows\CurrentVersion\Run"
!define RUNVALUE  "Nexa Coffee"
; WM_CLOSE 는 MUI2가 끌어오는 WinMessages.nsh 에 이미 있다

Name "${APPNAME} ${VERSION}"
OutFile "nexa-coffee-${VERSION}-windows-setup.exe"
InstallDir "$LOCALAPPDATA\Programs\NexaCoffee"
InstallDirRegKey HKCU "${APPKEY}" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma

VIProductVersion "@VIVERSION@"
VIAddVersionKey "ProductName"     "${APPNAME}"
VIAddVersionKey "CompanyName"     "${COMPANY}"
VIAddVersionKey "FileDescription" "Nexa Coffee Setup - tray timer that keeps the PC awake"
VIAddVersionKey "FileVersion"     "${VERSION}"
VIAddVersionKey "ProductVersion"  "${VERSION}"
VIAddVersionKey "LegalCopyright"  "Copyright (c) 2026 SosomLab. MIT License."

!define MUI_ICON   "nexa-coffee.ico"
!define MUI_UNICON "nexa-coffee.ico"
!define MUI_ABORTWARNING
; 마침 페이지 "실행" = 트레이 상주(windows 서브시스템 · 콘솔 없음)
!define MUI_FINISHPAGE_RUN "$INSTDIR\${EXENAME}"
!define MUI_FINISHPAGE_RUN_PARAMETERS ""

!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; 언어 선택 창 없이 OS 표시 언어를 따른다(앱의 자동 로케일과 같은 정책 · 모르면 첫 항목 English)
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Korean"
!insertmacro MUI_LANGUAGE "Japanese"
!insertmacro MUI_LANGUAGE "SimpChinese"

; 섹션 이름·설명 — 문구는 src/core/app.c의 S_LOGIN("Start at login" · "로그인 시 실행" · "ログイン時に起動" · "登录时启动")과 같게
LangString SEC_APP    ${LANG_ENGLISH}    "Nexa Coffee (required)"
LangString SEC_APP    ${LANG_KOREAN}     "Nexa Coffee (필수)"
LangString SEC_APP    ${LANG_JAPANESE}   "Nexa Coffee (必須)"
LangString SEC_APP    ${LANG_SIMPCHINESE} "Nexa Coffee (必需)"
LangString DESC_APP   ${LANG_ENGLISH}    "The tray timer itself (about 30 KB) and a Start menu shortcut."
LangString DESC_APP   ${LANG_KOREAN}     "트레이 타이머 본체(약 30 KB)와 시작 메뉴 바로가기."
LangString DESC_APP   ${LANG_JAPANESE}   "トレイタイマー本体(約 30 KB)とスタートメニューのショートカット。"
LangString DESC_APP   ${LANG_SIMPCHINESE} "托盘计时器本体(约 30 KB)和开始菜单快捷方式。"
LangString SEC_LOGIN  ${LANG_ENGLISH}    "Start at login"
LangString SEC_LOGIN  ${LANG_KOREAN}     "로그인 시 실행"
LangString SEC_LOGIN  ${LANG_JAPANESE}   "ログイン時に起動"
LangString SEC_LOGIN  ${LANG_SIMPCHINESE} "登录时启动"
LangString DESC_LOGIN ${LANG_ENGLISH}    "Launch Nexa Coffee in the tray when you sign in. You can turn this on or off later from the tray menu."
LangString DESC_LOGIN ${LANG_KOREAN}     "로그인하면 Nexa Coffee를 트레이에 띄웁니다. 나중에 트레이 메뉴에서 켜고 끌 수 있습니다."
LangString DESC_LOGIN ${LANG_JAPANESE}   "サインイン時に Nexa Coffee をトレイに起動します。後からトレイメニューで切り替えられます。"
LangString DESC_LOGIN ${LANG_SIMPCHINESE} "登录时在托盘中启动 Nexa Coffee。之后可在托盘菜单中开关。"

; 실행 중 인스턴스 종료 — 숨은 최상위 창(클래스 NexaCoffee)에 WM_CLOSE. 최대 3초(10×300 ms) 기다린다.
!macro CloseRunning
  StrCpy $1 0
  ${Do}
    FindWindow $0 "${WNDCLASS}" "${WNDTITLE}"
    ${If} $0 == 0
      ${ExitDo}
    ${EndIf}
    SendMessage $0 ${WM_CLOSE} 0 0
    Sleep 300
    IntOp $1 $1 + 1
  ${LoopUntil} $1 >= 10
!macroend

Section "$(SEC_APP)" SecApp
  SectionIn RO
  !insertmacro CloseRunning
  SetOutPath "$INSTDIR"
  ; 설치 파일 이름은 하나(nexa-coffee.exe) — 단일 인스턴스 뮤텍스·Run 값 경로·바로가기가 비트와 무관해진다
  ${If} ${RunningX64}
    File "/oname=${EXENAME}" "nexa-coffee-x64.exe"
  ${Else}
    File "/oname=${EXENAME}" "nexa-coffee-x86.exe"
  ${EndIf}
  File "LICENSE"

  CreateShortcut "$SMPROGRAMS\${APPNAME}.lnk" "$INSTDIR\${EXENAME}"

  WriteRegStr HKCU "${APPKEY}" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${APPKEY}" "Version"    "${VERSION}"

  ; 앱 목록·winget이 읽는 제거 정보. EstimatedSize는 KB.
  WriteRegStr   HKCU "${UNINSTKEY}" "DisplayName"          "${APPNAME}"
  WriteRegStr   HKCU "${UNINSTKEY}" "DisplayVersion"       "${VERSION}"
  WriteRegStr   HKCU "${UNINSTKEY}" "Publisher"            "${COMPANY}"
  WriteRegStr   HKCU "${UNINSTKEY}" "URLInfoAbout"         "https://github.com/SosomLab/nexa-coffee"
  WriteRegStr   HKCU "${UNINSTKEY}" "DisplayIcon"          "$INSTDIR\${EXENAME}"
  WriteRegStr   HKCU "${UNINSTKEY}" "InstallLocation"      "$INSTDIR"
  WriteRegStr   HKCU "${UNINSTKEY}" "UninstallString"      "$\"$INSTDIR\uninstall.exe$\""
  WriteRegStr   HKCU "${UNINSTKEY}" "QuietUninstallString" "$\"$INSTDIR\uninstall.exe$\" /S"
  WriteRegDWORD HKCU "${UNINSTKEY}" "EstimatedSize"        160
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoRepair" 1

  WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

; 기본 해제 — 무인 설치(/S)가 사용자 모르게 켜면 안 된다. 켜려면 체크하거나 /AUTOSTART=1.
Section /o "$(SEC_LOGIN)" SecLogin
  ; 값 이름·형식은 win.c login_set()과 같다: "Nexa Coffee" = "<exe 경로>"(따옴표 포함)
  WriteRegStr HKCU "${RUNKEY}" "${RUNVALUE}" "$\"$INSTDIR\${EXENAME}$\""
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecApp}   $(DESC_APP)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecLogin} $(DESC_LOGIN)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function .onInit
  ; 진실 원천 = 레지스트리: 이미 켜져 있으면(이전 설치본·앱 메뉴) 체크된 채로 시작 → 업그레이드가 설정을 끄지 않고 새 경로로 다시 쓴다
  ReadRegStr $0 HKCU "${RUNKEY}" "${RUNVALUE}"
  ${GetParameters} $R0
  ${GetOptions} $R0 "/AUTOSTART=" $R1
  ${If} $0 != ""
  ${OrIf} $R1 == "1"
    SectionGetFlags ${SecLogin} $0
    IntOp $0 $0 | ${SF_SELECTED}
    SectionSetFlags ${SecLogin} $0
  ${EndIf}
FunctionEnd

Section "Uninstall"
  !insertmacro CloseRunning
  ; 설치가 놓은 파일만 이름으로 지운다(RMDir /r 금지 — 사용자가 둔 것을 지우지 않는다)
  Delete "$INSTDIR\${EXENAME}"
  Delete "$INSTDIR\LICENSE"
  Delete "$INSTDIR\uninstall.exe"
  RMDir  "$INSTDIR"
  Delete "$SMPROGRAMS\${APPNAME}.lnk"
  DeleteRegValue HKCU "${RUNKEY}" "${RUNVALUE}"
  DeleteRegKey HKCU "${UNINSTKEY}"
  DeleteRegKey HKCU "${APPKEY}"
  ; %APPDATA%\nexa-coffee(설정)는 남긴다
SectionEnd
