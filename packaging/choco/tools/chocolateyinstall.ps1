$ErrorActionPreference = 'Stop'
$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$base = 'https://github.com/SosomLab/nexa-coffee/releases/download/v__VERSION__'
Install-ChocolateyZipPackage -PackageName 'nexa-coffee' `
  -Url "$base/nexa-coffee-__VERSION__-windows-x86.zip" -Checksum '__CHECKSUM32__' -ChecksumType 'sha256' `
  -Url64bit "$base/nexa-coffee-__VERSION__-windows-x64.zip" -Checksum64 '__CHECKSUM64__' -ChecksumType64 'sha256' `
  -UnzipLocation $toolsDir
# GUI 프로그램 — shim이 콘솔을 붙잡지 않도록
Get-ChildItem $toolsDir -Filter *.exe | ForEach-Object { New-Item "$($_.FullName).gui" -ItemType File -Force | Out-Null }
