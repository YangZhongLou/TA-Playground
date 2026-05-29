---
updated: 2026-05-29T04:30:00Z
phase: 6. Commit
---

# Blackboard — TA-Playground

## Current Milestone

**M1: 玉石材质 (Jade Material)**

| Phase | 内容 | Status |
|-------|------|--------|
| P1 基础材质 | 母材质节点网络 + BaseColor/Roughness/Specular 参数化 | done |
| P2 次表面散射 | Subsurface Profile 配置 | pending |
| P3 内部细节 | 程序化云雾脉络纹理 + Fresnel 边缘发光 | pending |
| P4 变体展示 | 白玉/翡翠/紫罗兰等多品种预设 + 展示关卡 | pending |

## Active Work

- **Current Phase**: P1 基础材质
- **Current Step**: `create_material_instance` + PBR 参数 + 场景搭建 → 20/20 命令通过
- **Blockers**: 无

## Recent Changes

| Date | What |
|------|------|
| 2026-05-29 | MCP Bug 修复: create_material_instance crash, MID auto-create, save_level auto-name |
| 2026-05-29 | UE 5.7 测试工程创建, UnrealMCP submodule 集成 |
| 2026-05-29 | Jade 材质 P1: MI_Jade_Green + Sphere + 3-point lighting → 20/20 通过 |
| 2026-05-29 | 安全权限规则: deny/ask 删除操作, 禁止毁灭性命令 |

## Risk Board

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| UE Editor 弹窗阻塞自动化 | High | High | 跳过 create_material, 用 Engine DefaultMaterial 做 parent |
| MCP `create_material` 创建空材质 | Confirmed | Med | 手动创建母材质, 或使用现有引擎材质 |
| save_current_level timeout | Confirmed | Low | 跳过, 改为运行时操作 |
| GitHub push 网络不稳定 | Med | Low | 本地 commit, 网络恢复后 push |

## Decisions

- 材质实例 parent 使用 `/Engine/EngineMaterials/DefaultMaterial`（有完整 PBR 节点）
- 不通过 MCP 创建母材质（`NewObject<UMaterial>` 只创建空壳）
- 流水线执行前先 `destroy_actor` 避免同名冲突
