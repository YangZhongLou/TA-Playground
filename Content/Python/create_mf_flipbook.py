"""Create the MF_Flipbook material function for the AI VFX workflow.

Run this inside the Unreal Editor Python console or via:
    File > Execute Python Script > create_mf_flipbook.py

Prerequisites:
    - PythonScriptPlugin and EditorScriptingUtilities enabled (see TA-Playground.uproject).
    - Unreal Engine 5.5+ (script written against 5.7 Python API).

The generated material function lives at:
    /Game/Materials/Functions/MF_Flipbook

Inputs (in order):
    Texture          - Texture2D object (flipbook atlas).
    Rows             - Scalar float, number of rows in the atlas.
    Columns          - Scalar float, number of columns in the atlas.
    Animation Phase  - Scalar float in [0, 1], normalized playback position.

Outputs:
    RGBA             - Sampled flipbook frame as Vector4.

Implementation notes:
    - Frame index = floor(AnimationPhase * Rows * Columns) mod (Rows * Columns).
    - UV layout assumes the atlas is stored left-to-right, top-to-bottom.
    - The function returns the texture sample's RGBA output so callers can mask
      RGB / A as needed.
"""

import unreal

DESTINATION_PATH = "/Game/Materials/Functions"
ASSET_NAME = "MF_Flipbook"


def ensure_directory_exists(path: str) -> None:
    """Create the content folder if it does not already exist."""
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)
        unreal.log(f"Created directory: {path}")


def create_material_function(asset_name: str, package_path: str) -> unreal.MaterialFunction | None:
    """Create a new empty Material Function asset."""
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialFunctionFactoryNew()

    full_path = f"{package_path}/{asset_name}.{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.log_warning(f"Asset already exists, loading instead of creating: {full_path}")
        return unreal.EditorAssetLibrary.load_asset(full_path)

    asset = asset_tools.create_asset(
        asset_name=asset_name,
        package_path=package_path,
        asset_class=unreal.MaterialFunction,
        factory=factory,
    )
    if asset is None:
        unreal.log_error(f"Failed to create Material Function: {full_path}")
        return None

    unreal.log(f"Created Material Function: {full_path}")
    return asset


def add_input_node(
    mf: unreal.MaterialFunction,
    name: str,
    input_type: unreal.FunctionInputType,
    sort_priority: int,
    x: int,
    y: int,
) -> unreal.MaterialExpressionFunctionInput:
    """Add a FunctionInput node to the material function."""
    node = unreal.MaterialEditingLibrary.create_material_expression_in_function(
        mf, unreal.MaterialExpressionFunctionInput, node_pos_x=x, node_pos_y=y
    )
    node.set_editor_property("input_name", name)
    node.set_editor_property("input_type", input_type)
    node.set_editor_property("sort_priority", sort_priority)
    node.set_editor_property("description", name)
    return node


def add_output_node(
    mf: unreal.MaterialFunction,
    name: str,
    sort_priority: int,
    x: int,
    y: int,
) -> unreal.MaterialExpressionFunctionOutput:
    """Add a FunctionOutput node to the material function."""
    node = unreal.MaterialEditingLibrary.create_material_expression_in_function(
        mf, unreal.MaterialExpressionFunctionOutput, node_pos_x=x, node_pos_y=y
    )
    node.set_editor_property("output_name", name)
    node.set_editor_property("sort_priority", sort_priority)
    node.set_editor_property("description", name)
    return node


def add_expr(
    mf: unreal.MaterialFunction,
    expr_class,
    x: int,
    y: int,
) -> unreal.MaterialExpression:
    """Helper to create a material expression inside the function."""
    return unreal.MaterialEditingLibrary.create_material_expression_in_function(
        mf, expr_class, node_pos_x=x, node_pos_y=y
    )


def connect(
    from_expr: unreal.MaterialExpression,
    to_expr: unreal.MaterialExpression,
    to_input_name: str,
    from_output_name: str = "",
) -> None:
    """Connect two material expressions."""
    unreal.MaterialEditingLibrary.connect_material_expressions(
        from_expression=from_expr,
        from_output_name=from_output_name,
        to_expression=to_expr,
        to_input_name=to_input_name,
    )


