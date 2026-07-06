# TA-Playground 工具安装与使用指南

> 本文档汇总 TA-Playground 项目中 AI 音频、Hunyuan3D 生成与 UnrealMCP 插件的安装和使用步骤。内容整理自项目 README、AGENTS.md、UnrealMCP 文档以及 Hunyuan 子仓库说明，按实际操作顺序排列。

---

## 1. 项目总览

TA-Playground 是一个基于 Unreal Engine 5 的技术美术实验项目，核心目标是通过 AI 驱动的工作流自动完成材质搭建、场景配置、视觉特效原型验证等任务。

主要工具：

| 工具 | 路径 | 用途 |
| --- | --- | --- |
| **UnrealMCP 插件** | `Plugins/UnrealMCP/` | 让 AI 助手通过 MCP 协议控制 Unreal Editor |
| **AI 音频服务** | `Plugins/UnrealMCP/audio_server/` | 本地 FastAPI 服务，封装 ACE-Step / Stable Audio Open / MMAudio |
| **Hunyuan3D 服务** | `hunyuan/vfx_generation_service.py` | 图生 3D 模型，输出 GLB + 贴图预览 |
| **技能库** | `SkillHub/` | AI Agent / Skill 定义（`technical-artist`、`audio-designer` 等） |

项目根目录结构简介：

```text
TA-Playground/
├── Config/                 # UE 项目配置
├── Content/                # UE 内容资产（材质、贴图、关卡、蓝图）
├── Plugins/
│   └── UnrealMCP/          # MCP 插件 + Rust Server + 音频服务
├── Source/                 # C++ 源码
├── SkillHub/               # 技能库（git submodule）
├── hunyuan/                # Hunyuan3D-2 / 2.1 子仓库 + 服务封装
├── scripts/                # UE 辅助脚本
├── weights/                # 本地模型权重缓存（gitignore）
├── TA-Playground.uproject  # UE 项目文件
└── README.md
```

---

## 2. 通用前置依赖

### 2.1 必需软件

| 软件 | 用途 | 推荐版本 | 检查命令 |
| --- | --- | --- | --- |
| Python | 运行 AI 模型与服务 | 3.10 | `python --version` |
| Git | 克隆仓库 | 最新 | `git --version` |
| Git LFS | 下载大文件权重 | 最新 | `git lfs version` |
| FFmpeg | 音频/视频编解码 | 4.x / 5.x | `ffmpeg -version` |
| CUDA Toolkit | NVIDIA GPU 加速 | 11.8 / 12.x | `nvidia-smi` |
| Rust | 编译 MCP Server | 1.80+ | `rustc --version` |
| Unreal Engine | 编辑器与运行时 | 5.4+（推荐 5.5/5.7） | — |

### 2.2 PowerShell 执行策略

项目提供的脚本为 `.ps1`，如果系统默认禁止执行脚本，先以管理员身份运行：

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

单次绕过策略运行脚本：

```powershell
powershell -ExecutionPolicy Bypass -File Plugins\UnrealMCP\scripts\install-audio-tools.ps1
```

### 2.3 硬件参考

| 工作流 | 最小显存 | 推荐显存 |
| --- | --- | --- |
| UnrealMCP 编辑器控制 | 无特殊要求 | 8 GB+ |
| ACE-Step 音乐生成 | 8 GB（开 CPU offload） | 12 GB+ |
| Stable Audio Open | 8 GB（Small） | 16 GB（Medium） |
| MMAudio Foley | 8 GB | 12 GB+ |
| Hunyuan3D-2.1 图生 3D | 10 GB（仅形状） | 29 GB（形状 + 纹理） |

---

## 3. UnrealMCP 插件

### 3.1 插件位置

`Plugins/UnrealMCP/` 已包含在项目内，无需手动复制。子目录说明：

```text
Plugins/UnrealMCP/
├── MCP_Server/             # Rust MCP Server
├── UnrealPlugin/           # UE C++ 插件源码
├── audio_server/           # AI 音频 FastAPI 服务
├── docs/                   # 插件文档
└── scripts/                # 安装/导入脚本
```

### 3.2 构建 Rust MCP Server

