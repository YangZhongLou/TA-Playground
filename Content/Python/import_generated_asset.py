"""UE Editor Python utilities for importing AI-generated assets.

This module is meant to run inside the Unreal Editor Python environment
(PythonScriptPlugin + EditorScriptingUtilities).

It provides three building blocks for the AI VFX pipeline:
    - import_mesh              : import GLB/OBJ/FBX into Content
    - create_material_instance_from_maps : create a MIC from texture maps
    - setup_actor_with_mesh_and_material : spawn a StaticMeshActor in the level
"""

from __future__ import annotations

import json
import os
from pathlib import Path

import unreal


# Maps common semantic map names to the texture parameter names expected by the
# generated master materials (see Phase 1.1).  If a key is not recognised, it is
# passed through as-is, allowing callers to use exact parameter names.
DEFAULT_MAP_PARAMETERS: dict[str, str] = {
    # --- Default Lit / Opaque / Masked ---
    "BaseColor": "BaseColor",
    "basecolor": "BaseColor",
    "BaseColorMap": "BaseColor",
    "Albedo": "BaseColor",
    "albedo": "BaseColor",
    "Diffuse": "BaseColor",
    "diffuse": "BaseColor",
    "Normal": "Normal",
    "normal": "Normal",
    "NormalMap": "Normal",
    "Roughness": "Roughness",
    "roughness": "Roughness",
    "RoughnessMap": "Roughness",
    "Metallic": "Metallic",
    "metallic": "Metallic",
    "MetallicMap": "Metallic",
    # --- Translucent / Unlit / Masked extras ---
    "Opacity": "Opacity",
    "opacity": "Opacity",
    "OpacityMap": "Opacity",
    "Alpha": "Opacity",
    "alpha": "Opacity",
    "Emissive": "Emissive",
    "emissive": "Emissive",
    "EmissiveColor": "Emissive",
    "EmissiveMap": "Emissive",
    "OpacityMask": "OpacityMask",
    "opacitymask": "OpacityMask",
    "OpacityMaskMap": "OpacityMask",
}

SUPPORTED_MESH_EXTENSIONS: tuple[str, ...] = (".fbx", ".obj", ".glb", ".gltf")


def _semantic_to_param_name(semantic: str) -> str:
    """Return the material parameter name for a semantic map name."""
    return DEFAULT_MAP_PARAMETERS.get(semantic, semantic)


def _ensure_directory(package_path: str) -> None:
    """Create the Content directory if it does not already exist."""
    if not unreal.EditorAssetLibrary.does_directory_exist(package_path):
        unreal.EditorAssetLibrary.make_directory(package_path)


def import_mesh(
    file_path: str,
    destination: str = "/Game/Generated/Meshes",
) -> str:
    """Import a static mesh file into the Content Browser.

    Args:
        file_path: Absolute path to the mesh file (GLB, OBJ, FBX, glTF).
        destination: Content path to import into, e.g. ``/Game/Generated/Meshes``.

    Returns:
        The imported StaticMesh asset path, e.g. ``/Game/Generated/Meshes/SM_Foo``.

    Raises:
        FileNotFoundError: If ``file_path`` does not exist.
        ValueError: If the file extension is not supported.
        RuntimeError: If the import task produces no assets.
    """
    if not os.path.isfile(file_path):
        raise FileNotFoundError(f"Mesh file not found: {file_path}")

    ext = os.path.splitext(file_path)[1].lower()
    if ext not in SUPPORTED_MESH_EXTENSIONS:
        raise ValueError(
            f"Unsupported mesh format '{ext}'. Supported: {SUPPORTED_MESH_EXTENSIONS}"
        )

    filename = os.path.basename(file_path)
    asset_name = os.path.splitext(filename)[0]

    _ensure_directory(destination)

    task = unreal.AssetImportTask()
    task.set_editor_property("automated", True)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("filename", file_path)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    # Let Unreal choose the correct factory based on the file extension.
    task.set_editor_property("factory", None)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset_tools.import_asset_tasks([task])

    imported_object_paths = task.get_editor_property("imported_object_paths")
    if not imported_object_paths:
        raise RuntimeError(f"Import produced no assets for {file_path}")

    # Prefer the StaticMesh when formats such as GLB also produce textures.
    for path in imported_object_paths:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if isinstance(asset, unreal.StaticMesh):
            unreal.log(f"Imported StaticMesh to {path}")
            return str(path)

    main_path = str(imported_object_paths[0])
    unreal.log_warning(f"No StaticMesh found in imported assets; returning {main_path}")
    return main_path


