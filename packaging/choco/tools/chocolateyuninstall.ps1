# nexa-coffee — 설치본 제거. 제거 정보(HKCU\...\Uninstall\NexaCoffee · installer.nsi)의 uninstall.exe를 /S로 부른다.
# 설정(%APPDATA%\nexa-coffee)은 설치본 제거와 마찬가지로 남는다.
$ErrorActionPreference = 'Stop'
[array]$keys = Get-UninstallRegistryKey -SoftwareName 'Nexa Coffee'
foreach ($k in $keys) {
  Uninstall-ChocolateyPackage -PackageName 'nexa-coffee' -FileType 'exe' -SilentArgs '/S' -File ($k.UninstallString -replace '"', '')
}
if ($keys.Count -eq 0) { Write-Warning 'Nexa Coffee 제거 정보를 찾지 못했다 — 이미 제거됐을 수 있다.' }
