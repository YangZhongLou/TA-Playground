# Hunyuan3D-2 / 2.1 部署指南

> 本文档覆盖 TA-Playground 中 Hunyuan3D 的本地部署：
> - **Shape 生成**：`hunyuan/Hunyuan3D-2.1`（图生白模）
> - **纹理生成**：`hunyuan/Hunyuan3D-2`（RGB Paint，hybrid_pipeline 使用）
> - **PBR / 服务化入口**：`hunyuan/Hunyuan3D-2.1/api_server.py`（VFX 服务调用）
>
> 目标硬件：RTX 3060 12GB；其他 NVIDIA GPU 请按显存调整参数。

---

## 硬件检查

```powershell
nvidia-smi --query-gpu=name,memory.total,driver_version --format=csv,noheader
```

| 项目 | 最低要求 | RTX 3060 12GB 参考 |
|------|---------|-------------------|
| GPU | NVIDIA GTX/RTX | RTX 3060 12GB |
| 驱动 | >= 551.78（CUDA 12.4） | 560.94（CUDA 12.6） |
| Shape 显存 | 10 GB | 12 GB |
| Texture 显存 | 8 GB | 12 GB（分阶段跑不叠加） |
| Python | 3.10 | 3.10.11 |
| 磁盘 | ~40 GB | — |

---

## 快速部署

以下路径默认在项目根目录 `D:/Playground/TA-Playground` 下执行。

### 1. 安装 Python 3.10

```powershell
# 推荐从官网下载 Python 3.10.11
# https://www.python.org/downloads/release/python-31011/
# 安装时勾选 "Add Python to PATH"

py -3.10 --version   # Python 3.10.11
```

### 2. 创建虚拟环境

```powershell
cd D:\Playground\TA-Playground\hunyuan
py -3.10 -m venv venv
```

### 3. 安装 PyTorch 2.5.1（CUDA 12.4）

> Hunyuan3D-2.1 官方在 `torch==2.5.1+cu124` 上测试。CUDA 版 PyTorch wheel 较大（~2.5 GB），网络慢时请加长超时。

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

### 4. 准备仓库

`hunyuan/Hunyuan3D-2.1` 与 `hunyuan/Hunyuan3D-2` 已随项目放置。如缺失，手动克隆：

```powershell
git clone https://github.com/Tencent-Hunyuan/Hunyuan3D-2.1.git
git clone https://github.com/Tencent-Hunyuan/Hunyuan3D-2.git
```

### 5. 安装 Python 依赖

> 关键：`transformers` 与 `diffusers` 版本必须和 `torch 2.5.1` 兼容。

```powershell
cd D:\Playground\TA-Playground\hunyuan\Hunyuan3D-2.1
pip install -r requirements.txt

cd D:\Playground\TA-Playground\hunyuan\Hunyuan3D-2
pip install -r requirements.txt

# 锁定关键版本
pip install transformers==4.46.3 diffusers==0.31.0
```

### 6. 编译 CUDA 扩展

根据你要用的入口选择编译哪一组扩展：

| 入口 | 需要的扩展 |
|------|-----------|
| `hybrid_pipeline.py`（Shape 2.1 + Paint 2.0） | `Hunyuan3D-2/hy3dgen/texgen/custom_rasterizer` 与 `differentiable_renderer` |
| `Hunyuan3D-2.1/api_server.py`（PBR / VFX 服务） | `Hunyuan3D-2.1/hy3dpaint/custom_rasterizer` 与 `Hunyuan3D-2.1/hy3dpaint/DifferentiableRenderer` |

> 需要 Visual Studio 2022（MSVC v143）。VS 2025/Insiders 可能与 CUDA 12.4 不兼容。

#### 6.1 Hunyuan3D-2 纹理扩展（hybrid_pipeline）

```powershell
# 根据 GPU 架构设置（RTX 30/40 为 8.6，其他请调整）
$env:TORCH_CUDA_ARCH_LIST = "8.6"

$vs2022 = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$vcvars = "$vs2022\VC\Auxiliary\Build\vcvarsall.bat"
$py = "$PWD\venv\Scripts\python.exe"

# custom_rasterizer
pushd Hunyuan3D-2\hy3dgen\texgen\custom_rasterizer
cmd /c "call `"$vcvars`" x64 && set DISTUTILS_USE_SDK=1 && `"$py`" -m pip install -e ."
popd

# differentiable_renderer
pushd Hunyuan3D-2\hy3dgen\texgen\differentiable_renderer
cmd /c "call `"$vcvars`" x64 && set DISTUTILS_USE_SDK=1 && `"$py`" setup.py install"
popd
```

#### 6.2 Hunyuan3D-2.1 PBR 扩展（api_server / VFX）

```powershell
$env:TORCH_CUDA_ARCH_LIST = "8.6"

# custom_rasterizer
pushd Hunyuan3D-2.1\hy3dpaint\custom_rasterizer
cmd /c "call `"$vcvars`" x64 && set DISTUTILS_USE_SDK=1 && `"$py`" -m pip install -e ."
popd

