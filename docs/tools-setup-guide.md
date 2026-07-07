# TA-Playground 工具安装总览与快速开始

> 本文档是项目工具链的**顶层索引 + 快速开始**，不重复子模块的详细说明。遇到具体问题时，请跟随链接进入对应专题文档。
>
> 覆盖的工作流：
> - UnrealMCP 插件（AI 控制 UE 编辑器）
> - Jade / 玉石材质原型
> - Hunyuan3D-2 / 2.1 图生 3D
> - AI 音频生成（ACE-Step / Stable Audio Open / MMAudio）
> - AI 动画原型（关键帧驱动 Actor / 相机）

---

## 1. 项目目标与架构

TA-Playground 是一个基于 Unreal Engine 5 的技术美术实验项目，核心目标是让 AI 助手通过 **MCP（Model Context Protocol）** 直接驱动编辑器，完成材质搭建、3D 资产生成、音频生成、动画验证等任务。

```text
AI 客户端（Claude / Cursor / Kimi Code CLI）
        │
        ▼
Rust MCP Server  (Plugins/UnrealMCP/MCP_Server)
        │  TCP 127.0.0.1:13377
        ▼
Unreal Editor  +  UnrealMCP C++ 插件
        │
        ├── Jade 材质流水线
        ├── Hunyuan3D 生成服务（hunyuan/vfx_generation_service.py）
        ├── AI 音频 FastAPI 服务（Plugins/UnrealMCP/audio_server/main.py）
        └── AI 动画原型（scripts/ai_animation_prototype.py）
```

主要工作流：

| 工作流 | 入口 | 说明 |
|--------|------|------|
| **Jade 材质** | `scripts/jade_nodialog.py` | 自动生成球体、应用玉石 PBR 材质、三点光照 |
| **Hunyuan3D** | `hunyuan/hybrid_pipeline.py` / `hunyuan/vfx_generation_service.py` | 图生 3D 模型（GLB + 贴图） |
| **AI 音频** | `Plugins/UnrealMCP/audio_server/main.py` | 本地 FastAPI 服务，生成音乐 / 音效 / Foley |
| **AI 动画** | `scripts/ai_animation_prototype.py` | 按 JSON keyframe 驱动 Actor 与相机 |

---

## 2. 通用前置依赖

### 2.1 必需软件

| 软件 | 用途 | 推荐版本 | 检查命令 |
|------|------|----------|----------|
| Python | 运行 AI 模型与服务 | 3.10 | `python --version` |
| Git | 克隆仓库 | 最新 | `git --version` |
| Git LFS | 下载大文件权重 | 最新 | `git lfs version` |
| FFmpeg | 音频/视频编解码 | 4.x / 5.x | `ffmpeg -version` |
| CUDA Toolkit | NVIDIA GPU 加速 | 11.8 / 12.x | `nvidia-smi` |
| Rust | 编译 MCP Server | 1.80+ | `rustc --version` |
| Unreal Engine | 编辑器与运行时 | 5.4+（推荐 5.5/5.7） | — |
| Visual Studio 2022 | 编译 UE 插件与 CUDA 扩展 | MSVC v143 | — |

### 2.2 PowerShell 执行策略

项目脚本多为 `.ps1`。若系统默认禁止执行，以管理员身份运行：

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

单次绕过：

```powershell
powershell -ExecutionPolicy Bypass -File Plugins\UnrealMCP\scripts\install-audio-tools.ps1
```

### 2.3 硬件 / 显存参考

| 工作流 | 最小显存 | 推荐显存 |
|--------|----------|----------|
| UnrealMCP 编辑器控制 | 无特殊要求 | 8 GB+ |
| ACE-Step 音乐生成 | 8 GB（CPU offload） | 12 GB+ |
| Stable Audio Open | 8 GB（Small） | 16 GB（Medium） |
| MMAudio Foley | 8 GB | 12 GB+ |
| Hunyuan3D-2.1 Shape | 10 GB | 12 GB+ |
| Hunyuan3D-2.1 Shape + PBR Texture | 24 GB | 29 GB |
| Hunyuan3D-2.1 Shape + 2.0 Paint | 10 GB（分阶段） | 12 GB+ |

