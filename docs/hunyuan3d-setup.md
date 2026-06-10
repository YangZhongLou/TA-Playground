# Hunyuan3D 混合管线部署文档

> RTX 3060 12GB | 2.1 Shape + 2.0 Paint | Windows 10/11

## 硬件检查

```powershell
nvidia-smi --query-gpu=name,memory.total,driver_version --format=csv,noheader
```

| 项目 | 最低要求 | 当前配置 |
|------|---------|---------|
| GPU | RTX 3060 12GB | RTX 3060 12GB |
| 驱动 | >=551.78 (CUDA 12.4) | 560.94 (CUDA 12.6) |
| 显存 Shape | 10 GB | 12 GB |
| 显存 Texture | 8 GB | 12 GB (分阶段跑不叠加) |
| Python | 3.10 | 3.10.11 |
| 磁盘 | ~40 GB | — |

---

## 快速部署

### 1. 安装 Python 3.10

```powershell
# 下载 Python 3.10.11
# https://www.python.org/downloads/release/python-31011/
# 安装时勾选 "Add Python to PATH"

py -3.10 --version   # Python 3.10.11
```

### 2. 创建虚拟环境

```powershell
cd <project_root>
mkdir hunyuan
cd hunyuan
py -3.10 -m venv venv
```

### 3. 安装 PyTorch 2.5.1 (CUDA 12.4)

> **已知问题**：所有国内镜像（阿里云、清华、豆瓣、上交）都不缓存 CUDA 版 PyTorch wheel。
> 2.5GB 的 `torch-2.5.1+cu124` 必须从 `download.pytorch.org` 下载，国内 ~300KB/s，需 1-2 小时。

```powershell
.\venv\Scripts\activate

pip install torch==2.5.1 torchvision==0.20.1 torchaudio==2.5.1 `
    --index-url https://download.pytorch.org/whl/cu124 `
    --default-timeout 7200
```

验证：
```powershell
python -c "import torch; print(torch.cuda.is_available()); print(torch.cuda.get_device_name(0))"
# True
# NVIDIA GeForce RTX 3060
```

### 4. 克隆仓库

```powershell
git clone https://github.com/Tencent-Hunyuan/Hunyuan3D-2.1.git
git clone https://github.com/Tencent-Hunyuan/Hunyuan3D-2.git
```

### 5. 安装 Python 依赖

> **版本对齐关键**：`transformers` 和 `diffusers` 必须与 `torch 2.5.1` 兼容。

```powershell
pip install ninja pybind11 einops opencv-python numpy omegaconf tqdm `
    trimesh pymeshlab pygltflib xatlas accelerate gradio fastapi uvicorn rembg onnxruntime `
    transformers==4.46.3 diffusers==0.31.0 `
    -i https://pypi.tuna.tsinghua.edu.cn/simple --default-timeout 600
```

### 6. 编译 2.0 纹理渲染器

> **已知问题**：需要 Visual Studio 2022（MSVC v143）。VS 2025/Insiders 版本 CUDA 不支持。
> 多版本 VS 需显式激活 2022：

```powershell
$env:TORCH_CUDA_ARCH_LIST = "8.6"

# custom_rasterizer
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"" x64 && set DISTUTILS_USE_SDK=1 && cd /d Hunyuan3D-2\hy3dgen\texgen\custom_rasterizer && D:\...\hunyuan\venv\Scripts\python.exe -m pip install -e ."

# differentiable_renderer
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"" x64 && set DISTUTILS_USE_SDK=1 && cd /d Hunyuan3D-2\hy3dgen\texgen\differentiable_renderer && D:\...\hunyuan\venv\Scripts\python.exe setup.py install"
```

### 7. HuggingFace 登录

```powershell
python -c "from huggingface_hub import login; login(token='hf_xxx')"
```

