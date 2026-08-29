# UE 5.8 消费路径

引擎里至少有四件都叫「voxel」的东西。数据格式、材质域、碰撞、可编辑性全部不同。

## 四条路径

| 路径 | 引擎对象 | 格子语义 | 可挖填 | 本仓库第一阶段 |
| --- | --- | --- | --- | --- |
| 方块网格 | Static Mesh、ISM、HISM | 实心立方体，调色板或顶点色 | 要自己做 | 主路径：`.vox` 导入 |
| 稀疏体积纹理 | `USparseVolumeTexture`、Heterogeneous Volume | 密度等介质属性 | 否（播放缓存） | 仅 VFX，与实心图生分开 |
| Nanite 体素 | 静态网格构建设置 | 远处簇的体素近似 | 否 | 不要当内容管线 |
| Voxel Plugin 2 | Voxel World + Stamp + Voxel Graph | SDF / 高度场地形 | 是，地形级 | 不引入插件 |

混用的典型失败：把 MagicaVoxel 模型当成 SVT 导入；把 Nanite `voxel_level` 调大当「开启体素世界」；
把 Voxel Plugin 2 当成 Minecraft。三者文档都正确，只是不是你要的那一层。

## 方块网格（物体级图生体素）

目标：一份调色板占用 → 场景里能摆、能碰、能打的道具。

推荐顺序：

1. CPU 写出 `.vox`（本目录脚本）。
2. 用 MagicaVoxel 看形和颜色。
3. 导入 UE：

| 做法 | 优点 | 代价 |
| --- | --- | --- |
| 合并成一张 Static Mesh（greedy meshing） | 一次 draw，Nanite 可用 | 破坏要切 mesh 或换资产 |
| 每格一个 cube ISM | 可逐格隐藏，破坏直观 | 实例数涨，要分 chunk |
| [VOX4U](https://github.com/mik14a/VOX4U) | 现成 `.vox` 导入 | 第三方插件，需审版本 |

第一阶段不要写 UE 插件。脚本出 `.vox` + 正交 BMP，人工或后续 MCP 再摆进关卡。
若只验证观感，把 greedy mesh 导出成 `.obj` 再 FBX 导入也够。

碰撞：合并网格用简化凸包或复杂碰撞；ISM 路径用 cube 碰撞或 overlap 查询占用表。
物体级不要用 SVT 做碰撞。

## Sparse Volume Texture

SVT 官方说明见 [references.md](references.md) 里的 Sparse Volume Textures（Experimental）。
页表 + 物理 3D tile 只存非空区域，给体积介质用。

来源是 **OpenVDB（`.vdb`）** 或 Niagara Fluids 缓存，不是 `.vox`。

消费方式：

| 方式 | 用途 |
| --- | --- |
| Heterogeneous Volume Actor | 场景里摆体积，走体积材质 |
| 体积雾 / 体积云 | 细节受雾分辨率限制 |
| Sparse Volume Texture Viewer | 调试单通道，不拿去出片 |

Attributes A/B 最多约 8 个通道。适合密度、温度、速度。
Path Tracer 对体积更完整；Deferred 有实时限制。

图生**实心**道具不要进 SVT。图生**烟火**不要进 `.vox`。

## Nanite 体素

UE 5.8 Python API 里 `MeshNaniteSettings` 有 `voxel_level`、`voxel_ndf`、`voxel_opacity`、`num_rays`。
这是构建静态网格时，对远处簇做体素近似，用来稳住超高面数资产的 LOD。

它不产生可编辑占用场，不能从图像生成，也不能当 MagicaVoxel 替代。
高面数静态网格可以开 Nanite；这与本目录的图生体素无关。

## Voxel Plugin 2

[概述](https://docs.voxelplugin.com/getting-started/working-with-voxel-plugin/)：交互地形，Stamp + Voxel Graph，主渲染走 Nanite。
立方体地形在 2.x 不是主线；Legacy 的 MagicaVoxel / Cubic 工作流不能迁到 2.x。

和本目录的差别：

| | 图生体素（物体） | Voxel Plugin 2 |
| --- | --- | --- |
| 尺度 | 道具、建筑块 | 地形 |
| 表示 | 调色板占用或网格体素化 | SDF / 高度 Stamp |
| 图生 | 图像 → 格子 | 节点图 / Stamp，不是 image-to-voxel |
| 本仓库 | 研究目录 | 第一阶段不引入 |

Hexagon 已经覆盖「离散格子战场」。再加一套 SDF 地形插件，重复的是地形，不是图生。

## 和 UnrealMCP / 现有 VFX 的接法

本目录的生成在编辑器外：挤出、高度图、Grok CSG。不要接到任何图生网格 → MIC 命令上。

合理的后续命令形状（未实现，只定边界）：

| 命令意图 | 输入 | 输出 |
| --- | --- | --- |
| 图像挤出为 `.vox` | PNG、厚度 | `out/*.vox` |
| 程序化预制件 | 形状名 | `out/grok_*.vox` |
| 网格体素化 | 已导入的 Static Mesh | 占用资产或 ISM |
| 体积导入 | `.vdb` | SVT（走编辑器 Import） |

第一阶段用脚本在编辑器外跑，避免未编译的 MCP 命令拖住研究。

## 材质提示

| 路径 | 材质域 | 法线 |
| --- | --- | --- |
| 方块网格 | Surface | 方块面法线或 greedy 后的硬边 |
| SVT | Volume | 密度梯度，不是顶点法线 |
| Voxel Plugin 2 | Surface，世界空间采样 | Flat Normal 节点可选 |

方块风关掉 Nanite 位移，用法线贴图或顶点色区分调色板。
不要把 Jade 母材质直接套在未拆 UV 的 cube soup 上；顶点色或 per-instance 调色板更省事。
