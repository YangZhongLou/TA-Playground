# Hex Terrain Editor 验证清单

> **使用方法**：进入 UE 编辑器，按顺序逐项执行。通过打 ✅，失败记录实际表现。

---

## 0. 前置准备

| # | 操作步骤 | 预期结果 |
|---|---------|---------|
| 0.1 | 在 Rider/VS 中 Build `Hexagon` 模块，启动 UE 编辑器 | 0 errors，编辑器正常启动 |
| 0.2 | 打开地图 `HexagonPlayground`（File → Open Level → 选择 HexagonPlayground） | 地图加载成功，视口可见 |
| 0.3 | 打开 Output Log（Window → Developer Tools → Output Log） | 日志面板可见，后续可观察自动重建日志 |

---

## 1. 基础地形创建

**测试目标**：验证通过 Place Actors 面板创建 terrain、自动生成、手动删除。

### 1.1 创建地形

| 步骤 | 操作 |
|------|------|
| ① | 打开 **Place Actors** 面板（Window → Place Actors） |
| ② | 在搜索栏输入 `HexTerrain` |
| ③ | 将 **AHexTerrain** 拖入视口（或双击） |
| ④ | 在 **Details** 面板 → **Hexagon \| Terrain** → 设置 `GridRadius = 10` |
| ⑤ | 设置 `CellRadius = 120` |
| ⑥ | 在 **Hexagon \| Debug** → 勾选 `bDebugChunkColors = true` |

**预期结果**：
- 视口中出现六边形地形（GridRadius=10，CellRadius=120）
- 因 `bAutoRegenerate` 默认开启，参数修改后 terrain 自动重建
- 每个 chunk 显示不同颜色（debug 模式）
- Output Log 输出 terrain 自动重建信息

### 1.2 放置灯光

| 步骤 | 操作 |
|------|------|
| ① | Place Actors → Lights → 将 **Directional Light** 拖入视口 |
| ② | 选中灯光 → Details → 设置 Intensity = 8.0，Light Color = 淡暖色 |
| ③ | Place Actors → Lights → 将 **Sky Light** 拖入视口 |
| ④ | 选中 → Details → 设置 Intensity = 1.5 |

**预期结果**：terrain 被照亮，阴影和高光可见，地形层次感清晰。

### 1.3 从 Details 面板查看 terrain 信息

| 步骤 | 操作 |
|------|------|
| ① | 选中 terrain → 查看 Details 面板各项参数 |

**预期结果**：
- `GridRadius = 10`，`CellRadius = 120`
- 总 cell 数可通过公式验证：`1 + 10 × (10 + 1) × 3 = 331`
- terrain 自动重建完成，视口中可见的 cell 数量与公式计算相符

### 1.4 删除地形

| 步骤 | 操作 |
|------|------|
| ① | 在视口或 **World Outliner** 中选中 terrain actor |
| ② | 按 **Delete** 键 |

**预期结果**：terrain 从视口中消失。World Outliner 中无 AHexTerrain 残留。

---

## 2. LayerMaterials 多材质

**测试目标**：验证按地形类型分配材质、编辑器实时修改材质触发生效。

### 2.1 创建带材质的混合地形

| 步骤 | 操作 |
|------|------|
| ① | Place Actors → 拖入 **AHexTerrain** |
| ② | Details → Hexagon \| Terrain → 设置 `GridRadius = 14`，`CellRadius = 120` |
| ③ | 展开 `TerrainConfig` → 设置 `WaterLevel = 0.15`，`SandLevel = 0.40`，`GrassLevel = 1.0`，`RockLevel = 1.0` |
| ④ | Details → Hexagon \| Material → 展开 `LayerMaterials` |
| ⑤ | 点击 **+** 添加 3 条映射：`Water → M_Water`、`Sand → M_Sand`、`Grass → M_Grass` |

**预期结果**：
- 视口中 terrain 自动重建
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

### 2.3 运行时添加新材质

| 步骤 | 操作 |
|------|------|
| ① | 在 `LayerMaterials` TMap 中点击 **+** 添加条目 |
| ② | Key 选 `Rock`，Value 下拉选择 `M_Rock` |
| ③ | 适当降低 `SandLevel` 或提高 `RockLevel` 以暴露出 Rock 区域 |
| ④ | 等待 terrain 自动重建（约 0.5-1 秒） |

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

### 3.3 通过 TMap 编辑器设置单个 cell

| 步骤 | 操作 |
|------|------|
| ① | Details → **Hexagon \| Manual** → 展开 `ManualCellTypes` |
| ② | 点击 **+** 添加条目 → Key 设为 `(5, 3)`，Value 选 `Grass` |
| ③ | 再次点击 **+** → Key 设为 `(5, 4)`，Value 选 `Water` |

**预期结果**：
- 坐标 (5,3) 的 cell 变绿色
- 坐标 (5,4) 的 cell 变蓝色
- 其余 cell 保持 Sand（黄色，DefaultManualType）