---

## 3. UnrealMCP 插件

`Plugins/UnrealMCP/` 是完整插件目录，已包含 Rust MCP Server、C++ 插件源码、音频服务与文档。

### 3.1 目录速览

```text
Plugins/UnrealMCP/
├── MCP_Server/             # Rust MCP Server
├── UnrealPlugin/           # UE C++ 插件源码
├── audio_server/           # AI 音频 FastAPI 服务
├── docs/                   # 插件详细文档
└── scripts/                # 安装/导入脚本
```

### 3.2 构建 Rust MCP Server

```powershell
cd Plugins\UnrealMCP\MCP_Server
cargo build --release
```

构建产物：

```text
Plugins/UnrealMCP/MCP_Server/target/release/unreal-mcp-server.exe
```

### 3.3 生成并编译 UE 工程

替换路径中的 `UE_5.7` 为你实际安装的引擎版本。

```powershell
# 1. 生成 Visual Studio 工程文件
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" `
  -projectfiles -project="D:\Playground\TA-Playground\TA-Playground.uproject" -game -rocket

# 2. 编译 Editor 目标
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" `
  TAPlaygroundEditor Win64 Development -Project="D:\Playground\TA-Playground\TA-Playground.uproject"
```

### 3.4 端口配置与连接

在 `Config/DefaultEngine.ini` 中添加：

```ini
[UnrealMCP]
CommandServerPort=13377
JsonRpcServerPort=13379
```

项目根目录 `.mcp.json` 示例：

```json
{
  "mcpServers": {
    "unreal": {
      "command": "D:\\Playground\\TA-Playground\\Plugins\\UnrealMCP\\MCP_Server\\target\\release\\unreal-mcp-server.exe",
      "env": {
        "UNREAL_MCP_ADDR": "127.0.0.1:13377"
      }
    }
  }
}
```

> `Plugins/UnrealMCP/docs/usage-guide.md` 目前使用 `ClanSimulator` 示例路径，阅读时请把路径替换为 `D:/Playground/TA-Playground`。

启动 Editor 后，在 Output Log 中搜索 `LogUnrealMCP`，应看到：

```text
LogUnrealMCP: MCP Command Server started on port 13377
LogUnrealMCP: MCP JSON-RPC Server started on port 13379
```

### 3.5 快速验证：Jade 材质流水线

```powershell
python scripts\jade_nodialog.py
```

预期：自动生成球体、应用 Jade PBR 材质、搭建三点光照并聚焦视口，命令无弹窗。

### 3.6 详细文档

- 插件说明与完整 API：[`Plugins/UnrealMCP/README.md`](../Plugins/UnrealMCP/README.md)
- 使用与排坑（含端口、路径配置）：[`Plugins/UnrealMCP/docs/usage-guide.md`](../Plugins/UnrealMCP/docs/usage-guide.md)
- 音频 UE 集成：[`Plugins/UnrealMCP/docs/audio-ue-integration.md`](../Plugins/UnrealMCP/docs/audio-ue-integration.md)
- VFX 接口协议：[`Plugins/UnrealMCP/docs/vfx_methods.md`](../Plugins/UnrealMCP/docs/vfx_methods.md)

---

## 4. AI 音频工具

本地 FastAPI 服务 `Plugins/UnrealMCP/audio_server/main.py` 封装了三个开源模型：

| 端点 | 模型 | 用途 |
|------|------|------|
| `POST /generate/music` | ACE-Step | 音乐 / BGM / 主题曲 |
| `POST /generate/sfx` | Stable Audio Open | 音效 / 环境声 / 短循环 |
| `POST /generate/foley` | MMAudio | 视频/录屏 Foley 配音 |

### 4.1 一键安装

```powershell
powershell -ExecutionPolicy Bypass -File Plugins\UnrealMCP\scripts\install-audio-tools.ps1
```

脚本会完成：检测 Python 3.10 / Git / FFmpeg / GPU、创建 venv、安装 CUDA 12.6 版 PyTorch、克隆并安装三个模型、生成 `audio_server/.env`。

### 4.2 启动服务

```powershell
cd Plugins\UnrealMCP\audio_server
python main.py
```

默认监听 `http://127.0.0.1:8123`。模型在第一次请求时懒加载。

