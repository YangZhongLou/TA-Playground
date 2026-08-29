#!/usr/bin/env python3
"""Image-to-voxel: sprite extrude or heightfield columns → MagicaVoxel .vox + BMP previews.

Usage:
    python image-voxel/scripts/image_to_voxels.py --demo sprite
    python image-voxel/scripts/image_to_voxels.py --demo height
    python image-voxel/scripts/image_to_voxels.py art.png --mode extrude --depth 8
    python image-voxel/scripts/image_to_voxels.py top.png --mode height --max-height 32
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "out"

# MagicaVoxel: color index 0 is empty. Palette[0] is unused in RGBA chunk convention:
# RGBA[i] maps to XYZI color index i+1 for i in 0..254.
MAX_PALETTE = 255


# ---------------------------------------------------------------------------
# Demo sprites ('.' = empty)
# ---------------------------------------------------------------------------

SPRITE_GEM = [
    "................",
    "......WWWW......",
    ".....WggggW.....",
    "....WggGGggW....",
    "...WggGGGGGgW...",
    "...WgGGGGGGGW...",
    "...WGGGSGGGGW...",
    "....GGSSSSGG....",
    "....GGSggSGG....",
    ".....GGGGGG.....",
    ".....GggggG.....",
    "......GGGG......",
    "......SGGS......",
    ".......SS.......",
    "................",
    "................",
]

SPRITE_COLORS = {
    "W": (230, 245, 235, 255),
    "g": (120, 200, 160, 255),
    "G": (40, 140, 100, 255),
    "S": (20, 70, 50, 255),
}


def demo_sprite_rgba() -> tuple[int, int, list[tuple[int, int, int, int]]]:
    h = len(SPRITE_GEM)
    w = len(SPRITE_GEM[0])
    pixels = []
    for row in SPRITE_GEM:
        if len(row) != w:
            raise ValueError("sprite rows must be equal length")
        for ch in row:
            pixels.append(SPRITE_COLORS.get(ch, (0, 0, 0, 0)))
    return w, h, pixels


def demo_height_rgba(size: int = 48) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    pixels = []
    cx = cy = (size - 1) / 2.0
    for v in range(size):
        for u in range(size):
            dx = (u - cx) / (size * 0.42)
            dy = (v - cy) / (size * 0.42)
            r = math.sqrt(dx * dx + dy * dy)
            bump = max(0.0, 1.0 - r)
            island = bump * bump
            ridge = math.exp(-((u - size * 0.35) ** 2 + (v - size * 0.4) ** 2) / (2 * 7.0**2))
            h = min(1.0, island * 0.75 + ridge * 0.55)
            if h < 0.05:
                pixels.append((0, 0, 0, 0))
                continue
            t = h
            r8 = int(30 + 40 * t)
            g8 = int(90 + 110 * t)
            b8 = int(70 + 50 * (1.0 - t))
            pixels.append((r8, g8, b8, 255))
    return size, size, pixels


# ---------------------------------------------------------------------------
# Image IO (BMP stdlib; PNG optional via Pillow)
# ---------------------------------------------------------------------------

def load_bmp(path: Path) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise ValueError(f"{path} is not a BMP")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    planes, bpp = struct.unpack_from("<HH", data, 26)
    compression = struct.unpack_from("<I", data, 30)[0]
    if planes != 1 or compression != 0 or bpp not in (24, 32):
        raise ValueError("only uncompressed 24/32-bit BMP is supported")
    if width <= 0:
        raise ValueError("invalid BMP width")
    top_down = height < 0
    height = abs(height)
    row_stride = ((width * bpp + 31) // 32) * 4
    pixels: list[tuple[int, int, int, int]] = [(0, 0, 0, 0)] * (width * height)
    for y in range(height):
        src_y = y if top_down else (height - 1 - y)
        row = pixel_offset + src_y * row_stride
        for x in range(width):
            i = row + x * (bpp // 8)
            b, g, r = data[i], data[i + 1], data[i + 2]
            a = data[i + 3] if bpp == 32 else 255
            pixels[y * width + x] = (r, g, b, a)
    return width, height, pixels


def load_image(path: Path) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    suffix = path.suffix.lower()
    if suffix == ".bmp":
        return load_bmp(path)
    try:
        from PIL import Image
    except ImportError as exc:
        raise SystemExit(
            f"reading {suffix} needs Pillow, or convert to 24-bit BMP"
        ) from exc
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    return w, h, list(im.getdata())


def write_bmp(path: Path, width: int, height: int, rgb: list[tuple[int, int, int]]) -> None:
    row_stride = (width * 3 + 3) & ~3
    payload = bytearray(row_stride * height)
    for y in range(height):
        src_y = height - 1 - y
        for x in range(width):
            r, g, b = rgb[src_y * width + x]
            i = y * row_stride + x * 3
            payload[i : i + 3] = bytes((b, g, r))
    header = struct.pack("<2sIHHI", b"BM", 54 + len(payload), 0, 0, 54)
    info = struct.pack(
        "<IiiHHIIiiII",
        40,
        width,
        height,
        1,
        24,
        0,
        len(payload),
        2835,
        2835,
        0,
        0,
    )
    path.write_bytes(header + info + payload)


# ---------------------------------------------------------------------------
# Palette + voxelize
# ---------------------------------------------------------------------------

def quantize_palette(
    colors: list[tuple[int, int, int, int]],
) -> tuple[list[tuple[int, int, int, int]], dict[tuple[int, int, int], int]]:
    """Return RGBA palette (index 0 unused conceptually) and RGB→XYZI index (1-based)."""
    unique: dict[tuple[int, int, int], int] = {}
    ordered: list[tuple[int, int, int, int]] = []
    for r, g, b, a in colors:
        if a <= 0:
            continue
        key = (r, g, b)
        if key not in unique:
            if len(ordered) >= MAX_PALETTE:
                continue
            ordered.append((r, g, b, 255))
            unique[key] = len(ordered)  # 1-based
    if not ordered:
        ordered = [(200, 200, 200, 255)]
    # Map leftovers to nearest palette color.
    for r, g, b, a in colors:
        if a <= 0:
            continue
        key = (r, g, b)
        if key in unique:
            continue
        best_i = 1
        best_d = 10**9
        for i, (pr, pg, pb, _) in enumerate(ordered, start=1):
            d = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
            if d < best_d:
                best_d = d
                best_i = i
        unique[key] = best_i
    return ordered, unique


def luma(r: int, g: int, b: int) -> float:
    return (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255.0


def downsample(
    width: int,
    height: int,
    pixels: list[tuple[int, int, int, int]],
    max_size: int,
) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    longest = max(width, height)
    if longest <= max_size:
        return width, height, pixels
    scale = longest / max_size
    nw = max(1, int(round(width / scale)))
    nh = max(1, int(round(height / scale)))
    out = []
    for y in range(nh):
        sy = min(height - 1, int(y * scale))
        for x in range(nw):
            sx = min(width - 1, int(x * scale))
            out.append(pixels[sy * width + sx])
    return nw, nh, out


def voxelize_extrude(
    width: int,
    height: int,
    pixels: list[tuple[int, int, int, int]],
    depth: int,
    alpha_cut: int,
) -> tuple[
    tuple[int, int, int],
    list[tuple[int, int, int, int]],
    list[tuple[int, int, int, int]],
]:
    """Front view: X=u, Z=up, Y=thickness."""
    kept = [(i, px) for i, px in enumerate(pixels) if px[3] > alpha_cut]
    palette, index_of = quantize_palette([px for _, px in kept] or pixels)
    voxels = []
    depth = max(1, depth)
    for i, px in kept:
        u = i % width
        v = i // width
        z = height - 1 - v
        c = index_of[(px[0], px[1], px[2])]
        for y in range(depth):
            voxels.append((u, y, z, c))
    size = (width, depth, height)
    return size, voxels, palette


def voxelize_height(
    width: int,
    height: int,
    pixels: list[tuple[int, int, int, int]],
    max_height: int,
    alpha_cut: int,
) -> tuple[tuple[int, int, int], list[tuple[int, int, int, int]], list[tuple[int, int, int, int]]]:
    """Top-down: X=u, Y=v, Z=up from luma."""
    kept_colors = [px for px in pixels if px[3] > alpha_cut]
    palette, index_of = quantize_palette(kept_colors or pixels)
    voxels = []
    max_height = max(1, max_height)
    for i, px in enumerate(pixels):
        if px[3] <= alpha_cut:
            continue
        u = i % width
        v = i // width
        col_h = max(1, int(round(luma(px[0], px[1], px[2]) * max_height)))
        c = index_of[(px[0], px[1], px[2])]
        for z in range(col_h):
            voxels.append((u, v, z, c))
    size = (width, height, max_height)
    return size, voxels, palette


def write_vox(
    path: Path,
    size: tuple[int, int, int],
    voxels: list[tuple[int, int, int, int]],
    palette: list[tuple[int, int, int, int]],
) -> None:
    sx, sy, sz = size
    if max(sx, sy, sz) > 256 or min(sx, sy, sz) < 1:
        raise ValueError(f"MagicaVoxel size must be 1..256 per axis, got {size}")
    if len(voxels) > 256**3:
        raise ValueError("too many voxels")

    def chunk(cid: bytes, content: bytes, children: bytes = b"") -> bytes:
        return cid + struct.pack("<II", len(content), len(children)) + content + children

    size_chunk = chunk(b"SIZE", struct.pack("<iii", sx, sy, sz))
    xyzi = struct.pack("<i", len(voxels)) + b"".join(
        struct.pack("<BBBB", x, y, z, c) for x, y, z, c in voxels
    )
    xyzi_chunk = chunk(b"XYZI", xyzi)
    rgba = bytearray(256 * 4)
    for i in range(256):
        rgba[i * 4 + 3] = 255
    for i, (r, g, b, a) in enumerate(palette[:256]):
        rgba[i * 4 : i * 4 + 4] = bytes((r, g, b, a))
    rgba_chunk = chunk(b"RGBA", bytes(rgba))
    main = chunk(b"MAIN", b"", size_chunk + xyzi_chunk + rgba_chunk)
    path.write_bytes(b"VOX " + struct.pack("<i", 150) + main)


def palette_rgb(palette: list[tuple[int, int, int, int]], index: int) -> tuple[int, int, int]:
    if index <= 0 or index > len(palette):
        return (0, 0, 0)
    r, g, b, _ = palette[index - 1]
    return (r, g, b)


def preview_orthographic(
    size: tuple[int, int, int],
    voxels: list[tuple[int, int, int, int]],
    palette: list[tuple[int, int, int, int]],
) -> tuple[list[tuple[int, int, int]], list[tuple[int, int, int]]]:
    sx, sy, sz = size
    # Top: look down -Z, nearest (highest z) wins.
    top_z = [-1] * (sx * sy)
    top_c = [0] * (sx * sy)
    # Side: look along -Y, nearest (lowest y) wins, image x=X, y=Z flipped for image space.
    side_y = [10**9] * (sx * sz)
    side_c = [0] * (sx * sz)
    for x, y, z, c in voxels:
        ti = y * sx + x
        if z >= top_z[ti]:
            top_z[ti] = z
            top_c[ti] = c
        si = (sz - 1 - z) * sx + x
        if y <= side_y[si]:
            side_y[si] = y
            side_c[si] = c
    bg = (18, 18, 22)
    top = [palette_rgb(palette, c) if c else bg for c in top_c]
    side = [palette_rgb(palette, c) if c else bg for c in side_c]
    return top, side


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Image to MagicaVoxel .vox")
    p.add_argument("image", nargs="?", help="PNG or BMP; omit with --demo")
    p.add_argument("--demo", choices=("sprite", "height"), help="built-in example")
    p.add_argument("--mode", choices=("extrude", "height"), help="required unless --demo")
    p.add_argument("--depth", type=int, default=6, help="extrude thickness in voxels")
    p.add_argument("--max-height", type=int, default=24, help="heightfield column cap")
    p.add_argument("--max-size", type=int, default=128, help="downsample longest image edge")
    p.add_argument("--alpha", type=int, default=16, help="alpha cutoff, 0-255")
    p.add_argument("--out", type=Path, default=None, help="output stem path without suffix")
    return p.parse_args()


def main() -> int:
    args = parse_args()
    if args.demo:
        if args.demo == "sprite":
            width, height, pixels = demo_sprite_rgba()
            mode = "extrude"
        else:
            width, height, pixels = demo_height_rgba()
            mode = "height"
        stem = args.out or (OUT_DIR / f"demo_{args.demo}")
    else:
        if not args.image:
            print("provide an image path or --demo", file=sys.stderr)
            return 2
        if not args.mode:
            print("--mode extrude|height is required without --demo", file=sys.stderr)
            return 2
        path = Path(args.image)
        if not path.is_file():
            print(f"not found: {path}", file=sys.stderr)
            return 2
        width, height, pixels = load_image(path)
        mode = args.mode
        stem = args.out or (OUT_DIR / path.stem)

    width, height, pixels = downsample(width, height, pixels, args.max_size)
    if mode == "extrude":
        size, voxels, palette = voxelize_extrude(
            width, height, pixels, args.depth, args.alpha
        )
    else:
        size, voxels, palette = voxelize_height(
            width, height, pixels, args.max_height, args.alpha
        )

    stem = Path(stem)
    stem.parent.mkdir(parents=True, exist_ok=True)
    vox_path = stem.with_suffix(".vox")
    write_vox(vox_path, size, voxels, palette)
    top, side = preview_orthographic(size, voxels, palette)
    sx, sy, sz = size
    write_bmp(stem.with_name(stem.name + "_top.bmp"), sx, sy, top)
    write_bmp(stem.with_name(stem.name + "_side.bmp"), sx, sz, side)

    print(f"mode     {mode}")
    print(f"image    {width} x {height}")
    print(f"size     {sx} x {sy} x {sz}")
    print(f"voxels   {len(voxels)}")
    print(f"palette  {len(palette)}")
    print(f"wrote    {vox_path}")
    print(f"         {stem.with_name(stem.name + '_top.bmp')}")
    print(f"         {stem.with_name(stem.name + '_side.bmp')}")
    if not voxels:
        print("warning: zero voxels (empty image or alpha cutoff)", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