### 3.4 通过 TMap 编辑器批量设置

| 步骤 | 操作 |
|------|------|
| ① | 在 `ManualCellTypes` 中继续添加条目，覆盖中心区域多个 cell |
| ② | 例如：(0,0)→Rock、(1,0)→Rock、(0,1)→Rock、(1,1)→Rock、(2,0)→Rock 等 |
| ③ | 观察 terrain 自动重建 |

**预期结果**：所添加的 cell 全部变为对应类型。未被覆盖的 cell 仍使用 `DefaultManualType`。

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

**预期结果**：TMap 编辑器列出之前添加的所有条目，格式：
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
| ① | 选中 terrain → 按 **Delete** 删除旧的 |
| ② | Place Actors → 拖入 **AHexTerrain** → 设置 `GridRadius = 12`，`CellRadius = 120` |
| ③ | `TerrainConfig` → `GrassLevel = 1.0`，`WaterLevel = 0`，`SandLevel = 0`，`RockLevel = 1.0`（令全部 cell 为 Grass） |
| ④ | 在 `LayerMaterials` 中添加 `Grass → M_Grass` |
| ⑤ | 菜单栏 → **Modes** 下拉（通常在工具栏左上角区域） |
| ⑥ | 选择 **Hex Terrain Paint** |

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

**预期结果**：terrain 自动重建（无报错）。如果使用了带纹理的材质，纹理图案更加密集。

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
| ① | 确保 bManualMode 开启 → 展开 `ManualCellTypes` |
| ② | 点击 **+** → Key 设为 `(999, 999)`，Value 选 `Snow` |

**预期结果**：
- **不崩溃、不卡顿**
- 坐标超出 terrain 范围，条目被加入 ManualCellTypes 但无匹配 cell（无可见变化）

### 6.2 ClampMin 值约束

| 步骤 | 操作 |
|------|------|
| ① | 选中 terrain → Details → Hexagon \| Terrain |
| ② | 尝试输入 `GridRadius = -1` |

**预期结果**：
- **不崩溃**
- ClampMin 限位生效，值被钳制为 0（或滑动条无法拖至负数）

### 6.3 删除 terrain 后的状态

| 步骤 | 操作 |
|------|------|
| ① | 选中 terrain → 按 **Delete** 键删除 |
| ② | 确认 World Outliner 中无 AHexTerrain |
| ③ | 查看 Details 面板和视口 |

**预期结果**：
- **不崩溃**
- Details 面板清空/显示 "Nothing Selected"
- 视口恢复正常，无残留渲染

### 6.4 无 terrain 时进入 Paint 模式

| 步骤 | 操作 |
|------|------|
| ① | 确认场景中无 AHexTerrain |
| ② | Modes 下拉 → 选择 **Hex Terrain Paint** |

**预期结果**：
- 模式正常进入
- 视口中无笔刷显示（因为没有 terrain）
- **不崩溃**

### 6.5 跨 Chunk 涂刷

| 步骤 | 操作 |
|------|------|
| ① | Place Actors → 拖入 AHexTerrain → GridRadius=12, CellRadius=120, 全部设为 Grass（同 §4.1） |
| ② | 进入 Hex Terrain Paint 模式 |
| ③ | BrushRadius=3，BrushTerrainType=Sand |
| ④ | 在 terrain 上**大范围快速拖拽**，跨越 chunk 边界 |

**预期结果**：
- 两侧 chunk 都正确更新
- Chunk 边界处无接缝、无漏刷

---

## 7. 性能

**测试目标**：验证大 terrain 增量重建的流畅度。

### 7.1 大 Terrain 涂刷

| 步骤 | 操作 |
|------|------|
| ① | 删除旧 terrain |
| ② | Place Actors → 拖入 **AHexTerrain** |
| ③ | Details → `GridRadius = 20`，`CellRadius = 100`（~1200 cell） |
| ④ | `GrassLevel = 1.0`，`WaterLevel = 0`，`SandLevel = 0`，`RockLevel = 1.0`（全部 Grass） |
| ⑤ | 进入 **Hex Terrain Paint** 模式 |
| ⑥ | BrushRadius=3，BrushTerrainType=Sand |
| ⑦ | **快速拖拽涂刷** 5-10 秒，覆盖大片区域 |

**预期结果**：
- 涂刷过程无明显卡顿或帧率骤降
- 仅受影响的 chunk 被重建（非全 terrain 重建）
- Output Log 无 error/exception

---

## 8. 自由地形编辑

**测试目标**：验证框选添加/删除 cell、toggle 切换、Ctrl 仅删除。

### 8.1 准备 & 进入模式

| 步骤 | 操作 |
|------|------|
| ① | Place Actors → 拖入 **AHexTerrain** → 设置 `GridRadius = 5`，`CellRadius = 120` |
| ② | Modes 下拉 → 选择 **Hex Terrain Edit** |

**预期结果**：
- 模式切换成功，Output Log 显示进入信息
- terrain 自动被选中