#### 4.2.1 共享 FFmpeg 库（Windows 必需）

`/generate/music` 依赖的 `torchaudio` → `torchcodec` 需要在运行时加载 `avcodec-*.dll`、`avformat-*.dll`、`avutil-*.dll` 等 FFmpeg 共享库，仅有一个静态 `ffmpeg.exe` 不够。使用 BtbN 的 **shared** Windows 构建：

- 下载：`https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip`
- 解压到 `tools/ffmpeg/bin/`，该目录下应出现 `avcodec-*.dll`、`avformat-*.dll`、`avutil-*.dll` 等。
- 启动服务前必须将 `tools/ffmpeg/bin` 加入 `PATH`：

```powershell
$env:PATH = "D:\Playground\TA-Playground\tools\ffmpeg\bin;$env:PATH"
cd Plugins\UnrealMCP\audio_server
python main.py
```

若缺少共享 FFmpeg，`/generate/music` 会报错：找不到 `libtorchcodec_core*.dll` / FFmpeg not found。

#### 4.2.2 ACE-Step Windows 补丁

当前组合 `accelerate==1.6` + `transformers==4.50` 下，ACE-Step 在 Windows 加载会触发 `Cannot copy out of meta tensor; no data!`，且新版 `torchaudio` 会忽略 `backend="soundfile"` 参数并走 `torchcodec` 路径。需要对 ACE-Step 源码做两处修改：

- 在 `acestep/pipeline_ace_step.py` 中，给 transformer 与 text encoder 的 `from_pretrained` 传入 `low_cpu_mem_usage=False`。
- 在 `acestep/music_dcae/music_dcae_pipeline.py` 中，给 DCAE 与 vocoder 的 `from_pretrained` 传入 `low_cpu_mem_usage=False`。
- 在 `save_wav_file` 中，把 `torchaudio.save(..., backend="soundfile")` 替换为直接调用 `soundfile.write`。

已提供的补丁文件：

```text
Plugins/UnrealMCP/scripts/patches/ace-step-low-cpu-mem-and-soundfile.patch
```

应用方式：

```powershell
cd Plugins\UnrealMCP\third_party\ACE-Step
git apply ..\..\scripts\patches\ace-step-low-cpu-mem-and-soundfile.patch
```

如果补丁文件尚未同步到仓库，可按上述三处手动修改。

### 4.3 HTTP 调用示例

Health check：

```powershell
curl -X POST http://127.0.0.1:8123/health
```

生成音乐：

```powershell
curl -X POST http://127.0.0.1:8123/generate/music `
  -H "Content-Type: application/json" `
  -d '{"prompt":"cinematic orchestral, tense, strings and brass, 110 BPM, instrumental","duration_seconds":30}'
```

生成音效：

```powershell
curl -X POST http://127.0.0.1:8123/generate/sfx `
  -H "Content-Type: application/json" `
  -d '{"prompt":"heavy footsteps on wet concrete, close-miked","duration_seconds":4}'
```

生成 Foley（文本驱动）：

```powershell
curl -X POST http://127.0.0.1:8123/generate/foley `
  -H "Content-Type: application/json" `
  -d '{"prompt":"coffee shop ambiance with gentle chatter and espresso machine","duration_seconds":8}'
```

返回 JSON 包含 `audio_base64` 和本地 `file_path`。

### 4.4 离线权重

- 完整下载指南、repo ID、校验值：[`Plugins/UnrealMCP/audio_server/MODEL_DOWNLOAD_GUIDE.md`](../Plugins/UnrealMCP/audio_server/MODEL_DOWNLOAD_GUIDE.md)
- 安装细节：[`Plugins/UnrealMCP/docs/audio-tools-installation.md`](../Plugins/UnrealMCP/docs/audio-tools-installation.md)
- 服务接口说明：[`Plugins/UnrealMCP/audio_server/README.md`](../Plugins/UnrealMCP/audio_server/README.md)

### 4.5 验证脚本与生成耗时

