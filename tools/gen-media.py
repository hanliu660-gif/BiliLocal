# -*- coding: utf-8 -*-
# gen-media.py - generate the local hook-gate media fixture on demand.
# Output: <dir>/sample-60s.y4m (raw YUV4MPEG2 video, libvlc plays it natively,
#         no transcoder needed on this machine) and <dir>/sample-60s.xml
#         (dense mode-1 danmaku; auto-loaded via the %{File}.xml template).
# Usage:  python gen-media.py [outdir]     (default: tools\.media next to this file)

import os
import sys


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '.media')
    os.makedirs(out_dir, exist_ok=True)

    w, h, fps, sec = 320, 180, 15, 60
    frames = fps * sec

    y4m_path = os.path.join(out_dir, 'sample-60s.y4m')
    with open(y4m_path, 'wb') as f:
        f.write(b'YUV4MPEG2 W%d H%d F%d:1 Ip A1:1 C420jpeg\n' % (w, h, fps))
        row = bytearray(w)
        uv_size = (w // 2) * (h // 2)
        for t in range(frames):
            f.write(b'FRAME\n')
            # moving luminance bars: visible motion without any codec
            shift = (t * 3) % w
            for x in range(w):
                row[x] = 16 + ((x + shift) % 224)
            y_row = bytes(row)
            f.write(y_row * h)
            uv = 128 + ((t % 32) - 16)
            f.write(bytes([uv]) * uv_size)
            f.write(bytes([256 - uv]) * uv_size)

    xml_path = os.path.join(out_dir, 'sample-60s.xml')
    lines = ['<?xml version="1.0" encoding="UTF-8"?><i>']
    i = 0
    t = 1.0
    # one mode-1 comment every 0.05s (~20/s): Running's thinnest-lane
    # allocator fills lanes top-down, so the (600,320) hold point is only
    # occupied once concurrency pushes lanes past y=320 (~128 on screen)
    while t < sec - 1.0:
        lines.append('<d p="%.3f,1,25,16777215,1792731812,0,UID%d,0">gate %d</d>' % (t, i, i))
        t += 0.05
        i += 1
    lines.append('</i>')
    with open(xml_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))

    print('written: %s (%d bytes)' % (y4m_path, os.path.getsize(y4m_path)))
    print('written: %s (%d comments)' % (xml_path, i))


if __name__ == '__main__':
    main()
