# Texture → Terrain Workflow Implementation Plan

> Created: 2026-06-09 | Status: Approved

---

## Context

The Hexagon terrain system generates terrain (noise → height → terrain type → colored mesh) and assigns
materials per terrain type via `LayerMaterials`. UnrealMCP can create materials with texture sampling.
But these two pipelines are disconnected:

- You can create a textured material — but must manually assign it in the Details panel
- Once assigned, you can't swap textures without creating a new material
- Vertex colors carry terrain type info but materials don't use them for blending
- UVs work for simple tiling but lack per-layer scale control

### Problem Statement

1. No runtime texture replacement on terrain layers — requires full mesh rebuild
2. No bridge between UnrealMCP material commands and Hexagon terrain system
3. Hard cuts between terrain types, no smooth blending
4. UV generation not optimized for textured materials (sides, tiling variation)

### Intended Outcome

An artist/developer can:
- Create a material with textures → apply it to a terrain layer via UnrealMCP or Blueprint
- Swap textures on terrain layers at runtime without rebuilding meshes
- See textures tile correctly and blend naturally across terrain type boundaries

---

## Current Architecture (Reference)

```
AHexTerrain::RegenerateTerrain()
  → FHexTerrainGenerator::Generate()         (noise → height → classify → color)
  → BuildAllChunks()
    → UHexTerrainChunk::SetCells(cells, ..., LayerMaterials, UVTileSize)
      → RebuildMeshForLOD(0, ...)
        ├─ Per-layer mode: group cells by type → separate mesh sections
        │    Section 0 (Water)  → SetMaterial(0, M_Water)
        │    Section 1 (Sand)   → SetMaterial(1, M_Sand)
        │    ...
        └─ Single-section mode: all cells → one section → SetMaterial(0, TerrainMaterial)

FHexPrismGenerator::Generate()
  → UVs: local (angle-based) or world-space planar (X/Tile, Y/Tile)
  → Vertex colors: flat terrain type color (blue/yellow/green/gray/white)
  → Normals, tangents computed

UnrealMCP MaterialCommands:
  → create_material, create_material_instance, set_material, set_material_parameter
  → All work at actor/mesh-component level, not terrain-layer level
```

Key files:
- `Plugins/Hexagon/Source/Hexagon/Public/AHexTerrain.h`
- `Plugins/Hexagon/Source/Hexagon/Private/AHexTerrain.cpp`
- `Plugins/Hexagon/Source/Hexagon/Public/HexTerrainChunk.h`
- `Plugins/Hexagon/Source/Hexagon/Private/HexTerrainChunk.cpp`
- `Plugins/Hexagon/Source/Hexagon/Public/HexPrismGenerator.h`
- `Plugins/Hexagon/Source/Hexagon/Private/HexPrismGenerator.cpp`
- `Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/MaterialCommands.cpp`
- `Plugins/Hexagon/DESIGN.md`

---

## Phase 1: Foundation — MID Cache + Runtime Texture Swap

### Goal

Each chunk creates Material Instance Dynamics (MIDs) so texture parameters can be
changed at runtime without mesh rebuilds.

### Why This First

This is the critical enabler. Without MIDs, every texture change requires creating
a new material asset and triggering a full `RegenerateTerrain()`. With MIDs, we call
`SetTextureParameterValue()` and the change is instant.

### Changes

**`HexTerrainChunk.h`** — add to `UHexTerrainChunk`:

```cpp
// Cached MIDs per section index (created from the assigned material)
UPROPERTY()
TMap<int32, TObjectPtr<UMaterialInstanceDynamic>> SectionMIDs;

// Set a texture parameter on the MID for a specific terrain type
void SetLayerTexture(EHexTerrainType Type, FName ParameterName, UTexture* Texture);

// Set a scalar parameter on the MID for a specific terrain type
void SetLayerScalarParameter(EHexTerrainType Type, FName ParameterName, float Value);

// Get the number of mesh sections (for inspection)
int32 GetNumSections() const { return GetNumSections(); }
```

**`HexTerrainChunk.cpp`** — modify `ApplyLayerMaterials()`:

