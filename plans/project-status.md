# UE + AI 自动生成特效资源工作流 — 项目状态跟踪

> 角色：@project-manager  
> 生成时间：2026-07-03  
> 跟踪范围：`plans/ue-ai-vfx-workflow-plan.md` 中 Phase 1~5 及验收标准

---

## 1. 总体完成度

| 维度 | 状态 | 说明 |
| --- | --- | --- |
| 代码/脚本实现 | **约 85%** | Python/Rust/C++ 骨架与业务逻辑已落地，单元测试覆盖核心路径 |
| UE 资产生成 | **0%** | 母材质、Niagara 模板、Flipbook 函数均为 `.py` 创建脚本，未在 Editor 中执行 |
| UE 插件编译 | **未验证** | `UnrealMCP` 二进制仍为 6 月 8 日旧版本，新增 VFX 命令未进 DLL |
| 端到端测试 | **未执行** | 当前环境无法启动 Unreal Editor / Hunyuan3D 推理 |

**结论**：工作流处于“代码完成、待 UE 环境跑通”阶段。下一步强依赖：**启动 Unreal Editor + 重新编译 UnrealMCP + 运行 Python 资产创建脚本**。

---

## 2. 里程碑跟踪表

### Phase 1：基础资产与工具准备

| 任务 | 负责人 | 计划状态 | 实际状态 | 阻塞/备注 |
| --- | --- | --- | --- | --- |
| 1.1 创建 4 个 PBR 母材质 `.uasset` | @technical-artist | ✅ | ❌ 未生成 | `Content/Python/create_generated_master_materials.py` 已写好；需在 UE 内执行生成 `.uasset` |
| 1.1 输出母材质参数规范 README | @technical-artist | ✅ | ✅ | `Content/Materials/Generated/README.md` 完整 |
| 1.2 `hunyuan/vfx_generation_service.py` | @programmer | ✅ | ✅ | 已实现 image/text 两路、重试、元数据 |
| 1.2 单元测试 | @qa-engineer | ✅ | ✅ | `hunyuan/tests/test_vfx_generation_service.py` 20 项通过（venv 内） |
| 1.3 `Content/Python/import_generated_asset.py` | @programmer | ✅ | ✅ | import_mesh / create_material_instance / setup_actor 已实现 |
| 1.3 验证 Python Editor Scripting 启用 | @qa-engineer | ✅ | ✅ | `.uproject` 已启用 `PythonScriptPlugin` + `EditorScriptingUtilities` |

### Phase 2：扩展 UnrealMCP

| 任务 | 负责人 | 计划状态 | 实际状态 | 阻塞/备注 |
| --- | --- | --- | --- | --- |
| 2.1 接口协议 `vfx_methods.md` | @architect | ✅ | ✅ | 文档完整，5 method + 异步状态查询 |
| 2.2 C++ `VfxCommands.cpp` | @programmer | ✅ | ✅ | 5 handler + 异步 job  bookkeeping 已实现 |
| 2.2 `MCPCommandServer.cpp` 注册分支 | @programmer | ✅ | ✅ | 6 个 method 分支 + JSON-RPC id 包装已加 |
| 2.2 `UnrealMCP.Build.cs` Niagara 依赖 | @programmer | — | ✅ | 已添加 `Niagara` / `NiagaraCore` editor 依赖 |
| 2.3 Rust MCP Server tool | @programmer | ✅ | ✅ | 6 个 tool 已内联在 `server.rs` |
| 2.4 异步子进程不阻塞 Editor | @programmer | ✅ | ✅ | `LaunchGenerationJob` 使用 `Async(EAsyncExecution::Thread)` |
| 2.4 材质装配优先调用 Python | @programmer | ✅ | ✅ | `HandleCreateMaterialFromTextures` 调用 `import_generated_asset.py` |
| 2.4 QA 验证工具注册 & UE 响应 | @qa-engineer | ⬜ | ❌ 未验证 | 需编译后启动 Editor 实测 |

### Phase 3：Niagara 特效模板库

