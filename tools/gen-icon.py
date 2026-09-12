#!/usr/bin/env python3
"""Nexa Coffee 앱 아이콘 SVG 생성기 — packaging/branding/icon.svg 의 원천.

사용자 확정(09-12): 원의 1/3은 실선, 남은 길이의 1/3(= 2/9)은 다른 실선, 나머지(4/9)는 완전한 원(점)들로.
계열 규칙(nexa-beep/clip/dir2): 풀블리드 라운드 스퀘어(rx 232/1024) · 세로 그라디언트 배경 · 흰 전경.
점 위치는 계산해서 박아 넣으므로 이 스크립트가 SSOT다. 사용: python3 tools/gen-icon.py > packaging/branding/icon.svg
"""
import math

CX = CY = 512
R = 296          # 링 중심선 반지름
W = 96           # 링 두께 = 점 지름
GAP = 8.0        # 이음새(도)
A0 = -90.0       # 12시
SEG1 = 120.0     # 1/3 바퀴
SEG2 = 80.0      # 남은 240°의 1/3

def pt(deg):
    a = math.radians(deg)
    return CX + R * math.cos(a), CY + R * math.sin(a)

def arc(start, end, color, opacity=1.0):
    x1, y1 = pt(start); x2, y2 = pt(end)
    large = 1 if end - start > 180 else 0
    return (f'  <path d="M{x1:.1f} {y1:.1f} A{R} {R} 0 {large} 1 {x2:.1f} {y2:.1f}" fill="none" '
            f'stroke="{color}" stroke-width="{W}" stroke-linecap="round" opacity="{opacity}"/>')

s1 = (A0 + GAP / 2, A0 + SEG1 - GAP / 2)
s2 = (A0 + SEG1 + GAP / 2, A0 + SEG1 + SEG2 - GAP / 2)
d0, d1 = A0 + SEG1 + SEG2 + GAP / 2 + 6, A0 + 360 - GAP / 2 - 6
span = d1 - d0
pitch_deg = math.degrees(W * 1.55 / R)
n = max(2, int(span / pitch_deg))
dots = [d0 + span * (2 * i + 1) / (2 * n) for i in range(n)]

print(f'''<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024" role="img" aria-label="Nexa Coffee">
  <!--
    Nexa Coffee 앱 아이콘 (SSOT · tools/gen-icon.py 가 생성 — 손으로 고치지 말 것)
    - SosomLab "Nexa" 계열: 풀블리드 라운드 스퀘어 배경(rx 232/1024) · 세로 그라디언트 · 흰 전경.
    - 모티프: 타이머 링 — 1/3 실선(시간) · 2/9 옅은 실선(분) · 4/9 점선(초 = 흘러가는 순간들).
      트레이의 대기 아이콘(src/core/icon_idle.c)이 같은 모양을 단색으로 그린다.
    - 색: 커피 앰버 #F0A24B → #B5561B — beep 파랑·clip 청록·dir2 초록과 한눈에 구분.
  -->
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#F0A24B"/>
      <stop offset="1" stop-color="#B5561B"/>
    </linearGradient>
  </defs>
  <rect x="0" y="0" width="1024" height="1024" rx="232" ry="232" fill="url(#bg)"/>
  <!-- ① 1/3 실선 -->
{arc(*s1, "#FFFFFF")}
  <!-- ② 남은 길이의 1/3 — 옅은 실선 -->
{arc(*s2, "#FFFFFF", 0.55)}
  <!-- ③ 나머지 — 완전한 원 {n}개 -->''')
for d in dots:
    x, y = pt(d)
    print(f'  <circle cx="{x:.1f}" cy="{y:.1f}" r="{W/2}" fill="#FFFFFF"/>')
print('</svg>')
