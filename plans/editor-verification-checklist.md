# Hex Terrain Editor 验证清单

> **使用方法**：进入 UE 编辑器，按顺序逐项执行。通过打 ✅，失败记录实际表现。

---

## 0. 前置准备

| # | 操作步骤 | 预期结果 |
|---|---------|---------|
| 0.1 | 在 Rider/VS 中 Build `Hexagon` 模块，启动 UE 编辑器 | 0 errors，编辑器正常启动 |
| 0.2 | 打开地图 `HexagonPlayground`（File → Open Level → 选择 HexagonPlayground） | 地图加载成功，视口可见 |
| 0.3 | 打开 Output Log（Window → Developer Tools → Output Log） | 日志面板可见，后续可观察命令输出 |

---

## 1. 基础地形创建

**测试目标**：验证 terrain 能正常生成、打印统计、清理。

### 1.1 创建测试场景

| 步骤 | 操作 |
|------|------|
| ① | 在 Output Log 底部的控制台输入：`hex.CreateTestScene 10 120` |
| ② | 按 Enter 执行 |

**预期结果**：
- 视口中出现一个六边形地形（GridRadius=10，CellRadius=120）
- 地形上方有方向光（DirectionalLight）和天空光（SkyLight）
- Output Log 输出类似：
  ```
  ========== Test Scene Ready ==========
  ========== HexTerrain Stats ==========
    Total Cells:    331
    Chunks:         N  (CHUNK_SIZE=16)
    Terrain types:  Water=X Sand=Y Grass=Z Rock=W Snow=V
  ========================================
  ```

### 1.2 查看统计

| 步骤 | 操作 |
|------|------|
| ① | 控制台输入：`hex.Stats` |

**预期结果**：Output Log 再次输出 terrain stats，与 1.1 一致。

### 1.3 清理

| 步骤 | 操作 |
|------|------|
| ① | 控制台输入：`hex.DestroyAll` |

**预期结果**：视口中 terrain、灯光全部消失。Log 显示 `destroyed N hex actors`。

---

## 2. LayerMaterials 多材质

**测试目标**：验证按地形类型分配材质、编辑器实时修改材质触发生效。

### 2.1 创建混合地形

| 步骤 | 操作 |
|------|------|
| ① | 控制台输入：`hex.CreateGrassSandTerrain 14` |

**预期结果**：
- 视口中出现 terrain（GridRadius=14，CellRadius=120）
- 低洼处显示 **蓝色**（Water），中低处显示 **黄色**（Sand），高处显示 **绿色**（Grass）
- 三种颜色/材质在同一 terrain 上同时可见

### 2.2 查看已分配的材质

| 步骤 | 操作 |
|------|------|
| ① | 在视口中 **单击选中** terrain actor |
| ② | Details 面板 → 找到 **Hexagon \| Material** 分类 |
| ③ | 展开 `LayerMaterials` 条目 |

**预期结果**：TMap 编辑器显示如下映射：

| Key | Value |
|-----|-------|
| Water | `M_Water` |
| Sand | `M_Sand` |
| Grass | `M_Grass` |
| Rock | `M_Rock` |
| Snow | `M_Snow` |

### 2.3 运行时添加新材质

| 步骤 | 操作 |
|------|------|
| ① | 在 `LayerMaterials` TMap 中点击 **+** 添加条目 |
| ② | Key 选 `Rock`，Value 下拉选择 `M_Rock` |
| ③ | 等待 terrain 自动重建（约 0.5-1 秒） |

**预期结果**：terrain 重建后，高海拔的 Rock 区域绑定 M_Rock 材质，颜色变化可见。

### 2.4 删除材质条目

| 步骤 | 操作 |
|------|------|
| ① | 在 `LayerMaterials` 中点击刚添加的 Rock 条目的 **×** 删除 |

**预期结果**：terrain 重建，Rock 区域回退到 `TerrainMaterial`（默认灰色）。

