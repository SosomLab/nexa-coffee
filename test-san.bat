@echo off
rem Nexa Coffee - Windows runtime security test (MSVC AddressSanitizer). T-24, docs/19.
rem Windows counterpart of `make test-san` (Linux/macOS: ASan+UBSan). Builds the core unit
rem tests and the icon dump with /fsanitize=address and runs them. MSVC has no UBSan, so this
rem is ASan only - it still adds LLP64 (32-bit long) and a second compiler's integer promotion.
rem test_dbus is not built here: src/plat/dbus.c is POSIX sockets (Linux only).
rem If cl is not on PATH, vswhere + vcvars64 are used (no Native Tools prompt needed).
rem Comments are ASCII on purpose: cmd.exe misparses UTF-8 comment lines in some codepages.
setlocal
where cl >nul 2>nul
if not errorlevel 1 goto have_cl
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_cl
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT goto no_cl
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
where cl >nul 2>nul
if errorlevel 1 goto no_cl
:have_cl
if not exist build mkdir build
if not exist build\icons mkdir build\icons
if not exist build\san mkdir build\san
set CORE=src\core\app.c src\core\draw.c src\core\font.c src\core\icon_active.c src\core\icon_idle.c src\core\timer.c src\core\util.c
set SAN=/nologo /utf-8 /std:c11 /W3 /Od /Z7 /MD /D_CRT_SECURE_NO_WARNINGS /fsanitize=address

cl %SAN% test\test_core.c %CORE% /Fo:build\san\ /Fe:build\test_core_san.exe
if errorlevel 1 exit /b 1
build\test_core_san.exe
if errorlevel 1 exit /b 1

cl %SAN% test\icon_dump.c %CORE% /Fo:build\san\ /Fe:build\icon_dump_san.exe
if errorlevel 1 exit /b 1
build\icon_dump_san.exe build\icons
if errorlevel 1 exit /b 1

echo.
echo TEST-SAN OK (MSVC ASan: test_core + icon_dump)
exit /b 0

:no_cl
echo [error] cl.exe not found. Install VS Build Tools (C++), or run from "x64 Native Tools Command Prompt for VS".
exit /b 1
