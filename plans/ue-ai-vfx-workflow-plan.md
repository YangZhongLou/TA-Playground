# UE + AI 自动生成特效资源工作流实施方案

## 1. 目标与范围

在 `TA-Playground` 项目内，基于已有的 **UnrealMCP** 插件和 **Hunyuan3D-2/2.1**
本地管线，搭建一套可在 Unreal Editor 中运行的 AI 自动生成特效资源工作流。

**本期目标（MVP）**：
- 通过自然语言/参考图，自动生成 3D 静态模型并导入 UE、创建 PBR 材质、摆放到场景。
- 预置一组 Niagara 特效模板，AI 可参数化调用。
- 提供一键式 Python 编排脚本，串联“生成 → 导入 → 装配 → 截图反馈”。

**不做的事（明确边界）**：
- 不实现“text-to-Niagara 从零生成”（技术不成熟）。
- 不实现角色骨骼/动画/复杂物理模拟的自动生成。
- 不改动 Hexagon 插件核心逻辑，只把它作为特效摆放的目标场景。

---

## 2. 总体架构

```
用户输入（prompt / 参考图）
    │
    ▼
┌─────────────────────────────────────────────────────────────┐
│  scripts/ai_vfx_pipeline.py                                  │
│  - 解析 prompt                                               │
│  - 调用 Hunyuan3D 生成 GLB                                   │
│  - 调用 UnrealMCP 导入/装配/截图                             │
└─────────────────────────────────────────────────────────────┘
    │                              │
    ▼                              ▼
Hunyuan3D 本地服务        UnrealMCP (Rust + C++)
(hunyuan/Hunyuan3D-2.1)        │
    │                          ▼
    │               UE Editor API
    ▼                          │
GLB/OBJ + Texture   ◄───────── import_asset / create_material /
                     spawn_actor / set_static_mesh / set_material /
                     spawn_effect / take_screenshot
```

---

## 3. Subagent 任务分配

> 每条任务均标注了推荐负责 subagent，可独立委派执行。完成后在勾选框打勾。
>
> 注：本文使用 `@xxx` 指代 AGENTS.md 中定义的对应 agent（如
> `@technical-artist` 对应 `/technical-artist`）。

### Phase 1：基础资产与工具准备（预计 2-3 天）

#### 3.1.1 创建 PBR 母材质库

- [x] **@technical-artist**：在 `Content/Materials/Generated/` 下创建以下母材质，
  供 AI 生成 MIC：
  - [x] `M_Generated_Opaque`（Opaque / Default Lit / BaseColor, Normal,
    Roughness, Metallic）
  - [x] `M_Generated_Translucent`（Translucent / Default Lit / BaseColor,
    Opacity, Emissive, Normal）
  - [x] `M_Generated_Unlit_Additive`（Additive / Unlit / BaseColor, Opacity,
    EmissiveColor）
  - [x] `M_Generated_Masked`（Masked / Default Lit / BaseColor, OpacityMask,
    Normal）
- [x] **@technical-artist**：确认母材质参数命名规范，并输出到
  `Content/Materials/Generated/README.md`。

#### 3.1.2 创建 Hunyuan3D 本地服务封装

- [x] **@programmer**：在 `hunyuan/` 下新增 `vfx_generation_service.py`：
  - [x] 启动 `Hunyuan3D-2.1/api_server.py` 子进程（如未运行）。
  - [x] 实现 `generate_from_image(image_path, params)`：上传 `/send`、
    轮询 `/status/{uid}`、下载 `textured.glb`。
  - [x] 实现 `generate_from_text(text_prompt, params)`：调用文生图模型生成参考图，
    再 image-to-3D（本期可选本地 SD/FLUX 或要求用户供图）。
  - [x] 输出规范路径：`hunyuan/output/<uuid>/mesh.glb` 和 `mesh.png`。
  - [x] 记录元数据：`hunyuan/output/<uuid>/meta.json`
    （prompt、seed、steps、model_version）。
- [x] **@qa-engineer**：为 `vfx_generation_service.py` 编写单元测试，
  覆盖 image/text 两路调用与失败重试。

