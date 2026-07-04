"""Create PBR master materials for the AI-generated VFX workflow.

Run inside Unreal Editor via Python Editor Scripting (or the console command
``py <path>``). Creates four parameterized master materials under
``/Game/Materials/Generated/`` that AI tooling can parent Material Instances to.

Materials created:
  - M_Generated_Opaque        (Opaque / Default Lit)
  - M_Generated_Translucent   (Translucent / Default Lit)
  - M_Generated_Unlit_Additive (Additive / Unlit)
  - M_Generated_Masked        (Masked / Default Lit)
"""

import unreal


DEST_DIR = "/Game/Materials/Generated"
DEFAULT_NORMAL = "/Engine/EngineMaterials/DefaultNormal.DefaultNormal"
DEFAULT_WHITE = "/Engine/EngineMaterials/WhiteSquareTexture.WhiteSquareTexture"


def _load_texture(path):
    """Safely load a texture asset; return None if unavailable."""
    if not path:
        return None
    try:
        tex = unreal.EditorAssetLibrary.load_asset(path)
        if tex and isinstance(tex, unreal.Texture):
            return tex
    except Exception:
        pass
    return None


def _ensure_dir():
    """Make sure the destination content directory exists."""
    unreal.EditorAssetLibrary.make_directory(DEST_DIR)


def _asset_exists(path):
    return unreal.EditorAssetLibrary.does_asset_exist(path)


def _create_or_load_material(name):
    """Create a new Material asset or load an existing one for reuse."""
    full_path = f"{DEST_DIR}/{name}"
    if _asset_exists(full_path):
        unreal.log_warning(f"Reusing existing material: {full_path}")
        return unreal.EditorAssetLibrary.load_asset(full_path)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialFactoryNew()
    material = asset_tools.create_asset(
        asset_name=name,
        package_path=DEST_DIR,
        asset_class=unreal.Material,
        factory=factory,
    )
    return material


def _add_vector_param(material, param_name, default_value, x, y):
    """Add a VectorParameter expression and return it."""
    expr = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, x, y
    )
    expr.set_editor_property("parameter_name", param_name)
    expr.default_value = unreal.LinearColor(
        default_value[0], default_value[1], default_value[2], default_value[3]
    )
    return expr


def _add_scalar_param(material, param_name, default_value, x, y):
    """Add a ScalarParameter expression and return it."""
    expr = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, x, y
    )
    expr.set_editor_property("parameter_name", param_name)
    expr.default_value = default_value
    return expr


def _add_texture_param(material, param_name, default_texture, x, y):
    """Add a TextureSampleParameter2D expression and return it."""
    expr = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, x, y
    )
    expr.set_editor_property("parameter_name", param_name)
    if default_texture:
        expr.set_editor_property("texture", default_texture)
    return expr


def _connect(expr, output_name, material_property):
    """Wire an expression output to a material property."""
    unreal.MaterialEditingLibrary.connect_material_property(
        expr, output_name, material_property
    )


def _save(material):
    """Save the material asset."""
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    material.set_editor_property("bCanMaskedBeAssumedOpaque", True)
    unreal.EditorAssetLibrary.save_loaded_asset(material)


def create_opaque_master():
    """M_Generated_Opaque: Opaque / Default Lit."""
    name = "M_Generated_Opaque"
    material = _create_or_load_material(name)
    if not material:
        unreal.log_error(f"Failed to create {name}")
        return

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT
    )

    base_color = _add_vector_param(
        material, "BaseColor", (1.0, 1.0, 1.0, 1.0), -300, -150
    )
    _connect(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    normal = _add_texture_param(
        material, "Normal", _load_texture(DEFAULT_NORMAL), -300, 50
    )
    _connect(normal, "", unreal.MaterialProperty.MP_NORMAL)

    roughness = _add_scalar_param(material, "Roughness", 0.5, -300, 250)
    _connect(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metallic = _add_scalar_param(material, "Metallic", 0.0, -300, 450)
    _connect(metallic, "", unreal.MaterialProperty.MP_METALLIC)

    _save(material)
    unreal.log(f"Created master material: {DEST_DIR}/{name}")


def create_translucent_master():
    """M_Generated_Translucent: Translucent / Default Lit."""
    name = "M_Generated_Translucent"
    material = _create_or_load_material(name)
    if not material:
        unreal.log_error(f"Failed to create {name}")
        return

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT
    )

    base_color = _add_vector_param(
        material, "BaseColor", (1.0, 1.0, 1.0, 1.0), -300, -150
    )
    _connect(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    opacity = _add_scalar_param(material, "Opacity", 0.5, -300, 50)
    _connect(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    emissive = _add_vector_param(
        material, "Emissive", (0.0, 0.0, 0.0, 1.0), -300, 250
    )
    _connect(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    normal = _add_texture_param(
        material, "Normal", _load_texture(DEFAULT_NORMAL), -300, 450
    )
    _connect(normal, "", unreal.MaterialProperty.MP_NORMAL)

    _save(material)
    unreal.log(f"Created master material: {DEST_DIR}/{name}")


def create_unlit_additive_master():
    """M_Generated_Unlit_Additive: Additive / Unlit."""
    name = "M_Generated_Unlit_Additive"
    material = _create_or_load_material(name)
    if not material:
        unreal.log_error(f"Failed to create {name}")
        return

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_UNLIT
    )

    # BaseColor is exposed for consistency but is ignored by the Unlit shading
    # model; EmissiveColor drives the visible color.
    base_color = _add_vector_param(
        material, "BaseColor", (1.0, 1.0, 1.0, 1.0), -300, -150
    )
    _connect(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    emissive = _add_vector_param(
        material, "EmissiveColor", (1.0, 1.0, 1.0, 1.0), -300, 50
    )
    _connect(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    opacity = _add_scalar_param(material, "Opacity", 1.0, -300, 250)
    _connect(opacity, "", unreal.MaterialProperty.MP_OPACITY)

    _save(material)
    unreal.log(f"Created master material: {DEST_DIR}/{name}")


def create_masked_master():
    """M_Generated_Masked: Masked / Default Lit."""
    name = "M_Generated_Masked"
    material = _create_or_load_material(name)
    if not material:
        unreal.log_error(f"Failed to create {name}")
        return

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property(
        "shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT
    )

    base_color = _add_vector_param(
        material, "BaseColor", (1.0, 1.0, 1.0, 1.0), -300, -150
    )
    _connect(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)

    opacity_mask = _add_texture_param(
        material, "OpacityMask", _load_texture(DEFAULT_WHITE), -300, 50
    )
    _connect(opacity_mask, "R", unreal.MaterialProperty.MP_OPACITY_MASK)

    normal = _add_texture_param(
        material, "Normal", _load_texture(DEFAULT_NORMAL), -300, 250
    )
    _connect(normal, "", unreal.MaterialProperty.MP_NORMAL)

    _save(material)
    unreal.log(f"Created master material: {DEST_DIR}/{name}")


def main():
    unreal.log("=== Creating Generated PBR Master Materials ===")
    _ensure_dir()

    create_opaque_master()
    create_translucent_master()
    create_unlit_additive_master()
    create_masked_master()

    unreal.log("=== Done ===")


if __name__ == "__main__":
    main()
