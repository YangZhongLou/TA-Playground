# Material Functions for AI VFX Workflow

This folder hosts reusable material functions used by the AI-generated VFX pipeline.

## `MF_Flipbook`

Flipbook texture sampling utility.

- **Path:** `/Game/Materials/Functions/MF_Flipbook`
- **Created by:** `Content/Python/create_mf_flipbook.py`
- **Inputs:**
  - `Texture` (Texture2D) – the flipbook atlas.
  - `Rows` (Scalar) – number of rows in the atlas.
  - `Columns` (Scalar) – number of columns in the atlas.
  - `Animation Phase` (Scalar, 0–1) – normalized frame to display.
- **Output:**
  - `RGBA` (Vector4) – sampled frame color/alpha.

### How to create the asset

1. Make sure the following plugins are enabled in `TA-Playground.uproject`:
   - `PythonScriptPlugin`
   - `EditorScriptingUtilities`
2. Open the project in Unreal Editor.
3. Run `Content/Python/create_mf_flipbook.py`:
   - **File > Execute Python Script** and select the script, or
   - paste the script contents into the **Python** output log console.
4. The asset `/Game/Materials/Functions/MF_Flipbook` is created (or updated if it already exists).

### Manual verification checklist

- [ ] Open `MF_Flipbook` in the Material Editor.
- [ ] Confirm four inputs are visible in the order: Texture, Rows, Columns, Animation Phase.
- [ ] Confirm one output named `RGBA`.
- [ ] Connect a flipbook texture to a test material, set Rows/Columns, and animate `Animation Phase` to verify the frames advance left-to-right, top-to-bottom.
- [ ] Reference `MF_Flipbook` from a Niagara sprite material (e.g. by placing a `MaterialFunctionCall` node).

### Notes

- The UV math assumes a left-to-right, top-to-bottom atlas layout.
- `Animation Phase` is wrapped so values outside `[0, 1]` loop, and `1.0` maps back to frame `0`.
- The output is `RGBA`; use a `ComponentMask` in the calling material if you only need `RGB` or `A`.
