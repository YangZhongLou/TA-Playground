# Hexagon Terrain — 纹理工作流操作指南

> 最后更新: 2026-06-09 | 适用版本: Phase 1–4 完成

---

## 概述

Hex Terrain 支持**按地形类型分配材质**，并且可以在运行时**动态替换纹理**，无需重建 mesh。
核心能力：

- 5 种地形类型各分配独立材质 (Water / Sand / Grass / Rock / Snow)
- 运行时替换纹理参数，瞬时生效
- Vertex Color 驱动的相邻 cell 颜色混合
- 世界空间 UV + per-layer 缩放 + 随机偏移 + 侧边 triplanar

---

## 1. 材质系统

### 1.1 两种材质模式

| 模式 | 触发条件 | 行为 |
|------|---------|------|
| **单一材质** | `LayerMaterials` 为空 | 所有 cell 共用一个 `TerrainMaterial` |
| **分层材质** | `LayerMaterials` 非空 | 每种地形类型 = 独立 Mesh Section + 独立材质 |

### 1.2 材质要求

为使纹理替换生效，材质必须包含 **Texture2D 参数**（如 `BaseTex`）：

```
材质图:
  TextureSampleParameter2D("BaseTex") → BaseColor
  TextureSampleParameter2D("NormalTex") → Normal
  ScalarParameter("Roughness") → Roughness
```

每个 Section 会自动创建 **MaterialInstanceDynamic (MID)**，参数可在运行时修改。

---

## 2. 快速开始

### 2.1 在 UE Editor 中操作

1. 选中 `AHexTerrain` Actor → Details 面板
2. 展开 **Hexagon | Material**：
   - `TerrainMaterial` → 默认材质（fallback）
   - `LayerMaterials` → 添加条目，如 `{Grass: M_Grass}`
3. LayerMaterials 非空时，地形自动按类型分 Section

### 2.2 通过 UnrealMCP 命令操作

UnrealMCP 通过 TCP 13377 端口向 UE Editor 发送命令。3 个新命令：

#### set_terrain_layer_texture — 替换指定 terrain type 的纹理

```json
{
  "method": "set_terrain_layer_texture",
  "params": {
    "terrainName": "HexTerrain_0",
    "terrainType": "Grass",
    "parameterName": "BaseTex",
    "texturePath": "/Game/Textures/Grass_01"
  }
}
```

#### set_terrain_layer_material — 替换指定 terrain type 的材质

```json
{
  "method": "set_terrain_layer_material",
  "params": {
    "terrainName": "HexTerrain_0",
    "terrainType": "Sand",
    "materialPath": "/Game/Materials/M_Sand"
  }
}
```

#### get_terrain_layer_info — 查询 terrain 状态

```json
{
  "method": "get_terrain_layer_info",
  "params": {
    "terrainName": "HexTerrain_0"
  }
}
```

返回示例：
```json
{
  "success": true,
  "result": {
    "terrainName": "HexTerrain_0",
    "gridRadius": 5,
    "cellRadius": 100.0,
    "cellCount": 91,
    "chunkCount": 1,
    "textureTileSize": 200.0,
    "bManualMode": false,
    "layers": [
      {"type": "Water", "activeChunks": 1},
      {"type": "Sand",  "activeChunks": 0},
      {"type": "Grass", "activeChunks": 1},
      {"type": "Rock",  "activeChunks": 1},
      {"type": "Snow",  "activeChunks": 1}
    ]
  }
}
```

### 2.3 Python 调用示例