#### 3.1.3 创建 UE Python 导入脚本

- [x] **@programmer**：在 `Content/Python/import_generated_asset.py` 中实现：
  - [x] `import_mesh(file_path: str, destination: str = "/Game/Generated/Meshes") -> str`
  - [x] `create_material_instance_from_maps(name, parent_path, maps, destination) -> str`
  - [x] `setup_actor_with_mesh_and_material(actor_name, mesh_path, material_path, location) -> None`
- [x] **@qa-engineer**：验证 Python Editor Scripting 插件已在 `.uproject` 中启用。

---

### Phase 2：扩展 UnrealMCP（预计 4-5 天）

#### 3.2.1 接口协议约定

- [x] **@architect**：定义 5 个新 method 的 JSON 请求/响应 schema，
  输出到 `Plugins/UnrealMCP/docs/vfx_methods.md`。

#### 3.2.2 C++ 端新增命令

- [x] **@programmer**：新建 `Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/VfxCommands.cpp`，
  实现：
  - [x] `HandleGenerateAndImport3D`（method: `generate_and_import_3d`）
  - [x] `HandleCreateMaterialFromTextures`（method: `create_material_from_textures`）
  - [x] `HandleSetTextureParameter`（method: `set_texture_parameter`）
  - [x] `HandleDuplicateNiagaraSystem`（method: `duplicate_niagara_system`）
  - [x] `HandleSetNiagaraParameter`（method: `set_niagara_parameter`）
- [x] **@programmer**：修改 `Plugins/UnrealMCP/Source/UnrealMCP/Private/MCPCommandServer.cpp`：
  - [x] 添加 forward declaration。
  - [x] 在 `ProcessCommand` 中增加对应 method 分支。
- [x] **@code-reviewer**：Review C++ 端新增命令的内存安全、错误返回与 UE API 使用。

#### 3.2.3 Rust MCP Server 端新增 tool

- [x] **@programmer**：在 `Plugins/UnrealMCP/MCP_Server/src/server.rs` 的
  `#[tool(tool_box)] impl UnrealMcpServer` 中新增：
  - [x] `generate_and_import_3d`
  - [x] `get_generate_and_import_3d_status`
  - [x] `create_material_from_textures`
  - [x] `set_texture_parameter`
  - [x] `duplicate_niagara_system`
  - [x] `set_niagara_parameter`
- [x] **@programmer**：按需拆分 `Plugins/UnrealMCP/MCP_Server/src/tools/mod.rs`
  （本次 6 个工具规模较小，直接内联在 `server.rs`，未新增 `vfx_tools.rs`）。
- [x] **@code-reviewer**：Review Rust 端 tool 参数定义、序列化与错误处理。

#### 3.2.4 关键实现细节

- [x] **@programmer**：实现 `generate_and_import_3d` 子进程异步轮询，不阻塞 Editor。
- [x] **@programmer**：`create_material_from_textures` 优先调用 UE Python 脚本完成材质装配。
- [x] **@qa-engineer**：验证新增工具在 MCP Server 中注册成功，且 UE 端能正确响应。
  （集成测试脚本 `Content/Python/test_vfx_mcp_integration.py` 已覆盖四个核心方法。）

---

### Phase 3：Niagara 特效模板库（预计 2-3 天）

#### 3.3.1 预置 Niagara 模板

- [x] **@technical-artist**：在 `Content/VFX/Templates/` 下创建 Niagara System，
  每个暴露关键 User Parameters：
  - [x] `NS_BurstBase`（User.Color, User.Size, User.Rate, User.Lifetime, User.Velocity）
  - [x] `NS_TrailBase`（User.Color, User.Width, User.Lifetime, User.Speed）
  - [x] `NS_AmbientBase`（User.Color, User.Density, User.Size, User.Speed）
  - [x] `NS_ImpactBase`（User.Color, User.Size, User.DecalSize, User.Lifetime）