# DifferentiableRenderer（Windows 下用 Git Bash 执行 shell 脚本）
pushd Hunyuan3D-2.1\hy3dpaint\DifferentiableRenderer
bash compile_mesh_painter.sh
popd
```

### 7. HuggingFace 登录与条款

```powershell
python -c "from huggingface_hub import login; login(token='hf_xxx')"
```

并在以下页面点击 **Agree and access**：

- [tencent/Hunyuan3D-2.1](https://huggingface.co/tencent/Hunyuan3D-2.1)
- [tencent/Hunyuan3D-2](https://huggingface.co/tencent/Hunyuan3D-2)

### 8. 下载模型权重

`hunyuan/` 下提供多个预下载脚本：

| 脚本 | 用途 | 目标路径 |
|------|------|---------|
| `download_weights.py` | 下载 2.1 Shape 模型 | `~/.cache/hy3dgen/tencent/Hunyuan3D-2.1/hunyuan3d-dit-v2-1/` |
| `download_texture_weights.py` | 下载 2.0 Paint 纹理模型 | `~/.cache/hy3dgen/tencent/Hunyuan3D-2/` |
| `download_turbo.py` | 下载 turbo paint 模型并转换 `.bin` | `~/.cache/hy3dgen/tencent/Hunyuan3D-2/` |
| `download_resume.py` | 带断点续传的纹理权重下载 | `~/.cache/hy3dgen/tencent/Hunyuan3D-2/` |
| `check_status.py` | 检查纹理权重下载完整性 | — |

使用示例：

```powershell
cd D:\Playground\TA-Playground\hunyuan
.\venv\Scripts\python.exe download_weights.py
.\venv\Scripts\python.exe download_texture_weights.py
.\venv\Scripts\python.exe check_status.py
```

国内网络慢时，可改用 [ModelScope](https://www.modelscope.cn/) 或浏览器手动下载，再放到上表中的缓存路径。详见本文档「常见问题」。

---

## 目录结构

```text
hunyuan/                          # gitignored
├── venv/                         # Python 3.10 venv
├── Hunyuan3D-2.1/                # Shape / PBR 纹理 repo
│   ├── hy3dshape/                # 2.1 shape 包
│   ├── hy3dpaint/                # 2.1 PBR 纹理包
│   └── api_server.py             # FastAPI 服务入口
├── Hunyuan3D-2/                  # 2.0 RGB 纹理 repo
│   └── hy3dgen/texgen/           # 2.0 纹理包
├── hybrid_pipeline.py            # CLI：2.1 Shape + 2.0 Paint
├── vfx_generation_service.py     # UE VFX 集成封装
├── download_weights.py           # 权重预下载脚本
├── check_status.py               # 权重完整性检查
└── ...
```

权重缓存：

```text
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

## 版本兼容矩阵

| 组件 | 版本 | 显存 | RTX 3060 12GB | 备注 |
|------|------|------|:-------------:|------|
| Shape | 2.1（3.3B） | ~10 GB | OK | 推荐，效果最好 |
| Shape | 2.0（2.6B） | ~8 GB | OK | 备用 |
| Paint | 2.0 RGB（1.3B） | ~8 GB | OK | hybrid_pipeline 使用 |
| Paint | 2.1 PBR（2.0B） | ~21 GB | NO | api_server PBR，需 24GB+ |

### 已验证的关键包版本

| 包 | 版本 | 备注 |
|------|------|------|
| torch | 2.5.1+cu124 | 不建议升级 |
| transformers | 4.46.3 | 精确匹配 |
| diffusers | 0.31.0 | 精确匹配 |
| huggingface_hub | 0.36.2 | — |
| trimesh | 4.12.2 | — |

---

## 常见问题

### PyTorch 下载超时 / 极慢

- 原因：国内无 CUDA wheel 镜像缓存
- 解决：`--default-timeout 7200`，或换稳定网络下载

### 模型权重下载失败

- 原因：HuggingFace CDN 国内不稳定
- 解决：用 ModelScope、`download_resume.py`，或浏览器手动下载
- **Git Bash 不走 Windows TUN 模式 VPN**：大文件下载请在 PowerShell / CMD 中进行

### CUDA Out of Memory

- `nvidia-smi` 查看占用，关闭 Chrome / Unreal Editor 等 GPU 程序
- Shape ~10GB，纹理 ~8GB，两阶段不叠加
- 需要时先 `--shape-only` 只出白模

### `transformers` / `diffusers` 版本冲突

- 症状：`torch has no attribute float8_e8m0fnu`、`cannot import Dinov2WithRegistersConfig`
- 解决：锁定 `transformers==4.46.3 diffusers==0.31.0`

### VS 编译报错 unsupported compiler

- 症状：`fatal error C1189: unsupported Microsoft Visual Studio version`
- 原因：CUDA 12.4 不支持 VS 2025+
- 解决：用 VS 2022 的 `vcvarsall.bat` 激活环境后编译

### Python 3.13 兼容性

大量 AI 库没有 3.13 wheel，必须使用 Python 3.10。

---

## 下一步

- 使用手册（CLI、API、UE 集成）：[`hunyuan3d-usage.md`](hunyuan3d-usage.md)
- VFX / UE 集成协议：[`../Plugins/UnrealMCP/docs/vfx_methods.md`](../Plugins/UnrealMCP/docs/vfx_methods.md)
- 上游 README：[`../hunyuan/Hunyuan3D-2.1/README.md`](../hunyuan/Hunyuan3D-2.1/README.md)
- 上游 API 文档：[`../hunyuan/Hunyuan3D-2.1/API_DOCUMENTATION.md`](../hunyuan/Hunyuan3D-2.1/API_DOCUMENTATION.md)