```powershell
cd Plugins\UnrealMCP\MCP_Server
cargo build --release
```

构建产物位于 `Plugins/UnrealMCP/MCP_Server/target/release/`。

### 3.3 生成并编译 UE 项目

```powershell
# 1. 生成 Visual Studio 工程文件
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" `
  -projectfiles -project="D:\Playground\TA-Playground\TA-Playground.uproject" -game -rocket

# 2. 编译 Editor 目标（包含 UnrealMCP C++ 插件）
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" `
  TAPlaygroundEditor Win64 Development -Project="D:\Playground\TA-Playground\TA-Playground.uproject"
```

> 将路径中的 `UE_5.7` 替换为你实际安装的引擎版本。

### 3.4 启动与连接

1. 启动 Unreal Editor：

   ```powershell
   "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" `
     "D:\Playground\TA-Playground\TA-Playground.uproject"
   ```

2. 插件会自动在 `127.0.0.1:13377` 启动 TCP Server。

3. 在 AI 客户端（Claude Desktop / Cursor）中配置 MCP Server：

   ```json
   {
     "mcpServers": {
       "unreal": {
         "command": "D:\\Playground\\TA-Playground\\Plugins\\UnrealMCP\\MCP_Server\\target\\release\\unreal-mcp-server.exe"
       }
     }
   }
   ```

### 3.5 基础验证

运行项目自带的 Jade 材质流水线验证脚本：

```powershell
python scripts\jade_nodialog.py
```

预期结果：自动生成球体、应用 Jade PBR 材质、搭建三点光照并聚焦视口，15/15 命令无弹窗。

---

## 4. AI 音频工具

AI 音频服务由 `Plugins/UnrealMCP/audio_server/main.py` 提供，封装了三个开源模型：

| Endpoint | 模型 | 用途 |
| --- | --- | --- |
| `/generate/music` | ACE-Step | 音乐 / BGM / 主题曲 |
| `/generate/sfx` | Stable Audio Open | 音效 / 环境声 / 短循环 |
| `/generate/foley` | MMAudio | 视频/录屏 Foley 配音 |

### 4.1 一键安装（推荐）

```powershell
powershell -ExecutionPolicy Bypass -File Plugins\UnrealMCP\scripts\install-audio-tools.ps1
```

脚本会自动完成：

- 检测 Python 3.10、Git、FFmpeg、NVIDIA GPU
- 创建 `.venv` 或 conda 环境
- 安装 CUDA 12.6 版 PyTorch
- 克隆并安装 ACE-Step、Stable Audio Open、MMAudio
- 生成 `audio_server/.env`

### 4.2 手动安装环境

```powershell
conda create -n ai-audio python=3.10 -y
conda activate ai-audio

# 根据你的驱动选择 CUDA 版本
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu126

# 安装音频服务依赖
pip install -r Plugins\UnrealMCP\audio_server\requirements.txt
```

### 4.3 各模型安装要点

#### ACE-Step

```powershell
git clone https://github.com/ace-step/ACE-Step.git
cd ACE-Step
pip install -e .
```

验证：

```powershell
acestep --port 7865
```

首次运行会从 HuggingFace 自动下载权重到 `~/.cache/ace-step/checkpoints`。

#### Stable Audio Open

```powershell
git clone https://github.com/Stability-AI/stable-audio-tools.git
cd stable-audio-tools
pip install -e ".[train,ui]"

# 可选但推荐
pip install flash-attn --no-build-isolation

# 登录 HuggingFace 并接受模型条款
pip install huggingface-hub
huggingface-cli login
```

然后访问 <https://huggingface.co/stabilityai/stable-audio-open-1.0> 点击 Accept。

验证：

```powershell
python run_gradio.py --pretrained-name stabilityai/stable-audio-open-1.0
```

#### MMAudio

```powershell
git clone https://github.com/hkchengrex/MMAudio.git
cd MMAudio
pip install -r requirements.txt
```

### 4.4 模型权重目录约定

MMAudio 默认需要以下文件（默认使用 `medium_44k` 或 `large_44k_v2`，以 `config.yaml` 为准）：

