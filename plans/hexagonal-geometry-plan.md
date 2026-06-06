# 六边体/六边形几何生成系统 — 开发计划

> 目标：在 UE 5.7 中构建一套可参数化的六边形几何生成系统，覆盖单个体、网格阵列、以及程序化地形。支持 C++ Runtime 生成和 UnrealMCP AI 驱动两种调用方式。

---

## 一、范围定义

| 层级 | 产物 | 说明 |
|------|------|------|
| L1 — 单六角柱 | `AHexPrism` Actor | 基础几何体，可调半径/高度/分段 |
| L2 — 六边形网格 | `AHexGrid` Actor | 六边形平铺阵列，支持轴向坐标系 |
| L3 — 参数化地形 | `AHexTerrain` Actor | 在网格基础上叠加高度图/噪声/材质 |
| L4 — MCP 集成 | Python/技能命令 | 自然语言一键生成与调整 |

---

## 二、六边形数学基础

### 2.1 轴向坐标系（Axial Coordinates）

采用 **pointy-topped** 朝向，轴向坐标 `(q, r)`，推导第三个隐式轴 `s = -q - r`。

```
              ___
            /     \
      ___ / (0,0) \ ___
    /     \  ___  /     \
   /(-1,0) \ /   \ (1,-1)\    ← q 轴 (右下)
   \  ___  / \___/  ___  /
    /     \ /     \ /     \
   /(-1,1) \ (0,1) \ (1,0)\
    \_____/  \_____/  \____/
            ↑ r 轴 (右)
```

### 2.2 顶点公式（Pointy-Topped，中心原点）

```
angle = 60° * i                      // i = 0..5
x = center.x + radius * cos(angle)
y = center.y + radius * sin(angle)
```

顶点偏移角：0°, 60°, 120°, 180°, 240°, 300°

### 2.3 邻居索引（六方向）

```cpp
static const FIntPoint HexDirections[6] = {
    {+1,  0}, {+1, -1}, { 0, -1},
    {-1,  0}, {-1, +1}, { 0, +1}
};
```

### 2.4 核心算法清单

| 算法 | 输入 | 输出 | 复杂度 |
|------|------|------|--------|
| `HexToWorld(q, r)` | 轴向坐标 | FVector 世界位置 | O(1) |
| `WorldToHex(pos)` | FVector | FIntPoint 轴向坐标 | O(1) |
| `HexDistance(a, b)` | 两个轴向坐标 | 整数步数 | O(1) |
| `HexRing(center, radius)` | 中心+半径 | 该环上所有坐标 | O(r) |
| `HexSpiral(center, maxR)` | 中心+最大半径 | 范围内所有坐标 | O(r²) |
| `HexLine(a, b)` | 起点+终点 | 直线路径坐标 | O(d) |

---

## 三、层级详细设计

### L1 — 单六角柱（Hexagonal Prism）

#### 3.1.1 功能
- 程序化生成六角柱的 `UProceduralMeshComponent` 或 `UDynamicMeshComponent`（UE5.7 推荐后者）
- 参数：外接圆半径、高度、顶/底面是否封闭、侧面分段数、材质

#### 3.1.2 拓扑结构

```
底面：1个中心点 + 6个周边顶点 = 7点，6个三角形
顶面：同上，z偏移 + height
侧面：6个矩形面，每面可按分段拆分为 2*segments 个三角形
```

#### 3.1.3 UV 映射策略
- 顶/底面：平面投影 UV，中心 (0.5, 0.5)，周边顶点环绕
- 侧面：圆柱展开 UV，按弧长均匀分布

#### 3.1.4 文件结构
```
Source/TAPlayground/
├── Hexagon/
│   ├── HexGeometry.h/.cpp      # 纯数学工具（无UE依赖）
│   └── HexPrismGenerator.h/.cpp # 网格生成器
└── Hexagon/
    └── AHexPrism.h/.cpp         # Actor 封装
```

---

### L2 — 六边形网格（Hex Grid）

#### 3.2.1 功能
- 生成 NxM 或半径 R 的六边形平铺阵列
- 每个单元格独立 Actor 或合并为单个 Mesh（InstancedStaticMesh）
- 支持轴向坐标查询、邻居遍历、距离计算

#### 3.2.2 三种渲染策略对比

| 策略 | 优点 | 缺点 | 适用场景 |
|------|------|------|----------|
| 独立 Actor/每格 | 每格独立材质/动画 | DrawCall爆炸 | < 50格 |
| InstancedStaticMesh | 单DrawCall | 材质统一 | < 5000格 |
| 合并为单个DynamicMesh | 可动态修改拓扑 | 重建开销 | 地形/需要挖洞 |

