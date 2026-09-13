$ErrorActionPreference = 'Stop'
$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$base = 'https://github.com/SosomLab/nexa-coffee/releases/download/v@VERSION@'
$packageArgs = @{
  packageName    = 'nexa-coffee'
  unzipLocation  = $toolsDir
  url            = "$base/nexa-coffee-@VERSION@-windows-x86.zip"
  checksum       = '@SHA_WIN_X86@'
  checksumType   = 'sha256'
  url64bit       = "$base/nexa-coffee-@VERSION@-windows-x64.zip"
  checksum64     = '@SHA_WIN_X64@'
  checksumType64 = 'sha256'
}
Install-ChocolateyZipPackage @packageArgs
# GUI 프로그램 — shim이 콘솔을 붙잡지 않도록 .gui 마커(nexa-shortcut 선례)
Get-ChildItem $toolsDir -Filter 'nexa-coffee-*.exe' | ForEach-Object { New-Item "$($_.FullName).gui" -ItemType File -Force | Out-Null }