```text
D:/Playground/TA-Playground/weights/
├── mmaudio_medium_44k.pth              # 或 mmaudio_large_44k_v2.pth
├── open_clip_pytorch_model.bin         # CLIP，约 3.9 GB
└── nvidia_bigvgan_v2_44khz_128band_512x/
│   ├── config.json
│   └── bigvgan_generator.pt            # BigVGAN 声码器，约 466 MB

Plugins/UnrealMCP/audio_server/ext_weights/
├── v1-44.pth                           # 44.1 kHz VAE，约 1.2 GB
└── synchformer_state_dict.pth          # 视觉同步编码器，约 950 MB
```

`main.py` 启动时会自动检测 `weights/open_clip_pytorch_model.bin` 和 `weights/nvidia_bigvgan_v2_44khz_128band_512x/`，并通过环境变量让 MMAudio 跳过 HuggingFace 在线下载。

### 4.5 本地权重环境变量

在 `Plugins/UnrealMCP/audio_server/.env` 中可配置：

```powershell
# 覆盖默认模型路径
ACE_STEP_CHECKPOINT_PATH=D:/AudioWeights/ACE-Step-v1-3.5B
STABLE_AUDIO_PRETRAINED_NAME=D:/AudioWeights/stable-audio-open-1.0
MMAUDIO_MODEL_NAME=medium_44k

# 让 MMAudio 离线加载 CLIP / BigVGAN
MMAUDIO_CLIP_PATH=D:/Playground/TA-Playground/weights/open_clip_pytorch_model.bin
BIGVGAN_LOCAL_DIR=D:/Playground/TA-Playground/weights/nvidia_bigvgan_v2_44khz_128band_512x
MMAUDIO_WEIGHTS_DIR=D:/Playground/TA-Playground/weights

# 服务监听配置
AUDIO_SERVER_HOST=127.0.0.1
AUDIO_SERVER_PORT=8123
AUDIO_SERVER_DEVICE=auto
```

### 4.6 断点续传下载脚本

如果 HuggingFace 可以连接但速度慢、容易中断，使用项目自带的续传脚本：

```powershell
$python = "D:\Playground\TA-Playground\Plugins\UnrealMCP\.venv\Scripts\python.exe"

& $python D:\Playground\TA-Playground\weights\_download_clip.py
& $python D:\Playground\TA-Playground\weights\_download_bigvgan.py
& $python D:\Playground\TA-Playground\weights\_download_progress.py
```

特性：

- 使用 HTTP `Range` 断点续传
- 最多 50 次重试，指数退避
- 自动写入项目根目录 `weights/`
- `_download_progress.py` 显示当前大小、总大小、完成百分比和日志尾部

### 4.7 启动音频服务

```powershell
cd Plugins\UnrealMCP\audio_server
python main.py
```

默认监听 `http://127.0.0.1:8123`。模型会在第一次请求时懒加载。

### 4.8 HTTP 调用示例

#### Health check

```powershell
curl -X POST http://127.0.0.1:8123/health
```

#### 生成音乐

```powershell
curl -X POST http://127.0.0.1:8123/generate/music `
  -H "Content-Type: application/json" `
  -d '{"prompt":"cinematic orchestral, tense, strings and brass, 110 BPM, instrumental","duration_seconds":30}'
```

#### 生成音效

```powershell
curl -X POST http://127.0.0.1:8123/generate/sfx `
  -H "Content-Type: application/json" `
  -d '{"prompt":"heavy footsteps on wet concrete, close-miked","duration_seconds":4}'
```

#### 生成 Foley（文本驱动）

```powershell
curl -X POST http://127.0.0.1:8123/generate/foley `
  -H "Content-Type: application/json" `
  -d '{"prompt":"coffee shop ambiance with gentle chatter and espresso machine","duration_seconds":8}'
```

#### 生成 Foley（视频驱动）

```powershell
curl -X POST http://127.0.0.1:8123/generate/foley `
  -H "Content-Type: application/json" `
  -d '{"video_path":"D:/videos/gameplay.mp4","prompt":"footsteps on gravel and cloth rustling","duration_seconds":8}'
```

