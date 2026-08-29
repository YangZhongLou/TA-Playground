# 网络同步研究

游戏联网的核心问题：在延迟、丢包、带宽上限和作弊风险下，让多台机器对同一份模拟达成可玩的一致。

方案研究在本目录。框架见 [technical-design.md](technical-design.md)。
可运行实现是 UE 插件 `Plugins/NetworkSync`，入口 [impl/README.md](impl/README.md)。
引擎自带复制见 [unreal.md](unreal.md)。

## 怎么读

按层读，不要在概念文里找类名，也不要在规格里找品类对比。

1. [technical-design.md](technical-design.md) — 框架：决策、分层、模块边界、不变量、验收范围。
2. [overview.md](overview.md) — 问题空间、权威模型、拓扑。
3. `schemes/` — 四类方案的协议细则（节拍、丢包、变体、失败模式）。
4. `impl/` — 可编码规格；线上字节见 [impl/packet-format.md](impl/packet-format.md)；代码在 `Plugins/NetworkSync`。
5. [techniques.md](techniques.md) — 预测、插值、补偿、兴趣管理。
6. [comparison.md](comparison.md) — 优劣机理与品类对照。
7. `cases/` — 守望先锋、Dota 2、王者荣耀。
8. [unreal.md](unreal.md) — 本仓库对应的 UE5 复制模型。
9. [references.md](references.md) — 论文、演讲、官方文档。

先读框架，再按 [schemes/README.md](schemes/README.md) 进细则。
改代码从 [impl/README.md](impl/README.md) 进。横向取舍看 comparison。

## 目录

| 文件 | 内容 |
| --- | --- |
| [technical-design.md](technical-design.md) | 框架：分层、模块、不变量、测试金字塔 |
| [overview.md](overview.md) | 同步要解决什么，三层模型（拓扑 / 权威 / 协议） |
| [schemes/README.md](schemes/README.md) | 四篇主线怎么读、和案例如何对应 |
| [schemes/lockstep.md](schemes/lockstep.md) | 帧同步：节拍变体、确定性、可靠有序、不同步 |
| [schemes/lockstep-variants.md](schemes/lockstep-variants.md) | 帧同步变种：节拍、输入形态、追帧、Quantum |
| [schemes/hybrid/README.md](schemes/hybrid/README.md) | 结合四包：检查点、切段、停拍拉齐、锁步加门 |
| [schemes/state-sync.md](schemes/state-sync.md) | 状态同步：快照增量、插值、预测、滞后补偿 |
| [schemes/rollback.md](schemes/rollback.md) | 回滚：确认/预测帧、存档、输入延迟、与 GGPO |
| [schemes/replication.md](schemes/replication.md) | 对象复制：Role/Owner、属性与 RPC、Graph/Iris |
| [techniques.md](techniques.md) | 预测、插值、滞后补偿、AOI、压缩 |
| [impl/README.md](impl/README.md) | 实现规格入口、插件怎么跑 |
| [impl/lockstep.md](impl/lockstep.md) | 乐观 15Hz 规格 |
| [impl/lockstep-kinds.md](impl/lockstep-kinds.md) | 四支锁步内核：互斥、可独立落地 |
| [impl/hybrid/README.md](impl/hybrid/README.md) | 结合四包落地；停拍拉齐、锁步加门 |
| [impl/packet-format.md](impl/packet-format.md) | 20 字节头与八种 payload 的逐字节布局 |
| [comparison.md](comparison.md) | 各方案优劣机理、同轴对照、品类总表 |
| [cases/compare-three.md](cases/compare-three.md) | 守望先锋 / Dota 2 / 王者荣耀三款对照 |
| [cases/overwatch.md](cases/overwatch.md) | 守望先锋：权威状态同步 + 重预测 |
| [cases/dota2.md](cases/dota2.md) | Dota 2：Source 快照插值，几乎不预测 |
| [cases/honor-of-kings.md](cases/honor-of-kings.md) | 王者荣耀：15Hz 乐观帧同步 |
| [unreal.md](unreal.md) | UE5 Replication / Replication Graph / Iris |
| [references.md](references.md) | 参考文献 |

## 当前工业界的主线

国内讨论习惯把方案收成两类：**帧同步**与**状态同步**。工程上还要单独列出第三类：**回滚**。引擎内的 Actor/对象复制是状态同步的落地形态，不是第四种物理定律。

| 主线 | 传什么 | 典型品类 | 代表 |
| --- | --- | --- | --- |
| 帧同步（Lockstep） | 输入 | RTS、手游 MOBA、自走棋 | 星际争霸、王者荣耀 |
| 状态同步（State Sync） | 世界状态 / 增量 | FPS、BR、MMO、开放世界 | Source、Fortnite、Valorant、Dota 2、守望先锋 |
| 回滚（Rollback） | 输入 + 本地推测 | 格斗、部分平台动作 | GGPO、街霸 6、Photon Quantum 默认 |

配套技术（预测、插值、兴趣管理）可以叠在任何主线上，选型时不要把它们和主线方案混为一谈。

## 术语

| 术语 | 含义 |
| --- | --- |
| 权威（Authority） | 谁拥有最终正确状态 |
| Tick / Frame | 模拟步。帧同步里一帧输入对应一步模拟 |
| 确定性（Determinism） | 相同输入 + 相同初态 → 相同结果 |
| 快照（Snapshot） | 某一 Tick 的世界状态切片 |
| 增量（Delta） | 相对上一份已确认快照的差 |
| 预测（Prediction） | 客户端先按本地输入模拟，等服务器确认 |
| 和解（Reconciliation） | 预测与服务器结果不一致时回退重演 |
| 插值延迟（Interp Delay） | 客户端故意落后若干毫秒，用两份快照平滑 |
| AOI / Relevancy | 只同步玩家当前关心的实体 |

## 研究边界

本目录覆盖**实时对战游戏**的同步方案。协同编辑里的 OT / CRDT、通用分布式共识（Raft / Paxos）不在范围内。
