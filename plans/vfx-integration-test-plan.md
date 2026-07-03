# UE + AI VFX 集成测试计划

> 负责角色：`qa-engineer`  
> 关联计划：`plans/ue-ai-vfx-workflow-plan.md` Phase 5  
> 当前状态：**脚本与计划已生成，未在真实 UE 环境中执行**

## 1. 目标

验证 UnrealMCP VFX 扩展的四个核心方法在 Unreal Editor 内可以正确协同工作：

1. `generate_and_import_3d`
2. `create_material_from_textures`
3. `duplicate_niagara_system`
4. `set_niagara_parameter`

同时提供一个可复用的截图视觉评估框架，用于端到端测试反馈闭环。

## 2. 无法执行的限制说明

当前环境**不能**启动 Unreal Editor，也**不能**运行 Hunyuan3D 推理。因此：

- 本计划中的脚本均为“可在 UE 中执行”的代码与文档，**未经过真实 UE 运行时验证**。
- `generate_and_import_3d` 的 Hunyuan 生成路径不在本次验证范围内；集成测试只走 `meshFile` 直接导入路径。
- Niagara 模板与母材质 `.uasset` 是否存在以运行时的 Editor 内容库为准；脚本会在缺失时跳过并报告。

## 3. 测试脚本

### 3.1 UE 内集成测试脚本

**文件：** `Content/Python/test_vfx_mcp_integration.py`

**运行方式：**

1. 打开 `TA-Playground.uproject`。
2. 确认插件启用：
   - `PythonScriptPlugin`
   - `EditorScriptingUtilities`
   - `UnrealMCP`
   - `Niagara`（用于 Niagara 测试）
3. 在 Editor 中执行：
   - **Toolbar:** Edit → Plugins → Python → Execute Python Script → 选择本文件
   - **Console:** `py D:/Playground/TA-Playground/Content/Python/test_vfx_mcp_integration.py`
   - **Python 输出日志:** 粘贴脚本内容并执行

**验证范围：**

| 方法 | 测试内容 | 通过标准 |
| --- | --- | --- |
| `generate_and_import_3d` | 通过 `meshFile` 导入临时 OBJ 立方体 | `success=true`, `status=completed`, `assetPath`/`actorName` 有效，且 Actor 在场景中 |
| `create_material_from_textures` | 导入测试 PNG 贴图，创建 MIC 并赋给 Actor | MIC 资产存在，返回 `path` 有效 |
| `set_texture_parameter` | 在 Actor 的 MIC 上切换 `BaseColor` 贴图 | `success=true` |
| `duplicate_niagara_system` | 复制首个存在的 Niagara 模板并 spawn Actor | 新系统资产存在，Actor 在场景中 |
| `set_niagara_parameter` | 修改 spawn 出的 Niagara Actor 的 `User.Color` | `success=true`，返回 `valueType` 正确 |

**预期输出示例：**

```json
{
  "success": true,
  "passed": 5,
  "total": 5,
  "elapsedMs": 2840.5,
  "results": [
    {
      "name": "generate_and_import_3d (direct import)",
      "passed": true,
      "message": "actor=AIVFXTest_Cube_a1b2c3",
      "elapsedMs": 520.1
    }
  ],
  "createdActors": ["AIVFXTest_Cube_a1b2c3", "AIVFXTest_Niagara_d4e5f6"],
  "createdAssets": ["/Game/Generated/Test/Meshes/..."]
}
```

### 3.2 端到端 Prompt 清单

**文件：** `plans/vfx-end-to-end-prompts.md`

包含 10 组不同方向的 prompt、推荐 Niagara 模板、参考图关键词、验收断言与数据记录表。

## 4. 视觉反馈框架

**文件：** `scripts/evaluate_screenshot.py`

**功能：**

- 无需 UE 环境，独立分析截图 PNG/JPG。
- 内置像素级指标：亮度、画面覆盖率、物体中心偏移、主色调、与参考色距离。
- 可选 SSIM 对比参考图（需 `scikit-image`）。
- 可选 OpenAI Vision API 文本反馈（需 `OPENAI_API_KEY`）。
- 输出 JSON 报告，可直接被端到端测试脚本汇总。

**用法示例：**

```bash
python scripts/evaluate_screenshot.py \
  Saved/Screenshots/aivfx_rock_abc123.png \
  --reference-color 0.5,0.4,0.3 \
  --max-color-distance 0.4 \
  --output reports/aivfx_rock_abc123.json
```

## 5. 已知风险与边界

| 风险 | 影响 | 当前处理 |
| --- | --- | --- |
| 母材质 / Niagara 模板不存在 | 部分用例跳过 | 脚本在运行前检查资产存在性，缺失时明确报告 |
| `import_asset` 对 PNG 支持不一致 | 材质测试失败 | 使用极简 8x8 PNG；如失败则跳过材质测试 |
| MCP 端口未监听 | 全部失败 | 脚本首先 `get_editor_info` 检查连接 |
| `set_texture_parameter` 目标材质无目标参数 | 失败 | 测试使用已知 `BaseColor` 参数 |
| 并发运行测试 | Actor/资产命名冲突 | 使用 `uuid` 后缀隔离 |

## 6. 后续待办

- [ ] 在真实 UE 5.7 环境中运行 `Content/Python/test_vfx_mcp_integration.py`。
- [ ] 根据首次运行结果修复脚本中 asset path / actor lookup 的兼容性问题。
- [ ] 运行 `plans/vfx-end-to-end-prompts.md` 中的 10 组 prompt，回填数据记录表。
- [ ] 统计成功率与耗时，评估是否达到 ≥70% / ≤5min 目标。
- [ ] 根据结果决定是否需要补充母材质、模板参数或 Hunyuan3D 生成参数调优。