返回 JSON 包含 `audio_base64` 和本地 `file_path`，可在 UE 中用 Runtime Audio Importer 等插件转为 `USoundWave`。

---

## 5. Hunyuan3D-2 / 2.1

项目通过 `hunyuan/vfx_generation_service.py` 封装 Hunyuan3D-2.1 的 `api_server.py`，提供图生 3D 服务。

### 5.1 环境创建与依赖安装

在 `hunyuan/` 目录下创建 Python 3.10 虚拟环境：

```powershell
cd D:\Playground\TA-Playground\hunyuan
python -m venv venv
.\venv\Scripts\Activate.ps1
```

安装 PyTorch（以 CUDA 12.4 为例，Hunyuan3D-2.1 官方测试环境）：

```powershell
pip install torch==2.5.1 torchvision==0.20.1 torchaudio==2.5.1 --index-url https://download.pytorch.org/whl/cu124
```

安装 Hunyuan3D-2.1 依赖和自定义扩展：

```powershell
cd Hunyuan3D-2.1
pip install -r requirements.txt

# 安装光栅化与可微渲染扩展
cd hy3dpaint\custom_rasterizer
pip install -e .
cd ..\..
cd hy3dpaint\DifferentiableRenderer
bash compile_mesh_painter.sh
cd ..\..

# 下载 Real-ESRGAN 放大模型（纹理增强用）
wget https://github.com/xinntao/Real-ESRGAN/releases/download/v0.1.0/RealESRGAN_x4plus.pth -P hy3dpaint\ckpt
```

> Windows 下执行 `bash` 命令需要 Git Bash 或 WSL。

### 5.2 权重下载脚本

`hunyuan/` 下提供多个预下载脚本，按需求选择：

| 脚本 | 用途 | 目标路径 |
| --- | --- | --- |
| `download_weights.py` | 下载 Hunyuan3D-2.1 形状模型 | `~/.cache/hy3dgen/tencent/Hunyuan3D-2.1/hunyuan3d-dit-v2-1/` |
| `download_texture_weights.py` | 下载 2.0 Paint + Delight 纹理模型 | `~/.cache/hy3dgen/tencent/Hunyuan3D-2/` |
| `download_turbo.py` | 下载 turbo paint 模型并转换 `.bin` | `~/.cache/hy3dgen/tencent/Hunyuan3D-2/hunyuan3d-paint-v2-0-turbo/` |
| `download_resume.py` | 带断点续传的纹理权重下载 | `~/.cache/hy3dgen/tencent/Hunyuan3D-2/` |
| `check_status.py` | 检查纹理权重下载完整性 | — |

使用示例：

```powershell
cd D:\Playground\TA-Playground\hunyuan
.\venv\Scripts\python.exe download_weights.py
.\venv\Scripts\python.exe download_texture_weights.py
.\venv\Scripts\python.exe check_status.py
```

### 5.3 启动 VFX 生成服务

#### 命令行方式

```powershell
cd D:\Playground\TA-Playground\hunyuan
.\venv\Scripts\python.exe vfx_generation_service.py `
  D:\refs\rock.png `
  --output-root D:\Playground\TA-Playground\hunyuan\output `
  --model-path tencent/Hunyuan3D-2.1 `
  --device cuda `
  --low-vram-mode