#### 3.2.3 推荐方案：分层混合
- 小范围可视化网格 → `UHierarchicalInstancedStaticMeshComponent`
- 大范围背景地形 → 合并为单个 `UDynamicMesh` + 材质实例参数化

#### 3.2.4 数据结构

```cpp
USTRUCT()
struct FHexCellData {
    GENERATED_BODY()
    FIntPoint Axial;           // (q, r)
    float Height;              // 海拔高度（用于L3地形）
    int32 TerrainType;         // 地形类型索引（草原/山地/水域）
    FLinearColor VertexColor;  // 可传递给材质
};
```

---

### L3 — 参数化六边形地形（Hex Terrain）

#### 3.3.1 功能
- 在 L2 网格基础上叠加高度和噪声
- 支持 Simplex/Perlin 噪声驱动海拔变化
- 水/陆地/山地自动分类（按高度阈值）
- 边缘过渡柔和（六边形之间高度差平滑插值）

#### 3.3.2 高度插值策略

不采用纯六边形"台阶"地形，而是在六边形单元之间做 **顶点级插值**：

```
每个六边形角点（corner）的高度 = 周围3个单元格高度的加权平均
这样相邻六边形之间形成连续曲面，而非突兀断层
```

#### 3.3.3 材质方案
- 母材质：基于 Vertex Color 或 Height 的多层地形混合
- 图层：水域（低）、沙地、草地、岩石、雪地（高）
- 使用 `LandscapeLayerBlend` 类似思路但适配 DynamicMesh

---

### L4 — UnrealMCP 集成

#### 3.4.1 命令设计

| 自然语言指令 | MCP 动作 | 参数 |
|-------------|----------|------|
| "生成一个半径50的六角柱" | 调用 `AHexPrism::Spawn` | Radius=50, Height=100 |
| "创建 5x5 的六边形网格" | 调用 `AHexGrid::Generate` | Radius=5, CellSize=100 |
| "给地形添加Perlin噪声高度" | 调用 `AHexTerrain::ApplyNoise` | Octaves=4, Scale=0.01 |
| "把坐标(2,3)的格子变成水域" | 坐标查询+修改 CellData | Coord=(2,3), Type=Water |

#### 3.4.2 接口层

在 `AHexPrism` / `AHexGrid` / `AHexTerrain` 上暴露 `UFUNCTION(BlueprintCallable)` 接口，MCP 通过蓝图函数调用或反射调用。

---

## 四、开发阶段

### Phase 1 — 基础几何（预计 2-3 天）

**目标**：可编译运行，生成单个六角柱

| 任务 | 文件 | 验收标准 |
|------|------|----------|
| 创建 HexGeometry 数学库 | `Source/.../HexGeometry.h/.cpp` | 所有核心算法通过单元测试 |
| 实现 HexPrismGenerator | `Source/.../HexPrismGenerator.h/.cpp` | 生成合法 UDynamicMesh，法线/UV 正确 |
| 实现 AHexPrism Actor | `Source/.../AHexPrism.h/.cpp` | 拖拽到场景即可看到六角柱，参数面板可调半径/高度 |
| 材质占位 | `Content/Materials/Hex/MI_HexPrism_White` | 基础白色材质，双面渲染 |

**关键决策**：
- 使用 `UDynamicMeshComponent`（Geometry Script Plugin）还是 `UProceduralMeshComponent`？
  - **推荐 UDynamicMesh**：UE5.4+ 原生，支持 Mesh Boolean、Remesh 等后续操作
  - 需要在 TA-Playground.uproject 启用 `GeometryScript` 插件

### Phase 2 — 网格系统（预计 3-4 天）

**目标**：可生成任意大小的六边形网格，支持坐标查询

| 任务 | 说明 |
|------|------|
| AHexGrid Actor | 集成 InstancedStaticMesh，支持半径/行列两种模式 |
| HexCellData 结构 | 每格存储坐标、高度、类型、自定义数据 |
| 邻居查询 API | `GetNeighbors(FIntPoint)` → `TArray<FIntPoint>` |
| 选框/射线检测 | 鼠标悬停高亮当前格子（MCP 验证用） |
| 网格材质变体 | 基于 Vertex Color 或 Custom Primitive Data 的多色材质 |

### Phase 3 — 地形与噪声（预计 3-4 天）

