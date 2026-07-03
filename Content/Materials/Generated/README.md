# Generated PBR Master Materials

This folder contains the master materials used by the AI-generated VFX workflow.
Each material is parameterized so that Python / MCP tooling can create Material
Instances and drive them with generated textures and scalar values.

## Quick Start

1. Open the project in Unreal Editor.
2. Ensure the **Python Editor Scripting** plugin is enabled.
3. Run `Content/Python/create_generated_master_materials.py` via:
   - **Toolbar**: Edit → Plugins → Python → Execute Python Script
   - **Console**: `py <ProjectRoot>/Content/Python/create_generated_master_materials.py`
   - **Python Editor Script Plugin** output log.
4. The four `.uasset` files are created under `/Game/Materials/Generated/`.

## Naming Convention

- Master materials: `M_Generated_<BlendMode>`
- Material instances created by AI tooling: `MI_Generated_<AssetName>_<Variant>`
- Texture parameters use the exact names below so that `set_material_parameter`
  and `create_material_instance_from_maps` can set them programmatically.

## Master Material Catalog

### `M_Generated_Opaque`

| Property | Value |
| --- | --- |
| Blend Mode | Opaque |
| Shading Model | Default Lit |
| Use case | Standard opaque props, rocks, metals, wood, etc. |

| Parameter | Type | Default | Connected To |
| --- | --- | --- | --- |
| `BaseColor` | Vector | `(1, 1, 1, 1)` | Base Color |
| `Normal` | Texture2D | Engine default normal | Normal |
| `Roughness` | Scalar | `0.5` | Roughness |
| `Metallic` | Scalar | `0.0` | Metallic |

### `M_Generated_Translucent`

| Property | Value |
| --- | --- |
| Blend Mode | Translucent |
| Shading Model | Default Lit |
| Use case | Glass, ice, ghosts, energy shields, semi-transparent surfaces. |

| Parameter | Type | Default | Connected To |
| --- | --- | --- | --- |
| `BaseColor` | Vector | `(1, 1, 1, 1)` | Base Color |
| `Opacity` | Scalar | `0.5` | Opacity |
| `Emissive` | Vector | `(0, 0, 0, 1)` | Emissive Color |
| `Normal` | Texture2D | Engine default normal | Normal |

### `M_Generated_Unlit_Additive`

| Property | Value |
| --- | --- |
| Blend Mode | Additive |
| Shading Model | Unlit |
| Use case | Glows, flares, holograms, magic sparks, light cards. |

| Parameter | Type | Default | Connected To | Note |
| --- | --- | --- | --- | --- |
| `BaseColor` | Vector | `(1, 1, 1, 1)` | Base Color | Ignored by Unlit shading model; kept for API consistency. |
| `EmissiveColor` | Vector | `(1, 1, 1, 1)` | Emissive Color | Drives the visible color. |
| `Opacity` | Scalar | `1.0` | Opacity | Controls additive intensity/fade. |

### `M_Generated_Masked`

| Property | Value |
| --- | --- |
| Blend Mode | Masked |
| Shading Model | Default Lit |
| Use case | Foliage, chain-link fences, decals with cutout, cloth with holes. |

| Parameter | Type | Default | Connected To |
| --- | --- | --- | --- |
| `BaseColor` | Vector | `(1, 1, 1, 1)` | Base Color |
| `OpacityMask` | Texture2D | Engine default white | Opacity Mask (R channel) |
| `Normal` | Texture2D | Engine default normal | Normal |

## AI Tooling Notes

- When a generated asset has no normal map, leave the `Normal` parameter at its
  default; the master material falls back to a flat normal.
- For masked assets, provide a single-channel greyscale mask texture in the
  `OpacityMask` slot; the material samples the **R** channel.
- The `Opacity` parameter on translucent and unlit materials is expected to be
  set as a scalar via `set_material_parameter`.
- All four materials are intentionally minimal to keep import/compilation fast
  and to avoid shader feature mismatches between generated assets.
