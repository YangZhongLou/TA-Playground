# UE5 网络同步

本仓库是 UE5 项目。引擎自带的路径是**服务器权威的对象复制**，属于状态同步。UE5 在旧复制系统之上加了 Replication Graph 与 Iris。

官方入口：[Networking and Multiplayer](https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-and-multiplayer-in-unreal-engine)。

## 角色与框架对象

| 对象 | 网络职责 |
| --- | --- |
| `UNetDriver` / `UNetConnection` | 连接、通道、打包 |
| `AGameMode` | 仅服务器，规则与生成 |
| `AGameState` | 复制给所有人的对局状态 |
| `APlayerController` | Owner-only；输入、HUD、Server RPC 入口 |
| `APlayerState` | 分数、名字、阵营，给所有人 |
| `APawn` | 移动与姿态；拥有者预测，他人插值 |
| `UCharacterMoverComponent` | Mover：模块化移动 + Network Prediction 回滚 |

`ROLE_Authority`、`ROLE_AutonomousProxy`、`ROLE_SimulatedProxy` 决定谁模拟、谁预测、谁插值。弄错 Role 是复制 bug 的第一排查点。

## 属性复制

- `UPROPERTY(Replicated)` 或 `ReplicatedUsing=OnRep_X`。
- `GetLifetimeReplicatedProps` 里声明条件：`COND_OwnerOnly`、`COND_SkipOwner`、`COND_SimulatedOnly` 等。
- 推送模式（`REPNOTIFY_OnChanged`）避免值没变也回调。

能用属性表达的当前值，不要改成每帧 RPC。`OnRep` 是“别人看到变化时”的表现钩子，不要在里面改权威逻辑。

## RPC

| 方向 | 宏 | 谁执行 |
| --- | --- | --- |
| Server | `UFUNCTION(Server, Reliable)` | 服务器，须由 Owner 调用 |
| Client | `UFUNCTION(Client)` | 该对象的 Owner 客户端 |
| NetMulticast | `UFUNCTION(NetMulticast)` | 服务器调用，所有相关客户端执行 |

Multicast 从客户端调用不会自动转发到服务器。可靠 RPC 会头包阻塞。开火、脚步用不可靠；交易、拾取确认用可靠。

## 相关性与休眠

| API / 机制 | 作用 |
| --- | --- |
| `NetCullDistanceSquared` | 距离外视为不相关 |
| `IsNetRelevantFor` | 自定义相关（队伍、房间、楼层） |
| `NetUpdateFrequency` / `MinNetUpdateFrequency` | 复制频率 |
| `NetDormancy` | `DORM_DormantAll` 让静止物停更 |
| `FlushNetDormancy` | 被交互时唤醒 |

默认相关性是距离球。开放世界要用网格或 Graph，否则每个连接每 Tick 对所有 Actor 做相关判断。

## Replication Graph

Fortnite 同场大规模时，穷举 `IsNetRelevantFor` 成为 CPU 瓶颈。Replication Graph 按空间与阵营把 Actor 放进预建节点，连接只向订阅的节点取列表。

适合百人、建筑物多、需要按格子订阅的关卡。人数少、对象少时，默认 replicator 更简单，不必上 Graph。

## Iris

UE 5.1 引入的复制后端，目标替换旧 `FRepLayout` 路径。核心变化：

- 用 Handle / 片段描述状态，而不是按 UObject 反射硬走一遍。
- Filter 系统替代部分 Graph 职责。
- 序列化与对象生命周期解耦，便于大规模与预测。

Iris 与旧复制在项目设置里切换。新项目值得默认评估 Iris；已有复杂 Graph 的项目迁移成本单独估。文档：[Iris](https://dev.epicgames.com/documentation/en-us/unreal-engine/iris-in-unreal-engine)。

## 移动与物理

本仓库关卡原型走 **Mover**（UE 5.8 `Engine/Plugins/Experimental/Mover`），不接 CMC。
`UCharacterMoverComponent` 挂在 `APawn` 上；`SetReplicatingMovement(false)`，由 Network Prediction 按共享时间线预测、缓冲输入、广播状态再决定是否 rollback+resim。
不要每帧复制 `SetActorLocation`。

CMC 是 `ACharacter` 上那套客户端 RPC 移步、服务器立刻重放再纠错的旧模型。Mover 仍标 Experimental，API 会变；本仓库接受这一点，换移动系统时只动 Mover 侧。

Chaos 物理联网仍在演进。载具、可破坏物不要假设“开了复制就会确定重演”。需要物理回滚时用 ChaosMover liaison，等于另一条后端。

Gameplay Ability System（GAS）有自己的 Prediction Key。技能预测与移动预测是两套账，对不齐会出现“技能放出去但人还在墙外”。

## 本仓库若做联网原型

1. 用 Dedicated Server 或 Listen Server 跑一份权威。
2. 角色移动走 Mover（`UCharacterMoverComponent`），不要 CMC，不要手写位置 RPC。
3. 状态用 `Replicated` 属性；一次性事件用 RPC。
4. 先把 Owner / Role / Relevancy 做对，再谈压缩。
5. 人数或地图撑大之后，再开 Replication Graph 或 Iris。

锁步与回滚在 UE 里没有一等公民。本仓库把逻辑放在插件 `Plugins/NetworkSync`：
`FNsWorld` / `FNsFakeNet` 与三套协议是纯 C++；`ANsNetManager` 只画 debug sphere。
对象复制走 `ANsReplicatedActor`。控制台：`ns.SelfTest`、`ns.SpawnDemo`。