```

参数说明：

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
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

#### Python API 方式

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

### 5.4 输出目录约定

每次生成会写入：

```text
hunyuan/output/<uuid>/
├── mesh.glb      # 带纹理的 3D 模型
├── mesh.png      # 贴图预览（从 GLB 提取或占位图）
└── meta.json     # 生成元数据（seed、steps、prompt 等）
```

### 5.5 直接调用 Hunyuan3D-2.1 API Server

也可以手动启动 `Hunyuan3D-2.1/api_server.py`：

```powershell
cd D:\Playground\TA-Playground\hunyuan\Hunyuan3D-2.1
..\venv\Scripts\python.exe api_server.py --host 0.0.0.0 --port 8081
```

交互式文档：

- Swagger UI: <http://localhost:8081/docs>
- ReDoc: <http://localhost:8081/redoc>

主要端点：

| 端点 | 方法 | 说明 |
| --- | --- | --- |
| `/health` | GET | 服务健康检查 |
| `/send` | POST | 提交异步生成任务，返回 `uid` |
| `/status/{uid}` | GET | 查询任务状态与结果 |
| `/generate` | POST | 同步生成 3D 模型 |

异步调用示例：

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

## 6. VFX / UE 集成工作流

UnrealMCP 为 VFX 工作流新增了 `generate_and_import_3d` 等工具，详细协议见 `Plugins/UnrealMCP/docs/vfx_methods.md`。

### 6.1 `generate_and_import_3d` 工具

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `meshFile` | string | 条件 | 已生成的 GLB/OBJ/FBX 绝对路径。与 `prompt` 二选一 |
| `prompt` | string | 条件 | 文本提示，驱动 Hunyuan3D 生成 |
| `referenceImage` | string | 条件 | 参考图绝对路径，与 `prompt` 同时提供时优先按图生成 |
| `destinationPath` | string | ✅ | UE 导入目标路径，如 `/Game/Generated/Meshes` |
| `actorName` | string | — | 生成后 StaticMeshActor 名称 |
| `location` | `[f64;3]` | — | Actor 位置，默认 `[0,0,0]` |
| `rotation` | `[f64;3]` | — | Actor 旋转，默认 `[0,0,0]` |
| `scale` | `[f64;3]` | — | Actor 缩放，默认 `[1,1,1]` |
| `generationParams` | object | — | `steps`、`seed`、`guidanceScale`、`turbo`、`outputDir` |
| `waitForCompletion` | bool | — | 是否阻塞等待生成完成，默认 `false` |

### 6.2 异步流程

1. UE 插件收到 `generate_and_import_3d` 请求。
2. 如果需要生成，C++ 启动 `hunyuan/vfx_generation_service.py` 子进程。
3. 子进程启动 `Hunyuan3D-2.1/api_server.py` 并提交 `/send` 任务。
4. 立即返回 `jobId` 和 `status: pending`。
5. 通过 `get_generate_and_import_3d_status` 轮询任务状态，直到 `completed` 或 `failed`。
6. 生成完成后，C++ 导入 `hunyuan/output/<uuid>/mesh.glb` 到 `destinationPath` 并生成 Actor。

### 6.3 状态查询响应

```json
{
  "id": "req-6",
  "success": true,
  "result": {
    "jobId": "gen-3d-...",
    "status": "running",
    "progress": 0.45,
    "message": "Generating mesh...",
    "assetPath": null,
    "actorName": null
  }
}
```

状态取值：`pending` | `running` | `completed` | `failed`。

### 6.4 相关辅助工具

| 工具 | 用途 |
| --- | --- |
| `create_material_from_textures` | 根据贴图集合创建材质实例并连接母材参数 |
| `set_texture_parameter` | 修改 Actor 材质实例的 Texture 参数 |
| `duplicate_niagara_system` | 基于 Niagara 模板复制新系统并可生成 Actor |
| `set_niagara_parameter` | 修改 Niagara User Parameter |

---

## 7. 快速验证清单

| 步骤 | 验证命令/操作 | 成功标志 |
| --- | --- | --- |
| Python 版本 | `python --version` | `3.10.x` |
| GPU 可用 | `nvidia-smi` | 显示 NVIDIA GPU 与驱动 |
| Rust 编译 | `cd Plugins\UnrealMCP\MCP_Server && cargo build --release` | 生成 `.exe` |
| UE 工程生成 | UnrealBuildTool `-projectfiles` | 生成 `.sln`/`.vcxproj` |
| UE Editor 编译 | UnrealBuildTool `TAPlaygroundEditor Win64 Development` | 编译成功 |
| UE 插件监听 | 启动 Editor 后检查日志 | 出现 `127.0.0.1:13377` 监听 |
| Jade 流水线 | `python scripts\jade_nodialog.py` | 15/15 命令成功，无弹窗 |
| 音频服务启动 | `python Plugins\UnrealMCP\audio_server\main.py` | 监听 `127.0.0.1:8123` |
| 音频 health | `curl -X POST http://127.0.0.1:8123/health` | 返回 JSON，状态正常 |
| ACE-Step | `acestep --port 7865` | Gradio 可生成 30 秒音乐 |
| Stable Audio Open | `python run_gradio.py --pretrained-name stabilityai/stable-audio-open-1.0` | 可生成 5 秒音效 |
| MMAudio | `curl /generate/foley` | 返回 `audio_base64` |
| Hunyuan 权重 | `python hunyuan\download_weights.py` | 输出 `DONE` |
| Hunyuan API | `python hunyuan\Hunyuan3D-2.1\api_server.py` | `/health` 返回 200 |
| VFX 服务 | `python hunyuan\vfx_generation_service.py ref.png` | 输出 `mesh.glb` + `mesh.png` |

