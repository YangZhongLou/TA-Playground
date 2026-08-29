# 引擎对象复制

把状态同步切到引擎对象上：每个 Actor / Networked Object 自己声明复制哪些属性、多常发、发给谁、用 RPC 处理离散事件。

UE Replication、Unity Netcode for GameObjects、Mirror、Fish-Net、Photon Fusion 的主机/状态模式，同属这一族。
它不是第四种物理定律，是 [state-sync.md](state-sync.md) 的工业接口。
UE5 专项见 [unreal.md](../unreal.md)。

Photon Quantum 是确定性输入引擎（默认 predict-rollback），不是对象复制。Fusion Shared Mode 偏客户端权威，也不要和主机模式混为一谈。

## 一个对象要带的三件事

| 概念 | 作用 | 弄错时的样子 |
| --- | --- | --- |
| 所有权 Owner | 谁能对该对象发 Server RPC；谁预测它 | 两人同时预测同一辆车，互拉 |
| 网络角色 Role | 这台机器上它是权威、自治代理、还是模拟代理 | UI 看到不该看的 CD；别人改我的血 |
| 复制集 | 哪些属性下行、条件、频率、是否休眠 | 带宽打满，或门只对主机开 |

UE 术语（其他引擎名字不同、结构同类）：

| Role | 谁 | 典型行为 |
| --- | --- | --- |
| Authority | 服务器（Listen 上则是主机） | 跑真模拟，决定复制 |
| AutonomousProxy | 拥有该 Pawn 的客户端 | 预测自己，发输入 / Server RPC |
| SimulatedProxy | 其他客户端 | 插值别人，不预测 |

## 框架对象怎么切

切错是联网 bug 的第一来源。按「谁需要看见、谁需要操作」分。

| 对象 | 复制给谁 | 职责 |
| --- | --- | --- |
| GameMode | 谁也不复制（只服务器） | 规则、生成、流 |
| GameState | 所有人 | 阶段、比分、全局开关 |
| PlayerState | 通常所有人（或队友） | 名字、阵营、个人分数 |
| PlayerController | **仅 Owner** | 输入、HUD、Server RPC 入口 |
| Pawn / Character | 相关客户端 | 位姿；Owner 预测，他人插值 |
| 场景机关 | 相关客户端；可休眠 | 门开关用属性，不要每帧 RPC |

PlayerController 若复制给所有人，别人就能看到你的私有 UI 数据。
血量若只放在 Controller 上且 OwnerOnly，别人看到的你永远满血。

## 属性复制：当前值

语义是「现在是什么」。丢包由下一份属性纠正。

声明时要写清：

- 条件：`OwnerOnly`、`SkipOwner`（自己已经预测了别再下行覆盖）、`SimulatedOnly`、`InitialOnly`、自定义。
- 通知：值变化时是否回调（OnRep）。OnRep 是表现钩子，不要在里面改权威规则。
- 量化 / 阈值：位置变化小于 x 不发。
- 频率：该对象每秒最多推几次。

能表达成当前值的，不要改成每帧 RPC。位置、血量、门开关、是否隐身，都是当前值。

## RPC：发生过的事

| 方向 | 谁调用 | 谁执行 |
| --- | --- | --- |
| Server | 必须是 Owner 客户端 | 服务器 |
| Client | 服务器 | 该对象的 Owner 客户端 |
| NetMulticast | 服务器 | 所有相关客户端 |

客户端调用 Multicast 不会自动变成「先到服务器再广播」。必须自己 Server RPC 再转。

可靠 RPC：保证到达且有序，丢包时头包阻塞。买东西、解锁、进入下一阶段用它。
不可靠 RPC：开火提示、脚步、短动画。丢了就丢了，下一状态会纠正或玩家再开一枪。

用可靠 RPC 同步每帧位置：丢一个包，后面所有位置排队，人会「卡住再瞬移」。这是最常见的误用。

## 相关性、频率、休眠、优先级

对象级复制的意义就是：不是全图同一频率。

| 机制 | 做什么 |
| --- | --- |
| 距离 / `NetCullDistance` | 远了当不相关，停发 |
| `IsNetRelevantFor` / 自定义 | 房间、队伍、楼层、副本 |
| `bAlwaysRelevant` | 全局都必须看见（慎用） |
| `bOnlyRelevantToOwner` | 只有拥有者看见 |
| NetUpdateFrequency | 该对象发送上限 |
| 休眠 Dormancy | 长期不动则完全停更；被碰到再唤醒 |
| 优先级 | 带宽不够时先发近处、先发正在开枪的 |

