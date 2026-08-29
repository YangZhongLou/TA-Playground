# 生成管线

图像进，格子出。按代价从低到高排。挤出、高度图、CSG 本目录脚本已覆盖。

## 1. 像素挤出

把精灵图当成前视图：不透明像素变成沿厚度方向复制的一串体素，颜色来自像素。

```text
输入 PNG/BMP（前视）
  → 读 RGBA
  → alpha > 阈值的像素保留
  → 体素 (x, y in 0..depth-1, z = height-1-row)
  → 量化到 ≤255 色
  → 写 .vox
```

厚度是假的：侧面是拉伸色带，没有真实侧向信息。适合图标、像素道具、立绘卡片。
`--demo sprite` 走这条。

## 2. 高度图柱体

把图当成俯视图。亮度（或 alpha）决定柱高，颜色沿柱复制或只涂顶面。

```text
输入俯视图
  → 高度 h(u,v) = luma * max_height
  → 对 z in 0..h-1 写入占用
  → 写 .vox
```

没有悬空、没有桥、没有洞。适合地块、像素岛屿、简单屋顶。Minecraft 世界生成经常是这条的放大版。
`--demo height` 走这条。

## 3. 程序化 CSG

用球、柱、盒的布尔组合写占用。Grok 写规则，脚本栅格化。不是从图推断侧面。

```powershell
python image-voxel/scripts/grok_direct.py potion
python image-voxel/scripts/grok_direct.py crate
```

适合对称道具、建筑块。不适合「这张照片里的马」。见 [models.md](models.md)。

## 4. 剪影空间雕刻

多张正交（或已知相机）剪影：从每个相机射出体素柱，只保留所有视图都判为「物体内」的格子。

凸物体大致正确。凹槽、把手围成的孔会被填实，除非补侧面或透视视图。
代价低，质量上限明显，适合做神经方法的基线，不适合当正式资产管线。

## 5. 网格体素化

已有三角网格（Blender / 其他 DCC 导出的 `.glb` / `.obj`）→ 占用场。

常见做法：

| 方法 | 要点 |
| --- | --- |
| 栅格化三角形 | 把面覆盖的格子标为表面，再 flood-fill 内部 |
| 射线交叉奇偶 | 沿轴打射线，奇数次交叉视为内部 |
| 实心 SDF | 算到网格的距离，负值内部 |

这条把自备网格接进体素消费：破坏、占用查询、方块化预览。
脚本：[scripts/mesh_to_voxels.py](scripts/mesh_to_voxels.py)，需要本机 `pip install trimesh`。
不要为了喂它去跑图生 3D。预制件优先 [grok_direct.py](scripts/grok_direct.py)。

薄片、开放面、非流形 UV 缝会让填实抖动。封闭实体网格适合这条。

## 6. 神经占用 / 体素扩散

早期单图 3D 直接预测 32³–128³ 占用或 SDF（Occupancy Networks、3D-R2N2、Voxel Diffusion）。
分辨率低，方块感重，细节靠超分或再 meshing。

现在物体级 SOTA 不再把稠密低分辨率体积当最终表示，而是：

1. 先预测稀疏结构（哪些格子非空）。
2. 再在非空格上预测特征向量或 SDF / 外观。
3. 解码成网格、高斯或体积。

体素仍在，只是从「最终方块画布」变成「稀疏潜空间」。

## 7. TRELLIS：SLAT

[Structured 3D Latents](https://arxiv.org/abs/2412.01506)（CVPR 2025）：

1. 在约 64³ 网格上标表面体素。
2. 用 DINOv2 多视角特征填每个激活格，压成 SLAT。
3. 两段 rectified flow：先生成稀疏结构，再生成每格潜向量。
4. 不同解码器输出高斯、辐射场或网格。

输入可以是图或文。同一套体素潜空间能换输出格式。本目录第一阶段不接入。见 [models.md](models.md)。

## 8. TRELLIS.2：O-Voxel

[TRELLIS.2-4B](https://huggingface.co/microsoft/TRELLIS.2-4B) 把潜空间换成 O-Voxel：无场、稀疏、几何和 PBR 一起走。
生成分结构 / 形状 / 纹理多段 flow，体素分辨率可到 1536³，导出 GLB 仍带贴图。
要方块风，仍需在导出后体素化。本目录不接入，也不用别的图生 3D 顶上。

## 选型

| 目标 | 用哪条 |
| --- | --- |
| 今天就要一份 `.vox` 看方块感 | 挤出或高度图 |
| 药瓶、木箱、规则几何 | [grok_direct.py](scripts/grok_direct.py) |
| 已有 DCC 网格，要占用 | [mesh_to_voxels.py](scripts/mesh_to_voxels.py) |
| 要可换格式的神经 3D | 未接入；不要用 Hunyuan 顶上 |
| 要烟火而非实心 | 去做 VDB |

不要用对话模型去挤一张像素画。像素画用挤出。本目录不调用 Hunyuan。
