# 大模型怎么接

本目录**不调用 Hunyuan**。不跑 `hybrid_pipeline.py`，不读 `hunyuan/` 下的 GLB。

生成层用三件事：像素挤出、高度图、Grok 写的程序化占用。聊天模型不直接输出照片级 `(x,y,z)` 列表。图生 3D 权重（TRELLIS 等）另开任务，不和这条 CPU 线混装。

## 先分清两种「大模型」

| 你说的大模型 | 本目录怎么用 |
| --- | --- |
| Grok / 聊天模型 | 写 CSG 预制件、评切片、改脚本。不写照片级占用数组 |
| 图生 3D（TRELLIS 等） | 未接入。本目录不拿 Hunyuan 当形状先验 |

## 直接用 Grok

这条对话里的模型就是 Grok。Imagine 出图和视频，不出网格、`.vox` 或占用场。

| 接法 | 实际产物 | 本目录 |
| --- | --- | --- |
| 写 `(x,y,z)` 列表 | 文本坐标 | 不用。16³ 以上会穿插、漏面 |
| 写程序化 CSG | 球、柱、盒的占用 | **主路径之一** |
| Imagine 出图再挤出 | 2.5D 色带 | 像素画可以；写实侧面是假的 |

```powershell
python image-voxel/scripts/grok_direct.py potion
python image-voxel/scripts/grok_direct.py crate
```

宝石 `--demo sprite` 是手写挤出，不是对话吐坐标。

## 推荐工作流

```text
像素画 / 图标  → image_to_voxels.py --mode extrude
俯视地块      → image_to_voxels.py --mode height
药瓶、木箱    → grok_direct.py
DCC 已有网格  → mesh_to_voxels.py（可选，需自备 trimesh）
```

```powershell
python image-voxel/scripts/image_to_voxels.py --demo sprite
python image-voxel/scripts/grok_direct.py potion
```

已有 Blender / Magica 以外的三角网格时，再体素化。不要为了喂 `mesh_to_voxels.py` 去跑任何图生 3D。

## 大模型解决什么、不解决什么

Grok 能做：对称预制件、规则几何、调 `--res` 和调色板。

Grok 做不到：单张照片重建封闭立体。那是图生 3D 的事；本目录明确不走 Hunyuan，也不把 TRELLIS 装进来凑。

不要用更大的对话模型去挤一张像素画。像素画用挤出。

## 和 TRELLIS

TRELLIS 的体素是潜空间锚点，不是 `.vox`。要方块风仍要采样。本目录第一阶段不接入。3060 12GB 能硬跑，但与「Grok + CPU」并行会抢显存、抢环境。

## 验收

- `grok_direct.py potion`：侧视是圆腹 + 木塞，不是一张挤出的色带。
- `--demo sprite` / `--demo height`：体素数 > 0。
- 未验收：Hunyuan 推理、dump 八叉树、TRELLIS、聊天模型写 `.vox` 坐标。