去 [huggingface.co/tencent/Hunyuan3D-2.1](https://huggingface.co/tencent/Hunyuan3D-2.1) 和 [Hunyuan3D-2](https://huggingface.co/tencent/Hunyuan3D-2) 各点一次 **Agree and access**。

### 8. 下载模型权重

> **关键**：Git Bash / MinGW **不走** Windows TUN 模式 VPN！必须用 PowerShell 或浏览器。

**方法 A: BITS（推荐，支持断点续传）**

```powershell
# 创建目录并下载 config
$dir = "$env:USERPROFILE\.cache\hy3dgen\tencent\Hunyuan3D-2.1\hunyuan3d-dit-v2-1"
New-Item -ItemType Directory -Force -Path $dir | Out-Null

# config.yaml（秒下）
Invoke-WebRequest "https://huggingface.co/tencent/Hunyuan3D-2.1/resolve/main/hunyuan3d-dit-v2-1/config.yaml" -OutFile "$dir\config.yaml"

# model.fp16.ckpt（~7GB，BITS 后台下载，断点续传）
Start-BitsTransfer `
    -Source "https://huggingface.co/tencent/Hunyuan3D-2.1/resolve/main/hunyuan3d-dit-v2-1/model.fp16.ckpt" `
    -Destination "$dir\model.fp16.ckpt" `
    -Asynchronous

# 查看进度
Get-BitsTransfer | Format-List JobState, BytesTransferred, BytesTotal
```

**方法 B: 浏览器手动下载**

从 [HuggingFace](https://huggingface.co/tencent/Hunyuan3D-2.1/tree/main/hunyuan3d-dit-v2-1) 下载，放到对应路径：

| 文件 | 本地路径 |
|------|---------|
| `config.yaml` | `~/.cache/hy3dgen/tencent/Hunyuan3D-2.1/hunyuan3d-dit-v2-1/config.yaml` |
| `model.fp16.ckpt` | `~/.cache/hy3dgen/tencent/Hunyuan3D-2.1/hunyuan3d-dit-v2-1/model.fp16.ckpt` |

纹理权重（2.0 Paint ~2.6GB）同理，从 `tencent/Hunyuan3D-2` 下载到 `~/.cache/hy3dgen/tencent/Hunyuan3D-2/`。

---

## 目录结构

```
hunyuan/                          # gitignored
├── venv/                         # Python 3.10 venv
├── Hunyuan3D-2.1/                # Shape repo
│   └── hy3dshape/                #   shape package
├── Hunyuan3D-2/                  # Texture repo
│   └── hy3dgen/texgen/           #   texture package
├── hybrid_pipeline.py            # CLI entry point
└── download_weights.py           # weight pre-downloader
```

权重缓存：
```
~/.cache/hy3dgen/
└── tencent/
    ├── Hunyuan3D-2.1/
    │   └── hunyuan3d-dit-v2-1/
    │       ├── config.yaml
    │       └── model.fp16.ckpt      (~6.6 GB)
    └── Hunyuan3D-2/
        ├── hunyuan3d-delight-v2-0/  (~2.6 GB)
        └── hunyuan3d-paint-v2-0/    (~2.6 GB)
```

---

## 使用方式

### 命令行

```powershell
.\venv\Scripts\activate

# 仅白模（首次验证推荐）
python hybrid_pipeline.py input.png -o output.glb --shape-only

# 完整流程：图片 -> 带纹理 3D 模型
python hybrid_pipeline.py input.png -o output.glb
```

### Python API

```python
import sys, gc, torch

sys.path.insert(0, 'Hunyuan3D-2.1/hy3dshape')
sys.path.insert(0, 'Hunyuan3D-2')

# Step 1: shape (peak ~10GB VRAM)
from hy3dshape.pipelines import Hunyuan3DDiTFlowMatchingPipeline
shape = Hunyuan3DDiTFlowMatchingPipeline.from_pretrained('tencent/Hunyuan3D-2.1')
mesh = shape(image='input.png')[0]
del shape; gc.collect(); torch.cuda.empty_cache()

# Step 2: texture (peak ~8GB VRAM)
from hy3dgen.texgen import Hunyuan3DPaintPipeline
paint = Hunyuan3DPaintPipeline.from_pretrained('tencent/Hunyuan3D-2')
textured = paint(mesh, image='input.png')
textured.export('output.glb')
```

---

## 常见问题

### PyTorch 下载超时 / 极慢
- 原因：国内无镜像缓存 CUDA wheel，必须从 pytorch.org 拉
- 解决：`--default-timeout 7200`，等 1-2 小时

### 模型权重下载失败
- 原因：HuggingFace CDN 国内慢，hf-mirror 不缓存该模型
- 解决：用 **BITS**（Windows 原生断点续传）或浏览器手动下
- **Git Bash 不走 TUN VPN**：必须用 PowerShell 运行下载命令

### CUDA Out of Memory
- `nvidia-smi` 查看占用，关掉 Chrome / Unity 等 GPU 程序
- Shape 阶段 ~10GB，纹理阶段 ~8GB，不叠加
- 卡住就 `--shape-only` 只出白模

### transformers / diffusers 版本冲突
- 症状：`torch has no attribute float8_e8m0fnu` 或 `cannot import Dinov2WithRegistersConfig`
- 解决：锁定 `transformers==4.46.3 diffusers==0.31.0`

### VS 编译报错 unsupported compiler
- 症状：`fatal error C1189: unsupported Microsoft Visual Studio version`
- 原因：CUDA 12.5 不支持 VS 2025+
- 解决：用 VS 2022 的 vcvarsall.bat 激活环境后编译

### Python 3.13 兼容性
- 大量 AI 库无 3.13 wheel，必须用 Python 3.10

### Windows GBK 编码报错
- 脚本避免使用 emoji / Unicode 特殊字符即可

### 开了 VPN 但下载不通
- **Git Bash (MinGW) 不走 Windows TUN 模式 VPN**，必须用 PowerShell 或 CMD
- 验证：PowerShell 跑 `Invoke-WebRequest https://huggingface.co -TimeoutSec 10` 应返回 200
- 大文件下载用 BITS（`Start-BitsTransfer`），支持断点续传，VPN 断了自动恢复

---

## 版本兼容矩阵

| 组件 | 版本 | 显存 | RTX 3060 12GB | 备注 |
|------|------|------|:---:|------|
| Shape | 2.1 (3.3B) | ~10 GB | OK | 推荐，效果最好 |
| Shape | 2.0 (2.6B) | ~8 GB | OK | 备用 |
| Paint | 2.0 (1.3B) | ~8 GB | OK | RGB 纹理 |
| Paint | 2.1 (2.0B) | ~21 GB | NO | PBR 纹理，需 24GB+ |

### 已验证兼容的 Python 包版本

| 包 | 版本 | 备注 |
|------|------|------|
| torch | 2.5.1+cu124 | 不可升级 |
| transformers | 4.46.3 | 精确匹配 |
| diffusers | 0.31.0 | 精确匹配 |
| huggingface_hub | 0.36.2 | — |
| trimesh | 4.12.2 | — |

---

部署时间：2026-06-09  
最后验证：2026-06-10
