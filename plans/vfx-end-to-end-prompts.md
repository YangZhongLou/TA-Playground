# UE + AI VFX 端到端测试 Prompt 清单

> 本清单为 `scripts/ai_vfx_pipeline.py` 的 10 组端到端回归测试用例。
> 每组用例覆盖一个典型美术方向，并给出期望的 Niagara 模板、参考图要求与验收断言。
>
> 说明：当前环境无法启动 Unreal Editor / Hunyuan3D，因此本清单为**可执行计划**，
> 真实成功率、耗时等数据需在完整 UE + GPU 环境中实测后回填。

## 测试环境要求

- Unreal Editor 5.7 已启动并加载 `TA-Playground.uproject`
- UnrealMCP 插件已加载，TCP 端口 `13377` 可用
- `Content/Materials/Generated/` 四个母材已创建
- `Content/VFX/Templates/` 四个 Niagara 模板已创建并暴露 `User.*` 参数
- Hunyuan3D-2.1 API server 已启动（默认 `http://localhost:8081`）
- 参考图目录：`D:/Playground/TA-Playground/hunyuan/tests/fixtures/`（可另备）

## 通用验收标准

单组用例“成功”需同时满足：

1. `generate_and_import_3d` 返回 `status=completed` 并提供有效的 `assetPath` / `actorName`
2. `create_material_from_textures` 返回有效的 MIC 路径并写入至少一张贴图
3. Actor 在世界场景中可见（`get_actor_list` 能查到）
4. `take_screenshot` 返回有效截图路径
5. 如启用 `--spawn-effect`，`duplicate_niagara_system` 成功并生成 Niagara Actor

整体目标（见 `plans/ue-ai-vfx-workflow-plan.md` 第 7 节）：

- 10 组成功率 ≥ 70%
- 单组总耗时 ≤ 5 分钟（不含 Hunyuan3D 首次模型加载）

## Prompt 列表

| 编号 | 类别 | Prompt | 期望 Niagara 模板 | 参考图关键词 | 主要断言 |
| --- | --- | --- | --- | --- | --- |
| E2E-01 | 自然/岩石 | `a mossy boulder with rough stone surface` | 无 / ambient（可选尘埃） | 苔藓岩石照片 | 生成网格为封闭固体；BaseColor 贴图包含绿色调；截图中物体位于画面中心 |
| E2E-02 | 魔法/水晶 | `a glowing purple magic crystal on a dark base` | burst | 紫色发光水晶 | Emissive/Translucent 材质被使用；晶体中心有高光；Burst 粒子颜色偏紫 |
| E2E-03 | 机械/金属 | `a rusty sci-fi cargo crate with metal panels` | 无 | 生锈金属箱 | Opaque 母材；Roughness/Metallic 贴图存在；表面有明显金属反光 |
| E2E-04 | 火焰/能量 | `a floating fire orb with embers` | ambient + burst | 火球概念图 | Unlit/Additive 或 Translucent 材质；截图整体色温偏暖（橙/红 dominant hue） |
| E2E-05 | 植物/树木 | `a small stylized pine tree with snow on branches` | ambient | 积雪松树 | Masked 母材可能用于 alpha 树叶；绿色 dominant hue；截图亮度适中 |
| E2E-06 | 武器/剑 | `a frost longsword with icy blade` | trail | 冰霜长剑 | 剑身半透明/冰晶感；Trail 粒子沿剑身方向；截图色温偏冷（蓝/青） |
| E2E-07 | 建筑/遗迹 | `an ancient stone altar with rune carvings` | ambient | 石制祭坛 | 几何结构稳定；法线贴图增强浮雕；整体偏灰/棕色 |
| E2E-08 | 爆炸/冲击 | `a magical impact flash with sparks and debris` | impact | 法术爆炸 | Impact 模板成功 spawn；粒子寿命短；截图中心有强亮斑 |
| E2E-09 | 道具/药水 | `a glass potion bottle with glowing green liquid` | ambient | 玻璃瓶药水 | Translucent 材质；液体部分绿色 dominant hue；瓶体有折射高光 |
| E2E-10 | 角色/头盔 | `a futuristic combat helmet with visor` | 无 | 科幻头盔 | Opaque/Metallic 材质；visor 区域半透明或高反光；网格无穿模 |

## 推荐执行命令模板

```bash
# 需要提前准备参考图，例如 E2E-02
python scripts/ai_vfx_pipeline.py \
  --prompt "a glowing purple magic crystal on a dark base" \
  --ref-image hunyuan/tests/fixtures/e2e_02_crystal.png \
  --location 0,0,120 \
  --spawn-effect \
  --effect-template burst \
  --effect-params '{"Color":[0.6,0.2,1.0],"Size":2.0,"Rate":80}' \
  --screenshot-name e2e_02_crystal
```

## 数据记录表（实测后回填）

| 编号 | 参考图路径 | 生成时间(s) | 导入时间(s) | 总耗时(s) | 成功 | 失败原因 / 备注 |
| --- | --- | --- | --- | --- | --- | --- |
| E2E-01 | | | | | | |
| E2E-02 | | | | | | |
| E2E-03 | | | | | | |
| E2E-04 | | | | | | |
| E2E-05 | | | | | | |
| E2E-06 | | | | | | |
| E2E-07 | | | | | | |
| E2E-08 | | | | | | |
| E2E-09 | | | | | | |
| E2E-10 | | | | | | |

## 视觉评估建议

每组用例生成截图后，建议使用 `scripts/evaluate_screenshot.py` 进行自动初筛：

```bash
python scripts/evaluate_screenshot.py \
  Saved/Screenshots/e2e_02_crystal.png \
  --reference-color 0.6,0.2,1.0 \
  --max-color-distance 0.45 \
  --output reports/e2e_02_crystal.json
```

对未通过的用例，可进一步启用 `--vision`（需配置 `OPENAI_API_KEY`）获取语言模型反馈。
