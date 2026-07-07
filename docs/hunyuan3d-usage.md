# Hunyuan3D-2 / 2.1 使用手册

> 本手册基于已部署的混合管线，覆盖三种使用方式：
> - `hybrid_pipeline.py`：本地 CLI，2.1 Shape + 2.0 Paint
> - `Hunyuan3D-2.1/api_server.py`：FastAPI 服务，供外部 HTTP 调用
> - `vfx_generation_service.py`：UnrealMCP VFX 工作流的 UE 集成封装
>
> 部署步骤详见 [`hunyuan3d-setup.md`](hunyuan3d-setup.md)。

---

## 前置条件

每次使用前激活环境（在项目根目录执行）：

```powershell
cd D:\Playground\TA-Playground\hunyuan
.\venv\Scripts\activate
```

确认权重已就位：

```text
~/.cache/hy3dgen/tencent/
├── Hunyuan3D-2.1/hunyuan3d-dit-v2-1/       # Shape 权重
└── Hunyuan3D-2/hunyuan3d-paint-v2-0/        # 2.0 Paint 纹理权重
```

检查脚本：

```powershell
python download_weights.py      # 下载 / 补全 2.1 Shape 权重
python check_status.py          # 检查 2.0 纹理权重完整性
```

---

## 输入图片要求

| 项目 | 建议 |
|------|------|
| 格式 | JPG、PNG |
| 主体 | 单一物体，避免多个重叠主体 |
| 姿态 | 全身或半身侧视图效果最佳 |
| 背景 | 简单背景更易去背；复杂背景可由 `rembg` 处理 |
| 光照 | 均匀自然光，避免过曝或死黑 |
| 透明图 | 已带 Alpha 通道的图片会跳过自动去背 |

---

## 方式一：hybrid_pipeline.py（推荐 CLI）

### 仅生成白模

速度快、显存占用低，首次验证推荐：

```powershell
python hybrid_pipeline.py horse_input.jpg -o horse_shape.glb --shape-only
```

### 生成带纹理的完整模型

```powershell
python hybrid_pipeline.py horse_input.jpg -o horse_textured.glb
```

完整流程分两步：

1. Shape 生成白模（约 1–3 分钟）
2. Paint 生成 RGB 贴图（约 2–5 分钟）

### 参数详解

#### Shape 质量参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--steps` | 50 | 扩散步数，越大细节越稳定，建议 50–100 |
| `--guidance` | 5.0 | 分类器自由引导尺度，建议 3.0–7.0 |
| `--octree` | 384 | 八叉树分辨率，越大网格越精细，建议 384–512 |
| `--mc-level` | 0.0 | Marching Cubes 等值面阈值，通常保持 0.0 |
| `--seed` | None | 固定随机种子，便于复现结果 |

#### Texture 参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--texture-size` | 2048 | 贴图分辨率，可选 1024 / 2048 / 4096 |
| `--turbo` | False | 使用 turbo 模型，速度更快但质量稍低 |

#### 预处理与后处理

| 参数 | 说明 |
|------|------|
| `--no-preprocess` | 跳过自动去背、居中和缩放 |
| `--input-size` | 预处理后的输入尺寸，默认 768 |
| `--smooth` | 对白模做轻度 Laplacian 平滑 |
| `--smooth-iter` | 平滑迭代次数，默认 2 |
| `--no-clean` | 不移除退化面片 |

### 推荐参数组合

#### 快速预览

```powershell
python hybrid_pipeline.py horse_input.jpg -o preview.glb `
  --steps 30 --octree 256 --texture-size 1024 --turbo
```

#### 平衡质量

```powershell
python hybrid_pipeline.py horse_input.jpg -o output.glb `
  --steps 50 --octree 384 --texture-size 2048
```

#### 最佳质量

```powershell
python hybrid_pipeline.py horse_input.jpg -o output_best.glb `
  --steps 100 --guidance 6.0 --octree 512 --texture-size 2048
```

---

## 方式二：Hunyuan3D-2.1 API Server

直接启动上游 FastAPI 服务，提供交互式文档和异步任务接口。

### 启动服务

```powershell
cd D:\Playground\TA-Playground\hunyuan\Hunyuan3D-2.1
..\venv\Scripts\python.exe api_server.py --host 0.0.0.0 --port 8081
```

交互式文档：

- Swagger UI：<http://localhost:8081/docs>
- ReDoc：<http://localhost:8081/redoc>

### 主要端点

| 端点 | 方法 | 说明 |
|------|------|------|
| `/health` | GET | 服务健康检查 |
| `/send` | POST | 提交异步生成任务，返回 `uid` |
| `/status/{uid}` | GET | 查询任务状态与结果 |
| `/generate` | POST | 同步生成 3D 模型 |

### 异步调用示例

```python
import requests, base64