### 2.5 修改默认材质

| 步骤 | 操作 |
|------|------|
| ① | 在 **Hexagon \| Material** 中找到 `TerrainMaterial` |
| ② | 下拉选择 `M_Grass` |

**预期结果**：所有未在 `LayerMaterials` 中分配到的 cell（如 Rock/Snow）全部变绿。

---

## 3. 手动模式

**测试目标**：验证手动覆盖 terrain type 取代自动噪声分类。

### 3.1 启用手动模式

| 步骤 | 操作 |
|------|------|
| ① | 选中 terrain |
| ② | Details → **Hexagon \| Manual** → 勾选 `bManualMode` |

**预期结果**：
- terrain 全部 cell 变成 `DefaultManualType` 的颜色（默认=Grass，绿色）
- 原来的 Water/Sand 区域消失

### 3.2 切换默认类型

| 步骤 | 操作 |
|------|------|
| ① | 修改 `DefaultManualType` 下拉 → 选 `Sand` |

**预期结果**：terrain 重建，全部 cell 变 Sand（黄色）。

### 3.3 SetCell 单个 cell

| 步骤 | 操作 |
|------|------|
| ① | 控制台输入：`hex.SetCell 5 3 Grass` |
| ② | 控制台输入：`hex.SetCell 5 4 Water` |

**预期结果**：
- 坐标 (5,3) 的 cell 变绿色
- 坐标 (5,4) 的 cell 变蓝色
- Output Log 输出：`hex.SetCell: (5, 3) → EHexTerrainType::Grass`
- 其余 cell 保持 Sand（黄色）

### 3.4 FillRing 批量设置

| 步骤 | 操作 |
|------|------|
| ① | 控制台输入：`hex.FillRing 0 0 2 Rock` |
| ② | 控制台输入：`hex.FillRing 8 3 1 Sand` |

**预期结果**：
- 中心 (0,0) 半径 2 环共 **19 个 cell** 全变 Rock（灰色）
- (8,3) 周围 1 环共 **7 个 cell** 全变 Sand（黄色）
- Output Log 分别输出设置的 cell 数量

### 3.5 手动↔自动切换

| 步骤 | 操作 |
|------|------|
| ① | 取消勾选 `bManualMode` |
| ② | 重新勾选 `bManualMode` |

**预期结果**：
- 取消后：terrain 恢复 noise 自动分类（Water/Sand/Grass 回归）
- 重新勾选后：之前手动设置的所有覆盖项**保留并生效**，未覆盖 cell 用 `DefaultManualType`

### 3.6 查看 TMap 数据

| 步骤 | 操作 |
|------|------|
| ① | 在 **Hexagon \| Manual** → 展开 `ManualCellTypes` |

**预期结果**：TMap 编辑器列出之前 setCell/FillRing 添加的所有条目，格式：
```
(5,3)  → Grass
(5,4)  → Water
(0,0)  → Rock
...
```

---

## 4. 笔刷绘制

**测试目标**：验证 FEdMode 视口笔刷交互全流程。

### 4.1 准备 & 进入模式

| 步骤 | 操作 |
|------|------|
| ① | 控制台输入：`hex.DestroyAll` 清理 |
| ② | 控制台输入：`hex.CreateGrassTerrain 12` |
| ③ | 菜单栏 → **Modes** 下拉（通常在工具栏左上角区域） |
| ④ | 选择 **Hex Terrain Paint** |

**预期结果**：
- 菜单中出现 "Hex Terrain Paint" 选项
- 点击后：Output Log 显示 `HexTerrainPaint mode entered. Left-click to paint, Ctrl+Left-click to erase.`
- terrain 自动被选中（Details 面板显示其属性）
- Details 面板可见 **Hexagon \| Brush** 和 **Hexagon \| Manual** 分类

### 4.2 笔刷悬停预览

