# 参考文献

读原文。不要把「体素」营销页和表示层论文混成一套最佳实践。

## 神经生成

| 资源 | 为什么读 |
| --- | --- |
| [TRELLIS / SLAT](https://microsoft.github.io/TRELLIS/) | 表面稀疏体素 + 视觉特征的结构化潜空间 |
| [Structured 3D Latents (arXiv:2412.01506)](https://arxiv.org/abs/2412.01506) | SLAT 编码、两段 flow、多解码器 |
| [TRELLIS.2-4B](https://huggingface.co/microsoft/TRELLIS.2-4B) | O-Voxel、高分辨率稀疏 VAE |
| [Native and Compact Structured Latents (arXiv:2512.14692)](https://arxiv.org/abs/2512.14692) | TRELLIS.2 技术报告 |

## 经典体素

| 资源 | 为什么读 |
| --- | --- |
| [MagicaVoxel](https://ephtracy.github.io/) | `.vox` 工作流与调色板编辑 |
| [MagicaVoxel .vox spec](https://github.com/ephtracy/voxel-model) | `SIZE` / `XYZI` / `RGBA` 字节布局 |
| OpenVDB 文档 | 稀疏体积树，SVT 的上游格式 |
| Occupancy Networks (CVPR 2019) | 连续占用场，不是稠密 32³ 画布 |

## 引擎

| 资源 | 为什么读 |
| --- | --- |
| [Sparse Volume Textures](https://dev.epicgames.com/documentation/en-us/unreal-engine/sparse-volume-textures-in-unreal-engine) | UE 5.8 SVT、Heterogeneous Volume、VDB 导入 |
| [Heterogeneous Volumes](https://dev.epicgames.com/documentation/en-us/unreal-engine/heterogeneous-volumes-in-unreal-engine) | 体积 Actor 怎么画 SVT |
| [Rendering Large Volume Datasets in UE5](https://arxiv.org/html/2504.07485) | SVT 页表、规模上限、和 chunk 方案的差别 |
| [Voxel Plugin 2 概述](https://docs.voxelplugin.com/getting-started/working-with-voxel-plugin/) | SDF 地形，不是立方体世界 |
| [VOX4U](https://github.com/mik14a/VOX4U) | UE5 导入 `.vox` 的现成插件 |

## 本仓库

| 资源 | 关系 |
| --- | --- |
| [Plugins/Hexagon/Documents/DESIGN.md](../Plugins/Hexagon/Documents/DESIGN.md) | 2.5D 离散格子，对照用 |
| [models.md](models.md) | Grok 接法；本目录不用 Hunyuan |
| [network-sync/README.md](../network-sync/README.md) | 同风格研究目录的分层写法 |