```python
import json, socket

class UEClient:
    def __init__(self, host="127.0.0.1", port=13377):
        self.host, self.port = host, port
    def cmd(self, method, params=None):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(30.0)
        sock.connect((self.host, self.port))
        msg = json.dumps({"id": method, "method": method, "params": params or {}}) + "\n"
        sock.sendall(msg.encode("utf-8"))
        resp = json.loads(sock.recv(65536).decode("utf-8").strip())
        sock.close()
        ok = "OK" if resp.get("success") else f"FAIL: {resp.get('error','')}"
        print(f"  [{ok}] {method}")
        return resp

ue = UEClient()

# 1. 创建带纹理参数的主材质
ue.cmd("create_material", {
    "path": "/Game/Materials/M_Terrain_Grass",
    "shadingModel": "default_lit",
    "baseColor": [0.2, 0.6, 0.15]
})

# 2. 分配到 Grass layer
ue.cmd("set_terrain_layer_material", {
    "terrainName": "HexTerrain_0",
    "terrainType": "Grass",
    "materialPath": "/Game/Materials/M_Terrain_Grass"
})

# 3. 替换纹理
ue.cmd("set_terrain_layer_texture", {
    "terrainName": "HexTerrain_0",
    "terrainType": "Grass",
    "parameterName": "BaseTex",
    "texturePath": "/Game/Textures/Grass_Variant_02"
})

# 4. 查询状态
info = ue.cmd("get_terrain_layer_info", {"terrainName": "HexTerrain_0"})
print(json.dumps(info, indent=2))
```

---

## 3. UV 纹理控制

### 3.1 属性一览

| 属性 | 位置 | 默认值 | 说明 |
|------|------|--------|------|
| `TextureTileSize` | `Hexagon\|Texture` | 200 | 世界空间 UV 平铺尺寸（cm）。0 = 局部 UV |
| `LayerUVScales` | `Hexagon\|Texture` | 空 | Per-type UV 缩放。1.0=默认，0.5=加倍平铺 |

### 3.2 UV 模式对比

| 模式 | 顶面 | 侧面 | 跨 cell | 适用场景 |
|------|------|------|---------|---------|
| **局部** (`TileSize=0`) | 径向，每 cell 独立 | `(sideIdx/6, hFrac)` | ❌ 断裂 | 纯色 / 无纹理材质 |
| **世界空间** (`TileSize>0`) | Planar X/Y 投影 | Triplanar (XZ/YZ) | ✅ 连续 | 带纹理的材质 |

### 3.3 UV 偏移

每个 cell 有一个**确定性随机偏移**（由 hex 坐标哈希生成），幅度 ±0.15 texel。
这能打破相邻同类型 cell 的可见平铺重复感，且重建时稳定不变。

### 3.4 调整示例

```
// 让 Grass 纹理更密（每 100cm 重复一次）
LayerUVScales[Grass] = 0.5

// 让 Snow 纹理更宽（每 400cm 重复一次）
LayerUVScales[Snow] = 2.0

// 全局 tile size 改小 → 纹理更密
TextureTileSize = 100
```

---

## 4. Vertex Color 混合

### 4.1 工作原理

每个六边形 cell 有 6 个角点。每个角点被 3 个 cell 共享（cell 自身 + 2 个邻居）。

BuildCellMesh 在生成 mesh 时：
1. 查询各角点的 3 个共享 cell（通过 hex 坐标 lookup）
2. 取 3 个 cell 颜色的平均值作为该角点的 vertex color
3. 中心顶点保持 cell 自身颜色

效果：相邻不同 terrain type 的 cell 边界处有 ~1 cell 宽的渐变过渡。

### 4.2 材质中读取 Vertex Color

```
VertexColor 节点 → 直接连入 BaseColor（自动获得混合效果）
或
VertexColor.R → Lerp(TexA, TexB, weight)  ← 手动控制
```

### 4.3 调试

- `bDebugChunkColors`: 每个 chunk 不同颜色，看清 chunk 边界
- `bDebugLODColors`: 绿=近景 / 黄=中景 / 红=远景
- Debug color 激活时，vertex color 混合暂停（调试色覆盖）

---

## 5. 性能考量

| 操作 | 代价 | 说明 |
|------|------|------|
| 替换纹理 (`SetLayerTexture`) | **极小** | 仅更新 MID 参数 + 标记渲染脏 |
| 替换材质 (`SetLayerMaterial`) | **中** | 重新创建所有 chunk 的 MID |
| 修改 LayerMaterials 配置 | **大** | 触发全量 `RegenerateTerrain()` |
| 修改 UV 参数 | **大** | 同上 |

---

## 6. 完整工作流示例

以下脚本展示从零到有纹理地形的完整流程：