| 步骤 | 操作 |
|------|------|
| ① | 鼠标光标移到 terrain 上方（不点击） |

**预期结果**：
- 光标处出现 **蓝色同心圆**（3 圈），表示笔刷范围
- 圆形下方出现 **白色六边形轮廓线**，圈出将被涂刷的具体 cell
- 鼠标移出 terrain → 圆圈和轮廓消失

### 4.3 调整笔刷大小

| 步骤 | 操作 |
|------|------|
| ① | Details → **Hexagon \| Brush** → 修改 `BrushRadius = 3` |

**预期结果**：蓝色圆圈变大，白色 cell 轮廓覆盖更多六边形。

| 步骤 | 操作 |
|------|------|
| ② | 修改 `BrushRadius = 6` |

**预期结果**：蓝色圆圈继续变大，白色 cell 轮廓线**消失**（>5 不画轮廓以保持性能）。

| 步骤 | 操作 |
|------|------|
| ③ | 设置 `BrushRadius = 2` |

**预期结果**：恢复适中大小。

### 4.4 单次点击涂刷

| 步骤 | 操作 |
|------|------|
| ① | 设置 `BrushTerrainType = Sand` |
| ② | **左键单击** terrain 上某个位置 |

**预期结果**：
- 点击瞬间笔刷变**橙色**
- 笔刷覆盖范围内的 cell 从 Grass（绿）变为 Sand（黄）
- 鼠标松开后笔刷恢复蓝色

### 4.5 拖拽连续涂刷

| 步骤 | 操作 |
|------|------|
| ① | **左键按住**在 terrain 上缓慢拖拽 |

**预期结果**：
- 笔刷保持**橙色**
- 拖拽路径上的 cell 连续变为 Sand（黄）
- 经过的 chunk 实时重建，边界与未涂刷区域清晰

### 4.6 切换涂刷类型

| 步骤 | 操作 |
|------|------|
| ① | 修改 `BrushTerrainType = Water` |
| ② | 左键拖拽涂刷另一区域 |

**预期结果**：新涂刷区域变 Water（蓝），与之前的 Sand（黄）区域并排可见。

### 4.7 Ctrl 擦除

| 步骤 | 操作 |
|------|------|
| ① | 按住 **Ctrl** 键不放 |
| ② | 在之前涂刷的 Sand 区域上 **左键点击** |

**预期结果**：被擦除的 cell 恢复为 `DefaultManualType`（Grass，绿），不是变回自动分类。

### 4.8 视角操作不受影响

| 步骤 | 操作 |
|------|------|
| ① | **右键按住拖拽** → 旋转视角 |
| ② | **滚轮滚动** → 缩放 |
| ③ | **中键按住拖拽** → 平移 |

**预期结果**：三种视角操作正常工作，不触发涂刷。

### 4.9 退出笔刷模式

| 步骤 | 操作 |
|------|------|
| ① | 按键盘 **Esc** 键 |

**预期结果**：退出 Hex Terrain Paint 模式，恢复默认编辑模式。视口中笔刷消失。可以用鼠标正常选择/移动 actor。

---

## 5. 纹理 UV

**测试目标**：验证 `TextureTileSize` 控制 World-Space UV。

### 5.1 检查默认值

| 步骤 | 操作 |
|------|------|
| ① | 选中 terrain → Details → **Hexagon \| Texture** |
| ② | 查看 `TextureTileSize` |

**预期结果**：值为 `200.0`，滑块可拖动，下限为 0（ClampMin=0）。

### 5.2 调整 TileSize（密集平铺）

| 步骤 | 操作 |
|------|------|
| ① | 设置 `TextureTileSize = 100` |

**预期结果**：terrain 自动重建（无报错）。如果使用了带纹理的材质，纹理图案会更加密集。

### 5.3 调整 TileSize（稀疏平铺）

| 步骤 | 操作 |
|------|------|
| ① | 设置 `TextureTileSize = 500` |