def create_material_instance_from_maps(
    name: str,
    parent_path: str,
    maps: dict[str, str],
    destination: str = "/Game/Generated/Materials",
    scalar_parameters: dict[str, float] | None = None,
    vector_parameters: dict[str, list[float]] | None = None,
    reuse: bool = True,
) -> str:
    """Create a Material Instance Constant and assign texture maps.

    Args:
        name: Name of the new MIC asset.
        parent_path: Path to the parent Material, e.g.
            ``/Game/Materials/Generated/M_Generated_Opaque``.
        maps: Mapping from semantic map name (e.g. ``BaseColor``) to texture
            asset path. Unknown keys are used as literal parameter names.
        destination: Content path to create the MIC in.
        scalar_parameters: Optional scalar overrides, e.g. ``{"Roughness": 0.8}``.
        vector_parameters: Optional vector/color overrides, e.g.
            ``{"EmissiveColor": [1.0, 0.5, 0.0]}``.
        reuse: If True and the target MIC already exists, reuse it.

    Returns:
        The created MIC asset path, e.g. ``/Game/Generated/Materials/MI_Foo``.

    Raises:
        FileNotFoundError: If the parent material does not exist.
        RuntimeError: If the MIC cannot be created.
    """
    if not unreal.EditorAssetLibrary.does_asset_exist(parent_path):
        raise FileNotFoundError(f"Parent material not found: {parent_path}")

    parent_material = unreal.EditorAssetLibrary.load_asset(parent_path)
    if not isinstance(parent_material, unreal.Material):
        raise TypeError(f"Parent asset is not a Material: {parent_path}")

    _ensure_directory(destination)

    mic_path = f"{destination}/{name}.{name}"
    mic = None
    reused = False

    if unreal.EditorAssetLibrary.does_asset_exist(mic_path):
        mic = unreal.EditorAssetLibrary.load_asset(mic_path)
        if reuse and isinstance(mic, unreal.MaterialInstanceConstant):
            unreal.log(f"Reusing existing MIC at {mic_path}")
            reused = True
        else:
            unreal.EditorAssetLibrary.delete_asset(mic_path)
            mic = None

    if mic is None:
        factory = unreal.MaterialInstanceConstantFactoryNew()
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        mic = asset_tools.create_asset(
            asset_name=name,
            package_path=destination,
            asset_class=unreal.MaterialInstanceConstant,
            factory=factory,
        )
        if mic is None:
            raise RuntimeError(f"Failed to create MIC at {mic_path}")

    mic.set_editor_property("parent", parent_material)

    for semantic, texture_path in (maps or {}).items():
        if not texture_path:
            continue
        if not unreal.EditorAssetLibrary.does_asset_exist(texture_path):
            unreal.log_warning(f"Texture not found for '{semantic}': {texture_path}")
            continue

        texture = unreal.EditorAssetLibrary.load_asset(texture_path)
        if not isinstance(texture, unreal.Texture):
            unreal.log_warning(
                f"Asset for '{semantic}' is not a Texture: {texture_path}"
            )
            continue

        param_name = _semantic_to_param_name(semantic)
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
            mic, param_name, texture
        )
        unreal.log(f"Set MIC parameter '{param_name}' to {texture_path}")

    for param_name, value in (scalar_parameters or {}).items():
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            mic, param_name, float(value)
        )
        unreal.log(f"Set MIC scalar '{param_name}' to {value}")

    for param_name, value in (vector_parameters or {}).items():
        if not isinstance(value, (list, tuple)) or len(value) < 3:
            unreal.log_warning(f"Invalid vector value for '{param_name}': {value}")
            continue
        color = unreal.LinearColor(
            float(value[0]),
            float(value[1]),
            float(value[2]),
            float(value[3]) if len(value) > 3 else 1.0,
        )
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            mic, param_name, color
        )
        unreal.log(f"Set MIC vector '{param_name}' to {value}")

    unreal.EditorAssetLibrary.save_loaded_asset(mic)

    # Stash reuse info on the asset for callers.
    unreal.EditorAssetLibrary.set_metadata_tag(mic, "Reused", str(reused))
    return mic_path