- [x] **@technical-artist**：统一模板内部使用 Sprite Renderer + 基础圆形/噪点贴图。
- [ ] **@qa-engineer**：验证 4 个模板均能通过 `duplicate_niagara_system` 复制并 spawn。

#### 3.3.2 Flipbook 材质函数

- [x] **@technical-artist**：在 `Content/Materials/Functions/MF_Flipbook` 中创建 Flipbook
  采样函数（Input: Texture, Rows, Columns, Animation Phase；Output: RGBA）。
- [ ] **@qa-engineer**：验证 Flipbook 函数可被 Niagara 材质正确引用。

---

### Phase 4：AI 编排脚本（预计 2-3 天）

- [x] **@programmer**：在 `scripts/ai_vfx_pipeline.py` 中实现 `AIVfxPipeline`：
  - [x] 初始化 `UnrealMCPClient(mcp_host, mcp_port)`。
  - [x] `run(prompt, reference_image)` 方法：生成/确认参考图 → Hunyuan3D 生成 GLB →
    导入 UE → 创建 MIC → 摆放 Actor → 可选 Niagara → 调整灯光/相机 → 截图返回路径。
  - [x] 提供 CLI：`--prompt`、`--ref-image`、`--location`、`--spawn-effect`。
- [x] **@qa-engineer**：为编排脚本编写集成测试与 mock MCP 断言。
- [x] **@code-reviewer**：Review 脚本错误处理、超时与日志。

---

### Phase 5：测试与迭代（预计 2 天）

> 注：Phase 5 的测试脚本、Prompt 清单与截图评估框架已生成；
> 真实 UE 运行验证、Hunyuan3D 端到端生成与成功率统计需在 UE + GPU 环境中执行。

- [x] **@qa-engineer**：单元测试：
  - [x] `generate_and_import_3d` 成功导入 GLB。
  - [x] `create_material_from_textures` 贴图参数正确。
  - [x] `duplicate_niagara_system` 复制后的系统可正常 spawn。
- [x] **@qa-engineer**：端到端测试：
  - [x] 准备 10 组不同 prompt（自然、魔法、机械、火焰等）。
  - [ ] 记录成功率、生成时间、导入时间。
- [x] **@qa-engineer**：视觉反馈闭环：
  - [ ] 用 `take_screenshot` 截图。
  - [x] 可选接入视觉模型做简单评估（颜色、位置合理性）。
- [x] **@architect**：根据当前实现输出迭代建议与下一阶段方案（见 `plans/architecture-next-steps.md`）。

---

### 项目管理

- [x] **@project-manager**：制定里程碑与依赖关系，跟踪各 Phase 进度，
  并在延期时协调裁剪范围。
  - 交付物：`plans/project-status.md`（里程碑表、阻塞项、依赖图、已验证/待验证清单、下一步动作）

---

## 4. 需要新建/修改的文件清单

### 新建文件

- `Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/VfxCommands.cpp`
- `hunyuan/vfx_generation_service.py`
- `Content/Python/import_generated_asset.py`
- `scripts/ai_vfx_pipeline.py`
- `Content/Materials/Generated/M_Generated_Opaque.uasset`
- `Content/Materials/Generated/M_Generated_Translucent.uasset`
- `Content/Materials/Generated/M_Generated_Unlit_Additive.uasset`
- `Content/Materials/Generated/M_Generated_Masked.uasset`
- `Content/VFX/Templates/NS_BurstBase.uasset`
- `Content/VFX/Templates/NS_TrailBase.uasset`
- `Content/VFX/Templates/NS_AmbientBase.uasset`
- `Content/VFX/Templates/NS_ImpactBase.uasset`
- `Content/Materials/Functions/MF_Flipbook.uasset`
- `plans/architecture-next-steps.md`
- `Content/Python/test_vfx_mcp_integration.py`
- `scripts/evaluate_screenshot.py`
- `plans/vfx-integration-test-plan.md`
- `plans/vfx-end-to-end-prompts.md`

### 修改文件

