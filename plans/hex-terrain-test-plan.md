# Hex Terrain 测试计划

> 最后更新: 2026-06-08
> 测试范围: 手动材质模式 + 笔刷绘制系统 + LayerMaterials 修复

---

## 1. 手动材质模式 (Manual Mode)

### 1.1 Details 面板属性

| # | 测试项 | 操作 | 预期结果 |
|---|--------|------|---------|
| 1.1 | bManualMode 切换 | 选中 Terrain → 勾选 `bManualMode` | 地形重建，所有 cell 变为 `DefaultManualType` |
| 1.2 | DefaultManualType | 设置 `DefaultManualType = Sand` | 地形重建，全部 cell 用 Sand 颜色 |
| 1.3 | ManualCellTypes TMap | 添加条目 `{(3,0): Grass}` | 地形重建，(3,0) 变为 Grass |
| 1.4 | ManualCellTypes 可视化 | 在 TMap 编辑器中查看 | 键值对正常显示 hex 坐标 → 类型映射 |
| 1.5 | 关闭 bManualMode | 取消勾选 `bManualMode` | 恢复 noise 自动分类 |
| 1.6 | 重新开启 | 再次勾选 `bManualMode` | 保留之前的 ManualCellTypes 覆盖项，恢复手动模式 |

### 1.2 控制台命令

| # | 测试项 | 命令 | 预期结果 |
|---|--------|------|---------|
| 2.1 | 单个 cell 设置 | `hex.SetCell 3 0 Grass` | (3,0) 变 Grass，自动启用手动模式 |
| 2.2 | 单个 cell 覆盖 | `hex.SetCell 3 0 Sand` | (3,0) 变 Sand，覆盖之前的值 |
| 2.3 | FillRing 小半径 | `hex.FillRing 0 0 1 Water` | 中心+第一环共 7 个 cell 变 Water |
| 2.4 | FillRing 大半径 | `hex.FillRing 0 0 5 Grass` | 91 个 cell 变 Grass |
| 2.5 | FillRing 偏离中心 | `hex.FillRing 8 3 2 Sand` | (8,3) 周围 2 环变 Sand |
| 2.6 | 类型名大小写 | `hex.SetCell 1 1 grass` | 大小写不敏感，成功解析 |
| 2.7 | 无效类型名 | `hex.SetCell 1 1 Lava` | 警告提示 + 无更改 |
| 2.8 | 无 Terrain | 先 `hex.DestroyAll` 再 `hex.SetCell` | 提示无 Terrain，不崩溃 |

### 1.3 边界情况

| # | 测试项 | 操作 | 预期结果 |
|---|--------|------|---------|
| 3.1 | 超出范围坐标 | `hex.SetCell 999 999 Grass` | 添加到 ManualCellTypes 但无匹配 cell，不崩溃 |
| 3.2 | FillRing 半径 0 | `hex.FillRing 2 2 0 Sand` | 仅 (2,2) 一个 cell 改变 |
| 3.3 | Negative 坐标 | `hex.SetCell -5 -3 Water` | 正常设置负坐标 cell |

---

## 2. 笔刷绘制系统 (Brush Painting)

### 2.1 编辑模式注册

| # | 测试项 | 操作 | 预期结果 |
|---|--------|------|---------|
| 4.1 | 模式出现 | UE 编辑器 → Modes 下拉菜单 | 显示 "Hex Terrain Paint" 选项 |
| 4.2 | 进入模式 | 选中 Terrain → 切换到 Hex Terrain Paint | 日志输出模式进入信息 |
| 4.3 | 自动选中 Terrain | 进入模式 | 首个 AHexTerrain 自动被选中，Details 面板显示 Brush 属性 |
| 4.4 | 退出模式 | 按 Esc 或切换回默认模式 | 恢复正常选择编辑 |

### 2.2 笔刷预览

| # | 测试项 | 操作 | 预期结果 |
|---|--------|------|---------|
| 5.1 | 悬停显示 | 鼠标移到 terrain 上方 | 蓝色同心圆笔刷光标 + hex cell 轮廓线 |
| 5.2 | 离开 terrain | 鼠标移出 terrain | 笔刷预览消失 |
| 5.3 | BrushRadius 变化 | 修改 `BrushRadius = 3` | 笔刷圆圈变大，覆盖更多 cell |
| 5.4 | 大笔刷 | `BrushRadius = 6` | 圆圈变大，cell 轮廓线不显示 (>5) |
| 5.5 | 笔刷颜色 | 左键按住拖拽 | 圆圈从蓝色变橙色 |

### 2.3 涂刷功能

| # | 测试项 | 操作 | 预期结果 |
|---|--------|------|---------|
| 6.1 | 单次点击 | 左键点击 terrain 上某个位置 | 笔刷范围内 cell 变为 BrushTerrainType |
| 6.2 | 拖拽涂刷 | 左键按住拖拽 | 路径上 cell 连续变为 BrushTerrainType |
| 6.3 | 切换类型 | 设置 `BrushTerrainType = Water` → 涂刷 | 新涂刷的 cell 变 Water |
| 6.4 | 擦除 Ctrl | 按住 Ctrl + 左键点击 | cell 恢复 DefaultManualType |
| 6.5 | 右键不触发 | 右键拖拽 | 正常旋转/移动视角（不涂刷） |
| 6.6 | 滚轮缩放 | 滚轮缩放 | 正常缩放（不涂刷） |

### 2.4 增量重建性能

