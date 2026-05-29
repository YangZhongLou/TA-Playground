# TA-Playground

Technical Artist Playground — 基于 Unreal Engine 的渲染与图形技术实验项目，通过 AI 驱动的工作流（UnrealMCP）进行材质开发、Shader 编程和视觉效果原型验证。

## 项目概述

本项目是技术美术（TA）在 Unreal Engine 中的渲染特性实验场。覆盖材质开发、Shader 编程、渲染管线探索、视觉特效原型等内容。

**核心工作流**: AI Assistant (Claude) → UnrealMCP Server → Unreal Editor Plugin → UE Editor API。通过自然语言即可操作 Unreal Editor，实现材质搭建、场景配置、效果验证的端到端自动化。

## 环境要求

| 组件 | 版本/说明 |
|------|-----------|
| Unreal Engine | 5.4+（推荐 5.5） |
| Rust | 1.80+（MCP Server 编译） |
| Python | 3.10+（MCP Server 辅助脚本） |
| 平台 | Windows 10/11 |

## 项目结构

```
TA-Playground/
├── Content/                    # UE 内容资源
│   ├── Materials/              #   母材质 & 材质实例
│   │   ├── Master/             #     母材质 (M_*)
│   │   └── Instances/          #     材质实例 (MI_*)
│   ├── Textures/               #   纹理资源
│   │   ├── Procedural/         #     程序化生成贴图
│   │   └── Reference/          #     参考素材
│   ├── Maps/                   #   展示关卡
│   └── Blueprints/             #   蓝图工具
├── Plugins/
│   └── UnrealMCP/              # AI 驱动的编辑器控制插件
├── Config/                     # 引擎与项目配置
├── Source/                     # C++ 源码（自定义模块）
├── SkillHub/                   # AI 技能库（git submodule）
├── TA-Playground.uproject      # UE 项目文件
└── README.md
```

## 技能系统

项目通过 SkillHub 技能库提供可复用的 AI 工作流技能：

| 技能 | 命令 | 说明 |
|------|------|------|
| technical-artist | `/technical-artist` | UE 材质开发、Shader 编写、视觉原型 |

详细技能列表见 [SkillHub/CLAUDE.md](SkillHub/CLAUDE.md)。

## 快速开始

### 1. 环境准备

```bash
git clone --recursive https://github.com/YangZhongLou/TA-Playground.git
```

### 2. 生成项目 & 编译

```bash
# Generate VS project files
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" \
  -projectfiles -project="TA-Playground.uproject" -game -rocket

# Build editor target (compile C++ modules + UnrealMCP plugin)
"C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" \
  TAPlaygroundEditor Win64 Development -Project="TA-Playground.uproject"
```

### 3. 启动

1. 启动 UE Editor: `UnrealEditor.exe TA-Playground.uproject`
2. UnrealMCP 插件自动加载，监听 `127.0.0.1:13377`
3. 通过 Python 脚本或 MCP Client 发送命令

### 4. 验证 — Jade 材质流水线

```bash
python scripts/jade_nodialog.py
```

自动完成: 球体生成 → Jade PBR 材质 → 三点光照 → 视口聚焦。15/15 命令零对话框。

### 3. 启动

1. 启动 Unreal Editor（自动加载 UnrealMCP 插件，监听端口 13377）
2. 启动 AI Client，连接 MCP Server
3. 通过自然语言操作 UE Editor

### 4. 验证

```
"检查 Unreal 连接状态"
"在场景中创建一个 Sphere"
"给 Sphere 应用一个绿色材质"
```

详细开发计划见 [plans/roadmap.md](plans/roadmap.md)。

## 工作流

```
需求分析 → 视觉参考拆解 → 母材质设计 → 材质实例制作 → 场景验证 → 参数调优
    │              │              │              │              │           │
    ▼              ▼              ▼              ▼              ▼           ▼
 /technical-   手动搜集       MCP创建       MCP创建       MCP搭建       MCP截图
  artist       参考资料      母材质        材质实例       展示场景      对比调参
```

## 注意事项

- 本项目为实验性质，部分实现可能并非生产环境最佳实践
- 大型贴图或缓存文件已配置 `.gitignore`
- 材质参数命名规范：`Scalar_` 前缀表示标量参数，`Vector_` 前缀表示向量参数，`Tex_` 前缀表示纹理参数
- 建议每次实验在新分支上进行，验证通过后合并

---

> 适用于技术美术、图形程序员及任何对 UE 渲染技术感兴趣的学习者。
