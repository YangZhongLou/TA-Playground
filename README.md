# TA-Playground

Technique Artist Playground —— 基于 Unreal Engine 的渲染与图形技术实验项目。

## 项目概述

本项目是技术美术（TA）在 Unreal Engine 中进行渲染特性、材质系统、后处理效果及图形学相关技术研究与验证的实验场。涵盖材质开发、Shader 编程、渲染管线探索、视觉特效原型等内容，用于快速迭代和验证图形方案。

## 环境要求

- **Unreal Engine**：建议使用 UE 5.x 版本（具体版本根据项目分支或标签确认）
- **平台**：Windows（主要开发平台）

## 项目结构

> 当前仓库为项目初始化阶段，以下为标准的 Unreal Engine 项目结构规划：

```
TA-Playground/
├── Content/          # UE 内容资源（材质、蓝图、关卡、贴图等）
├── Source/           # C++ 源码（如有自定义模块或插件）
├── Config/           # 引擎与项目配置文件
├── Plugins/          # 插件目录
├── TA-Playground.uproject    # UE 项目文件
└── README.md         # 项目说明
```

## 快速开始

1. 克隆本仓库到本地
2. 使用对应版本的 Unreal Engine 打开 `.uproject` 文件
3. 引擎提示缺失插件时，按提示启用或编译
4. 进入 `Content/` 目录浏览示例关卡与资源

## 注意事项

- 本项目为实验性质，部分实现可能并非生产环境最佳实践
- 大型贴图或缓存文件已配置 `.gitignore`，提交前请确认
- 如需在特定 UE 版本运行，建议查看分支或 Release 说明

---

> 适用于技术美术、图形程序员及任何对 UE 渲染技术感兴趣的学习者。