**预期结果**：terrain 自动重建。纹理图案更加稀疏。

### 5.4 关闭 World-Space UV

| 步骤 | 操作 |
|------|------|
| ① | 设置 `TextureTileSize = 0` |

**预期结果**：terrain 自动重建，回退到 per-cell 默认 UV（每个 cell 独立 [0,1] UV 空间）。

### 5.5 恢复

| 步骤 | 操作 |
|------|------|
| ① | 设置 `TextureTileSize = 200` |

**预期结果**：恢复正常 world-space UV。

---

## 6. 边界 & 鲁棒性

**测试目标**：验证异常操作的容错能力。

### 6.1 超出范围坐标

| 步骤 | 操作 |
|------|------|
| ① | 控制台输入：`hex.SetCell 999 999 Snow` |

**预期结果**：
- **不崩溃、不卡顿**
- 坐标超出 terrain 范围，条目被加入 ManualCellTypes 但无匹配 cell（无可见变化）

### 6.2 无效类型名

| 步骤 | 操作 |
|------|------|
| ① | 控制台输入：`hex.SetCell 0 0 Lava` |

**预期结果**：
- **不崩溃**
- Output Log 显示：`Unknown terrain type 'Lava'. Use: Water, Sand, Grass, Rock, Snow`
- terrain 无变化

### 6.3 无 terrain 时命令

| 步骤 | 操作 |
|------|------|
| ① | 控制台输入：`hex.DestroyAll` |
| ② | 控制台输入：`hex.SetCell 0 0 Grass` |

**预期结果**：
- **不崩溃**
- Output Log 显示：`No AHexTerrain found. Try hex.CreateTestScene first.`

### 6.4 无 terrain 时进入 Paint 模式

| 步骤 | 操作 |
|------|------|
| ① | Modes 下拉 → 选择 **Hex Terrain Paint** |

**预期结果**：
- 模式正常进入
- 视口中无笔刷显示（因为没有 terrain）
- **不崩溃**

### 6.5 跨 Chunk 涂刷

| 步骤 | 操作 |
|------|------|
| ① | `hex.CreateGrassTerrain 12` → 进入 Paint 模式 |
| ② | BrushRadius=3，在 terrain 上**大范围快速拖拽**，跨越 chunk 边界 |

**预期结果**：
- 两侧 chunk 都正确更新
- Chunk 边界处无接缝、无漏刷

---

## 7. 性能

**测试目标**：验证大 terrain 增量重建的流畅度。

### 7.1 大 Terrain 涂刷

| 步骤 | 操作 |
|------|------|
| ① | 控制台输入：`hex.DestroyAll` |
| ② | 控制台输入：`hex.CreateTestScene 20 100`（GridRadius=20，~1200 cell） |
| ③ | 进入 Hex Terrain Paint 模式 |
| ④ | BrushRadius=3，BrushTerrainType=Sand |
| ⑤ | **快速拖拽涂刷** 5-10 秒，覆盖大片区域 |

**预期结果**：
- 涂刷过程无明显卡顿或帧率骤降
- 仅受影响的 chunk 被重建（非全 terrain 重建）
- Output Log 无 error/exception

---

## 结果汇总

| 分类 | 项数 | 通过 | 失败 | 备注 |
|------|:---:|:---:|:---:|------|
| 0. 前置准备 | 3 | | | |
| 1. 基础地形 | 3 | | | |
| 2. LayerMaterials | 5 | | | |
| 3. 手动模式 | 6 | | | |
| 4. 笔刷绘制 | 9 | | | |
| 5. 纹理 UV | 5 | | | |
| 6. 边界鲁棒性 | 5 | | | |
| 7. 性能 | 1 | | | |
| **总计** | **37** | | | |

---

> **备注栏**：失败项记录实际表现（如：崩溃、无反应、颜色不对、报错信息等）。
