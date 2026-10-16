# 업그레이드/제거 전에 실행 중인 인스턴스를 끝낸다(트레이 상주 · exe 잠금).
# 설치본은 nexa-coffee.exe(NSIS가 WM_CLOSE로도 닫는다) · 0.1.3까지의 포터블 시절 패키지는 nexa-coffee-x64/-x86.
Get-Process -Name 'nexa-coffee','nexa-coffee-x64','nexa-coffee-x86' -ErrorAction SilentlyContinue | Stop-Process -Force
