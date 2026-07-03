---
name: ta-jade
description: Jade/玉石 material skill. Invoke for creating jade-like materials with subsurface scattering, internal veining, and multiple color variants.
metadata:
  type: skill
  trigger: manual
---

# TA — Jade Material

玉石材质专项 Skill。实现半透明散射、内部纹理、边缘发光等玉石核心视觉特征。

## Visual Breakdown

玉石（Jade）的核心视觉特征：

| 特性 | 描述 | PBR 映射 |
|------|------|----------|
| 半透明度 | 光线穿透表层，产生内部散射 | Subsurface Profile / Subsurface Color |
| 光滑表面 | 抛光玉石具有镜面般光泽 | Roughness 0.05-0.2 |
| 内部颜色渐变 | 薄处偏白/浅绿，厚处深绿 | Base Color + Thickness map |
| 内部纹理 | 云雾状、絮状、脉络纹理 | Noise + Voronoi 叠加 |
| 边缘发光 | 菲涅尔效应，边缘比中心亮 | Fresnel → Emissive 叠加 |
| 颜色多样性 | 白玉、青玉、碧玉、紫玉 | 不同母材质实例 |

## Phase 1: Foundation

**目标**: 创建可参数化的玉石母材质，实现基本的玉石外观。

```
母材质: M_Jade_Master
├── Base Color    → 绿色渐变（Lerp: 浅绿 ↔ 深绿，由厚度/Fresnel 驱动）
├── Roughness     → ScalarParameter (default 0.15)
├── Specular      → ScalarParameter (default 0.6)
├── Metallic      → Constant 0
├── Subsurface    → Subsurface Profile 或 Custom SSS 近似
├── Normal        → 细微噪点法线贴图
├── Fresnel Edge  → Fresnel * EmissiveColor（边缘微亮）
└── Opacity       → 可选 Thin Translucent 模式
```

**MCP 操作步骤**:

1. `spawn_actor` — 创建 StaticMeshActor，使用球体/雕塑 mesh
2. `set_static_mesh` — 设置展示 mesh（如 SM_Sphere / SM_Teapot）
3. `create_material_instance` — 从 M_Jade_Master 创建 MI_Jade_Green
4. `set_material` — 将 MI_Jade_Green 应用到 mesh
5. `set_material_parameter` — 调整 Scalar/Vector 参数
6. `set_light_parameters` — 配置三点光照（Key + Fill + Rim）
7. `focus_viewport` — 聚焦到展示对象
8. `take_screenshot` — 截图保存对比

## Phase 2: Subsurface Scattering

**目标**: 实现玉石特有的半透明散射效果。

- 启用 Shading Model → Subsurface Profile
- 创建/配置 Subsurface Profile 资产（Scatter Distance, Falloff Color）
- 添加 Thickness Map（基于 Noise 程序化生成或顶点色）
- 调整 Opacity 与 Subsurface Amount 参数
- 对比：Default Lit vs Subsurface Profile 模式

## Phase 3: Internal Details

**目标**: 添加云雾纹理、脉络和内部深度感。

- **云雾纹理**: Voronoi Noise 叠加 Gradient Noise → BaseColor 混合
- **脉络纹理**: Perlin Noise + Power 节点 → 锐利细线 → 叠加到 BaseColor
- **深度颜色**: PixelDepth / DistanceToNearestSurface → 驱动颜色深浅
- **结晶闪烁**: Specular breakup — Noise 微调 Roughness 和 Specular

## Phase 4: Variants & Display

**目标**: 制作多品种玉石预设，搭建展示场景。

| 品种 | Base Color | Roughness | 特点 |
|------|-----------|-----------|------|
| 和田白玉 | 乳白带微黄 | 0.08-0.15 | 温润油脂光 |
| 翡翠帝王绿 | 翠绿 | 0.05-0.10 | 镜面高光 |
| 翡翠冰种 | 白底透绿 | 0.10-0.18 | 高透明度 |
| 紫罗兰 | 淡紫 | 0.12-0.20 | 粉紫散射 |
| 黄玉 | 蜜黄 | 0.08-0.15 | 暖色调 |

搭建展示场景：
- 圆形展台，暗色背景
- 三点光照（主光 + 补光 + 轮廓光）
- 旋转展示（RotatingMovement Component）
- 多角度对比截图

## Quick Reference: Jade Material Parameters

```text
Base Color:       (0.05, 0.35, 0.15)  — 深翠绿
Subsurface Color: (0.10, 0.50, 0.25)  — 浅绿散射
Roughness:        0.12                  — 抛光
Specular:         0.55                  — 中等反射
Fresnel Exponent: 3.0                   — 边缘范围
Edge Glow Color:  (0.15, 0.40, 0.20)  — 边缘微亮
Scatter Depth:    0.8                   — 散射深度 (mm)
```
