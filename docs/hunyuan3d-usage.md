# Hunyuan3D 使用手册：图片生成 3D 模型

本手册基于已部署的混合管线：

- **Shape**：Hunyuan3D-2.1（3.3B 参数，白模生成）
- **Texture**：Hunyuan3D-2.0 Paint（RGB 贴图生成）
- **入口脚本**：`hunyuan/hybrid_pipeline.py`

目标硬件：RTX 3060 12GB，峰值显存约 10GB。

---

## 前置条件

部署已完成并验证。如需重新部署，参考 [`hunyuan3d-setup.md`](hunyuan3d-setup.md)。

确认以下项：

- Python 3.10 虚拟环境 `hunyuan/venv`
- PyTorch 2.5.1 + CUDA 12.4
- Hunyuan3D-2.1 与 Hunyuan3D-2 仓库已克隆
- 模型权重已下载到 `~/.cache/hy3dgen/tencent/`

每次使用前激活环境（在项目根目录下执行）：

```powershell
cd hunyuan
.\venv\Scripts\activate
```

---

## 检查模型权重

权重缓存位置：

```text
~/.cache/hy3dgen/tencent/
├── Hunyuan3D-2.1/
│   └── hunyuan3d-dit-v2-1/
│       ├── config.yaml
│       └── model.fp16.ckpt          (~6.6 GB)
└── Hunyuan3D-2/
    ├── hunyuan3d-delight-v2-0/      (~2.6 GB)
    └── hunyuan3d-paint-v2-0/        (~2.6 GB)
```

检查 2.0 纹理权重完整性：

```powershell
python check_status.py
```

检查 2.1 Shape 权重：

```powershell
python download_weights.py
```

如果权重缺失，`download_weights.py` 会自动从 HuggingFace 拉取。国内网络慢时，参考部署文档用 BITS 或浏览器手动下载。

---

## 输入图片要求

图片质量直接影响 3D 输出。遵循以下原则：

| 项目 | 建议 |
|------|------|
| 格式 | JPG、PNG |
| 主体 | 单一物体，避免多个重叠主体 |
| 姿态 | 全身或半身侧视图效果最佳 |
| 背景 | 简单背景更易去背；复杂背景也可由 `rembg` 处理 |
| 光照 | 均匀自然光，避免过曝或死黑 |
| 透明图 | 已带 Alpha 通道的图片会跳过自动去背 |

本项目中的 `horse_input.jpg` 和 `horse_input_new.jpg` 都是合适输入。

---

## 快速开始

### 仅生成白模

首次验证推荐，速度快、显存占用低：

```powershell
python hybrid_pipeline.py horse_input.jpg -o horse_shape.glb --shape-only
```

### 生成带纹理的完整模型

```powershell
python hybrid_pipeline.py horse_input.jpg -o horse_textured.glb
```

完整流程分两步：

1. Shape 生成白模（约 1-3 分钟）
2. Paint 生成贴图（约 2-5 分钟）

---

## 参数详解

### Shape 质量参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--steps` | 50 | 扩散步数，越大细节越稳定，建议 50-100 |
| `--guidance` | 5.0 | 分类器自由引导尺度，建议 3.0-7.0 |
| `--octree` | 384 | 八叉树分辨率，越大网格越精细，建议 384-512 |
| `--mc-level` | 0.0 | Marching Cubes 等值面阈值，通常保持 0.0 |
| `--seed` | None | 固定随机种子，便于复现结果 |

### Texture 参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--texture-size` | 2048 | 贴图分辨率，可选 1024 / 2048 / 4096 |
| `--turbo` | False | 使用 turbo 模型，速度更快但质量稍低 |

### 预处理与后处理

| 参数 | 说明 |
|------|------|
| `--no-preprocess` | 跳过自动去背、居中和缩放 |
| `--input-size` | 预处理后的输入尺寸，默认 768 |
| `--smooth` | 对白模做轻度 Laplacian 平滑 |
| `--smooth-iter` | 平滑迭代次数，默认 2 |
| `--no-clean` | 不移除退化面片 |