with open("D:/refs/rock.png", "rb") as f:
    image_b64 = base64.b64encode(f.read()).decode()

resp = requests.post("http://localhost:8081/send", json={
    "image": image_b64,
    "texture": True,
    "remove_background": True,
    "seed": 42,
})
uid = resp.json()["uid"]

while True:
    status = requests.get(f"http://localhost:8081/status/{uid}").json()
    if status["status"] == "completed":
        model_bytes = base64.b64decode(status["model_base64"])
        open("output.glb", "wb").write(model_bytes)
        break
    elif status["status"] == "error":
        raise RuntimeError(status.get("message"))
```

---

## 方式三：vfx_generation_service.py（UE 集成）

这是 UnrealMCP VFX 工作流使用的封装：自动启动 / 停止 `api_server.py`，并把结果整理到 `hunyuan/output/<uuid>/`。

### 命令行

```powershell
cd D:\Playground\TA-Playground\hunyuan
.\venv\Scripts\python.exe vfx_generation_service.py `
  D:\refs\rock.png `
  --output-root D:\Playground\TA-Playground\hunyuan\output `
  --model-path tencent/Hunyuan3D-2.1 `
  --device cuda `
  --low-vram-mode
```

### 常用参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `image` | — | 输入参考图路径 |
| `--output-root` | `hunyuan/output` | 输出根目录 |
| `--base-url` | `http://localhost:8081` | API Server 地址 |
| `--model-path` | `tencent/Hunyuan3D-2.1` | 模型路径或 HF repo ID |
| `--device` | `cuda` | `cuda` / `cpu` |
| `--low-vram-mode` | 关闭 | 低显存模式 |
| `--seed` | `1234` | 随机种子 |
| `--octree-resolution` | `256` | 网格分辨率 |
| `--num-inference-steps` | `5` | 推理步数 |
| `--guidance-scale` | `5.0` | 引导强度 |
| `--timeout` | `600` | 最大等待时间（秒） |
| `--no-auto-start` | 关闭 | 不自动启动 API Server |

### Python API

```python
from vfx_generation_service import VfxGenerationService

service = VfxGenerationService(
    base_url="http://localhost:8081",
    server_kwargs={
        "model_path": "tencent/Hunyuan3D-2.1",
        "device": "cuda",
        "low_vram_mode": True,
    },
    timeout=600.0,
)

result = service.generate_from_image(
    image_path="D:/refs/rock.png",
    params={"seed": 42, "num_inference_steps": 5},
)

print(result["mesh_path"])      # .../output/<uuid>/mesh.glb
print(result["preview_path"])   # .../output/<uuid>/mesh.png
print(result["meta_path"])      # .../output/<uuid>/meta.json
```

### 输出目录约定

```text
hunyuan/output/<uuid>/
├── mesh.glb      # 带纹理的 3D 模型
├── mesh.png      # 贴图预览
└── meta.json     # 生成元数据（seed、steps、prompt 等）
```

---

## 导入到 Unreal Engine

1. 将 `.glb` 拖入 Content Browser
2. 选择 `Import`
3. 在导入设置中启用需要的选项（如 `Transform Vertex Colors to RGB`）
4. 点击 `Import All`

或者通过 UnrealMCP 的 `generate_and_import_3d` 工具直接由 AI 调用，详见 [`../Plugins/UnrealMCP/docs/vfx_methods.md`](../Plugins/UnrealMCP/docs/vfx_methods.md)。

---

## 相关脚本

| 脚本 | 用途 |
|------|------|
| `hybrid_pipeline.py` | 主入口：图片生成 3D 模型（2.1 Shape + 2.0 Paint） |
| `vfx_generation_service.py` | UE VFX 集成封装，管理 API Server 子进程 |
| `prep_input.py` | 单独预处理图片：去背、居中、缩放 |
| `download_weights.py` | 下载 / 补全 2.1 Shape 权重 |
| `download_texture_weights.py` | 下载 2.0 Paint 纹理权重 |
| `check_status.py` | 检查 2.0 纹理权重完整性 |
| `inspect_glb.py` | 检查 GLB 文件结构 |

---

## 工作流总结

```text
输入图片 (JPG/PNG)
    │
    ▼
hybrid_pipeline.py / vfx_generation_service.py 预处理
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

## 常见问题

### 显存不足

- 关闭 Chrome、Unreal Editor 等占用显存的程序
- 先用 `--shape-only` 验证 Shape 阶段
- 降低 `--octree` 到 256 或 `--texture-size` 到 1024
- Shape 和 Paint 阶段不叠加，确保前一阶段完成后释放显存

### 生成结果模糊或缺失细节

- 提高 `--steps` 到 75–100
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