| # | 测试项 | 操作 | 预期结果 |
|---|--------|------|---------|
| 7.1 | 拖拽流畅度 | 在 GridRadius=15 terrain 上拖拽涂刷 | 无卡顿，增量重建只刷新受影响 chunk |
| 7.2 | 小笔刷效率 | BrushRadius=0，快速拖拽 | 每帧仅重建 1 个 cell 所在的 chunk |
| 7.3 | Chunk 边界 | 跨 chunk 边界拖拽 | 两个 chunk 都正确更新，无明显接缝 |

### 2.5 边界情况

| # | 测试项 | 操作 | 预期结果 |
|---|--------|------|---------|
| 8.1 | 无 Terrain 时进入 | `hex.DestroyAll` → 进入 Paint 模式 | 不崩溃，无笔刷显示 |
| 8.2 | 笔刷超出 terrain | 鼠标移到 terrain 边缘外 | 无命中，笔刷消失 |
| 8.3 | 涂刷到 cell 边界 | 拖拽经过各 cell | 所有经过 cell 都被涂刷（不遗漏） |

---

## 3. LayerMaterials 修复验证

| # | 测试项 | 操作 | 预期结果 |
|---|--------|------|---------|
| 9.1 | 空 LayerMaterials 生成 | 创建 terrain 时不设置 LayerMaterials | 单 section 模式，默认材质 |
| 9.2 | 后续添加材质 | 在 Details 面板添加 `{Grass: M_Grass}` | 地形重建，草 cell 用 M_Grass 材质 |
| 9.3 | 运行时添加 | 控制台创建一个 terrain，再设置材质 | 自动重建，mesh section 拆分 |
| 9.4 | 移除材质 | 从 LayerMaterials 删除条目 | 地形重建，该类型回退到 TerrainMaterial |
| 9.5 | TerrainMaterial 变化 | 修改 TerrainMaterial | 所有未分配 LayerMaterial 的 cell 更新材质 |

---

## 4. 综合场景测试

| # | 场景 | 步骤 | 预期结果 |
|---|------|------|---------|
| 10.1 | 草地+沙地+水域 | `hex.CreateGrassSandTerrain 14` → 进入 Paint 模式 → 用 Sand 笔刷刷一些区域 → 用 Water 笔刷刷另一些 | 三种材质同时显示，边界清晰 |
| 10.2 | 手动全控 | `hex.CreateGrassTerrain 12` → 勾选 bManualMode → 用 FillRing 画圆形湖泊 → 用笔刷精细调整边缘 | 混合使用命令和笔刷，材质正确 |
| 10.3 | 自动↔手动切换 | 自动 terrain → 切换到手动 → 修改 → 切回自动 → 再切回手动 | 手动覆盖数据保留，不丢失 |
| 10.4 | 大 terrain 性能 | GridRadius=20 → 笔刷半径=3 → 快速拖拽涂刷 | 增量重建每帧 <50ms |

---

## 5. World-Space UV 纹理

### 5.1 基本功能

| # | 测试项 | 操作 | 预期结果 |
|---|--------|------|---------|
| 11.1 | 默认 TileSize | 创建 terrain，检查 `TextureTileSize = 200` | 默认 200，UV 使用 world-space planar 投影 |
| 11.2 | TileSize 调整 | 修改 `TextureTileSize = 100` | 纹理平铺更密集（每 100cm 重复一次） |
| 11.3 | TileSize = 0 | 设置 `TextureTileSize = 0` | 回退到默认 per-cell UV（每个 cell 独立 UV） |
| 11.4 | 材质纹理 | 设置 LayerMaterials 使用带纹理的材质 | 纹理跨 cell 连续平铺，无接缝 |
| 11.5 | 顶面投影 | 从上方观察 terrain 顶面 | 纹理 X/Y 平面投影，连续无断裂 |

### 5.2 跨 cell 连续性

| # | 测试项 | 操作 | 预期结果 |
|---|--------|------|---------|
| 12.1 | 相邻 cell 纹理 | 创建两个相邻 cell，设置不同类型但相同 TileSize | 纹理坐标在边界处连续对齐 |
| 12.2 | 不同高度 cell | 低洼水 cell 旁边高处草 cell | 纹理在高差处拉伸可接受 |
| 12.3 | Chunk 边界 | 观察 chunk 边界处纹理 | 跨 chunk 纹理连续（UV 基于世界坐标） |

### 5.3 Editor 交互

| # | 测试项 | 操作 | 预期结果 |
|---|--------|------|---------|
| 13.1 | Details 面板 | 选中 Terrain → Hexagon\|Texture | `TextureTileSize` 可编辑，带 ClampMin=0 |
| 13.2 | 实时更新 | 在编辑器中拖动 TileSize 滑块 | 地形自动重建，UV 即时更新 |
| 13.3 | Brush 不受影响 | 修改 TileSize 后用笔刷涂刷 | 笔刷正常工作，Material 正确绑定 |

---

## 6. 编译验证

| # | 测试项 | 命令/操作 | 预期 |
|---|--------|----------|------|
| 11.1 | UE 编译 | 在 VS/Rider 中 Build Hexagon 模块 | 0 errors, 0 warnings |
| 11.2 | 头文件包含 | - | 无缺失 include，`FSlateIcon`/`FEdMode`/`FPrimitiveDrawInterface` 均可用 |
| 11.3 | 模块加载 | 启动 UE 编辑器 → 打开项目 | Hexagon 模块正常加载，命令注册成功 |

---

## 测试环境

- **项目**: TA-Playground
- **UE 版本**: 5.7
- **插件路径**: `Plugins/Hexagon/`
- **测试地图**: HexagonPlayground（默认启动地图）
