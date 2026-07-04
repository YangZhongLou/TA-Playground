"""Verify the Niagara VFX templates required by the AI VFX workflow.

Run inside the Unreal Editor Python console or via:
    File > Execute Python Script > verify_niagara_templates.py

Checks:
    - The four base systems exist under /Game/VFX/Templates.
    - Each system asset is a valid UNiagaraSystem.
    - Reports the User.* parameters that are currently exposed (manual review).
"""

import unreal


EXPECTED_TEMPLATES = {
    "/Game/VFX/Templates/NS_BurstBase": [
        "User.Color",
        "User.Size",
        "User.Rate",
        "User.Lifetime",
        "User.Velocity",
    ],
    "/Game/VFX/Templates/NS_TrailBase": [
        "User.Color",
        "User.Width",
        "User.Lifetime",
        "User.Speed",
    ],
    "/Game/VFX/Templates/NS_AmbientBase": [
        "User.Color",
        "User.Density",
        "User.Size",
        "User.Speed",
    ],
    "/Game/VFX/Templates/NS_ImpactBase": [
        "User.Color",
        "User.Size",
        "User.DecalSize",
        "User.Lifetime",
    ],
}


def collect_exposed_user_parameters(asset: unreal.NiagaraSystem) -> list[str]:
    """Return the list of User.* parameter names currently exposed on the system.

    Falls back to an empty list if the API is unavailable.
    """
    exposed = []
    try:
        # Prefer the newer UserParameterStore API when present.
        store = asset.get_editor_property("user_parameter_store")
        if store:
            for param in store.parameters:
                exposed.append(str(param.name))
    except Exception:
        pass

    # If the store API is not exposed, we cannot enumerate parameters automatically.
    return exposed


def main() -> None:
    """Run verification against the expected template set."""
    all_ok = True

    for path, expected_params in EXPECTED_TEMPLATES.items():
        unreal.log(f"\n--- Checking {path} ---")
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log_error(f"Missing asset: {path}")
            all_ok = False
            continue

        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(asset, unreal.NiagaraSystem):
            unreal.log_error(f"Asset is not a NiagaraSystem: {path}")
            all_ok = False
            continue

        unreal.log(f"OK: {path}")
        exposed = collect_exposed_user_parameters(asset)
        if exposed:
            for name in expected_params:
                status = "OK" if name in exposed else "MISSING"
                unreal.log(f"  [{status}] {name}")
        else:
            unreal.log_warning(
                "  Could not enumerate User parameters automatically; "
                "please confirm them manually in the Niagara Editor."
            )

    if all_ok:
        unreal.log("\n=== All Niagara templates are present ===")
    else:
        unreal.log_error("\n=== Some Niagara templates are missing or invalid ===")


if __name__ == "__main__":
    main()
