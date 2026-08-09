#!/usr/bin/env python3
"""Compare a dumped CGB frame (PPM P6/RGB888, see main.c's --ppm option
under --mode cgb) against cgb-acid2's own reference PNG (indexed-color,
PLTE-paletted) pixel-for-pixel.

Sibling to compare_frame.py (DMG's dmg-acid2 gate), but for RGB rather
than grayscale - cgb-acid2's reference.png is a color-type-3 (indexed/
PLTE) PNG, not grayscale, so this needs its own small decoder rather
than reusing compare_frame.py's. No image library dependency, same
reasoning as compare_frame.py.
"""
import sys
import struct
import zlib


def read_png_indexed(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"'{path}' is not a PNG file")

    pos = 8
    width = height = bit_depth = None
    palette = None
    idat = b""
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        ctype = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        if ctype == b"IHDR":
            width, height, bit_depth, color_type = struct.unpack(">IIBB", chunk[:10])
            if color_type != 3:
                raise ValueError(f"'{path}': expected indexed/PLTE color (color type 3), got {color_type}")
        elif ctype == b"PLTE":
            palette = [tuple(chunk[i:i + 3]) for i in range(0, len(chunk), 3)]
        elif ctype == b"IDAT":
            idat += chunk
        pos += 8 + length + 4

    raw = zlib.decompress(idat)
    row_bytes = (width * bit_depth + 7) // 8
    stride = row_bytes + 1
    indices = [[0] * width for _ in range(height)]
    prev = bytearray(row_bytes)
    idx = 0

    def paeth(a, b, c):
        p = a + b - c
        pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
        return a if pa <= pb and pa <= pc else (b if pb <= pc else c)

    for y in range(height):
        ftype = raw[idx]
        row = bytearray(raw[idx + 1:idx + 1 + row_bytes])
        idx += stride
        if ftype == 1:  # Sub
            for i in range(len(row)):
                left = row[i - 1] if i >= 1 else 0
                row[i] = (row[i] + left) & 0xFF
        elif ftype == 2:  # Up
            for i in range(len(row)):
                row[i] = (row[i] + prev[i]) & 0xFF
        elif ftype == 3:  # Average
            for i in range(len(row)):
                left = row[i - 1] if i >= 1 else 0
                row[i] = (row[i] + (left + prev[i]) // 2) & 0xFF
        elif ftype == 4:  # Paeth
            for i in range(len(row)):
                left = row[i - 1] if i >= 1 else 0
                upleft = prev[i - 1] if i >= 1 else 0
                row[i] = (row[i] + paeth(left, prev[i], upleft)) & 0xFF
        prev = row

        bitpos = 0
        for x in range(width):
            byte = row[bitpos // 8]
            shift = 8 - bit_depth - (bitpos % 8)
            mask = (1 << bit_depth) - 1
            indices[y][x] = (byte >> shift) & mask
            bitpos += bit_depth

    return width, height, [[palette[i] for i in row] for row in indices]


def read_ppm_rgb(path):
    with open(path, "rb") as f:
        magic = f.readline().strip()
        if magic != b"P6":
            raise ValueError(f"'{path}': expected a raw RGB PPM (P6)")
        dims = f.readline().split()
        w, h = int(dims[0]), int(dims[1])
        f.readline()  # maxval
        data = f.read(w * h * 3)
    pixels = [[tuple(data[(y * w + x) * 3:(y * w + x) * 3 + 3]) for x in range(w)] for y in range(h)]
    return w, h, pixels


def main():
    if len(sys.argv) < 3:
        print(f"usage: {sys.argv[0]} <output.ppm> <reference.png> [min_match_pct]")
        return 2

    ppm_path, png_path = sys.argv[1], sys.argv[2]
    min_pct = float(sys.argv[3]) if len(sys.argv) > 3 else 95.0

    ref_w, ref_h, ref = read_png_indexed(png_path)
    out_w, out_h, out = read_ppm_rgb(ppm_path)

    if (ref_w, ref_h) != (out_w, out_h):
        print(f"FAIL: dimension mismatch - reference {ref_w}x{ref_h}, output {out_w}x{out_h}")
        return 1

    matches = sum(
        1 for y in range(ref_h) for x in range(ref_w)
        if ref[y][x] == out[y][x]
    )
    total = ref_w * ref_h
    pct = 100.0 * matches / total
    print(f"{matches}/{total} pixels match ({pct:.2f}%)")

    if pct < min_pct:
        print(f"FAIL: match rate below the {min_pct}% baseline - looks like a real PPU regression")
        return 1

    print("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
