# Hexagon 自定义 Landscape 实现计划

## Context

当前 `AHexTerrain` 将所有 hex cell 合并到一个 `UProceduralMeshComponent` 中，导致：
- 修改任一 cell 都要重建整个 mesh（无法增量更新）
- 无 frustum culling（全部画或不画）
- 无 LOD（远处 cell 和近处一样精细）
- 碰撞精度差（整个 mesh 一个简单碰撞体）
- 与 UE Landscape 材质生态系统不兼容

目标：在不继承 `ALandscapeProxy` 的前提下，让 hex terrain 具备 Landscape 级别的工程质量和编辑器体验。

## 推荐方案：Component 分块 + LOD + 材质兼容

分三个阶段实现，每个阶段都可独立交付和验证。

---

## Phase 1: Component 分块架构

### 新类结构

```
AHexTerrain (AActor)                          ← 对外接口不变
  ├── USceneComponent (Root)                   ← 改为普通 SceneComponent 做根
  ├── UHexTerrainChunk (×N)                    ← 新类，每个管 CHUNK_SIZE×CHUNK_SIZE 个 hex cell
  │     └── 内部使用 UProceduralMeshComponent  ← 单 chunk 的 mesh
  └── TMap<FIntPoint, UHexTerrainChunk*>       ← Chunk坐标 → Chunk
```

注意：计划案使用 `Chunk` 命名以避免与 UE 自带的 `ULandscapeComponent` 混淆。如果后续讨论觉得 Component 更直观可以改名。

### UHexTerrainChunk 设计 (`HexTerrainChunk.h/.cpp`)

```cpp
UCLASS(ClassGroup=(Hexagon))
class UHexTerrainChunk : public USceneComponent  // 或 UProceduralMeshComponent 直接
{
    // Chunk 在 grid 中的坐标 (chunk 坐标，不是 hex 坐标)
    FIntPoint ChunkCoord;

    // 此 chunk 覆盖的 cell 数据（引用或拷贝）
    TArray<FHexTerrainCellData> Cells;

    // 内部 mesh
    UPROPERTY()
    UProceduralMeshComponent* Mesh;

    // 重建此 chunk 的 mesh
    void RebuildMesh(float EffectiveRadius, float Gap, const FHexTerrainConfig& Config);

    // 标记 dirty，下一帧或手动 flush 时重建
    void MarkDirty();
};
```

每个 Chunk 默认管 **16×16 = 256 个 hex cell**（用轴向坐标范围的矩形区域）。

### AHexTerrain 改造

- `GridRadius` 不变，内部计算总 cell 数 → 分配 chunk
- `RegenerateTerrain()` 改为：生成 cell 数据 → 分发到 chunk → 各 chunk 独立 rebuild
- 新增 `CHUNK_SIZE` 常量（可调，默认 16）
- `PostEditChangeProperty`：只标记受影响的 chunk dirty，不全量重建

### 关键实现细节

1. **Chunk 分配逻辑**:
   ```
   For each hex cell at (q, r):
       chunkX = Floor(q / CHUNK_SIZE)
       chunkY = Floor(r / CHUNK_SIZE)
       → 分配到 Chunk(chunkX, chunkY)
   ```

2. **Chunk 边界 cell 的侧边处理**: 邻接 chunk 的 cell 之间不需要共享顶点（和 Landscape quad grid 不同），因为 hex cell 是独立的 prism，只需确保两个相邻 cell 的 world position 一致。

3. **增量更新**: 当属性变化时，遍历所有 chunk 调用 `RebuildMesh()`。未来的优化可以在 `PostEditChangeProperty` 中做更精细的 dirty 标记。

4. **Collision**: 每个 chunk 的 `ProceduralMeshComponent` 独立碰撞，比之前的全 mesh 碰撞更精确。

### 验证方式

- 在编辑器中拖入 `AHexTerrain`，调整参数，确认渲染正常且无接缝
- 用 `hex.SpawnTerrain` 命令创建大 terrain（GridRadius=20, ~1200 cells），对比改造前后帧率
- 确认每个 chunk 的 collision 独立工作

---

## Phase 2: LOD 系统

### 设计思路

不为每个 chunk 做连续 LOD morphing（UE Landscape 的做法），而是**每个 chunk 根据到摄像机的距离选择一个 LOD 级别**。同一 chunk 内所有 cell 用相同的简化参数。

### LOD 级别定义

| LOD | HeightSegments | bCapTop | bCapBottom | 距离阈值 |
|-----|---------------|---------|------------|---------|
| 0   | 2             | true    | true       | < 2000  |
| 1   | 1             | true    | false      | < 5000  |
| 2   | 0 (flat hex)  | true    | false      | >= 5000 |

### 实现

```cpp
struct FHexTerrainLODSettings
{
    float LOD0Distance = 2000.0f;
    float LOD1Distance = 5000.0f;
    int32 LOD0HeightSegments = 2;
    int32 LOD1HeightSegments = 1;
};

// UHexTerrainChunk 中
void UpdateLOD(const FVector& CameraPosition);
void RebuildMeshForLOD(int32 LODLevel, ...);
```

- 在 `AHexTerrain::Tick` 中遍历所有 chunk，计算到 `GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetCameraLocation()` 的距离
- 距离变化超过阈值（比如 10%）时，chunk 切换到对应 LOD 并重建 mesh
- LOD 切换有 debounce，避免在边界附近反复切换

