# Hunyuan3D 混合管线部署文档

> RTX 3060 12GB | 2.1 Shape + 2.0 Paint | Windows 10/11

## 硬件检查

```powershell
nvidia-smi --query-gpu=name,memory.total,driver_version --format=csv,noheader
```

| 项目 | 最低要求 | 当前配置 |
|------|---------|---------|
| GPU | RTX 3060 12GB | ✅ RTX 3060 12GB |
| 驱动 | ≥551.78 (CUDA 12.4) | ✅ 560.94 (CUDA 12.6) |
| 显存(Shape) | 10 GB | ✅ 12 GB |
| 显存(Texture) | 8 GB | ✅ 12 GB (峰值分开跑，不叠加) |
| Python | 3.10 | ✅ 不要用 3.13 |
| 磁盘 | ~40 GB | — |

---

## 快速部署

### 1. 安装 Python 3.10

```powershell
# 下载 Python 3.10.11
# https://www.python.org/downloads/release/python-31011/
# 安装时勾选 "Add Python to PATH"

# 验证
py -3.10 --version   # Python 3.10.11
```

### 2. 创建虚拟环境

```powershell
cd D:\Playground\TA-Playground
mkdir hunyuan
cd hunyuan
py -3.10 -m venv venv
```

### 3. 安装 PyTorch 2.5.1 (CUDA 12.4)

> ⚠️ **实测重要**：所有国内镜像（阿里云、清华、豆瓣、上交）都**不缓存** CUDA 版 PyTorch wheel。
> 2.5GB 的 `torch-2.5.1+cu124` 必须从 `download.pytorch.org` 下载，国内速度约 300KB/s，**需 1~2 小时**。

```powershell
.\venv\Scripts\activate

# 官方源（推荐，cu124 无线程兼容问题）
pip install torch==2.5.1 torchvision==0.20.1 torchaudio==2.5.1 `
    --index-url https://download.pytorch.org/whl/cu124 `
    --default-timeout 7200

# 备用：清华镜像装 CPU 依赖快，但 PyTorch CUDA wheel 仍回源 pytorch.org
pip install torch==2.5.1 torchvision==0.20.1 torchaudio==2.5.1 `
    -i https://pypi.tuna.tsinghua.edu.cn/simple `
    --default-timeout 3600
```

验证:
```powershell
python -c "import torch; print(f'CUDA: {torch.cuda.is_available()}'); print(f'GPU: {torch.cuda.get_device_name(0)}')"
# CUDA: True
# GPU: NVIDIA GeForce RTX 3060
```

### 4. 克隆仓库

```powershell
git clone https://github.com/Tencent-Hunyuan/Hunyuan3D-2.1.git
git clone https://github.com/Tencent-Hunyuan/Hunyuan3D-2.git
```

### 5. 安装 2.1 Shape 依赖

```powershell
cd Hunyuan3D-2.1/hy3dshape
pip install -r requirements.txt
cd ../..
```

### 6. 安装 2.0 纹理模块

```powershell
cd Hunyuan3D-2
pip install -r requirements.txt

# 编译渲染器
cd hy3dgen/texgen/custom_rasterizer
python setup.py install
cd ../../..
cd hy3dgen/texgen/differentiable_renderer
python setup.py install
cd ../../..
```

> ⚠️ **编译坑**：需 Visual Studio **2022**（2017-2022 均可），VS 2025/Insiders 版本 CUDA 不支持。
> 如装有多版本 VS，必须显式激活 VS 2022 环境：
> ```powershell
> $env:TORCH_CUDA_ARCH_LIST="8.6"
> cmd /c "call ""C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"" x64 && set DISTUTILS_USE_SDK=1 && cd /d <path> && python -m pip install -e ."
> ```

### 7. HuggingFace 登录

```powershell
pip install huggingface_hub
huggingface-cli login
```

---

## 目录结构

```
hunyuan/                       # ← gitignored
├── venv/                      # Python 3.10 虚拟环境
├── Hunyuan3D-2.1/             # Shape 生成 (2.1)
│   └── hy3dshape/
├── Hunyuan3D-2/               # 纹理生成 (2.0)
│   └── hy3dgen/texgen/
└── hybrid_pipeline.py         # 混合调用入口
```

---

## 使用方式

### 命令行

```powershell
.\venv\Scripts\activate

# 完整流程：图片 → 带纹理 3D 模型
python hybrid_pipeline.py input.png -o output.glb

# 仅白模
python hybrid_pipeline.py input.png -o output.glb --shape-only
```

### Python 调用

```python
import sys, gc, torch

# 路径配置
sys.path.insert(0, 'Hunyuan3D-2.1/hy3dshape')
sys.path.insert(0, 'Hunyuan3D-2/hy3dgen')

# ── Step 1: 几何 (2.1 Shape, 峰值 ~10GB) ──
from hy3dshape.pipelines import Hunyuan3DDiTFlowMatchingPipeline
shape_pipe = Hunyuan3DDiTFlowMatchingPipeline.from_pretrained('tencent/Hunyuan3D-2.1')
mesh = shape_pipe(image='input.png')[0]

del shape_pipe; gc.collect(); torch.cuda.empty_cache()

# ── Step 2: 纹理 (2.0 Paint, 峰值 ~8GB) ──
from hy3dgen.texgen import Hunyuan3DPaintPipeline
paint_pipe = Hunyuan3DPaintPipeline.from_pretrained('tencent/Hunyuan3D-2')
textured_mesh = paint_pipe(mesh, image='input.png')

textured_mesh.export('output.glb')
```

---

## 模型权重

| 阶段 | 模型 | HF Repo | 大小 |
|------|------|---------|:---:|
| 几何 | Hunyuan3D-Shape-v2-1 | `tencent/Hunyuan3D-2.1` | 3.3B |
| 纹理 | Hunyuan3D-Paint-v2-0 | `tencent/Hunyuan3D-2` | 1.3B |

首次运行自动下载到 `~/.cache/huggingface/`。

---

## 常见问题

### PyTorch 下载超时 / 慢
- **根本原因**：国内没有源缓存 CUDA wheel，必须从 `download.pytorch.org` 拉
- `--default-timeout 7200` 防止 read timeout
- 耐心等待 1-2 小时；不建议中断

### CUDA Out of Memory
- 关闭其他 GPU 程序（Chrome、IDE、Unity 等）
- 检查显存占用：`nvidia-smi`
- Shape 阶段 ~10GB，纹理阶段 ~8GB，**分阶段跑不叠加**
- 卡住了改用 `--shape-only` 只出白模

### Python 版本
- 必须 Python 3.10；3.13 有大量包不兼容
- 验证：`py -3.10 --version`

### 编译渲染器失败
- 需要 Visual Studio Build Tools（MSVC v143）
- `bash` 命令需要 Git Bash 或 WSL

---

## 版本兼容矩阵

| 组件 | 版本 | 显存 | 12GB 能跑 | 备注 |
|------|------|------|:---:|------|
| Shape | 2.1 (3.3B) | ~10 GB | ✅ | 推荐 |
| Shape | 2.0 (2.6B) | ~8 GB | ✅ | 备用 |
| Paint | 2.0 (1.3B) | ~8 GB | ✅ | RGB 纹理 |
| Paint | 2.1 (2.0B) | ~21 GB | ❌ | PBR 纹理，需 24GB+ |

---

最后更新: 2026-06-09
