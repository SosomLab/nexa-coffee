# nexa-coffee — 설치본(NSIS setup.exe · 10-17 포터블 패키지 nexa-coffee-portable과 분리 · DR-16).
# setup.exe 하나가 x64·x86를 모두 담는다(설치 시 OS 비트에 맞는 exe를 놓는다) → url 하나.
# 패키지 매개변수 /AutoStart = 설치본의 "로그인 시 실행"(/AUTOSTART=1 · Run 값 "Nexa Coffee").
#   choco install nexa-coffee --params "'/AutoStart'"
$ErrorActionPreference = 'Stop'
$pp = Get-PackageParameters
$silent = '/S'
if ($pp['AutoStart']) { $silent += ' /AUTOSTART=1' }
$packageArgs = @{
  packageName    = 'nexa-coffee'
  fileType       = 'exe'
  url            = 'https://github.com/SosomLab/nexa-coffee/releases/download/v@VERSION@/nexa-coffee-@VERSION@-windows-setup.exe'
  checksum       = '@SHA_WIN_SETUP@'
  checksumType   = 'sha256'
  silentArgs     = $silent
  validExitCodes = @(0)
  softwareName   = 'Nexa Coffee*'
}
Install-ChocolateyPackage @packageArgs