def setup_actor_with_mesh_and_material(
    actor_name: str,
    mesh_path: str,
    material_path: str,
    location: tuple[float, float, float] = (0.0, 0.0, 0.0),
) -> None:
    """Spawn a StaticMeshActor, assign a mesh and a material.

    Args:
        actor_name: Label for the new actor.
        mesh_path: Path to the StaticMesh asset.
        material_path: Path to the Material or MIC asset.
        location: World location as ``(X, Y, Z)``.

    Raises:
        FileNotFoundError: If the mesh or material does not exist.
        RuntimeError: If the actor cannot be spawned.
    """
    if not unreal.EditorAssetLibrary.does_asset_exist(mesh_path):
        raise FileNotFoundError(f"StaticMesh not found: {mesh_path}")
    if not unreal.EditorAssetLibrary.does_asset_exist(material_path):
        raise FileNotFoundError(f"Material not found: {material_path}")

    mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
    material = unreal.EditorAssetLibrary.load_asset(material_path)

    if not isinstance(mesh, unreal.StaticMesh):
        raise TypeError(f"Asset is not a StaticMesh: {mesh_path}")

    actor_location = unreal.Vector(location[0], location[1], location[2])
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor.static_class(),
        actor_location,
    )
    if actor is None:
        raise RuntimeError("Failed to spawn StaticMeshActor")

    actor.set_actor_label(actor_name)

    mesh_component = actor.static_mesh_component
    mesh_component.set_static_mesh(mesh)
    mesh_component.set_material(0, material)

    unreal.log(f"Spawned actor '{actor_name}' at {location}")


def _main_cli() -> None:
    """Simple command-line entry point for ad-hoc testing in UE."""
    import argparse

    parser = argparse.ArgumentParser(description="Import generated assets into UE")
    sub = parser.add_subparsers(dest="command", required=True)

    p_import = sub.add_parser("import_mesh")
    p_import.add_argument("file_path")
    p_import.add_argument("--destination", default="/Game/Generated/Meshes")

    p_mat = sub.add_parser("create_material")
    p_mat.add_argument("name", nargs="?", default=None)
    p_mat.add_argument("parent_path", nargs="?", default=None)
    p_mat.add_argument("--maps", default="{}")
    p_mat.add_argument("--destination", default="/Game/Generated/Materials")
    p_mat.add_argument("--scalar-params", default="{}")
    p_mat.add_argument("--vector-params", default="{}")
    p_mat.add_argument("--reuse", type=lambda s: s.lower() in ("true", "1", "yes"), default="true")
    p_mat.add_argument("--params-file", default=None)
    p_mat.add_argument("--result-file", default=None)

    p_actor = sub.add_parser("setup_actor")
    p_actor.add_argument("actor_name")
    p_actor.add_argument("mesh_path")
    p_actor.add_argument("material_path")
    p_actor.add_argument("--location", default="0,0,0")

    args = parser.parse_args()

    def _write_result(result: dict[str, object] | None, error: str | None = None) -> None:
        payload = {"success": error is None, "error": error}
        if result is not None:
            payload.update(result)
        text = json.dumps(payload, indent=2, ensure_ascii=False)
        if args.result_file:
            result_path = Path(args.result_file)
            result_path.parent.mkdir(parents=True, exist_ok=True)
            result_path.write_text(text, encoding="utf-8")
        else:
            print(text)

    if args.command == "import_mesh":
        print(import_mesh(args.file_path, args.destination))
    elif args.command == "create_material":
        try:
            if args.params_file:
                params = json.loads(Path(args.params_file).read_text(encoding="utf-8"))
                name = params["name"]
                parent_path = params["parent_path"]
                maps = params.get("maps", {})
                destination = params.get("destination", "/Game/Generated/Materials")
                scalar_parameters = params.get("scalar_parameters", {})
                vector_parameters = params.get("vector_parameters", {})
                reuse = params.get("reuse", True)
            else:
                name = args.name
                parent_path = args.parent_path
                maps = json.loads(args.maps)
                destination = args.destination
                scalar_parameters = json.loads(args.scalar_params)
                vector_parameters = json.loads(args.vector_params)
                reuse = args.reuse

            if not name or not parent_path:
                raise ValueError("name and parent_path are required")

            path = create_material_instance_from_maps(
                name,
                parent_path,
                maps,
                destination,
                scalar_parameters=scalar_parameters,
                vector_parameters=vector_parameters,
                reuse=reuse,
            )
            mic = unreal.EditorAssetLibrary.load_asset(path)
            reused = unreal.EditorAssetLibrary.get_metadata_tag(mic, "Reused") == "True"
            _write_result({
                "path": path,
                "parent_path": parent_path,
                "maps": maps,
                "reused": reused,
            })
        except Exception as exc:  # noqa: BLE001
            _write_result(None, error=str(exc))
    elif args.command == "setup_actor":
        loc = tuple(float(x) for x in args.location.split(","))
        setup_actor_with_mesh_and_material(
            args.actor_name, args.mesh_path, args.material_path, loc
        )


if __name__ == "__main__":
    _main_cli()
