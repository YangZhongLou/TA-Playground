"""In-editor validation script for import_generated_asset.py.

Run from the Unreal Editor Python console (Output Log -> Python):

    import importlib
    import tests.test_import_generated_asset as t
    importlib.reload(t)
    t.run_all()

The script uses only engine default assets, so it is safe to run in any map.
If a generated sample mesh exists under ``hunyuan/output/`` the import test
will exercise ``import_mesh``; otherwise that test is skipped.
"""

from __future__ import annotations

import os
from typing import Callable

import unreal

import import_generated_asset as iga


TEST_TAG = "iga_val"


def _project_dir() -> str:
    return unreal.Paths.project_dir()


def _find_sample_mesh() -> str | None:
    """Return the first supported mesh file found under the project."""
    candidates = [
        os.path.join(_project_dir(), "hunyuan", "output"),
        os.path.join(_project_dir(), "Content"),
    ]
    for root in candidates:
        if not os.path.isdir(root):
            continue
        for dirpath, _, files in os.walk(root):
            # Avoid walking deep engine-style derived-data folders.
            if any(skip in dirpath for skip in ("DerivedDataCache", "Intermediate", "Saved")):
                continue
            for filename in files:
                if filename.lower().endswith(iga.SUPPORTED_MESH_EXTENSIONS):
                    return os.path.join(dirpath, filename)
    return None


def test_create_material_instance() -> bool:
    print("[TEST] create_material_instance_from_maps")

    parent_path = "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"
    if not unreal.EditorAssetLibrary.does_asset_exist(parent_path):
        print("  SKIP: DefaultMaterial not available")
        return False

    texture_path = (
        "/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq."
        "Good64x64TilingNoiseHighFreq"
    )
    if not unreal.EditorAssetLibrary.does_asset_exist(texture_path):
        print("  SKIP: default noise texture not available")
        return False

    mic_path = iga.create_material_instance_from_maps(
        name=f"MI_Test_{TEST_TAG}",
        parent_path=parent_path,
        maps={"BaseColor": texture_path, "Emissive": texture_path},
        destination="/Game/Generated/TestMaterials",
    )
    print(f"  Created MIC: {mic_path}")
    return True


def test_setup_actor() -> bool:
    print("[TEST] setup_actor_with_mesh_and_material")

    mesh_path = "/Engine/BasicShapes/Cube.Cube"
    material_path = "/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"

    if not unreal.EditorAssetLibrary.does_asset_exist(mesh_path):
        print("  SKIP: Cube mesh not available")
        return False
    if not unreal.EditorAssetLibrary.does_asset_exist(material_path):
        print("  SKIP: BasicShapeMaterial not available")
        return False

    iga.setup_actor_with_mesh_and_material(
        actor_name=f"TestActor_{TEST_TAG}",
        mesh_path=mesh_path,
        material_path=material_path,
        location=(0.0, 0.0, 200.0),
    )
    print("  Spawned actor")
    return True


def test_import_mesh() -> bool:
    print("[TEST] import_mesh")

    sample = _find_sample_mesh()
    if not sample:
        print("  SKIP: no sample mesh file found in project")
        return False

    print(f"  Importing: {sample}")
    imported = iga.import_mesh(
        sample,
        destination="/Game/Generated/TestMeshes",
    )
    print(f"  Imported to: {imported}")
    return True


def run_all() -> bool:
    tests: list[Callable[[], bool]] = [
        test_create_material_instance,
        test_setup_actor,
        test_import_mesh,
    ]
    results: list[tuple[str, bool]] = []

    for test in tests:
        try:
            results.append((test.__name__, test()))
        except Exception as exc:  # noqa: BLE001
            unreal.log_error(f"{test.__name__} failed: {exc}")
            results.append((test.__name__, False))

    print("\n=== Validation Results ===")
    for name, ok in results:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}")

    passed = sum(1 for _, ok in results if ok)
    print(f"\n{passed}/{len(results)} checks passed")
    return all(ok for _, ok in results)


if __name__ == "__main__":
    run_all()