- `Plugins/UnrealMCP/Source/UnrealMCP/Private/MCPCommandServer.cpp`
- `Plugins/UnrealMCP/MCP_Server/src/server.rs`
- `Plugins/UnrealMCP/MCP_Server/src/tools/mod.rs`（如新增工具模块）
- `TAPlayground.uproject`（确保 Python Editor Scripting 插件启用）

---

## 5. 关键设计决策

### 决策 1：Hunyuan3D 生成走子进程还是 MCP 直接调用？

**选择**：走子进程 + 文件输出，再由 MCP 导入。
**理由**：
- Hunyuan3D 推理慢（几十秒到几分钟），如果在 MCP 调用中同步执行会阻塞 Editor。
- 子进程模式可以独立重启/重试，不影响 UE 稳定性。
- 文件输出便于版本控制和人工检查。

### 决策 2：材质装配用 C++ 还是 Python？

**选择**：复杂材质装配用 Python `Content/Python/import_generated_asset.py`，
C++ MCP 只负责触发。
**理由**：
- Python 的 `MaterialEditingLibrary` API 更灵活，写节点图比 C++ 反射方便。
- 保持 C++ 插件稳定，减少编译次数。

### 决策 3：Niagara 用模板复制还是 AI 从零生成？

**选择**：模板复制 + 参数化。
**理由**：
- 当前没有可靠的 text-to-Niagara 模型。
- 模板库可由技术美术预先调优，AI 只负责选模板和调参，质量可控。

---

## 6. 风险与应对

| 风险 | 影响 | 应对 |
|---|---|---|
| Hunyuan3D 生成失败/显存不足 | 高 | 提供降级参数（turbo、低 octree）；支持失败重试和错误返回 |
| GLB 导入后材质丢失/错误 | 中 | 导入后统一用 Python 重建 MIC，不依赖 GLB 自带材质 |
| MCP 新增工具后编译失败 | 高 | 每次修改后本地 Build；新增工具时保持向前兼容 |
| Python Editor Scripting 未启用 | 中 | 在 `.uproject` 中显式启用，启动时检查 |
| Niagara 模板参数名不一致 | 中 | 模板文档化，脚本中用统一命名规范 |
| 生成资产风格不一致 | 中 | 固定 seed、使用参考图、记录元数据便于复现 |

---

## 7. 验收标准

- [ ] `generate_and_import_3d` 能从图片生成 GLB 并导入到 `/Game/Generated/Meshes/`。
- [ ] `create_material_from_textures` 能根据贴图集创建 MIC 并正确设置参数。
- [ ] `duplicate_niagara_system` 能复制模板，`spawn_effect` 能生成新系统。
- [ ] `scripts/ai_vfx_pipeline.py --prompt ...` 能一键完成
  “生成 → 导入 → 摆场景 → 截图”。
- [ ] 至少 4 个 Niagara 模板可用，覆盖 burst/trail/ambient/impact。
- [ ] 端到端测试 10 组 prompt，成功率 ≥ 70%（成功定义为 GLB 生成、导入、
  Actor 生成、截图路径均正常），单组总耗时 ≤ 5 分钟（不含 Hunyuan3D 首次加载）。

---

## 8. 推荐执行顺序

1. **Phase 1.1** `@technical-artist` 创建母材质库（手工在 UE 中完成）。
2. **Phase 1.2 + 1.3** `@programmer` 编写 Hunyuan 服务封装和 Python 导入脚本；
   `@qa-engineer` 补测试。
3. **Phase 2.1** `@architect` 先定义 JSON 协议。
4. **Phase 2.2 + 2.3** `@programmer` 并行实现 C++ 和 Rust 层；`@code-reviewer` 把关。
5. **Phase 3** `@technical-artist` 制作 Niagara 模板库。
6. **Phase 4** `@programmer` 编写 `ai_vfx_pipeline.py` 编排脚本。
7. **Phase 5** `@qa-engineer` 测试、修 bug；`@architect` 输出迭代建议。
8. 全程 `@project-manager` 跟踪里程碑与依赖。

预计总工期：**12-16 天**（单人全职），可压缩为 **7-10 天**
如果只实现 3D 道具导入 + 2 个 Niagara 模板的最小可用版本。
