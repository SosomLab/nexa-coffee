#!/usr/bin/env python3
"""PNG 프레임을 그대로 담는 ICO 패커(stdlib만). Vista 이후 Windows는 어떤 크기든 PNG 프레임을 읽는다.
BMP 프레임(48px 3장 = 15 KB)보다 3~4배 작다 — exe 크기의 40%가 아이콘이었다(09-13).
사용: python3 tools/make-ico.py out.ico 16.png 32.png 48.png"""
import struct, sys

def main():
    out, pngs = sys.argv[1], sys.argv[2:]
    frames = []
    for p in pngs:
        data = open(p, 'rb').read()
        assert data[:8] == b'\x89PNG\r\n\x1a\n', p
        w, h = struct.unpack('>II', data[16:24])
        frames.append((w, h, data))
    hdr = struct.pack('<HHH', 0, 1, len(frames))
    off = 6 + 16 * len(frames)
    entries, blobs = b'', b''
    for w, h, data in frames:
        entries += struct.pack('<BBBBHHII', w % 256, h % 256, 0, 0, 1, 32, len(data), off + len(blobs))
        blobs += data
    open(out, 'wb').write(hdr + entries + blobs)
    print(f"{out}: {len(frames)} frames, {6 + 16 * len(frames) + len(blobs)} bytes")

main()
