---
updated: 2026-05-29T05:00:00Z
phase: 6. Commit
---

# Blackboard — TA-Playground

## Current Milestone

**M1: 玉石材质 (Jade Material)**

| Phase | 内容 | Status |
|-------|------|--------|
| P1 基础材质 | MI_Jade_Green + PBR 参数 + Sphere + 3-point 光照 | done |
| P2 次表面散射 | Subsurface Profile 配置 + 散射颜色/深度调参 | pending |
| P3 内部细节 | 程序化云雾脉络纹理 + Fresnel 边缘发光 | pending |
| P4 变体展示 | 白玉/翡翠/紫罗兰等多品种预设 + 展示关卡 | pending |

## Active Work

- **Current Phase**: P1 基础材质
- **Current Step**: 完成：MI_Jade_Green (DefaultMaterial parent) + 球体 + PBR 参数 + 三点光照 + 截图
- **Blockers**: 
  - P2/P3 需在 UE 编辑器手动创建母材质（含 SSS Profile、Fresnel 节点），MCP `create_material` 只创建空壳

## Recent Changes

| Date | What |
|------|------|
| 2026-05-29 | Jade P1 重新跑通：17/22 核心命令通过，截图保存 |
| 2026-05-29 | Blackboard 系统上线：dev-flow + project-manager 集成黑板报协议 |
| 2026-05-29 | MCP Bug 修复: create_material_instance crash, MID auto-create |
| 2026-05-29 | UE 5.7 测试工程创建, UnrealMCP submodule 集成 |
| 2026-05-29 | 安全权限规则: deny/ask 删除操作 |

## Risk Board

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| MCP `create_material` 创建空材质 | Confirmed | High | P2/P3 需手动在 UE 编辑器创建母材质 |
| `spawn_actor` 同名冲突崩溃 | Confirmed | High | 每次执行前先 `destroy_actor` |
| UE Editor `save_current_level` timeout | Confirmed | Low | 跳过保存，纯运行时操作 |
| GitHub push 网络不稳定 | Med | Low | 本地 commit，网络恢复后 push |

## Decisions

- MI_Jade_Green parent = `/Engine/EngineMaterials/DefaultMaterial`（完整 PBR 节点，参数化可用）
- P2/P3 需要手动创建 M_Jade_Master（SSS Shading Model + Fresnel + Voronoi/Perlin 节点）
- P2/P3 不可通过当前 MCP 自动化（缺少 `UMaterialExpression` 构建命令）
