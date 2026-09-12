# Nexa Coffee 브랜딩 자산

앱 아이콘의 SSOT는 **`tools/gen-icon.py`**(→ `icon.svg`). 점 위치를 계산해 넣으므로 SVG를 손으로 고치지 않는다.

## 계열 안에서의 자리

| 앱 | 모티프 | 색 |
| --- | --- | --- |
| nexa-dir2 | 폴더 + `>_` | 다크 네이비 + 초록 액센트 |
| nexa-beep | 말풍선 + 파동 | 파랑 `#4A97FF → #2C6BE6` |
| nexa-clip | 클립보드 + 카드 스택 | 청록 `#22C3D6 → #0B7FA6` |
| ★ **nexa-coffee** | **타이머 링** — 1/3 실선 · 2/9 옅은 실선 · 4/9 점 | **커피 앰버** `#F0A24B → #B5561B` |

계열 공통 = 풀블리드 라운드 스퀘어(반경 232/1024) · 세로 그라디언트 · 흰 전경. 트레이의 **대기 아이콘**(`src/core/icon_idle.c`)이 이 링을 단색으로 그린다.

## 파일

| 파일 | 용도 |
| --- | --- |
| `icon.svg` | 벡터 원본(생성물) |
| `nexa-coffee-1024/256/64.png` | 스토어·Linux hicolor·문서 |
| `nexa-coffee.ico` | Windows 리소스(16·32·48 **PNG 프레임** · `tools/make-ico.py` · 3.7 KB · `res/`에 복사). BMP 프레임이면 15 KB — exe의 40%였다 |
| `nexa-coffee.icns` | macOS 번들(16·32·128·256 + @2x = 최대 512px · 62 KB). 1024px 프레임은 번들 크기만 키워 뺐다 |

## 재생성

```bash
python3 tools/gen-icon.py > packaging/branding/icon.svg
cd packaging/branding
for s in 1024 256 64; do rsvg-convert -w $s -h $s icon.svg -o nexa-coffee-$s.png; done
for s in 16 32 48; do rsvg-convert -w $s -h $s icon.svg -o /tmp/i$s.png; done
python3 ../../tools/make-ico.py nexa-coffee.ico /tmp/i16.png /tmp/i32.png /tmp/i48.png && cp nexa-coffee.ico ../../res/
mkdir -p /tmp/nc.iconset && for s in 16 32 128 256; do rsvg-convert -w $s -h $s icon.svg -o /tmp/nc.iconset/icon_${s}x${s}.png; rsvg-convert -w $((s*2)) -h $((s*2)) icon.svg -o /tmp/nc.iconset/icon_${s}x${s}@2x.png; done
iconutil -c icns /tmp/nc.iconset -o nexa-coffee.icns
```