- After `SetMaterial(SectionIdx, Mat)`, call `UMaterialInstanceDynamic::Create(Mat, this)`
- Store in `SectionMIDs` map
- `SetLayerTexture()`: look up MID → `SetTextureParameterValue()` → `MarkRenderStateDirty()`
- `SetLayerScalarParameter()`: same pattern with scalar

### Verification

1. Create terrain, assign material with `Texture2D` param `"BaseTex"` to Grass layer
2. Call `Chunk->SetLayerTexture(Grass, "BaseTex", SomeTexture)` from code
3. Grass cells update instantly, no mesh rebuild
4. Observe: `MarkRenderStateDirty` is the only refresh needed

---

## Phase 2: Bridge — UnrealMCP Terrain Commands

### Goal

Expose Phase 1 capabilities via UnrealMCP TCP commands, closing the gap
between the material pipeline and the terrain pipeline.

### New Commands

| Command | Key Parameters | Behavior |
|---------|---------------|----------|
| `set_terrain_layer_texture` | `terrainName`, `terrainType`, `parameterName`, `texturePath` | Iterates terrain's chunks → calls `SetLayerTexture()` on matching sections |
| `set_terrain_layer_material` | `terrainName`, `terrainType`, `materialPath` | Updates `LayerMaterials` → triggers `ApplyMaterialsToAllChunks()` |
| `get_terrain_layer_info` | `terrainName` | Returns JSON with terrain types, assigned materials, parameter names |

### Files to Modify

| File | Change |
|------|--------|
| `Commands/TerrainCommands.cpp` | **New**. Implement `HandleSetTerrainLayerTexture`, `HandleSetTerrainLayerMaterial`, `HandleGetTerrainLayerInfo` |
| `MCPCommandServer.cpp` | Forward declare + `else if (Method == TEXT("set_terrain_layer_texture"))` dispatch for 3 commands |
| `MCP_Server/server.rs` | Add 3 `#[tool]` async fns: `set_terrain_layer_texture`, `set_terrain_layer_material`, `get_terrain_layer_info` |
| `AHexTerrain.h` | Add `GetChunkMap()` accessor, `FindTerrainByName()` static helper |

### Finding the Terrain

Commands accept `terrainName` (actor label). Implementation uses:
```cpp
// In TerrainCommands.cpp
static AHexTerrain* FindTerrain(UWorld* World, const FString& Name)
{
    for (TActorIterator<AHexTerrain> It(World); It; ++It)
        if (It->GetName() == Name || It->GetActorLabel() == Name)
            return *It;
    return nullptr;
}
```

### Verification

1. Spawn terrain with `LayerMaterials` in UE Editor
2. From MCP client:
   ```python
   ue.cmd("set_terrain_layer_texture", {
       "terrainName": "HexTerrain_0",
       "terrainType": "Grass",
       "parameterName": "BaseTex",
       "texturePath": "/Game/Textures/Grass_01"
   })
   ```
3. Grass cells update texture without rebuild
4. `get_terrain_layer_info` returns correct state

---

## Phase 3: Vertex Color Blending + Master Material

### Goal

Enable smooth transitions between terrain types at cell boundaries using vertex
colors as blend weights in a master material.

### Approach

**A. Edge-aware vertex colors** — For each cell, check its 6 neighbors. If a neighbor
has a different terrain type, the shared edge's vertices get partial blend weights
for both types instead of the cell's flat color.

**B. Master material** (`M_HexTerrain_Master`) — Uses vertex color RGBA channels as
4-layer blend weights driving `Lerp` nodes between texture samples.

### Material Graph Concept

```
VertexColor.R → blend Water ↔ Sand
VertexColor.G → blend Sand  ↔ Grass
VertexColor.B → blend Grass ↔ Rock
VertexColor.A → blend Rock  ↔ Snow

Each blend = Lerp(TexA.Sample(UV), TexB.Sample(UV), weight)
→ Final BaseColor
```

4 channels cover 5 terrain types by chaining: Water↔Sand↔Grass↔Rock↔Snow.

### Files to Modify