**目标**：参数化地形，高度+类型自动分类

| 任务 | 说明 |
|------|------|
| 高度噪声集成 | 集成 FastNoiseLite（单头文件库）或使用 UE `FMath::PerlinNoise` |
| 角点高度插值 | 共享角点取周围格高度平均 |
| DynamicMesh 重建 | 带高度的六边形合并为连续曲面 |
| 地形材质 | 多层混合母材质，基于高度+坡度自动选择图层 |
| 水面膜片 | 水面单独生成，低于水位线的格子做凹陷处理 |

### Phase 4 — MCP 集成与验证（预计 2 天）

**目标**：通过自然语言控制六边形生成

| 任务 | 说明 |
|------|------|
| MCP 命令映射 | 在 UnrealMCP 侧注册 Hex 相关命令 |
| Python 验证脚本 | `scripts/hex_demo.py` — 一键生成场景 |
| 示例关卡 | `Content/Maps/LV_HexShowcase` — 展示所有层级效果 |
| 文档更新 | README 添加 Hex 系统使用说明 |

---

## 五、文件目录规划

```
TA-Playground/
├── Source/TAPlayground/
│   ├── Hexagon/
│   │   ├── HexGeometry.h/.cpp           # 数学：坐标转换、距离、邻居
│   │   ├── HexPrismGenerator.h/.cpp     # 六角柱网格生成
│   │   ├── HexGridGenerator.h/.cpp      # 网格阵列生成
│   │   ├── HexTerrainGenerator.h/.cpp   # 地形+噪声生成
│   │   └── HexTypes.h                   # 共享结构体/枚举
│   └── Hexagon/
│       ├── AHexPrism.h/.cpp             # L1 Actor
│       ├── AHexGrid.h/.cpp              # L2 Actor
│       └── AHexTerrain.h/.cpp           # L3 Actor
├── Content/
│   ├── Materials/Hex/
│   │   ├── M_HexPrism.uasset            # 六角柱基础材质
│   │   ├── M_HexGrid_MultiColor.uasset  # 多色网格材质
│   │   └── M_HexTerrain_Master.uasset   # 地形母材质
│   ├── Blueprints/Hex/
│   │   └── BP_HexTerrainBuilder.uasset  # 可选：纯蓝图封装
│   └── Maps/
│       └── LV_HexShowcase.umap          # 展示关卡
├── scripts/
│   └── hex_demo.py                      # MCP 验证脚本
└── plans/
    └── hexagonal-geometry-plan.md       # 本文件
```

---

## 六、关键技术决策

| 决策点 | 选项 A | 选项 B | 建议 |
|--------|--------|--------|------|
| 网格组件 | UDynamicMeshComponent | UProceduralMeshComponent | **UDynamicMesh** — 功能更强 |
| 网格合并 | 单 DynamicMesh | InstancedStaticMesh | **分层混合** — 小用ISM，大用DM |
| 噪声库 | FMath::PerlinNoise | FastNoiseLite | **FastNoiseLite** — 更多噪声类型、性能更好 |
| 坐标系统 | Axial (q, r) | Offset (col, row) | **Axial** — 算法更简洁，邻居计算 O(1) |
| 六角朝向 | Pointy-topped | Flat-topped | **Pointy-topped** — 垂直排列更自然 |

---

## 七、风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| Geometry Script 插件未启用 | Phase 1 阻塞 | 提前检查 `.uproject` 插件列表，必要时添加 |
| 大规模网格性能差 | Phase 2/3 帧率低 | 使用 LOD + 视锥裁剪 + 分块加载 |
| MCP 命令接口不稳定 | Phase 4 延期 | 先确保纯 C++ / 蓝图可运行，MCP 作为薄层 |
| DynamicMesh UV/法线异常 | 视觉瑕疵 | 每阶段完成后用 Editor 线框模式检查拓扑 |

---

## 八、验收 Checklist

- [ ] 编译通过，无警告
- [ ] `AHexPrism` 拖拽生成正确几何体，参数实时可调
- [ ] `AHexGrid` 生成 10x10 网格，射线检测能正确返回轴向坐标
- [ ] `AHexTerrain` 生成带噪声的地形，水面/陆地/山地视觉区分明显
- [ ] `scripts/hex_demo.py` 零对话框完成场景搭建
- [ ] 展示关卡 `LV_HexShowcase` 可在 Editor 中直接打开预览

---

*计划创建日期：2026-06-06*
*对应 UE 版本：5.7*
