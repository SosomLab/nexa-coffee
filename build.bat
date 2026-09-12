@echo off
rem Nexa Coffee — Windows 네이티브 빌드(Visual Studio / MSVC). nexa-shortcut build.bat 계승.
rem "x64 Native Tools Command Prompt for VS"에서 실행. CRT 미링크(/NODEFAULTLIB + /ENTRY:start).
setlocal
where cl >nul 2>nul
if errorlevel 1 (
    echo [오류] cl.exe 를 찾을 수 없습니다. "x64 Native Tools Command Prompt for VS" 에서 실행하세요.
    exit /b 1
)
if not exist build mkdir build
if not exist dist  mkdir dist
rc /nologo /fo build\nexa-coffee.res res\nexa-coffee.rc
if errorlevel 1 exit /b 1
set /p VERSION=<VERSION
cl /nologo /utf-8 /O1 /GS- /Oi- /DUNICODE /D_UNICODE /DCF_VERSION=\"%VERSION%\" ^
   src\plat\win.c src\core\app.c src\core\draw.c src\core\font.c src\core\icon_active.c ^
   src\core\icon_idle.c src\core\timer.c src\core\util.c build\nexa-coffee.res ^
   /Fo:build\ /Fe:dist\nexa-coffee-x64.exe ^
   /link /SUBSYSTEM:WINDOWS /ENTRY:start /NODEFAULTLIB /OPT:REF /OPT:ICF ^
   kernel32.lib user32.lib shell32.lib gdi32.lib
if errorlevel 1 exit /b 1
echo.
echo 빌드 완료: dist\nexa-coffee-x64.exe