def build_flipbook_graph(mf: unreal.MaterialFunction) -> bool:
    """Wire the flipbook sampling graph inside the material function."""
    try:
        # Clear any existing expressions so the script is idempotent.
        unreal.MaterialEditingLibrary.delete_all_material_expressions_in_function(mf)

        # --- Inputs ----------------------------------------------------------------
        in_texture = add_input_node(
            mf, "Texture", unreal.FunctionInputType.FUNCTION_INPUT_TEXTURE2D, 0, -900, 0
        )
        in_rows = add_input_node(
            mf, "Rows", unreal.FunctionInputType.FUNCTION_INPUT_SCALAR, 1, -900, 150
        )
        in_columns = add_input_node(
            mf, "Columns", unreal.FunctionInputType.FUNCTION_INPUT_SCALAR, 2, -900, 300
        )
        in_phase = add_input_node(
            mf, "Animation Phase", unreal.FunctionInputType.FUNCTION_INPUT_SCALAR, 3, -900, 450
        )

        # --- Frame index calculation -----------------------------------------------
        # total_frames = Rows * Columns
        total_frames = add_expr(mf, unreal.MaterialExpressionMultiply, -650, 150)
        connect(in_rows, total_frames, "A")
        connect(in_columns, total_frames, "B")

        # phase * total_frames
        phase_scaled = add_expr(mf, unreal.MaterialExpressionMultiply, -650, 450)
        connect(in_phase, phase_scaled, "A")
        connect(total_frames, phase_scaled, "B")

        # floor(phase * total_frames)
        frame_index = add_expr(mf, unreal.MaterialExpressionFloor, -500, 450)
        connect(phase_scaled, frame_index, "")

        # Wrap around at total_frames so phase == 1 maps back to frame 0.
        frame_wrapped = add_expr(mf, unreal.MaterialExpressionFmod, -350, 450)
        connect(frame_index, frame_wrapped, "A")
        connect(total_frames, frame_wrapped, "B")

        # --- Column / Row from frame index -----------------------------------------
        # col = fmod(frame, Columns)
        col = add_expr(mf, unreal.MaterialExpressionFmod, -200, 300)
        connect(frame_wrapped, col, "A")
        connect(in_columns, col, "B")

        # row = floor(frame / Columns)
        frame_div_cols = add_expr(mf, unreal.MaterialExpressionDivide, -200, 420)
        connect(frame_wrapped, frame_div_cols, "A")
        connect(in_columns, frame_div_cols, "B")
        row = add_expr(mf, unreal.MaterialExpressionFloor, -50, 420)
        connect(frame_div_cols, row, "")

        # --- UV scaling and offset -------------------------------------------------
        uv = add_expr(mf, unreal.MaterialExpressionTextureCoordinate, -650, -150)
        uv.set_editor_property("coordinate_index", 0)

        tile_count = add_expr(mf, unreal.MaterialExpressionAppendVector, -500, -50)
        connect(in_columns, tile_count, "A")
        connect(in_rows, tile_count, "B")

        scaled_uv = add_expr(mf, unreal.MaterialExpressionMultiply, -350, -50)
        connect(uv, scaled_uv, "A")
        connect(tile_count, scaled_uv, "B")

        local_uv = add_expr(mf, unreal.MaterialExpressionFrac, -200, -50)
        connect(scaled_uv, local_uv, "")

        # 1 / Columns and 1 / Rows
        one = add_expr(mf, unreal.MaterialExpressionConstant, -500, 600)
        one.set_editor_property("R", 1.0)

        inv_columns = add_expr(mf, unreal.MaterialExpressionDivide, -350, 600)
        connect(one, inv_columns, "A")
        connect(in_columns, inv_columns, "B")

        inv_rows = add_expr(mf, unreal.MaterialExpressionDivide, -350, 720)
        connect(one, inv_rows, "A")
        connect(in_rows, inv_rows, "B")

        # u_offset = col / Columns
        u_offset = add_expr(mf, unreal.MaterialExpressionMultiply, 0, 300)
        connect(col, u_offset, "A")
        connect(inv_columns, u_offset, "B")

        # v_offset = (Rows - 1 - row) / Rows  (top-down row order)
        rows_minus_one = add_expr(mf, unreal.MaterialExpressionSubtract, 0, 540)
        connect(in_rows, rows_minus_one, "A")
        connect(one, rows_minus_one, "B")

        row_top_down = add_expr(mf, unreal.MaterialExpressionSubtract, 150, 480)
        connect(rows_minus_one, row_top_down, "A")
        connect(row, row_top_down, "B")

        v_offset = add_expr(mf, unreal.MaterialExpressionMultiply, 300, 480)
        connect(row_top_down, v_offset, "A")
        connect(inv_rows, v_offset, "B")

        # offset = (u_offset, v_offset)
        offset = add_expr(mf, unreal.MaterialExpressionAppendVector, 450, 300)
        connect(u_offset, offset, "A")
        connect(v_offset, offset, "B")

        final_uv = add_expr(mf, unreal.MaterialExpressionAdd, 600, 150)
        connect(local_uv, final_uv, "A")
        connect(offset, final_uv, "B")

        # --- Texture sample and output ---------------------------------------------
        tex_sample = add_expr(mf, unreal.MaterialExpressionTextureSample, 800, 150)
        connect(final_uv, tex_sample, "Coordinates")
        connect(in_texture, tex_sample, "TextureObject")

        out_rgba = add_output_node(mf, "RGBA", 0, 1000, 150)
        connect(tex_sample, out_rgba, "", "RGBA")

        # Refresh the material function so the graph is compiled and saved.
        unreal.MaterialEditingLibrary.update_material_function(mf)
        return True

    except Exception as exc:
        unreal.log_error(f"Failed to build MF_Flipbook graph: {exc}")
        return False


def save_asset(asset: unreal.MaterialFunction) -> bool:
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
    """Create and wire the flipbook material function."""
    ensure_directory_exists(DESTINATION_PATH)

    mf = create_material_function(ASSET_NAME, DESTINATION_PATH)
    if mf is None:
        unreal.log_error("Aborting: could not create or load MF_Flipbook.")
        return

    if not build_flipbook_graph(mf):
        unreal.log_error("Aborting: failed to build flipbook graph.")
        return

    if save_asset(mf):
        unreal.log(
            "\n=== MF_Flipbook created successfully ===\n"
            "Next step: open /Game/Materials/Functions/MF_Flipbook in the Material "
            "Editor, verify the graph layout, and reference it from a Niagara sprite material."
        )
    else:
        unreal.log_error("MF_Flipbook graph built but asset could not be saved.")


if __name__ == "__main__":
    main()
