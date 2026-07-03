# AI 动画原型工作流（UE + Python + UnrealMCP）

基于现有 `scripts/ai_vfx_pipeline.py` 和 UnrealMCP 插件，搭建一条**可运行、可扩展**的原型动画管线。目标不是替代 Sequencer，而是用最小成本验证"AI 生成资产 + 程序化动画 + 自动拍摄"的闭环。

---

## 1. 原型定位

这是一个 **MVP 级动画工作流**：

- **输入**：一段文字描述 + 参考图 + 关键帧 JSON
- **处理**：AI 生成 3D 资产 → 程序化关键帧插值 → 通过 MCP 驱动 UE 场景
- **输出**：编辑器内动画预览 + 截图/录屏

它适合验证概念、快速出片、为后续 Sequencer 精调提供起点。

---

## 2. 能力边界

UnrealMCP 当前已暴露的动画相关能力：

| 能力 | MCP 方法 | 用途 |
| --- | --- | --- |
| Actor 位移/旋转/缩放 | `set_actor_transform` | 物体动画、相机动画 |
| 相机轨道 | `start_camera_rig` / `stop_camera_rig` / `set_camera_rig_speed` | 沿轨道滑行镜头 |
| 相机切换 | `switch_camera` / `next_camera` / `prev_camera` | 多机位剪辑感 |
| 运行时相机 | `set_runtime_camera_transform` / `focus_runtime_camera_on_actor` | 运行时跟拍 |
| 播放控制 | `play_in_editor` / `stop_play_in_editor` | 触发 PIE 录制 |
| 截图 | `take_screenshot` | 逐帧抓取画面 |

**直接暂不支持**：Sequencer 关键帧、骨骼动画播放命令、BlendShape、Niagara 时间轴。但可通过**预配置 Blueprint + 外部 AI 动作数据**间接实现骨骼动画原型。

---

## 3. 工作流步骤

```text
剧本/分镜（Markdown/JSON）
        ↓
AI 生成资产（Hunyuan3D / 即梦 / Midjourney）
        ↓
编写关键帧规格（JSON：时间 → 位置/旋转/缩放）
        ↓
ai_animation_prototype.py 执行：
    导入/复用资产 → 插值关键帧 → 逐帧更新 UE → 截图/录屏
        ↓
后期剪辑（Premiere / 剪映 / DaVinci）
```

---

## 4. 关键帧规格

用一个 JSON 文件描述动画。最小结构：

```json
{
  "duration": 5.0,
  "fps": 30,
  "actors": [
    {
      "name": "AIGenerated_Crystal_abc123",
      "keyframes": [
        {"time": 0.0, "location": [0, 0, 100], "rotation": [0, 0, 0], "scale": [1, 1, 1]},
        {"time": 2.5, "location": [0, 0, 150], "rotation": [0, 0, 180], "scale": [1.2, 1.2, 1.2]},
        {"time": 5.0, "location": [0, 0, 100], "rotation": [0, 0, 360], "scale": [1, 1, 1]}
      ],
      "easing": "ease_in_out"
    }
  ],
  "camera": {
    "type": "viewport",
    "keyframes": [
      {"time": 0.0, "location": [400, 0, 200], "rotation": [-10, 0, 0]},
      {"time": 5.0, "location": [200, 200, 250], "rotation": [-15, 45, 0]}
    ]
  },
  "screenshots": {
    "enabled": true,
    "prefix": "anim_crystal"
  }
}
```

---

## 5. 插值策略

原型提供三种简单插值，后续可替换为更复杂的曲线：

| 模式 | 说明 |
| --- | --- |
| `linear` | 匀速过渡，最直接 |
| `ease_in_out` | 慢-快-慢，适合展示性镜头 |
| `step` | 直接跳到下一关键帧，适合定格/抽帧风格 |

插值在 Python 端完成，按 `fps` 计算出每一帧的目标 Transform，再通过 MCP 发送到 UE。

---

## 6. 骨骼动画原型路径

UnrealMCP 目前没有直接播放 AnimSequence 或控制 Skeleton 的命令，但可以用**外部 AI 生成动作 + FBX 导入 + 预配置 Character Blueprint** 的方式绕过。

### 6.1 动作数据来源

| 来源 | 输出 | 特点 |
| --- | --- | --- |
| Mixamo | FBX 动画 | 免费、量大、自动绑骨，适合原型 |
| MotionGPT | SMPL / BVH / FBX | 文本驱动动作，适合自定义 |
| Rokoko Video / Move.ai | FBX / BVH | 视频动捕，适合真人动作复刻 |
| Audio2Face | BlendShape 曲线 | 语音驱动面部表情 |

### 6.2 UE 端准备

1. **导入 FBX**：把带动作的 FBX 放入 `/Game/Characters/MyChar`。
2. **创建 Character Blueprint**：
   - 父类选择 `Character`
   - Mesh 组件指定导入的 SkeletalMesh
   - AnimInstance 指定导入的 AnimBlueprint（或直接用 AnimSequence 的 Loop）
3. **配置自动播放**（二选一）：
   - 在 AnimBlueprint 的 Event Graph 中，从 `Event Blueprint Begin Play` 连到 `Play Anim`
   - 或在 Character Blueprint 的 BeginPlay 里调用 `Play Animation`