### 扩展考虑

- 后续可增加 **chunk culling**：chunk 完全不可见时直接 `SetVisibility(false)`
- LOD 设置暴露在 `AHexTerrain` 的 Details 面板中，用户可调

### 验证方式

- 在编辑器中调整摄像机距离，观察远处 cell 简化
- 对比 Phase 1 的大 terrain 帧率

---

## Phase 3: Landscape 材质兼容

### 目标

让 hex terrain **直接使用现有的 Landscape 材质**（`LandscapeLayerBlend`, `LandscapeLayerCoords` 等节点），而不需要继承 `ALandscapeProxy`。

### 原理

Landscape 材质节点与 `ALandscapeProxy` 的解耦点在于：
- `LandscapeLayerCoords` 节点读取的是 **vertex 的世界坐标** 和 **每个 component 上传的 UV 变换参数**
- `LandscapeLayerBlend` 节点读取的是 **weightmap 纹理**（材质参数中传入的 layer weight textures）
- `LandscapeLayerWeight` 做 height-based 混合（读 heightmap 纹理）

我们不需要这些节点在 hex terrain 上工作得和 Landscape 一模一样，只需要让常用的 layer blending 材质能跑。

### 实施方案：自定义材质参数提供

```cpp
// UHexTerrainChunk 创建时
void UHexTerrainChunk::SetupMaterialInstance(UMaterialInterface* BaseMaterial)
{
    // 1. 创建 MID
    MaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);

    // 2. 生成此 chunk 的 heightmap 纹理（RGBA8，和 Landscape 格式一致）
    //    - 对 chunk 内每个 cell，用其 world-space 位置采样高度
    //    - 写入一张小纹理（如 32×32）
    HeightmapTexture = CreateChunkHeightmap();

    // 3. 生成此 chunk 的 layer weight 纹理
    //    - 基于 EHexTerrainType 写入对应的 layer weight
    WeightmapTexture = CreateChunkWeightmap();

    // 4. 设置材质参数
    MaterialInstance->SetTextureParameterValue("Heightmap", HeightmapTexture);
    MaterialInstance->SetTextureParameterValue("Weightmap0", WeightmapTexture);

    // 5. 应用到 ProceduralMeshComponent
    Mesh->SetMaterial(0, MaterialInstance);
}
```

### 简化方案（推荐优先做这个）

与其试图完全复刻 Landscape 材质兼容（那需要 hack 材质编译器的内部行为），不如提供**更实用的材质方案**：

```cpp
// AHexTerrain 中
UPROPERTY(EditAnywhere, Category = "Material")
TMap<EHexTerrainType, UMaterialInterface*> TerrainLayerMaterials;

// 每个 chunk 根据其内部 cell 的 terrain type，使用不同 sub-material
// 类似 Landscape 的 layer blending，但在 mesh 层面预计算
```

这样用户可以直接拖入标准 UE 材质分别给水/沙/草/岩/雪。

### 实际推荐：Phase 3 做简化版

1. **Per-terrain-type 材质映射**（上面简化方案）
2. **Vertex color blending**：已经在做了 — `FHexTerrainCellData::Color` 写到 vertex color，用户材质可以读 vertex color 做简单混合
3. **运行时材质切换**：通过 `UMaterialInstanceDynamic` 支持

### 验证方式

- 给 hex terrain 的不同 layer 指定不同材质（水面材质、草地材质、岩石材质）
- 运行时修改参数验证材质更新

---

## 文件变更清单

### 新建文件

| 文件 | 说明 |
|------|------|
| `HexTerrainChunk.h` | `UHexTerrainChunk` 类声明 |
| `HexTerrainChunk.cpp` | `UHexTerrainChunk` 实现 |

### 修改文件

| 文件 | 变更 |
|------|------|
| `AHexTerrain.h` | 添加 chunk 管理成员、Tick override、chunk 大小常量 |
| `AHexTerrain.cpp` | RegenerateTerrain 改为分 chunk 逻辑、添加 LOD tick |
| `HexTerrainGenerator.h/.cpp` | 可能需要按 chunk 范围查询 cell（非全局重建） |
| `Hexagon.Build.cs` | 无新增依赖（Phase 1-2） |

---

## 里程碑与验证

### M1: Component 分块完成
- 编辑器可视化：每个 chunk 不同线框颜色区分
- 大 terrain（GridRadius=30, ~2700 cells）重建时间 < 500ms
- 所有现有功能正常（terrain type、color、gap、noise parameters）

### M2: LOD 完成
- GridRadius=30 terrain 在编辑器内稳定 60fps（当前单 mesh 预期 25-40fps）
- LOD 切换无视觉 pop（远距离时 cell 本身已很小）

### M3: 材质兼容
- 可以为每种 terrain type 指定独立材质
- 材质参数可以在编辑器和运行时实时调整

---

## 不做的事情（明确范围边界）

- 不继承 `ALandscapeProxy` 或 `ALandscape`
- 不做 World Partition / streaming
- 不做 spline-based terrain deformation
- 不做 grass 自动生成（可后续扩展）
- 不做 Nanite 支持
- 不做 edit layer 系统
