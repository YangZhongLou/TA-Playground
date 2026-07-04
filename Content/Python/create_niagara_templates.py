"""Create the base Niagara VFX templates for the AI VFX workflow.

Run this inside the Unreal Editor Python console or via:
    File > Execute Python Script > create_niagara_templates.py

Prerequisites:
    - Niagara plugin enabled.
    - Python Editor Scripting plugin enabled.
    - Unreal Engine 5.5+ (script was written against 5.7 API).

This script creates four empty Niagara Systems under /Game/VFX/Templates.
The template internals (emitters, sprite renderer, user parameters) must be
configured manually in the Niagara Editor after creation; the exact wiring is
documented in Content/VFX/Templates/README.md.
"""

import unreal


TEMPLATE_SPECS = [
    {
        "name": "NS_BurstBase",
        "label": "Burst / One-shot explosion/spark template",
        "parameters": [
            ("User.Color", "LinearColor", (1.0, 0.6, 0.2, 1.0)),
            ("User.Size", "Float", 1.0),
            ("User.Rate", "Float", 50.0),
            ("User.Lifetime", "Float", 1.0),
            ("User.Velocity", "Vector3", (1.0, 1.0, 1.0)),
        ],
    },
    {
        "name": "NS_TrailBase",
        "label": "Trail / Ribbon or particle trail template",
        "parameters": [
            ("User.Color", "LinearColor", (0.4, 0.7, 1.0, 1.0)),
            ("User.Width", "Float", 0.15),
            ("User.Lifetime", "Float", 0.5),
            ("User.Speed", "Float", 2.0),
        ],
    },
    {
        "name": "NS_AmbientBase",
        "label": "Ambient / Looping environmental dust/magic template",
        "parameters": [
            ("User.Color", "LinearColor", (0.6, 0.9, 0.7, 1.0)),
            ("User.Density", "Float", 20.0),
            ("User.Size", "Float", 0.3),
            ("User.Speed", "Float", 0.5),
        ],
    },
    {
        "name": "NS_ImpactBase",
        "label": "Impact / Hit flash, sparks and optional decal template",
        "parameters": [
            ("User.Color", "LinearColor", (1.0, 0.9, 0.5, 1.0)),
            ("User.Size", "Float", 1.0),
            ("User.DecalSize", "Float", 0.5),
            ("User.Lifetime", "Float", 0.3),
        ],
    },
]

DESTINATION_PATH = "/Game/VFX/Templates"


def ensure_directory_exists(path: str) -> None:
    """Create the content folder if it does not already exist."""
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)
        unreal.log(f"Created directory: {path}")


def create_niagara_system(asset_name: str, package_path: str) -> unreal.NiagaraSystem | None:
    """Create a new empty Niagara System asset."""
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.NiagaraSystemFactoryNew()

    # Avoid overwriting an existing asset.
    full_path = f"{package_path}/{asset_name}.{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.log_warning(f"Asset already exists, skipping creation: {full_path}")
        return unreal.EditorAssetLibrary.load_asset(full_path)

    asset = asset_tools.create_asset(
        asset_name=asset_name,
        package_path=package_path,
        asset_class=unreal.NiagaraSystem,
        factory=factory,
    )
    if asset is None:
        unreal.log_error(f"Failed to create Niagara System: {full_path}")
        return None

    unreal.log(f"Created Niagara System: {full_path}")
    return asset


def expose_user_parameters(asset: unreal.NiagaraSystem, parameters: list) -> None:
    """Attempt to register User namespace parameters on the system.

    Note: Exposing user parameters programmatically requires internal Niagara
    stack APIs that are not consistently exposed through Python. The function
    tries the available helpers and falls back to logging manual steps.
    """
    try:
        # Some engine builds expose a helper module for Niagara scripting.
        # If it exists, attempt to add parameters by name.
        niagara_helpers = unreal.load_class(None, "/Script/Niagara.NiagaraPythonHelpers")
        if niagara_helpers:
            unreal.log("NiagaraPythonHelpers found; parameter wiring may be partially automated.")
    except Exception as exc:
        unreal.log_warning(f"Could not load NiagaraPythonHelpers: {exc}")

    # There is no stable public Python API for adding User parameters directly
    # to a NiagaraSystem asset. Mark the asset dirty so the artist is prompted
    # to configure the parameters in the Niagara Editor.
    asset.set_editor_property("template_asset_description", "See README.md for parameter wiring.")
    unreal.log("Manual step required: expose the listed User.* parameters in the Niagara Editor.")


def save_asset(asset: unreal.NiagaraSystem) -> bool:
    """Save the created/modified asset to disk."""
    package_path = asset.get_path_name().split(".")[0]
    try:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        unreal.log(f"Saved asset: {package_path}")
        return True
    except Exception as exc:
        unreal.log_error(f"Failed to save asset {package_path}: {exc}")
        return False


def main() -> None:
    """Create all base Niagara templates."""
    ensure_directory_exists(DESTINATION_PATH)

    created = []
    failed = []

    for spec in TEMPLATE_SPECS:
        unreal.log(f"\n--- Processing {spec['name']} ---")
        asset = create_niagara_system(spec["name"], DESTINATION_PATH)
        if asset is None:
            failed.append(spec["name"])
            continue

        expose_user_parameters(asset, spec["parameters"])
        if save_asset(asset):
            created.append(spec["name"])
        else:
            failed.append(spec["name"])

    unreal.log("\n=== Niagara template creation complete ===")
    unreal.log(f"Created or updated: {created}")
    if failed:
        unreal.log_error(f"Failed: {failed}")

    unreal.log(
        "\nNext step: open each system in the Niagara Editor and wire the "
        "User.* parameters documented in Content/VFX/Templates/README.md."
    )


if __name__ == "__main__":
    main()
