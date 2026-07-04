# UE + AI VFX 工作流架构迭代建议

> 角色：architect  
> 日期：2026-07-03  
> 范围：基于当前已落地的 C++（UnrealMCP）、Rust（MCP Server）、Python（Hunyuan 服务 / 编排 / UE 脚本）与接口协议，输出迭代建议、已知技术债务与下一阶段可扩展方向。

---

## 1. 当前状态速览

| 层级 | 文件 / 模块 | 状态 |
|---|---|---|
| 协议 | `Plugins/UnrealMCP/docs/vfx_methods.md` | ✅ 已定义 5 个 method + 1 个异步状态查询 |
| C++ 命令 | `Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/VfxCommands.cpp` | ✅ 已实现 6 个 handler |
| C++ 分发 | `Plugins/UnrealMCP/Source/UnrealMCP/Private/MCPCommandServer.cpp` | ✅ 已注册 method 分支与 JSON-RPC id 补齐 |
| Rust tool | `Plugins/UnrealMCP/MCP_Server/src/server.rs` | ✅ 已实现 6 个 tool 透传 |
| 构建依赖 | `Plugins/UnrealMCP/Source/UnrealMCP/UnrealMCP.Build.cs` | ✅ 已添加 `Niagara` / `NiagaraCore` editor 依赖 |
| Hunyuan 服务 | `hunyuan/vfx_generation_service.py` | ✅ 已实现 image-to-3D、子进程管理、失败重试 |
| Hunyuan 测试 | `hunyuan/tests/test_vfx_generation_service.py` | ✅ 单元测试完整（依赖 venv 中的 `trimesh`） |
| UE Python 导入 / 材质 | `Content/Python/import_generated_asset.py` | ✅ 已实现 import_mesh / create MIC / setup_actor |
| 编排脚本 | `scripts/ai_vfx_pipeline.py` | ✅ 已实现端到端编排 + CLI |
| 编排测试 | `scripts/test_ai_vfx_pipeline.py` | ✅ mock 测试 10 个用例全部通过 |
| 母材质生成脚本 | `Content/Python/create_generated_master_materials.py` | ✅ 可在 UE 内生成 4 个母材质 |
| Niagara 模板生成脚本 | `Content/Python/create_niagara_templates.py` | ⚠️ 仅创建空 System，参数需手动 wire |
| Flipbook 函数生成脚本 | `Content/Python/create_mf_flipbook.py` | ✅ 可完整构建图并保存 |
| 项目配置 | `TA-Playground.uproject` | ✅ 已启用 `PythonScriptPlugin` / `EditorScriptingUtilities` |

---

## 2. 已验证 vs. 待真实 UE 环境验证

### 2.1 已验证（本地可运行）

- Rust MCP Server：`cargo check` 通过，`cargo test` 通过 34 个 mock 测试，61 个真实 UE 测试因无连接被忽略。
- Python 编排脚本：`python -m unittest scripts.test_ai_vfx_pipeline` 10/10 通过。
- Hunyuan 服务逻辑：测试文件完整，但当前全局 Python 缺少 `trimesh`；需在 `hunyuan/venv` 中运行（预期通过）。
- 协议文档：C++ / Rust / Python 三端字段名已对齐。

### 2.2 只能计划、不能本地执行

- C++ 插件编译与 Unreal Editor 启动：当前环境无可用 UE Editor，无法运行 UBT / 热重载。
- `generate_and_import_3d` 端到端：需要 UE Editor + Hunyuan3D GPU 推理。
- `create_material_from_textures` / `set_texture_parameter` / `duplicate_niagara_system` / `set_niagara_parameter`
  的运行时行为：依赖 UE 材质 / Niagara 资产。
- 4 个 Niagara 模板与 Flipbook 材质函数：脚本可生成/创建资产，但粒子行为、渲染正确性、参数绑定需在 Niagara Editor 中目视确认。
- 10 组 prompt 端到端成功率 / 耗时统计：需真实 UE + GPU 环境。
- 视觉反馈闭环（截图 + 视觉模型评估）：依赖 UE viewport 与外部视觉模型。

---

## 3. 已知技术债务

### 3.1 代码层面的债务

1. **子进程解析脆弱**  
`VfxCommands.cpp::LaunchGenerationJob` 通过扫描 stdout 中第一个 `{` 来定位 JSON，若 Hunyuan 服务输出非 JSON 日志或 `{` 出现在 JSON 之前会解析失败。建议改用
`--result-file` 约定或结构化 stdout 分隔符。

