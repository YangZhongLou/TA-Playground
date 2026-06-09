# Hunyuan3D 混合管线部署文档

> RTX 3060 12GB | 2.1 Shape + 2.0 Paint | Windows 10/11

## 硬件要求

| 项目 | 最低要求 | 当前配置 |
|------|---------|---------|
| GPU | RTX 3060 12GB | ✅ RTX 3060 12GB |
| 驱动 | ≥551.78 (CUDA 12.4) | ✅ 560.94 (CUDA 12.6) |
| 显存(Shape) | 10 GB | ✅ 12 GB |
| 显存(Texture) | ~8 GB | ✅ 12 GB |
| 磁盘 | ~40 GB | — |

## 快速部署

### 1. 安装 Python 3.10

```powershell
# 下载 Python 3.10.11
# https://www.python.org/downloads/release/python-31011/
# 安装时勾选 "Add Python to PATH"
```

### 2. 创建虚拟环境

```powershell
cd D:\Playground\TA-Playground
mkdir hunyuan
cd hunyuan
py -3.10 -m venv venv
```

### 3. 安装 PyTorch 2.5.1 (CUDA 12.4)

```powershell
.\venv\Scripts\activate
pip install torch==2.5.1 torchvision==0.20.1 torchaudio==2.5.1 --index-url https://download.pytorch.org/whl/cu124 --default-timeout 3600
```

> 国内用户下载慢/超时：2.5GB wheel 文件较大，`--default-timeout` 可防止超时。
> 如仍超时，可用阿里云镜像（将 `download.pytorch.org` 换为 `mirrors.aliyun.com/pytorch-wheels`）。

验证 GPU 可用:
```powershell
python -c "import torch; print(torch.cuda.is_available())"  # 应输出 True
```

### 4. 克隆 Hunyuan3D-2.1（Shape 模型）

```powershell
git clone https://github.com/Tencent-Hunyuan/Hunyuan3D-2.1.git
cd Hunyuan3D-2.1
pip install -r requirements.txt

# 编译自定义渲染器（纹理需要，跳过则只生成白模）
cd hy3dpaint/custom_rasterizer
pip install -e .
cd ../..
cd hy3dpaint/DifferentiableRenderer
bash compile_mesh_painter.sh
cd ../..
```

### 5. 下载 2.0 纹理模型

```powershell
# 回到 hunyuan 目录
cd ..
git clone https://github.com/Tencent-Hunyuan/Hunyuan3D-2.git
cd Hunyuan3D-2
pip install -r requirements.txt

# 编译 2.0 纹理渲染器
cd hy3dgen/texgen/custom_rasterizer
python setup.py install
cd ../../..
cd hy3dgen/texgen/differentiable_renderer
python setup.py install
cd ../../..
```

### 6. 模型权重下载

代码首次运行会自动从 HuggingFace 下载，或手动下载：

| 用途 | 模型 | HuggingFace |
|------|------|-------------|
| 几何生成 | Hunyuan3D-Shape-v2-1 (3.3B) | `tencent/Hunyuan3D-2.1` |
| 纹理生成 | Hunyuan3D-Paint-v2-0 (1.3B) | `tencent/Hunyuan3D-2` |

```powershell
# 如需手动下载，先登录 HuggingFace
pip install huggingface_hub
huggingface-cli login
```

## 目录结构

```
hunyuan/
├── venv/                  # Python 3.10 虚拟环境 (gitignored)
├── Hunyuan3D-2.1/         # Shape 生成代码
├── Hunyuan3D-2/           # 纹理生成代码
├── SETUP.md               # 本文档
├── hybrid_pipeline.py     # 混合调用脚本
└── output/                # 输出目录
```

## 使用方式

### 混合管线：2.1 Shape + 2.0 Paint

```python
import sys
sys.path.insert(0, './Hunyuan3D-2.1/hy3dshape')
sys.path.insert(0, './Hunyuan3D-2/hy3dgen')

from Hunyuan3D_2_1.hy3dshape.pipelines import Hunyuan3DDiTFlowMatchingPipeline as ShapePipeline
from Hunyuan3D_2.hy3dgen.texgen import Hunyuan3DPaintPipeline

# Step 1: 生成几何 (2.1 Shape, 峰值 ~10GB)
shape_pipe = ShapePipeline.from_pretrained('tencent/Hunyuan3D-2.1')
mesh = shape_pipe(image='input.png')[0]

# 释放 Shape 模型
del shape_pipe
import torch
torch.cuda.empty_cache()

# Step 2: 生成纹理 (2.0 Paint, 峰值 ~8GB)
paint_pipe = Hunyuan3DPaintPipeline.from_pretrained('tencent/Hunyuan3D-2')
textured_mesh = paint_pipe(mesh, image='input.png')
```

### 仅生成白模

```python
from Hunyuan3D_2_1.hy3dshape.pipelines import Hunyuan3DDiTFlowMatchingPipeline

pipe = Hunyuan3DDiTFlowMatchingPipeline.from_pretrained('tencent/Hunyuan3D-2.1')
mesh = pipe(image='input.png')[0]
mesh.export('output.glb')
```

## 常见问题

### CUDA Out of Memory
- 关闭其他占用 GPU 的程序（Chrome、游戏等）
- 纹理阶段尝试降低分辨率参数
- 仅跑 Shape 生成（白模），纹理用在线 Demo

### 编译渲染器失败
- 确保已安装 Visual Studio Build Tools（C++ 编译工具链）
- Windows 上 `bash` 命令需要 Git Bash 或 WSL

### Python 版本问题
- 必须使用 Python 3.10，3.11+ 部分依赖可能不兼容
- 使用 `py -3.10` 指定版本

## 版本兼容矩阵

| 组件 | 版本 | 显存 | 备注 |
|------|------|------|------|
| Shape | 2.1 (3.3B) | ~10 GB | 推荐，效果最好 |
| Shape | 2.0 (2.6B) | ~8 GB | 备用 |
| Paint | 2.0 (1.3B) | ~8 GB | RGB 纹理 |
| Paint | 2.1 (2.0B) | ~21 GB | PBR 纹理，需 24GB+ 卡 |

---

最后更新: 2026-06-09