默认实现往往是：每个连接、每个 Tick、对每个 Actor 问一次「要不要发」。
对象 × 连接一多，CPU 先于带宽死掉。Fortnite 量级必须换 Replication Graph 或 Iris：
先按空间格子和阵营建好列表，连接只向订阅的格子取 Actor。这是第二套架构，不是调一个频率能代替的。

休眠状态（UE 常见）：醒着、对所有连接休眠、对部分连接休眠、仅初始复制。
机关、装饰、远楼用休眠。玩家、子弹不要乱休眠。

## 所有权与预测

规则：谁拥有，谁预测；服务器仍是权威。

1. Owner 客户端立刻把输入用在本地 Pawn 上。
2. 输入或移动包到服务器，服务器模拟。
3. 复制结果回来，Owner 和解；非 Owner 插值。

两个客户端不要同时预测同一物体。载具换座、可拾取武器、可推箱子，必须把所有权交割写清楚：
哪一帧谁成为 Owner，旧 Owner 何时停预测。交割期间两边都预测，就是互拉和橡皮筋。

NetworkSync 插件不实现角色移动。引擎有 Mover 与 CMC 两条路径，选型在玩法项目里做。
不要 `SetActorLocation` 复制或位置 RPC。

GAS（Gameplay Ability System）另有 Prediction Key。技能预测和移动预测是两本账，
对不齐会出现「技能放出去但人还在墙外」。

## 生成与销毁

对象要进网，必须有稳定 NetGUID / NetworkObjectId。服务器 Spawn 再复制到客户端。
客户端本地 Spawn 一个「看起来一样」的 Actor，没有 ID，对端看不见，也不会被快照纠正。

销毁同理：服务器 Destroy，复制删除。客户端自己 Destroy 权威对象，下一包可能又被造回来，或进入无定义。

## 与手写整世界快照比什么

| | 对象复制 | 手写世界快照 |
| --- | --- | --- |
| 交货 | 快，属性 + RPC | 慢，要自研协议 |
| 调控 | 对象级频率、条件、休眠开箱 | 自己做订阅 |
| 包布局 | 引擎说了算，bit 级不好抠 | 可抠到每兵几 bit |
| 确定性 / 回滚 | 基本做不到当主线 | 可按需设计 |
| 大规模 | 要换 Graph / Iris | 从一开始按格子设计 |

要 RTS 那种每兵几 bit，对象复制通常不够用。要格斗回滚，不要把 Actor 反射布局当每帧存档。

## 中间件对照

| 名字 | 实际模型 |
| --- | --- |
| UE Replication / Iris | 服务器权威对象复制 |
| Unity NGO | NetworkVariable + RPC，默认服务器权威 |
| Unity Netcode for Entities | Ghost 快照 + 预测，偏确定性混合 |
| Photon Fusion 主机模式 | 状态同步 + 可预测 |
| Photon Fusion Shared | 偏客户端权威 |
| Photon Quantum | 确定性输入，默认回滚，不是复制 |
| Mirror / Fish-Net | 类 UE 的行为复制 |

问四件事再选包：要不要确定性、要不要预测、人数上限、谁托管服务器。
不要因为名字带 Photon 就当同一方案。

## 失败模式

| 现象 | 常见原因 |
| --- | --- |
| 只有主机看见门开了 | 门的开关走了本地调用，没走复制属性 / 服务器 RPC |
| 别人能开我的背包 | Controller 或库存对象复制范围过大 |
| 上车后两个人抢方向 | 所有权没交割，双边预测 |
| 远了的人瞬移出现 | 相关性切换没有平滑，或休眠唤醒没带初始全量 |
| 开火偶发没声音 | 不可靠 Multicast 被当可靠逻辑用 |
| 百人服务器卡在 Net | 仍在穷举 Relevancy，没上 Graph / Iris |

## 适用边界

默认适合：已在 UE / Unity 里的动作、射击、合作 PVE、会话生存。先把 Role / Owner / 属性对 RPC 做对，再谈压缩。
人数上百：加 AOI 与复制图。人数上千：分线、分空间，或降低权威粒度。
RTS / 格斗主线：离开这一族，分别去锁步或回滚。
