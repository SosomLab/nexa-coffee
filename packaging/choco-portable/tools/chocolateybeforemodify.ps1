# 업그레이드/제거 전에 실행 중인 인스턴스를 끝낸다(트레이 상주 · exe 잠금)
Get-Process -Name 'nexa-coffee-x64','nexa-coffee-x86' -ErrorAction SilentlyContinue | Stop-Process -Force