2. **材质装配走 `GEngine->Exec("py ...")`**  
`HandleCreateMaterialFromTextures` 通过控制台命令调用 Python，返回信息依赖临时 JSON 文件。这要求 `PythonScriptPlugin` 启用且 Editor
处于可执行脚本状态。建议未来改为直接调用 `FPythonScriptExecutor` 或 C++ `MaterialEditingLibrary` 以减少 shell 间接层。

3. **动态 MID 不持久化**  
`set_texture_parameter` 对 Actor 的材质槽创建 `UMaterialInstanceDynamic`，该 MID 是运行时的，不会保存为资产。如果工作流目标是“生成可复用资产”，应改为创建并保存
`MaterialInstanceConstant`，然后重新赋值。

4. **Niagara 复制无幂等 / reuse 逻辑**  
`duplicate_niagara_system` 在 `newPath` 已存在时会直接失败。建议像 `create_material_from_textures` 一样加入 `reuse` 参数，避免同一 prompt 二次运行报错。

5. **初始参数重复写入**  
`duplicate_niagara_system` 先把 `initialParameters` 写入 System 资产，又在 spawn Actor 后写入 Component，逻辑冗余。建议只写
Component（运行时生效）或根据需求二选一。

6. **Job 内存未主动清理**  
   `GenerationJobs` 仅在 `get_generate_and_import_3d_status` 查询到终态时才移除。若客户端轮询失败或从未查询，已完成任务会残留。建议增加 TTL / 定期清理。

7. **错误信息截断**  
   子进程失败时只取 `Output.Left(256)`，复杂错误（如 Python traceback）难以定位。建议完整写入日志文件并返回日志路径。

8. **Python 解释器硬编码**  
   C++ 侧默认使用 `hunyuan/venv/Scripts/python.exe`，回退到系统 `python`。应支持通过 `generationParams.pythonExecutable` 显式指定，并校验依赖。

### 3.2 资产层面的债务

1. **Niagara 模板是空壳**  
`create_niagara_templates.py` 只能创建空 System 并记录参数说明，真正的 Sprite Emitter、Renderer、User Parameter 绑定需要技术美术在 Niagara Editor
中手动完成。这是当前 MVP 最大的“人工程序化缺口”。

2. **母材质 / 模板 / 函数未生成 `.uasset`**  
当前 `Content/Materials/Generated/`、`Content/VFX/Templates/`、`Content/Materials/Functions/` 只有 README 与 Python
生成脚本，没有二进制资产。首次打开项目后必须手动运行脚本生成资产，否则 C++ handler 中的 LoadObject 会失败。

3. **缺少通用 Niagara sprite 材质**  
4 个模板建议使用 `M_Generated_Unlit_Additive` 作为 sprite 材质，但模板脚本没有自动创建并绑定该材质。应补充一个
`Content/Python/create_niagara_sprite_materials.py`。

### 3.3 仓库层面的债务

1. **UnrealMCP 是 submodule / junction，且被 `.gitignore` 忽略**  
`.gitignore` 第 19 行 `Plugins/UnrealMCP/` 导致所有在 `Plugins/UnrealMCP/` 内的修改（VfxCommands.cpp、server.rs 等）无法提交到主仓库。当前 `git
status` 显示 `m Plugins/UnrealMCP`（子模块修改）。这是最大风险：工作流核心代码不在主仓库版本控制内。

   **建议**：
   - 若 UnrealMCP 为 fork 后的 submodule，应将修改 push 到 submodule 远程并在主仓库更新子模块指针；或
   - 取消 `Plugins/UnrealMCP/` 的 ignore（如果它是项目专属 fork），把代码纳入主仓；或
   - 至少把新增文件清单写入计划文档，并在 CI 中显式 checkout submodule。

2. **Content 资产缺 `.uasset` 提交策略**  
   母材质、模板、材质函数目前靠 Python 脚本生成，但脚本本身不受 ignore。是否应在首次生成后把 `.uasset` 加入版本控制，还是每次 CI 重新生成，需要明确。

---

## 4. 迭代建议（按优先级排序）

### 4.1 P0：消除版本控制风险

- **行动**：决定 UnrealMCP 修改的归属（子模块远程 vs. 主仓库）。
- **验收**：`git status` 不再只显示 `m Plugins/UnrealMCP`，而是能看到具体源码改动；CI 能正确拉取包含 VFX 修改的 UnrealMCP。

### 4.2 P0：补全 Niagara 模板程序化生成

