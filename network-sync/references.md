# 参考文献

按主题分组。读原文，不读二手转述里的“最佳实践”口号。

## 基础模型

| 资源 | 为什么读 |
| --- | --- |
| [Source Multiplayer Networking](https://developer.valvesoftware.com/wiki/Source_Multiplayer_Networking) | 快照、插值、预测的工业原典 |
| [Latency Compensating Methods (Valve)](https://developer.valvesoftware.com/wiki/Latency_Compensating_Methods_in_Client/Server_In-game_Protocol_Design_and_Optimization) | 滞后补偿怎么倒带 |
| [Gaffer on Games — Networking](https://gafferongames.com/categories/game-networking/) | UDP、快照增量、确定性、物理联网 |
| [1500 Archers (Age of Empires)](https://www.gamedeveloper.com/programming/1500-archers-on-a-28-8-network-programming-in-age-of-empires-and-beyond) | 锁步 + bucket 同步 |
| Quake 3 网络代码 / Fabien Sanglard 剖析 | 快照与增量的早期形态 |

## 品类案例

| 资源 | 主题 |
| --- | --- |
| Timothy Ford, GDC 2017, *Overwatch Gameplay Architecture and Netcode* | 60Hz Command Frame、预测和解、ECS |
| *Networking Scripted Weapons and Abilities in Overwatch* | Statescript 自动预测 / 复制 |
| Valve Zoid 对 Dota 2「单位延迟」的说明 | 20Hz 快照 + 插值，不做 FPS 预测 |
| 孙勋 TGDC / 腾讯游戏学堂，《王者荣耀》后台复盘 | 66ms 逻辑帧、UDP 冗余、不同步 Hash |
| [三款对照](cases/compare-three.md) | 守望先锋、Dota 2、王者荣耀 |
| Valorant / CS Tickrate 讨论（Riot、Valve 公开博文） | 高 Tick 射击 |
| Fortnite Replication Graph (GDC / Epic 博客) | 大规模对象复制 |
| GGPO 源码与文档 | 回滚参考实现 |
| Photon Quantum / Fusion 官方文档 | 锁步中间件 vs 状态预测中间件 |

## 引擎文档

| 资源 | 主题 |
| --- | --- |
| [UE Networking and Multiplayer](https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-and-multiplayer-in-unreal-engine) | Actor 复制、RPC、Role |
| [UE Iris](https://dev.epicgames.com/documentation/en-us/unreal-engine/iris-in-unreal-engine) | UE5 新复制后端 |
| Unity Netcode for GameObjects | NetworkVariable / RPC |
| Unity Netcode for Entities | ECS 快照与预测 |
| Mirror / Fish-Net 文档 | 社区复制框架 |

## 国内工程语境

国内团队口头上的“帧同步 / 状态同步”二分，对应本目录的 lockstep 与 state-sync。手机 MOBA 的公开分享多集中在：

- 定点数与跨端确定性。
- 帧间隔、追帧、掉线托管。
- 用服务器校验输入，而不是广播全量位置。

搜具体项目名时注意区分：大厅与匹配用状态/HTTP，对战核心才是帧同步。不要把匹配服务当成同步方案。

孙勋把「dota」和星际并称为帧同步时，指的是 Dota 1（War3），不是 Dota 2。

## 建议阅读顺序

1. Gaffer 的 *What Every Programmer Needs to Know About Game Networking*。
2. Source 多人对战页 + Valve 滞后补偿。
3. 1500 Archers（锁步）与 GGPO README（回滚）。
4. 本目录 [unreal.md](unreal.md) + Epic 官方 Networking。
5. 按自己品类补一篇 GDC：Overwatch、Fortnite Graph、或格斗回滚。