4. **记录 Blueprint 路径**，如 `/Game/Characters/MyChar/BP_MyChar`

### 6.3 JSON 规格

在 `examples/anim_skeletal.json` 中，用 `blueprint_path` 替代静态 `name`：

```json
{
  "duration": 4.0,
  "fps": 24,
  "actors": [
    {
      "blueprint_path": "/Game/Characters/MyChar/BP_MyChar",
      "actor_name": "AIChar_Dance_01",
      "location": [0, 0, 0],
      "rotation": [0, 0, 0],
      "scale": [1, 1, 1],
      "keyframes": [
        {"time": 0.0, "location": [-200, 0, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1]},
        {"time": 4.0, "location": [200, 0, 0], "rotation": [0, 180, 0], "scale": [1, 1, 1]}
      ],
      "easing": "linear"
    }
  ],
  "camera": {
    "type": "viewport",
    "keyframes": [
      {"time": 0.0, "location": [300, -300, 200], "rotation": [-10, 45, 0]},
      {"time": 4.0, "location": [300, 300, 200], "rotation": [-10, 135, 0]}
    ]
  },
  "screenshots": {
    "enabled": true,
    "prefix": "anim_skeletal"
  }
}
```

### 6.4 运行方式

```bash
python scripts/ai_animation_prototype.py \
  --spec examples/anim_skeletal.json \
  --play-before-run \
  --screenshot-dir D:/Temp/anim_skeletal_frames
```

脚本会先 `spawn_blueprint_actor` 创建角色，然后按关键帧移动角色，骨骼动画由 Blueprint 自身在 PIE 中自动播放。

---

## 7. 运行方式

### 7.1 准备资产

先用现有管线生成一个资产：

```bash
python scripts/ai_vfx_pipeline.py \
  --prompt "a glowing magic crystal" \
  --ref-image D:/Temp/crystal.png \
  --location 0,0,100 \
  --spawn-effect \
  --effect-template ambient
```

记录返回的 `actorName`。

### 7.2 编写关键帧 JSON

把 `actorName` 填入 `anim_crystal.json`，调整关键帧。

### 7.3 运行动画脚本

```bash
python scripts/ai_animation_prototype.py \
  --spec D:/Temp/anim_crystal.json \
  --play-before-run \
  --screenshot-dir D:/Temp/anim_frames
```

参数说明：

| 参数 | 作用 |
| --- | --- |
| `--spec` | 关键帧 JSON 路径 |
| `--play-before-run` | 先进入 PIE，再开始动画 |
| `--screenshot-dir` | 逐帧截图保存目录 |
| `--mcp-host` / `--mcp-port` | MCP 连接地址 |

---

## 8. 与 AI 视频工具的衔接

原型不只限于 UE 内部。更完整的流程：

1. **AI 生成关键帧**：用 ComfyUI/Seedance 生成角色关键帧图
2. **提取运动描述**：视觉模型或人工标注出 `"time": 0.0 时角色在左侧，2.5 秒时跳起"`
3. **转为 JSON**：用脚本把自然语言/关键帧图转成关键帧规格
4. **UE 内执行**：驱动 3D 资产或图片平面（Billboard）完成动画
5. **输出到后期**：截图序列导入 Premiere，或用 AI 放大/补帧

---

## 9. 扩展路线

| 阶段 | 目标 | 所需工作 |
| --- | --- | --- |
| Phase 1（当前） | 物体 Transform 动画 + 截图 | `ai_animation_prototype.py` |
| Phase 2 | 骨骼动画原型（Blueprint 自动播放） | 外部 AI 动作 → FBX → Character Blueprint |
| Phase 3 | 多机位切换 + 轨道镜头 | 组合 `switch_camera` + `start_camera_rig` |
| Phase 4 | 骨骼/Sequencer 关键帧 | 在 UnrealMCP 中新增 `add_sequencer_track` / `add_keyframe` 命令 |
| Phase 5 | 语音/音频驱动口型 | 集成 Audio2Face 或 Sonic，输出 BlendShape 曲线 |

---

## 10. 注意事项

- **帧率稳定性**：MCP 是 TCP 命令，逐帧发送会有网络延迟。原型建议 `fps=15–24`，后期再升采样。
- **时间基准**：Python 端用 `time.monotonic()` 控制节奏，不要用 `time.sleep(1/fps)` 简单循环。
- **回退方案**：如果 MCP 连接失败，脚本应把关键帧数据保存为 CSV/JSON，供手动导入 Sequencer。
- **性能**：复杂场景建议关闭实时渲染（`r.DefaultFeature.AutoExposure 0`），已在 VFX 管线中处理。

---

## 11. 文件清单

| 文件 | 作用 |
| --- | --- |
| `plans/ai-animation-prototype-workflow.md` | 本文档，描述工作流 |
| `scripts/ai_animation_prototype.py` | 原型执行脚本 |
| `scripts/ai_vfx_pipeline.py` | 已有 AI 资产生成管线 |
| `examples/anim_crystal.json` | 静态物体示例 |
| `examples/anim_skeletal.json` | 骨骼动画示例 |
