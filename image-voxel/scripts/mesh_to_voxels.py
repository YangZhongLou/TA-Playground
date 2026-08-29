#!/usr/bin/env python3
"""Voxelize a triangle mesh (GLB/OBJ from DCC) into MagicaVoxel .vox.

Needs trimesh + numpy. DCC mesh only; do not feed image-to-3D outputs from this repo's Hunyuan folder.

    pip install trimesh numpy
    python image-voxel/scripts/mesh_to_voxels.py model.glb --res 64
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

SCRIPTS = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPTS))
from image_to_voxels import OUT_DIR, preview_orthographic, write_bmp, write_vox

try:
    import trimesh
except ImportError as exc:
    raise SystemExit("mesh_to_voxels.py needs trimesh: pip install trimesh") from exc


def load_mesh(path: Path) -> trimesh.Trimesh:
    loaded = trimesh.load(str(path), force="scene")
    geoms = [
        g for g in loaded.geometry.values() if isinstance(g, trimesh.Trimesh)
    ]
    if not geoms:
        raise ValueError(f"no triangle mesh in {path}")
    mesh = geoms[0] if len(geoms) == 1 else trimesh.util.concatenate(geoms)
    if mesh.faces is None or len(mesh.faces) == 0:
        raise ValueError(f"empty mesh in {path}")
    return mesh


def gltf_y_up_to_z_up(mesh: trimesh.Trimesh) -> None:
    """glTF Y-up → MagicaVoxel Z-up (X right, Y depth, Z up)."""
    mesh.apply_transform(
        trimesh.transformations.rotation_matrix(np.pi / 2.0, [1.0, 0.0, 0.0])
    )


def occupancy_to_voxels(
    occ: np.ndarray,
    color_index: int,
) -> tuple[tuple[int, int, int], list[tuple[int, int, int, int]]]:
    nx, ny, nz = (int(s) for s in occ.shape)
    if min(nx, ny, nz) < 1 or max(nx, ny, nz) > 256:
        raise ValueError(f"grid {occ.shape} outside MagicaVoxel 1..256")
    idx = np.argwhere(occ)
    voxels = [(int(x), int(y), int(z), color_index) for x, y, z in idx]
    return (nx, ny, nz), voxels


def voxelize_mesh(
    mesh: trimesh.Trimesh,
    res: int,
    fill: bool,
) -> np.ndarray:
    longest = float(np.max(mesh.extents))
    if longest <= 0:
        raise ValueError("degenerate AABB")
    pitch = longest / float(res)
    grid = mesh.voxelized(pitch=pitch)
    if fill:
        grid = grid.copy()
        grid.fill()
    return np.asarray(grid.matrix, dtype=bool)


def parse_color(text: str) -> tuple[int, int, int]:
    parts = [int(p.strip()) for p in text.split(",")]
    if len(parts) != 3 or any(c < 0 or c > 255 for c in parts):
        raise argparse.ArgumentTypeError("color must be R,G,B in 0-255")
    return parts[0], parts[1], parts[2]


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Mesh to MagicaVoxel .vox")
    p.add_argument("mesh", help="GLB / GLTF / OBJ")
    p.add_argument("--res", type=int, default=64, help="voxels along longest AABB axis")
    p.add_argument("--shell", action="store_true", help="surface only, do not fill interior")
    p.add_argument("--no-zup", action="store_true", help="do not rotate glTF Y-up to Z-up")
    p.add_argument("--color", type=parse_color, default=(40, 140, 100), help="R,G,B")
    p.add_argument("--out", type=Path, default=None, help="output stem without suffix")
    return p.parse_args()


def main() -> int:
    args = parse_args()
    if args.res < 4 or args.res > 256:
        print("--res must be 4..256", file=sys.stderr)
        return 2
    path = Path(args.mesh)
    if not path.is_file():
        print(f"not found: {path}", file=sys.stderr)
        return 2

    mesh = load_mesh(path)
    faces = len(mesh.faces)
    if not args.no_zup:
        gltf_y_up_to_z_up(mesh)
    occ = voxelize_mesh(mesh, args.res, fill=not args.shell)
    size, voxels = occupancy_to_voxels(occ, color_index=1)
    palette = [(*args.color, 255)]

    stem = Path(args.out) if args.out else (OUT_DIR / (path.stem + f"_r{args.res}"))
    stem.parent.mkdir(parents=True, exist_ok=True)
    vox_path = stem.with_suffix(".vox")
    write_vox(vox_path, size, voxels, palette)
    top, side = preview_orthographic(size, voxels, palette)
    sx, sy, sz = size
    write_bmp(stem.with_name(stem.name + "_top.bmp"), sx, sy, top)
    write_bmp(stem.with_name(stem.name + "_side.bmp"), sx, sz, side)

    print(f"mesh     {path}")
    print(f"faces    {faces}")
    print(f"fill     {not args.shell}")
    print(f"size     {sx} x {sy} x {sz}")
    print(f"voxels   {len(voxels)}")
    print(f"wrote    {vox_path}")
    print(f"         {stem.with_name(stem.name + '_top.bmp')}")
    print(f"         {stem.with_name(stem.name + '_side.bmp')}")
    if not voxels:
        print("warning: zero voxels", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