---

## 推荐参数组合

### 快速预览

```powershell
python hybrid_pipeline.py horse_input.jpg -o preview.glb `
  --steps 30 --octree 256 --texture-size 1024 --turbo
```

### 平衡质量

```powershell
python hybrid_pipeline.py horse_input.jpg -o output.glb `
  --steps 50 --octree 384 --texture-size 2048
```

### 最佳质量

```powershell
python hybrid_pipeline.py horse_input.jpg -o output_best.glb `
  --steps 100 --guidance 6.0 --octree 512 --texture-size 2048
```

---

## 完整示例

以项目中的新马图为例，从原图到带纹理模型：

```powershell
cd hunyuan
.\venv\Scripts\activate

# 1. 检查权重
python download_weights.py
python check_status.py

# 2. 生成白模（验证几何）
python hybrid_pipeline.py horse_input_new.jpg -o horse_new_shape.glb --shape-only

# 3. 生成完整带纹理模型
python hybrid_pipeline.py horse_input_new.jpg -o horse_new_textured.glb
```

输出文件可直接导入 Unreal Engine、Blender 或其他 3D 软件。

---

## 查看输出

### 用脚本检查 GLB 结构

```powershell
python inspect_glb.py
```

该脚本默认检查 `horse_textured.glb`。修改脚本中的 `path` 变量可检查其他文件。

### 导入 Unreal Engine

1. 将 `.glb` 文件拖入 Unreal Content Browser
2. 选择 `Import`
3. 在导入设置中启用 `Transform Vertex Colors to RGB`（如需）
4. 点击 `Import All`

### 导入 Blender

1. `File > Import > glTF 2.0 (.glb/.gltf)`
2. 选择生成的 `.glb` 文件

---

## 常见问题

### 显存不足

- 关闭 Chrome、Unreal Editor 等占用显存的程序
- 先用 `--shape-only` 验证 Shape 阶段
- 降低 `--octree` 到 256 或 `--texture-size` 到 1024
- Shape 和 Paint 阶段不叠加，确保前一阶段完成后释放显存

### 生成结果模糊或缺失细节

- 提高 `--steps` 到 75-100
- 提高 `--octree` 到 512
- 检查输入图片是否清晰、主体是否完整
- 确保预处理后的图片主体没有被过度裁剪

### 贴图颜色不对

- 输入图片背景复杂时，预处理可能残留背景色
- 先用 `prep_input.py` 或图像编辑工具得到干净的透明 PNG，再用 `--no-preprocess`

### 生成速度很慢

- 首次运行需要加载模型，后续同一进程更快
- 使用 `--turbo` 加速 Paint 阶段
- 降低 `--steps` 和 `--octree`

### 权重加载失败

- 确认 HuggingFace 登录：`python -c "from huggingface_hub import login; login()"`
- 确认已在模型页面点击 **Agree and access**
- 运行 `python download_weights.py` 与 `python check_status.py` 检查完整性

---

## 相关脚本

| 脚本 | 用途 |
|------|------|
| `hybrid_pipeline.py` | 主入口，图片生成 3D 模型 |
| `prep_input.py` | 单独预处理图片：去背、居中、缩放 |
| `download_weights.py` | 下载 2.1 Shape 权重 |
| `check_status.py` | 检查 2.0 纹理权重完整性 |
| `inspect_glb.py` | 检查 GLB 文件结构 |
| `remove_beard.py` | 示例：针对马嘴部区域的后处理修复 |

---

## 工作流总结

```text
输入图片 (JPG/PNG)
    │
    ▼
hybrid_pipeline.py 预处理（去背、居中、缩放为 768x768）
    │
    ▼
Hunyuan3D-2.1 Shape ──► 白模 .glb
    │
    ▼
Hunyuan3D-2.0 Paint ──► 带纹理 .glb
    │
    ▼
导入 Unreal / Blender / 其他 DCC
```

---

文档时间：2026-06-16
