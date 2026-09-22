#!/usr/bin/env python3
"""Write a small, deterministic RGBA PNG with four solid quadrants."""
import struct
import sys
import zlib


def chunk(kind, payload):
    return (struct.pack(">I", len(payload)) + kind + payload +
            struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff))


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: png_fixture.py OUTPUT")
    width, height = 3840, 2160
    # Large constant areas make the sampled pixel stable across browser
    # implementations while still exercising a real 4K decode and resize.
    colors = ((220, 30, 40, 255), (30, 220, 60, 255),
              (40, 70, 220, 255), (220, 190, 30, 255))
    rows = bytearray()
    half_w, half_h = width // 2, height // 2
    for y in range(height):
        top = y < half_h
        row = bytearray()
        row.extend(bytes(colors[0 if top else 2]) * half_w)
        row.extend(bytes(colors[1 if top else 3]) * half_w)
        rows.append(0)
        rows.extend(row)
    raw = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(bytes(rows), 9)) +
           chunk(b"IEND", b""))
    with open(sys.argv[1], "wb") as out:
        out.write(raw)


if __name__ == "__main__":
    main()
