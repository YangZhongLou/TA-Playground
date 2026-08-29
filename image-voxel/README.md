# 图生体素

把一张图变成三维占用场，再决定 UE 里怎么画、怎么玩。

体素是一种表示，不是一种画风。本目录用 CPU 挤出、高度图和 Grok 程序化占用，**不调用 Hunyuan**。

「图生」在这里指**图像条件生成**，不是 Voxel Plugin 的节点图。节点图是消费层工具，见 [unreal.md](unreal.md)。

## 怎么读

按层读。不要在表示文里找 UE 类名，也不要在引擎文里找模型论文。

1. [overview.md](overview.md) — 问题空间与三层模型（表示 / 生成 / 消费）。
2. [representations.md](representations.md) — 格子里存什么。
3. [pipelines.md](pipelines.md) — 图怎么变成格子：挤出、雕刻、程序化、网格体素化。
4. [models.md](models.md) — Grok 怎么接；本目录不用 Hunyuan。
5. [unreal.md](unreal.md) — UE 5.8 四条容易混名的路径。
6. [comparison.md](comparison.md) — 相对网格、Hexagon、高斯的取舍。
7. [next.md](next.md) — 本仓库下一步实验。
8. [references.md](references.md) — 论文、引擎文档、工具。

先跑 CPU 原型：

```powershell
python image-voxel/scripts/image_to_voxels.py --demo sprite
python image-voxel/scripts/image_to_voxels.py --demo height
python image-voxel/scripts/grok_direct.py potion
```

产物在 `image-voxel/out/`（已 gitignore）。`.vox` 用 MagicaVoxel 打开；`.bmp` 是正交切片预览。

## 目录

| 文件 | 内容 |
| --- | --- |
| [overview.md](overview.md) | 要解决什么；三层独立选择 |
| [representations.md](representations.md) | 密集体素、稀疏八叉树、占用、SDF、SLAT、O-Voxel |
| [pipelines.md](pipelines.md) | 经典挤出到 TRELLIS 潜空间（未接入） |
| [models.md](models.md) | Grok 程序化；不走 Hunyuan |
| [unreal.md](unreal.md) | 方块网格、SVT、Nanite 体素、Voxel Plugin |
| [comparison.md](comparison.md) | 何时用体素、何时用网格或六边形 |
| [next.md](next.md) | 可验证实验清单 |
| [references.md](references.md) | 文献 |
| [scripts/image_to_voxels.py](scripts/image_to_voxels.py) | 无 GPU 的最小图生体素 |
| [scripts/grok_direct.py](scripts/grok_direct.py) | Grok 程序化预制件（药瓶 / 木箱） |
| [scripts/mesh_to_voxels.py](scripts/mesh_to_voxels.py) | 自备 DCC 网格 → `.vox`（可选） |

## 当前结论

| 主线 | 传什么 | 典型用途 | 代表 |
| --- | --- | --- | --- |
| 像素挤出 / 高度图 | 2D 颜色 + 深度或高度 | 体素画、图标、方块地块 | 本目录 `image_to_voxels.py` |
| 程序化 CSG | 球 / 柱 / 盒规则 | 药瓶、木箱 | `grok_direct.py` |
| 网格体素化 | 已有三角网格 | DCC 模型占用 | `mesh_to_voxels.py` |
| 神经稀疏体素 | 图像 → 稀疏格子 + 特征 | 未接入 | TRELLIS |
| 体积场 | 密度 / 温度 / 速度 | 烟火流体，不是实心方块 | OpenVDB → UE SVT |

默认：像素画挤出，预制件走 Grok CSG。不要把 Nanite 的远处体素表示当成可编辑的体素世界。详见 [models.md](models.md)。

## 术语

| 术语 | 含义 |
| --- | --- |
| 体素 | 规则三维格子上的一个单元格 |
| 占用 | 该格实心或空心 |
| SDF | 到表面的有符号距离，零等值面即表面 |
| 稀疏体素 | 只存储非空（或靠近表面）的格子 |
| SLAT | TRELLIS 的结构化潜变量，贴在表面体素上 |
| O-Voxel | TRELLIS.2 的无场稀疏体素，同时编码几何和外观 |
| SVT | UE Sparse Volume Texture，稀疏体积纹理 |
| `.vox` | MagicaVoxel 文件，调色板实心方块 |

## 研究边界

覆盖**物体级**图像到体素，以及在本仓库 UE 5.8 里的消费方式。

不覆盖医学 CT 重建、大规模体素 MMO 同步、TRELLIS 训练。
本目录不调用 Hunyuan。Voxel Plugin 2 只作对照，第一阶段不引入该插件。