仓库提供的测试脚本可用于验证音频服务：

- `Plugins/UnrealMCP/audio_server/tests/test_smoke.py`：健康检查 + 基础生成测试。
- `Plugins/UnrealMCP/audio_server/tests/test_ace_step_long.py`：ACE-Step 长音频压力测试。
- `Plugins/UnrealMCP/audio_server/tests/debug_ace_step.py`：单步调试 ACE-Step 加载与生成。

本机验证耗时参考：

- 30 秒音乐：约 14 秒。
- 180 秒（3 分钟）音乐：约 143 秒。

---

## 5. Hunyuan3D-2 / 2.1

项目通过 `hunyuan/hybrid_pipeline.py`（本地 CLI）和 `hunyuan/vfx_generation_service.py`（UE 集成）封装 Hunyuan3D 的图生 3D 能力。

### 5.1 环境准备（快速步骤）

```powershell
cd D:\Playground\TA-Playground\hunyuan
py -3.10 -m venv venv
.\venv\Scripts\activate

pip install torch==2.5.1 torchvision==0.20.1 torchaudio==2.5.1 `
  --index-url https://download.pytorch.org/whl/cu124

pip install transformers==4.46.3 diffusers==0.31.0
```

### 5.2 编译扩展

| 入口 | 需要编译的扩展 |
|------|---------------|
| `hybrid_pipeline.py` | `Hunyuan3D-2/hy3dgen/texgen/custom_rasterizer` 与 `differentiable_renderer` |
| `Hunyuan3D-2.1/api_server.py` / `vfx_generation_service.py` | `Hunyuan3D-2.1/hy3dpaint/custom_rasterizer` 与 `DifferentiableRenderer` |

详细命令、VS 2022 环境变量、常见编译错误见 [`hunyuan3d-setup.md`](hunyuan3d-setup.md)。

### 5.3 下载权重

```powershell
cd D:\Playground\TA-Playground\hunyuan
.\venv\Scripts\python.exe download_weights.py      # 2.1 Shape
.\venv\Scripts\python.exe download_texture_weights.py  # 2.0 Paint
.\venv\Scripts\python.exe check_status.py        # 完整性检查
```

### 5.4 启动生成服务

本地 CLI：

```powershell
python hybrid_pipeline.py horse_input.jpg -o horse_textured.glb
```

UE VFX 封装：

```powershell
.\venv\Scripts\python.exe vfx_generation_service.py `
  D:\refs\rock.png `
  --output-root D:\Playground\TA-Playground\hunyuan\output `
  --model-path tencent/Hunyuan3D-2.1 `
  --device cuda `
  --low-vram-mode
```

直接启动 API Server：

```powershell
cd Hunyuan3D-2.1
..\venv\Scripts\python.exe api_server.py --host 0.0.0.0 --port 8081
```

### 5.5 详细文档

- 完整部署指南：[`hunyuan3d-setup.md`](hunyuan3d-setup.md)
- 使用手册（CLI、API、UE 集成）：[`hunyuan3d-usage.md`](hunyuan3d-usage.md)
- 上游 README：[`../hunyuan/Hunyuan3D-2.1/README.md`](../hunyuan/Hunyuan3D-2.1/README.md)
- 上游 API 文档：[`../hunyuan/Hunyuan3D-2.1/API_DOCUMENTATION.md`](../hunyuan/Hunyuan3D-2.1/API_DOCUMENTATION.md)

---

## 6. VFX / UE 集成工作流

UnrealMCP 为 VFX 工作流新增了 `generate_and_import_3d`、`create_material_from_textures`、`duplicate_niagara_system`、`set_niagara_parameter` 等工具。典型流程：

1. AI 调用 `generate_and_import_3d`，传入 `referenceImage` / `prompt` 与 `destinationPath`
2. C++ 启动 `hunyuan/vfx_generation_service.py` 子进程异步生成
3. 返回 `jobId`，通过 `get_generate_and_import_3d_status` 轮询
4. 完成后自动导入 `hunyuan/output/<uuid>/mesh.glb` 并生成 Actor

完整协议、参数 schema、Rust/C++ 对齐要求：