---

## 8. 常见问题与排错

| 问题 | 可能原因 | 解决办法 |
| --- | --- | --- |
| `CUDA out of memory` | 显存不足 | 开启 `--low-vram-mode` / `--cpu_offload`、降低 `duration_seconds`、使用量化模型 |
| HuggingFace 下载慢/超时 | 网络到 `huggingface.co` 不稳定 | 改用 ModelScope：`modelscope download --local-dir ./weights <repo>`；或使用 `weights/_download_clip.py` / `_download_bigvgan.py` |
| `hf-mirror.com` 无效 | 镜像会 308 跳转回官方 | 换 ModelScope 或预下载 |
| Stable Audio Open 下载被拒 | 未接受条款 | 访问模型页面点击 Accept，并运行 `huggingface-cli login` |
| MMAudio 提示缺少权重 | CLIP / BigVGAN 未预下载 | 放到 `weights/` 并设置 `MMAUDIO_CLIP_PATH`、`BIGVGAN_LOCAL_DIR`、`MMAUDIO_WEIGHTS_DIR` |
| PyTorch 变成 CPU 版本 | 某个模型包降级了 torch | 强制重装 CUDA 版：`pip install --force-reinstall torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu126` |
| ACE-Step 首次启动慢 | 自动下载权重 | 等待完成，或预下载后放到 `~/.cache/ace-step/checkpoints` |
| `flash-attn` 编译失败 | CUDA/PyTorch 版本不匹配 | 跳过 Flash Attention，模型仍可用 |
| PowerShell 脚本无法执行 | 执行策略限制 | `Set-ExecutionPolicy RemoteSigned -Scope CurrentUser` 或加 `-ExecutionPolicy Bypass` |
| 音频服务端口被占用 | 8123 已被占用 | 在 `.env` 中设置 `AUDIO_SERVER_PORT=8124` |
| Hunyuan 子进程启动失败 | venv 未激活 / api_server.py 路径错 | 确认 `hunyuan/venv/Scripts/python.exe` 存在，`Hunyuan3D-2.1/api_server.py` 路径正确 |
| VFX 生成任务 `failed` | Hunyuan API 返回 error | 查看 `hunyuan/output/<uuid>/` 是否有日志，检查 `api_server.py` 控制台输出 |
| UE 导入 WAV 失败 | 格式不支持 | 确保为 16/24 bit WAV，采样率 44.1 kHz 或 48 kHz |
| UE 导入 GLB 失败 | 路径或格式问题 | 确认 `mesh.glb` 存在且路径为 Windows 绝对路径 |

---

## 9. 相关文档索引

- 项目总览：`README.md`
- Agent / Skill 说明：`AGENTS.md`
- UnrealMCP 插件说明：`Plugins/UnrealMCP/README.md`
- 音频工具安装详情：`Plugins/UnrealMCP/docs/audio-tools-installation.md`
- 音频权重下载详情：`Plugins/UnrealMCP/audio_server/MODEL_DOWNLOAD_GUIDE.md`
- 音频服务使用：`Plugins/UnrealMCP/audio_server/README.md`
- VFX 接口协议：`Plugins/UnrealMCP/docs/vfx_methods.md`
- Hunyuan3D-2.1 安装与 API：`hunyuan/Hunyuan3D-2.1/README.md`、`hunyuan/Hunyuan3D-2.1/API_DOCUMENTATION.md`
