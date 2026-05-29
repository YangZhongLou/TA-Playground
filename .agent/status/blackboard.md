---
updated: 2026-05-29T09:30:00Z
phase: 1. Plan
---

# Blackboard — TA-Playground

## Current Milestone

**M1: 玉石材质 (Jade Material)**

| Phase | 内容 | Status |
|-------|------|--------|
| P1 基础材质 | M_Jade_Master + PBR nodes + MI_Jade_Green + Sphere + 3-point | done |
| P2 次表面散射 | M_Jade_SSS + SubsurfaceProfile auto-create + 逆光场景 | done |
| P3 内部细节 | 程序化云雾脉络纹理 + Fresnel 边缘发光 | pending |
| P4 变体展示 | 白玉/翡翠/紫罗兰等多品种预设 + 展示关卡 | pending |

## Active Work

- **Current Phase**: P2 完成
- **Current Step**: 18/18 PASS: M_Jade_SSS (subsurface_profile + SP_Jade) + MI_Jade_SSS + 逆光 + 截图
- **Blockers**: 
  - P3 Fresnel/纹理节点需 MaterialExpression 扩展（当前只支持 Constant/Constant3Vector）

## Recent Changes

| Date | What |
|------|------|
| 2026-05-29 | P2 SSS 完成：18/18 PASS, subsurfaceProfile 自动创建, M_Jade_SSS + MI_Jade_SSS + 逆光 |
| 2026-05-29 | P1 重测通过：18/18 PASS, M_Jade_Master 带 PBR 节点, MI parent 修复, 光照修复 |
| 2026-05-29 | create_material 修复: 真实材质节点 + PreEdit/PostEdit + Modify + reparent support |
| 2026-05-29 | Submodule 重构：UnrealMCP 移至 Plugins/UnrealMCP + 插件文件提至 repo root + MaterialEditor 模块依赖修复 |
| 2026-05-29 | Phase 6 Commit: UnrealMCP submodule 移至 Plugins/UnrealMCP, 插件文件提至 repo root, push 成功 |
| 2026-05-29 | Jade P1 重新跑通：17/22 核心命令通过，截图保存 |
| 2026-05-29 | Blackboard 系统上线：dev-flow + project-manager 集成黑板报协议 |
| 2026-05-29 | MCP Bug 修复: create_material_instance crash, MID auto-create |
| 2026-05-29 | UE 5.7 测试工程创建, UnrealMCP submodule 集成 |
| 2026-05-29 | 安全权限规则: deny/ask 删除操作 |

## Risk Board

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| MCP `create_material` 创建空材质 | Resolved | - | Fixed: Constant3Vector/Constant nodes + PreEdit/PostEdit + SubsurfaceProfile |
| `spawn_actor` 同名冲突崩溃 | Confirmed | High | 每次执行前先 `destroy_actor` |
| UE Editor `save_current_level` timeout | Confirmed | Low | 跳过保存，纯运行时操作 |
| GitHub push 网络不稳定 | Med | Low | 本地 commit，网络恢复后 push |

## Decisions

- P1: M_Jade_Master 通过 create_material 自动创建（Constant3Vector + Constant 节点连接 PBR 属性）
- P2: M_Jade_SSS 通过 create_material 自动创建（subsurface_profile + auto-create SP_Jade）
- P3: 需扩展 MaterialExpression 类型支持（Fresnel/Voronoi/Perlin 等节点）
