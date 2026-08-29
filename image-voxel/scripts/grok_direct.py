#!/usr/bin/env python3
"""What Grok-direct voxel generation actually is: procedural primitives, not photo-to-3D.

    python image-voxel/scripts/grok_direct.py potion
    python image-voxel/scripts/grok_direct.py crate
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPTS))
from image_to_voxels import OUT_DIR, preview_orthographic, write_bmp, write_vox

Grid = list[list[list[int]]]


def empty_grid(n: int) -> Grid:
    return [[[0 for _ in range(n)] for _ in range(n)] for _ in range(n)]


def occ_to_voxels(occ: Grid) -> tuple[tuple[int, int, int], list[tuple[int, int, int, int]]]:
    nx, ny, nz = len(occ), len(occ[0]), len(occ[0][0])
    voxels = []
    for x in range(nx):
        for y in range(ny):
            for z in range(nz):
                c = occ[x][y][z]
                if c:
                    voxels.append((x, y, z, c))
    return (nx, ny, nz), voxels


def build_potion(n: int = 24) -> tuple[Grid, list[tuple[int, int, int, int]]]:
    """Glass flask + liquid + cork. Authored as CSG, not inferred from a photo."""
    occ = empty_grid(n)
    cx = cy = n // 2
    body_z = int(n * 0.40)
    rx, ry, rz = n * 0.34, n * 0.34, n * 0.30
    inner = 0.72
    liquid_top = body_z + int(rz * 0.25)
    neck_r = max(2, n // 10)
    neck_z0 = int(body_z + rz * 0.62)
    neck_z1 = int(n * 0.80)
    cork_z1 = min(n, neck_z1 + max(2, n // 12))
    for x in range(n):
        for y in range(n):
            for z in range(n):
                dx = (x - cx) / rx
                dy = (y - cy) / ry
                dz = (z - body_z) / rz
                r2 = dx * dx + dy * dy + dz * dz
                if r2 <= 1.0:
                    if r2 >= inner * inner:
                        occ[x][y][z] = 4 if (dx < -0.2 and dz > 0.25) else 1
                    elif z <= liquid_top:
                        occ[x][y][z] = 2
                dist2 = (x - cx) ** 2 + (y - cy) ** 2
                if neck_z0 <= z < neck_z1 and (neck_r - 1) ** 2 <= dist2 <= neck_r**2:
                    occ[x][y][z] = 1
                if neck_z1 - 1 <= z < cork_z1 and dist2 <= (neck_r + 1) ** 2:
                    occ[x][y][z] = 3
    palette = [
        (70, 180, 150, 255),
        (20, 90, 70, 255),
        (140, 90, 45, 255),
        (220, 240, 230, 255),
    ]
    return occ, palette


def build_crate(n: int = 16) -> tuple[Grid, list[tuple[int, int, int, int]]]:
    occ = empty_grid(n)
    w = 2
    for x in range(n):
        for y in range(n):
            for z in range(n):
                wall = (
                    x < w
                    or x >= n - w
                    or y < w
                    or y >= n - w
                    or z < w
                    or z >= n - w
                )
                if not wall:
                    continue
                occ[x][y][z] = 2 if (z == n // 2 or x == n // 2) else 1
    palette = [
        (150, 100, 50, 255),
        (90, 55, 25, 255),
    ]
    return occ, palette


SHAPES = {
    "potion": build_potion,
    "crate": build_crate,
}


def main() -> int:
    p = argparse.ArgumentParser(description="Grok-authored procedural voxels")
    p.add_argument("shape", choices=sorted(SHAPES), help="procedural prefab")
    p.add_argument("--out", type=Path, default=None)
    args = p.parse_args()
    occ, palette = SHAPES[args.shape]()
    size, voxels = occ_to_voxels(occ)
    stem = Path(args.out) if args.out else (OUT_DIR / f"grok_{args.shape}")
    stem.parent.mkdir(parents=True, exist_ok=True)
    write_vox(stem.with_suffix(".vox"), size, voxels, palette)
    top, side = preview_orthographic(size, voxels, palette)
    sx, sy, sz = size
    write_bmp(stem.with_name(stem.name + "_top.bmp"), sx, sy, top)
    write_bmp(stem.with_name(stem.name + "_side.bmp"), sx, sz, side)
    print(f"source   grok procedural ({args.shape})")
    print(f"size     {sx} x {sy} x {sz}")
    print(f"voxels   {len(voxels)}")
    print(f"wrote    {stem.with_suffix('.vox')}")
    if not voxels:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