- **行动**：扩展 `create_niagara_templates.py`，使用 Niagara Python / C++ API 真正创建：
  - Sprite Emitter（Spawn Rate / Burst）
  - Sprite Renderer
  - 绑定 `User.*` 参数到 Initialize Particle / Color / Size / Lifetime / Velocity
  - 保存为 `.uasset`
- **验收**：无需打开 Niagara Editor，`duplicate_niagara_system` 复制出的系统直接 spawn 即可看到粒子效果。
- **参考**：Niagara 的 Python API 在 5.x 中仍不稳定，可能需要补充一个 C++ handler `create_niagara_template` 或 Editor Utility Blueprint
  作为过渡。

### 4.3 P1：失败重试与降级策略

- **行动**：
  1. 在 `VfxCommands.cpp` / `vfx_generation_service.py` 之间增加统一的 JSON 结果文件约定，替代 stdout 扫描。
  2. `ai_vfx_pipeline.py` 增加可配置重试：
     - Hunyuan 失败 → 自动降级到 `turbo` / 更低 `octree_resolution` / 更低 face_count。
     - 导入失败 → 尝试 FBX/OBJ 中转或返回错误资产路径。
  3. 记录每次失败原因到 `hunyuan/output/<job>/error.json`。
- **验收**：10 组 prompt 端到端成功率 ≥ 70%（当前目标）。

### 4.4 P1：材质装配稳健化

- **行动**：
  1. `create_material_from_textures` 返回更详细的参数写入结果（成功 / 失败的参数列表）。
  2. `set_texture_parameter` 支持持久化到 MIC（新增 `persistent: bool` 参数，true 时创建/修改并保存 MIC 资产）。
  3. 增加 `create_material_instance` 的批量标量/向量覆盖，减少多次 MCP 调用。
- **验收**：材质装配失败时能明确告诉调用方哪个参数不存在，且材质资产可保存复用。

### 4.5 P2：Text-to-Niagara 预研

- **现状**：协议与模板已支持“选模板 + 调参”，但不支持从零生成 Niagara System。
- **可行路线**（非本期）：
  1. **模板库扩展**：把模板拆成更细粒度模块（Spawn、Initialize、Renderer、Force），AI 选择并拼接模块。风险低，但仍是“模板组合”。
  2. **LLM + 反射生成 JSON**：让 LLM 输出 Niagara System 的 JSON 描述（emitter 类型、module 参数），再由 C++ 反射创建。需要大量 prompt
    工程，适合魔法/火焰等常见效果。
  3. **视觉-动作闭环**：生成 Niagara 后截图，由视觉模型评估颜色/密度/位置，迭代调参。依赖外部视觉模型 API。
- **建议**：本期继续走“模板 + 参数”，但把模板数量从 4 个扩展到 8-10 个（如 `NS_FireBase`、`NS_MagicBase`、`NS_WaterBase`、`NS_SmokeBase`），覆盖更多 prompt
  关键词。

### 4.6 P2：更多母材质与特殊效果

- **新增母材质候选**：
  - `M_Generated_Subsurface`（玉石、蜡、皮肤）——复用 `/ta-jade` skill 的 SSS 经验。
  - `M_Generated_ClearCoat`（车漆、光滑塑料）。
  - `M_Generated_WorldAligned`（地形/岩石自动 UV）。
  - `M_Generated_FlipbookParticle`（用于 Niagara sprite 的 flipbook 材质）。
- **验收**：每个母材质都有对应的 `create_generated_master_materials.py` 创建函数和参数文档。

### 4.7 P2：视觉反馈闭环

- **行动**：
  1. 在 `ai_vfx_pipeline.py` 中封装 `evaluate_screenshot(image_path, prompt)` 接口，支持接入 OpenAI / Claude / 本地视觉模型。
  2. 评估维度：颜色是否符合 prompt、物体是否在画面中央、是否存在明显穿帮（如纯黑/纯白截图）。
  3. 根据评估结果自动调整灯光位置、相机 FOV 或 Niagara 参数。
- **验收**： pipeline 输出中包含 `visual_score` 与 `visual_feedback` 字段。

### 4.8 P2：异步状态增强

- **行动**：
  1. `get_generate_and_import_3d_status` 增加 `stage` 字段（`waiting` / `shape_generation` / `texture_generation` /
    `importing` / `spawning`）。
  2. `vfx_generation_service.py` 把 `/status` 响应中的中间阶段写入 `stage.json`，C++ 读取后更新 job。
  3. 支持取消任务 `cancel_generate_and_import_3d`。