```python
# scripts/texture_terrain_workflow.py
import json, socket, sys

class UEClient:
    def __init__(self, host="127.0.0.1", port=13377):
        self.host, self.port = host, port
    def cmd(self, method, params=None):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(30.0)
        sock.connect((self.host, self.port))
        msg = json.dumps({"id": method, "method": method, "params": params or {}}) + "\n"
        sock.sendall(msg.encode("utf-8"))
        resp = json.loads(sock.recv(65536).decode("utf-8").strip())
        sock.close()
        return resp

def main():
    ue = UEClient()
    print("Connected to UE Editor\n")

    terrain_name = "HexTerrain_Workflow"
    types = ["Water", "Sand", "Grass", "Rock", "Snow"]
    colors = {
        "Water": [0.1, 0.3, 0.6],
        "Sand":  [0.8, 0.7, 0.4],
        "Grass": [0.2, 0.6, 0.15],
        "Rock":  [0.45, 0.4, 0.38],
        "Snow":  [0.95, 0.95, 0.98],
    }

    # === Step 1: 为每种地形类型创建材质 ===
    print("--- Creating materials ---")
    for t in types:
        mat_path = f"/Game/Materials/M_Terrain_{t}"
        r = ue.cmd("create_material", {
            "path": mat_path,
            "shadingModel": "default_lit",
            "baseColor": colors[t],
            "roughness": 0.8,
            "texture": "/Engine/EngineResources/DefaultTexture"
        })
        if r.get("success"):
            print(f"  {t}: created")

    # === Step 2: 创建 terrain (如果不存在) ===
    print("--- Setting up terrain ---")
    r = ue.cmd("run_console_command", {
        "command": f"hex.CreateFullTerrain {terrain_name} 8"
    })

    # === Step 3: 分配材质到各 layer ===
    print("--- Assigning layer materials ---")
    for t in types:
        r = ue.cmd("set_terrain_layer_material", {
            "terrainName": terrain_name,
            "terrainType": t,
            "materialPath": f"/Game/Materials/M_Terrain_{t}"
        })
        status = "OK" if r.get("success") else f"FAIL: {r.get('error','')}"
        print(f"  {t}: {status}")

    # === Step 4: 调整 UV ===
    print("--- Configuring UV ---")
    r = ue.cmd("set_actor_property", {
        "actorName": terrain_name,
        "propertyName": "TextureTileSize",
        "propertyValue": "200"
    })

    # === Step 5: 验证 ===
    print("--- Verification ---")
    info = ue.cmd("get_terrain_layer_info", {"terrainName": terrain_name})
    if info.get("success"):
        r = info["result"]
        print(f"  Cells: {r['cellCount']}, Chunks: {r['chunkCount']}")
        for layer in r["layers"]:
            print(f"  {layer['type']}: {layer['activeChunks']} active chunks")

    # === Step 6: 截图 ===
    ue.cmd("focus_viewport", {"actorName": terrain_name})
    ue.cmd("take_screenshot", {"filename": "texture_terrain_workflow"})
    print("\nScreenshot saved. Done!")

if __name__ == "__main__":
    main()
```

---

## 7. 常见问题

### Q: 纹理替换后没有效果？
- 确认材质使用了 `TextureSampleParameter2D` 节点（不是普通 `TextureSample`）
- 确认参数名拼写完全一致（区分大小写）
- 检查 `get_terrain_layer_info` 确认该 terrain type 有 active chunks

### Q: UV 纹理跨 cell 不对齐？
- 确保 `TextureTileSize > 0`（启用世界空间 UV）
- 确保 LayerUVScales 值一致（相邻不同类型的 cell）

### Q: 如何恢复默认材质？
- 将 `LayerMaterials` 中对应条目移除，该类型回退到 `TerrainMaterial`
- 或将材质路径指向 `/Engine/BasicShapes/BasicShapeMaterial`

---

## 8. 相关文档

| 文档 | 路径 |
|------|------|
| 实现计划 | `plans/texture-terrain-workflow.md` |
| 架构设计 | `Plugins/Hexagon/DESIGN.md` |
| 测试计划 | `plans/hex-terrain-test-plan.md` |