- [`Plugins/UnrealMCP/docs/vfx_methods.md`](../Plugins/UnrealMCP/docs/vfx_methods.md)

---

## 7. AI 动画工作流

AI 动画部分目前为**原型阶段**：按 JSON keyframe 规格驱动 Actor transform 与相机，用于快速验证镜头与动作，再决定是否进入 Sequencer。

- 研究报告与工具链选型：[`plans/ai-animation-workflow.md`](../plans/ai-animation-workflow.md)
- 可执行原型：[`scripts/ai_animation_prototype.py`](../scripts/ai_animation_prototype.py)
- 示例规格：[`examples/anim_crystal.json`](../examples/anim_crystal.json)、[`examples/anim_skeletal.json`](../examples/anim_skeletal.json)

快速体验：

```powershell
python scripts\ai_animation_prototype.py `
  --spec examples\anim_crystal.json `
  --play-before-run `
  --screenshot-dir D:\Temp\anim_frames
```

---

## 8. 快速验证清单

| 步骤 | 验证命令 / 操作 | 成功标志 |
|------|-----------------|----------|
| Python 版本 | `python --version` | `3.10.x` |
| GPU 可用 | `nvidia-smi` | 显示 NVIDIA GPU 与驱动 |
| Rust 编译 | `cd Plugins\UnrealMCP\MCP_Server && cargo build --release` | 生成 `.exe` |
| UE 工程生成 | UnrealBuildTool `-projectfiles` | 生成 `.sln`/`.vcxproj` |
| UE Editor 编译 | UnrealBuildTool `TAPlaygroundEditor Win64 Development` | 编译成功 |
| UE 插件监听 | 启动 Editor 后检查日志 | `Command Server started on port 13377` |
| Jade 流水线 | `python scripts\jade_nodialog.py` | 命令成功，无弹窗 |
| 音频服务启动 | `python Plugins\UnrealMCP\audio_server\main.py` | 监听 `127.0.0.1:8123` |
| 音频 health | `curl -X POST http://127.0.0.1:8123/health` | 返回 JSON，状态正常 |
| Hunyuan 权重 | `python hunyuan\download_weights.py` | 输出完成 |
| Hunyuan API | `python hunyuan\Hunyuan3D-2.1\api_server.py` | `/health` 返回 200 |
| VFX 服务 | `python hunyuan\vfx_generation_service.py ref.png` | 输出 `mesh.glb` + `mesh.png` |
| 动画原型 | `python scripts\ai_animation_prototype.py --spec examples\anim_crystal.json` | 完成并返回帧列表 |

---

## 9. 常见问题索引