| 任务 | 负责人 | 计划状态 | 实际状态 | 阻塞/备注 |
| --- | --- | --- | --- | --- |
| 3.1 预置 4 个 Niagara System `.uasset` | @technical-artist | ✅ | ❌ 未生成 | `Content/Python/create_niagara_templates.py` 已写好；参数文档在 `Content/VFX/Templates/README.md` |
| 3.1 统一 Sprite Renderer + 基础贴图 | @technical-artist | ✅ | ❌ 未生成 | 脚本仅创建空 System，内部 emitter/renderer 需人工补完 |
| 3.1 QA 验证复制 & spawn | @qa-engineer | ⬜ | ❌ 未验证 | 依赖 `.uasset` 生成 + 编译后测试 |
| 3.2 Flipbook 材质函数 `.uasset` | @technical-artist | ✅ | ❌ 未生成 | `Content/Python/create_mf_flipbook.py` 已写好，含完整节点图 |
| 3.2 QA 验证 Niagara 引用 | @qa-engineer | ⬜ | ❌ 未验证 | 依赖 `.uasset` 生成 |

### Phase 4：AI 编排脚本

| 任务 | 负责人 | 计划状态 | 实际状态 | 阻塞/备注 |
| --- | --- | --- | --- | --- |
| 4.0 `scripts/ai_vfx_pipeline.py` | @programmer | ✅ | ✅ | `AIVfxPipeline` + CLI 已实现 |
| 4.0 集成测试 & mock MCP 断言 | @qa-engineer | ✅ | ✅ | `scripts/test_ai_vfx_pipeline.py` 10 项通过 |
| 4.0 Code Review（错误处理/超时/日志） | @code-reviewer | ⬜ | ⚠️ 建议 Review | 实现已存在，但尚未走正式 review |

### Phase 5：测试与迭代

| 任务 | 负责人 | 计划状态 | 实际状态 | 阻塞/备注 |
| --- | --- | --- | --- | --- |
| 5.1 单元测试：generate/import/MIC/Niagara | @qa-engineer | ⬜ | ❌ 未执行 | 依赖 UE 编译 + Editor |
| 5.2 端到端 10 组 prompt 测试 | @qa-engineer | ⬜ | ❌ 未执行 | 依赖 Hunyuan3D 服务 + UE Editor |
| 5.3 视觉反馈闭环（截图+可选视觉评估） | @qa-engineer | ⬜ | ❌ 未执行 | 依赖 Editor |
| 5.4 迭代建议 | @architect | ⬜ | ❌ 未执行 | 需 5.1~5.3 数据 |

### 项目管理

| 任务 | 负责人 | 计划状态 | 实际状态 | 阻塞/备注 |
| --- | --- | --- | --- | --- |
| 制定里程碑与依赖跟踪 | @project-manager | ⬜ | ✅ | 本文档即为交付物 |

---

## 3. 关键阻塞项（按优先级）

| # | 阻塞项 | 优先级 | 影响范围 | 解除条件 |
| --- | --- | --- | --- | --- |
| 1 | **UnrealMCP 插件未重新编译** | P0 | Phase 2 所有 C++ 命令无法在 Editor 生效 | 在能启动 UBT/UE 的环境中重新构建 `TAPlaygroundEditor` 或 `UnrealMCP` 模块 |
| 2 | **母材质/Niagara/Flipbook `.uasset` 未生成** | P0 | Phase 1.1、3 验收标准；`create_material_from_textures`/`duplicate_niagara_system` 运行时会报资产不存在 | 启动 Unreal Editor → 运行 `Content/Python/create_generated_master_materials.py`、`create_niagara_templates.py`、`create_mf_flipbook.py` |
| 3 | **Niagara 模板内部结构为空** | P1 | `duplicate_niagara_system` 能复制资产，但无粒子效果 | 技术美术在 Niagara Editor 中补 emitter、sprite renderer、贴图，并确保 User.* 参数暴露 |
| 4 | **端到端测试环境缺失** | P1 | 无法验证 10 组 prompt 成功率 | 准备带 GPU + Hunyuan3D 权重的工作站，按 Phase 5 执行 |
| 5 | **C++ Code Review 未完成** | P2 | 内存安全、错误返回、UE API 使用未正式 review | @code-reviewer 在 PR 中 review `VfxCommands.cpp`、`MCPCommandServer.cpp`、`server.rs` |

---

## 4. 依赖关系图

```
Phase 1.2/1.3 Python 脚本 ──┐
Phase 2.1 协议文档          │
Phase 2.2/2.3 MCP 扩展      ├─► 重新编译 UnrealMCP ──► 启动 UE Editor
Phase 4 编排脚本            │
                            ▼
            运行 Python 资产创建脚本（母材质 / Niagara / Flipbook）
                            │
                            ▼
        Phase 5 单元测试 + 端到端测试 + 视觉反馈闭环
                            │
                            ▼
                @architect 输出迭代建议
```

