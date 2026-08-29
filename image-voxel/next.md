# 下一步实验

每条都有可观察的输出。做完再考虑 UE 插件。
本目录不调用 Hunyuan，不读 `hunyuan/` 产物。

## 阶段 0 — 表示跑通（已做）

```powershell
python image-voxel/scripts/image_to_voxels.py --demo sprite
python image-voxel/scripts/image_to_voxels.py --demo height
```

验收：`image-voxel/out/` 下有 `.vox` 与正交 `.bmp`；体素数 > 0。
用 MagicaVoxel 打开 `.vox`，轴为 Z-up。

## 阶段 1 — 真图进脚本

```powershell
python image-voxel/scripts/image_to_voxels.py path\to\art.png --mode extrude --depth 8
python image-voxel/scripts/image_to_voxels.py path\to\top.png --mode height --max-height 32
```

验收：最长边不超过 `--max-size`；调色板 ≤255；透明像素不占体素。
失败模式：照片挤出侧面花——方法上限，不是 bug。

## 阶段 2 — Grok 程序化预制件（已做）

```powershell
python image-voxel/scripts/grok_direct.py potion
python image-voxel/scripts/grok_direct.py crate
```

验收：药瓶 `24³`、1946 体素，侧视圆腹 + 木塞；木箱空心。说明见 [models.md](models.md)。

## 阶段 3 — UE 里看见

选一种，不要三种一起做：

1. `.vox` → 手工或 VOX4U → Static Mesh 进测试关卡。
2. 脚本加写 `.obj`（greedy 或一格一面），FBX/OBJ 导入。
3. MCP 只负责摆 actor 和截图，生成仍在编辑器外。

验收：视口里能对上 MagicaVoxel 预览的轮廓；碰撞至少有盒。
不把 SVT 当验收对象，除非输入是 VDB。

## 明确不做

- 不调用 Hunyuan 推理，不依赖 `hunyuan/` 下的 GLB 或 venv。
- 不 dump 任何图生 3D 的八叉树。
- 第一阶段不引入 Voxel Plugin 2。
- 不把 Nanite `voxel_level` 当图生体素验收。
- 不在 UnrealMCP 里加半成品 generate-voxel 命令。
- 不训练、不另装 TRELLIS。聊天模型不写 `.vox` 坐标。