- **验收**：客户端能看到进度条从 0% 到 100% 的合理分段。

---

## 5. 下一阶段可扩展方向

| 方向 | 描述 | 依赖 |
|---|---|---|
| **text-to-Niagara** | LLM 选择模板并调参，未来可尝试模块组合生成 | 更多模板、Niagara Python/C++ API 稳定 |
| **更多母材质** | Subsurface、ClearCoat、WorldAligned、FlipbookParticle | 技术美术调参、材质生成脚本 |
| **失败重试策略** | Hunyuan 参数降级、模型热切换、超时重试 | 统一错误码、结果文件约定 |
| **视觉反馈闭环** | 截图 → 视觉模型评分 → 自动调参 | 外部视觉模型 API / 本地 GPU |
| **批量生成管线** | 一次输入 N 个 prompt，并发生成、排队导入 | 异步 job 队列、磁盘空间管理 |
| **资产版本与复现** | 每个生成资产记录完整 meta（seed、model、prompt、参考图哈希） | meta.json 扩展、DDC 缓存策略 |
| **CI 回归测试** | 在 headless UE 或 mock 模式下跑通全部 MCP method | 容器化 UE、GitHub Actions 自托管 runner |
| **子模块治理** | 把 UnrealMCP VFX 修改固定到主仓库可追踪的版本 | submodule 远程权限或取消 ignore |

---

## 6. 风险更新

| 风险 | 等级 | 当前应对 | 建议补充 |
|---|---|---|---|
| UnrealMCP 修改不在主仓 | 🔴 高 | 无 | P0 解决子模块提交策略 |
| Niagara 模板是空壳 | 🔴 高 | README 说明手动步骤 | P0 补全程序化生成 |
| Hunyuan 生成失败/显存不足 | 🟠 中 | turbo / low-vram 参数 | 增加自动降级重试 |
| C++ stdout JSON 解析脆弱 | 🟠 中 | 取第一个 `{` | 改用结果文件约定 |
| MID 不持久化 | 🟡 低 | 运行时生效 | 增加 persistent 模式 |
| 缺少通用 sprite 材质 | 🟡 低 | 依赖母材质 | 创建 Niagara 专用材质 |
| 环境无法启动 UE | — | 明确标注不可验证 | 所有 UE 内验证留给真实环境 |

---

## 7. 推荐下一步任务清单

- [ ] **@programmer**：把 UnrealMCP 的 VFX 修改提交到子模块远程，并更新主仓库子模块指针（或调整 `.gitignore`）。
- [ ] **@technical-artist**：在真实 UE 环境中运行
  `create_generated_master_materials.py`、`create_niagara_templates.py`、`create_mf_flipbook.py`，并手动 wire Niagara 参数，保存
  `.uasset`。
- [ ] **@programmer**：实现 Niagara 模板的程序化 wire（或降级为 Editor Utility Blueprint）。
- [ ] **@qa-engineer**：在真实 UE + GPU 环境中跑 Phase 5 的单元测试与 10 组 prompt 端到端测试。
- [ ] **@architect**：评审失败重试与结果文件约定方案，确认后拆分到具体任务。

---

## 8. 文件清单（新增 / 修改建议）

### 新增

- `Content/Python/create_niagara_sprite_materials.py` — 为 Niagara 模板生成通用 sprite 材质。
- `Content/Python/create_niagara_template_wired.py`（或扩展现有脚本）— 完整创建可运行的 Niagara System。
- `hunyuan/output/<job>/stage.json` — 生成阶段快照。
- `hunyuan/output/<job>/error.json` — 失败原因结构化记录。

### 修改

- `.gitignore` 或 `.gitmodules` — 解决 UnrealMCP 子模块版本控制问题。
- `Plugins/UnrealMCP/Source/UnrealMCP/Private/Commands/VfxCommands.cpp` — stdout 解析、job 清理、持久化 MIC、Niagara reuse。
- `Plugins/UnrealMCP/Source/UnrealMCP/Private/MCPCommandServer.cpp` — 如新增 `cancel_generate_and_import_3d`。
- `Plugins/UnrealMCP/MCP_Server/src/server.rs` — 新增/扩展 Rust tool。
- `Plugins/UnrealMCP/docs/vfx_methods.md` — 协议更新（stage、persistent、reuse、cancel）。
- `scripts/ai_vfx_pipeline.py` — 失败重试、视觉评估接口、批量模式。
- `hunyuan/vfx_generation_service.py` — 阶段文件输出、更详细的 `/status` 响应。