---

## 5. 已验证 vs 待验证

### 已验证（无需 UE/Hunyuan 运行）

- `Content/Python/tests/test_import_generated_asset_unit.py`：**8/8 通过**
- `hunyuan/tests/test_vfx_generation_service.py`：**20/20 通过**（使用 `hunyuan/venv/Scripts/python.exe`）
- `scripts/test_ai_vfx_pipeline.py`：**10/10 通过**
- Rust MCP Server 编译：`cargo check` 通过（1 个 dead_code warning）
- `.uproject` 已启用 `PythonScriptPlugin` + `EditorScriptingUtilities`

### 待验证（必须真实 UE 环境）

- UnrealMCP C++ 模块编译是否通过
- 6 个新增 MCP method 在 Editor 中是否可调用、返回格式是否符合 `vfx_methods.md`
- `generate_and_import_3d` 异步子进程是否能正确拉起 `hunyuan/vfx_generation_service.py`
- `create_material_from_textures` 委托 Python 脚本是否能正确创建 MIC
- `duplicate_niagara_system` 复制 + spawn 是否工作
- 4 个母材质、4 个 Niagara 模板、1 个 Flipbook 函数 `.uasset` 是否能正确创建
- `scripts/ai_vfx_pipeline.py --prompt ... --ref-image ...` 一键端到端
- Phase 5 的 10 组 prompt 成功率/耗时

---

## 6. 风险更新

| 风险 | 当前评估 | 缓解措施 |
| --- | --- | --- |
| 编译失败 | **高**（未验证） | 优先单独构建 `UnrealMCP` 模块，关注 Niagara/UMG/EditorScripting 链接 |
| 资产未生成 | **高**（已确认） | 将运行 3 个 Python 脚本作为进入 Phase 5 的前置门禁 |
| Niagara 模板为空 | **中** | 技术美术需在脚本创建资产后手工补全 emitter |
| Hunyuan3D 显存/失败 | **中** | 服务层已实现重试 + `low_vram_mode` + 超时 |
| 端到端成功率不达标（<70%） | **未知** | 先跑 10 组 prompt，再决定是调 prompt、降参还是裁剪范围 |

---

## 7. 建议的下一步动作（Owner + 验收点）

| 顺序 | 动作 | Owner | 验收点 |
| --- | --- | --- | --- |
| 1 | 在可启动 UE 的 Windows 工作站上重新编译 `TAPlaygroundEditor` | @programmer | `Plugins/UnrealMCP/Binaries/Win64/UnrealEditor-UnrealMCP.dll` 时间戳更新 |
| 2 | 启动 Editor 并运行 3 个资产创建脚本 | @technical-artist | 生成 4 个母材质 `.uasset`、4 个 Niagara System `.uasset`、1 个 `MF_Flipbook.uasset` |
| 3 | 补全 Niagara 模板内部 emitter/renderer | @technical-artist | 每个模板能在 Niagara Preview 中看到有粒子输出 |
| 4 | 在 Editor 中手动调用 6 个 MCP method | @qa-engineer | 所有方法返回 `success: true` 且资产/actor 正确创建 |
| 5 | 运行 `scripts/ai_vfx_pipeline.py` 端到端 | @qa-engineer | 至少 1 组参考图能走完“生成 → 导入 → MIC → 摆场景 → 截图” |
| 6 | 执行 Phase 5 测试矩阵 | @qa-engineer | 输出 10 组 prompt 成功率/耗时表 |
| 7 | Code Review + 迭代建议 | @code-reviewer / @architect | 阻塞 bug 修复、下一版本方案 |

---

## 8. 范围裁剪建议（若工期紧张）

若无法立即解除“编译 + 资产生成”阻塞，可保留以下最小可用版本（MVP-）：

1. **只保留 2 个 Niagara 模板**：`NS_BurstBase` + `NS_AmbientBase`。
2. **先不跑 text-to-3D**：继续要求用户给参考图（当前代码已如此）。
3. **Flipbook 函数延后**：对 MVP 一键流程非必需。
4. **视觉模型评估延后**：只保留 `take_screenshot` 路径截图，人工评估。

这样可把 Phase 3/5 工期从 ~5 天压缩到 ~2 天。