### 8.2 悬停单 cell 预览

| 步骤 | 操作 |
|------|------|
| ① | 鼠标移到 terrain 内部已有 cell 上方 |
| ② | 鼠标移到 terrain 外部空白区域 |

**预期结果**：
- 已有 cell 上方：**红色六边形轮廓**（表示 Ctrl+拖拽可删除）
- 空白区域：**绿色六边形轮廓**（表示点击/拖拽可添加）

### 8.3 单击添加 cell

| 步骤 | 操作 |
|------|------|
| ① | 鼠标移到 terrain 外空白区域 |
| ② | **左键点击** |

**预期结果**：绿色位置新增一个 cell，Output Log 显示 `HexTerrainEdit: X added, 0 removed`。

### 8.4 拖拽框选批量添加

| 步骤 | 操作 |
|------|------|
| ① | 在 terrain 外空白区域 **按住左键拖拽**，形成矩形框 |
| ② | 松开鼠标 |

**预期结果**：
- 拖拽过程中：框内 cell 全部显示绿色轮廓
- 松开后：框内所有空 cell 一次性添加，Output Log 显示 added 数量

### 8.5 Ctrl+拖拽仅删除

| 步骤 | 操作 |
|------|------|
| ① | 按住 **Ctrl** |
| ② | 在已有 cell 区域 **按住左键拖拽** |
| ③ | 松开鼠标 |

**预期结果**：
- 拖拽过程中：已有 cell 显示红色轮廓，空白 cell **不显示**（仅删除模式）
- 松开后：框内已有 cell 被删除，空白 cell 不受影响
- Output Log 显示 removed 数量

### 8.6 Toggle 切换

| 步骤 | 操作 |
|------|------|
| ① | 在混合区域（部分 cell 已存在、部分空白）**不按 Ctrl 拖拽框选** |
| ② | 松开鼠标 |

**预期结果**：空白 cell 被添加，已有 cell 被删除。Output Log 同时显示 added 和 removed 数量。

### 8.7 右键不受影响

| 步骤 | 操作 |
|------|------|
| ① | **右键按住拖拽** → 旋转视角 |
| ② | **滚轮** → 缩放 |
| ③ | 按 **Esc** → 退出模式 |

**预期结果**：右键/滚轮正常，不触发增删。Esc 退出后恢复默认编辑模式。

### 8.8 跨 Chunk 编辑

| 步骤 | 操作 |
|------|------|
| ① | 大 GridRadius terrain → 进入 Edit 模式 |
| ② | 在 chunk 边界附近拖拽框选 |

**预期结果**：两侧 chunk 正确重建，无接缝。

### 8.9 编辑后切换 Paint 模式

| 步骤 | 操作 |
|------|------|
| ① | Edit 模式添加/删除一些 cell |
| ② | 切换到 Hex Terrain Paint 模式 |
| ③ | 对新增 cell 涂刷 |

**预期结果**：新增/删除的 cell 在 Paint 模式下正常响应涂刷。

---

## 9. LOD 调试 & 性能日志

**测试目标**：验证 bDebugLODColors 和 bLogPerformance。

### 9.1 LOD 着色

| 步骤 | 操作 |
|------|------|
| ① | 选中 terrain → Details → Hexagon\|Debug → 勾选 `bDebugLODColors` |

**预期结果**：chunk 按 LOD 着色（LOD0=绿，LOD1=黄，LOD2=红）。

### 9.2 性能日志

| 步骤 | 操作 |
|------|------|
| ① | 勾选 `bLogPerformance` |
| ② | 修改 GridRadius 触发重建 |

**预期结果**：Output Log 输出超过阈值（16ms）的 chunk 重建耗时。

---

## 10. Editor 菜单

**测试目标**：验证 Window → Hexagon 子菜单。

| # | 操作 | 预期结果 |
|---|------|---------|
| 10.1 | Window → Hexagon | 子菜单出现，含 5 个命令 |
| 10.2 | Create Test Scene | 生成 terrain + 灯光 |
| 10.3 | Print Stats | Output Log 输出统计信息 |

---

## 结果汇总

| 分类 | 项数 | 通过 | 失败 | 备注 |
|------|:---:|:---:|:---:|------|
| 0. 前置准备 | 3 | | | |
| 1. 基础地形 | 4 | | | |
| 2. LayerMaterials | 5 | | | |
| 3. 手动模式 | 6 | | | |
| 4. 笔刷绘制 | 9 | | | |
| 5. 纹理 UV | 5 | | | |
| 6. 边界鲁棒性 | 5 | | | |
| 7. 性能 | 1 | | | |
| 8. 自由地形编辑 | 9 | | | |
| 9. LOD/性能调试 | 2 | | | |
| 10. Editor 菜单 | 3 | | | |
| **总计** | **52** | | | |

---

> **备注栏**：失败项记录实际表现（如：崩溃、无反应、颜色不对、报错信息等）。