| 症状 | 排查文档 / 解决方案 |
|------|---------------------|
| `command not found` / 找不到 `unreal-mcp-server.exe` | 运行 `cargo build --release`，检查 `.mcp.json` 绝对路径与双反斜杠；参考 [`Plugins/UnrealMCP/docs/usage-guide.md`](../Plugins/UnrealMCP/docs/usage-guide.md) |
| `Not connected to Unreal Engine` | 启动 UE Editor，确认 `.uproject` 中 `UnrealMCP` 已启用 |
| `Failed to bind socket to port 13377` | 修改 `Config/DefaultEngine.ini` 与 `.mcp.json` 中的端口，重启 Editor |
| 端口占用 / 连接关闭 | 检查是否有其他 UE 实例或僵尸 `unreal-mcp-server.exe` |
| `CUDA out of memory` | 开启 `--low-vram-mode` / `--cpu_offload`、降低音频 `duration_seconds`、使用 `--shape-only` |
| HuggingFace 下载慢 / 超时 | 改用 ModelScope，或使用 `weights/_download_clip.py` / `_download_bigvgan.py` / `hunyuan/download_resume.py` |
| Stable Audio Open 下载被拒 | 访问模型页面点击 Accept，并运行 `huggingface-cli login` |
| MMAudio 提示缺少权重 | 预放 `weights/open_clip_pytorch_model.bin` 与 `weights/nvidia_bigvgan_v2_44khz_128band_512x/`，或设置 `.env`；详见 [`MODEL_DOWNLOAD_GUIDE.md`](../Plugins/UnrealMCP/audio_server/MODEL_DOWNLOAD_GUIDE.md) |
| PyTorch 变成 CPU 版本 | 强制重装 CUDA 版：`pip install --force-reinstall torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu124` |
| `flash-attn` 编译失败 | 跳过 Flash Attention，模型仍可用 |
| PowerShell 脚本无法执行 | `Set-ExecutionPolicy RemoteSigned -Scope CurrentUser` 或加 `-ExecutionPolicy Bypass` |
| 音频服务端口被占用 | 在 `audio_server/.env` 中设置 `AUDIO_SERVER_PORT=8124` |
| `/generate/music` 报 `libtorchcodec_core*.dll` / FFmpeg not found | 使用 BtbN **shared** FFmpeg，并确保 `tools/ffmpeg/bin` 在 `PATH` 中；参考 4.2.1 |
| ACE-Step `Cannot copy out of meta tensor; no data!` | 应用补丁 `Plugins/UnrealMCP/scripts/patches/ace-step-low-cpu-mem-and-soundfile.patch`；参考 4.2.2 |
| Hunyuan 子进程启动失败 | 确认 `hunyuan/venv/Scripts/python.exe` 存在，`Hunyuan3D-2.1/api_server.py` 路径正确 |
| VFX 生成任务 `failed` | 查看 `hunyuan/output/<uuid>/` 日志与 `api_server.py` 控制台输出 |
| UE 导入 WAV 失败 | 确保为 16/24 bit WAV，采样率 44.1 kHz 或 48 kHz |
| UE 导入 GLB 失败 | 确认 `mesh.glb` 存在且路径为 Windows 绝对路径 |
| Hunyuan VS 编译报错 | 使用 VS 2022 的 `vcvarsall.bat`，设置 `TORCH_CUDA_ARCH_LIST`；详见 [`hunyuan3d-setup.md`](hunyuan3d-setup.md) |

---

## 10. 相关文档索引

| 文档 | 内容 |
|------|------|
| [`README.md`](../README.md) | 项目总览 |
| [`AGENTS.md`](../AGENTS.md) | Agent / Skill 说明 |
| [`Plugins/UnrealMCP/README.md`](../Plugins/UnrealMCP/README.md) | UnrealMCP 插件总览 |
| [`Plugins/UnrealMCP/docs/usage-guide.md`](../Plugins/UnrealMCP/docs/usage-guide.md) | 插件连接、端口、AI 客户端配置（注意替换 ClanSimulator 路径） |
| [`Plugins/UnrealMCP/docs/audio-tools-installation.md`](../Plugins/UnrealMCP/docs/audio-tools-installation.md) | ACE-Step / Stable Audio Open / MMAudio 安装详情 |
| [`Plugins/UnrealMCP/audio_server/README.md`](../Plugins/UnrealMCP/audio_server/README.md) | 音频服务 FastAPI 端点与示例 |
| [`Plugins/UnrealMCP/audio_server/MODEL_DOWNLOAD_GUIDE.md`](../Plugins/UnrealMCP/audio_server/MODEL_DOWNLOAD_GUIDE.md) | 音频模型权重、校验值、离线导入 |
| [`Plugins/UnrealMCP/docs/audio-ue-integration.md`](../Plugins/UnrealMCP/docs/audio-ue-integration.md) | Blueprint / Python HTTP 集成 |
| [`Plugins/UnrealMCP/docs/vfx_methods.md`](../Plugins/UnrealMCP/docs/vfx_methods.md) | VFX/UE JSON-RPC 协议 |
| [`hunyuan3d-setup.md`](hunyuan3d-setup.md) | Hunyuan3D 环境、扩展编译、权重下载 |
| [`hunyuan3d-usage.md`](hunyuan3d-usage.md) | Hunyuan3D CLI / API / UE 集成用法 |
| [`plans/ai-animation-workflow.md`](../plans/ai-animation-workflow.md) | AI 动画工作流研究报告 |
| [`scripts/ai_animation_prototype.py`](../scripts/ai_animation_prototype.py) | AI 动画原型脚本 |