| File | Change |
|------|--------|
| `HexTerrainChunk.cpp` | `BuildCellMesh()`: for each cell, look up 6 neighbors via `AHexTerrain::GetCell()`. Compute per-vertex blend weights. Pass as `FLinearColor` to prism generator. |
| `HexPrismGenerator.h/.cpp` | `Generate()` already accepts `BaseColor` per cell. Add optional per-vertex color override array. When provided, use it instead of the flat `BaseColor`. |
| `HexTerrainGenerator.h/.cpp` | Add `GetNeighborCoords(FHexCoord)` → returns 6 adjacent hex coords (static utility). |
| `Content/M_HexTerrain_Master.uasset` | **New asset**. Material with 5 `Texture2D` params + vertex-color blend graph. |
| `Content/MI_HexTerrain_Base.uasset` | **New asset**. Default instance with placeholder textures. |

### Verification

1. Create mixed Water/Sand/Grass terrain
2. Apply `M_HexTerrain_Master` as `TerrainMaterial`
3. Observe: cell edges have a ~1-cell gradient between types instead of hard cuts
4. Swap a texture parameter on the material → blending updates correctly

---

## Phase 4: UV Improvements

### Goal

Textures look natural on hex prisms — no visible tiling patterns, side faces get
proper mapping, and each terrain type can have independent UV scale.

### Three Independent Improvements

**A. Per-layer UV scale** — Add `TMap<EHexTerrainType, float> LayerUVScales` to
`AHexTerrain`. When generating UVs, multiply by the per-type factor. Sand typically
wants more tiling (smaller scale) than Snow.

**B. Per-cell random UV offset** — Deterministic offset seeded by `(Q * 2654435761)`
to break up visible tiling patterns across adjacent cells of the same type.

**C. Side-face triplanar UVs** — Currently sides use `(sideIdx/6, heightFrac)`.
Add a second approach: when `TextureTileSize > 0`, sides also get world-space UVs
using a simple triplanar-style projection (pick XZ or YZ plane based on normal).

### Files to Modify

| File | Change |
|------|--------|
| `AHexTerrain.h` | Add `LayerUVScales` property |
| `HexPrismGenerator.cpp` | `MakeUV`: apply per-type scale factor and per-cell offset. Add triplanar side UV option. |
| `HexTerrainChunk.cpp` | Pass scale + offset through to generator |

### Verification

1. Set `LayerUVScales[Grass] = 0.5` → grass repeats 2× as often
2. Set `LayerUVScales[Sand] = 2.0` → sand tiles wider
3. Observe: adjacent Grass cells don't show visible tiling grid
4. Side faces: texture no longer stretched on steep terrain

---

## Phase 5: End-to-End Workflow Script

### Goal

A single Python script that demonstrates the complete workflow, serving as both
documentation and integration test.

### Script: `scripts/texture_terrain_workflow.py`

```python
# 1. Create master material with texture params
# 2. Create material instances per terrain type
# 3. Spawn terrain via UnrealMCP
# 4. Assign materials to layers
# 5. Assign textures to each layer
# 6. Adjust per-layer UV scales
# 7. Take screenshot for visual verification
# 8. Print terrain layer info as JSON
```

### Verification

1. Fresh UE Editor → run script
2. Terrain appears with 5 distinct per-layer textures
3. Textures tile correctly, blending visible at boundaries
4. Screenshot confirms visual quality

---

## Summary

| Phase | Deliverable | Key Files |
|-------|------------|-----------|
| 1 | MID cache + runtime texture swap on chunks | `HexTerrainChunk.h/.cpp` |
| 2 | UnrealMCP terrain texture commands | New `TerrainCommands.cpp`, `MCPCommandServer.cpp`, `server.rs`, `AHexTerrain.h` |
| 3 | Vertex color blending + master material | `HexPrismGenerator`, `HexTerrainChunk`, new material assets |
| 4 | Per-layer UV scale, random offset, triplanar sides | `AHexTerrain.h`, `HexPrismGenerator.cpp`, `HexTerrainChunk.cpp` |
| 5 | Python workflow script | `scripts/texture_terrain_workflow.py` |

Each phase is independently testable. Phase 1 is the critical foundation —
everything else depends on having MIDs cached per section.
