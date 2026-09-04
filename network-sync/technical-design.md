# NetworkSync 技术方案

本仓库用 UE 5.8 插件 `Plugins/NetworkSync` 对照实现四套实时对战同步。
本文是框架蓝图：回答选哪套、层怎么切、数据归谁、做到哪一步。
协议概念见 `schemes/`。可编码规格见 `impl/`。线上字节见 [impl/packet-format.md](impl/packet-format.md)。

读者应在 30 秒内知道：四套协议用 `ENsScheme` 切换；逻辑是整数固定步；自研协议走 `INsNet`；复制走引擎 `UNetDriver`。

## 文档怎么分层

| 层 | 读什么 | 写什么 | 不写什么 |
| --- | --- | --- | --- |
| 问题 | [overview.md](overview.md) | 拓扑 / 权威 / 协议三层模型 | 类名、包布局 |
| 概念 | `schemes/`、`cases/`、[comparison.md](comparison.md) | 四种方案机理与品类 | 插件 API |
| 框架 | 本文 | 决策、分层、模块边界、验收范围 | 逐字节、逐行主循环 |
| 规格 | `impl/` | 常量、循环、包语义、怎么跑 | 产品对比 |
| 线上布局 | [impl/packet-format.md](impl/packet-format.md) | 24 字节头与九种 payload | 策略论述 |

改协议语义：先改规格，再改代码，最后改本文的不变量表。
改字节：只改 `packet-format.md` 与 `NsCodec.cpp`。

## 问题与约束

实时对战要在延迟、丢包、带宽和不可信客户端下，让多台机器对同一场模拟达成可玩的一致。

| 约束 | 对本原型的含义 |
| --- | --- |
| 延迟 | 假网络默认 RTT 80ms；手感靠乐观节拍或预测，不靠 TCP |
| 丢包 | 应用层冗余或 ACK；传输层是不可靠数据报 |
| 带宽 | 两人一维整数世界；目标是把协议走通 |
| 信任 | 锁步 / 回滚各端自模拟；状态同步与复制以服务器为真 |
| MTU | 每个 UDP 数据报 ≤ 1200 字节，禁止 IP 分片 |

品类决定协议。MOBA 兵线用锁步；射击用权威快照；格斗用回滚；UE 关卡原型用复制。

## 选型结论

四套方案同时保留，用 `ENsScheme` 切换。不要合成「万能同步器」。
锁步内部再用 `ENsLockstepKind` 切四支内核，同样互斥。新 Kind 另开类型和泵，禁止在 `FNsLockstepServer::Tick` 里分支。

| 方案 | 传什么 | 权威 | 本插件拓扑 | 对标 |
| --- | --- | --- | --- | --- |
| 乐观帧同步 | 输入帧 | 各端同一份确定性 `FNsWorld` | 逻辑服务器 + 两客户端 | 王者荣耀 15Hz |
| 权威状态同步 | 快照 / 增量 | 服务器 pawn | 逻辑服务器 + 两客户端 | Source / 守望先锋一类 |
| 回滚 | 对等输入 | 输入磁带，无服务器世界 | P2P 两端 | GGPO |
| UE 复制 | 属性 + RPC | `ROLE_Authority` | Listen + Client | 引擎默认路径 |

守望先锋口头上的 rollback 是预测和解，不是 GGPO。本插件 `Rollback` 分支只实现后者。

本仓库默认联机产品路径仍是：**Dedicated / Listen 权威 + Actor 复制**。
锁步与回滚只在明确做 RTS/MOBA 或格斗时启用，且逻辑必须离开 Chaos / 动画图。

## 分层架构

三层禁止串写：渲染 `DeltaSeconds` 不得进入逻辑 `Step`。

```text
表现     ANsNetManager（debug 球 / lerp）
         ANsReplicatedActor  ANsDoor
           | 只读 FNsWorld，或走引擎复制
泵       NsPump*（Drain + 分发消息，Manager 与自测共用）
协议     FNsLockstep*   FNsStateSync*   FNsRollbackPeer
           | 固定毫秒步，整数状态
传输     INsNet
           FNsSeqWindow
           FNsFakeNet   延迟 / 抖动 / 丢包 / 序号
           FNsUdpNet    本机或分进程数据报（IPv4）
           NsStun       Binding 查询映射地址（非 TANS）
         UNetDriver     仅 Replication
```

| 层 | 职责 | 时钟 |
| --- | --- | --- |
| 表现 | 画、插值、按键采样 | 渲染帧 |
| 协议 | 组包、解包、模拟步进 | `LogicDtMs` / `SimDtMs` / `RollbackDtMs` |
| 传输 | 投递字节，不解释玩法 | `Now` 毫秒 |

锁步、状态同步、回滚在 PIE 里是**单进程双端**（一份 Manager 模拟 Sv/C0/C1）。
Replication 才走引擎 `UNetDriver`。勾选 `bUseUdp` 后，前三套可改走真 socket。

## 模块与数据所有权

| 模块 | 写什么 | 不写什么 |
| --- | --- | --- |
| `FNsWorld` | `X[2]`、`Rng` | 网络、渲染 |
| `NsPacket` / `ENsMsg` | 报文字段 | 投递 |
| `NsCodec` | 小端字节 ↔ `FNsPacket` | 玩法语义 |
| `FNsSeqWindow` | 每对 `(Dst,Src)` 的 seq / 去重窗 | 消息 type |
| `INsNet` | `Send` / `Drain` / `Now` | 玩法 |
| `FNsFakeNet` / `FNsUdpNet` | 投递、延迟、拆 MTU | 玩家身份解释 |
| `NsPump*` | 从 `INsNet` 取出包并喂协议 | 编解码 |
| `FNsLockstep*` | `Latest`、`Hist`、`Buf`、`ExecFrame` | 快照增量 |
| `FNsStateSync*` | Inbox、权威 x、PredX、插值缓冲 | 输入磁带回滚 |
| `FNsRollbackPeer` | `Local` / `RealRemote` / `Saves` | 服务器世界 |
| `ANsNetManager` | 选方案、累加器、按键、画球 | 编解码 |
| 复制 Actor | `Counter` / `bOpen` | `INsNet` |

玩家身份：锁步与状态同步用 `NsPlayerIdFromAddr(Src)`。payload 里的 `player_id` 不可信。

## 公共内核

所有自研协议共用 `NsTypes.h`。逻辑状态禁止 float。

| 常量 | 值 | 用途 |
| --- | --- | --- |
| `PlayerCount` | 2 | 两人局 |
| `LogicDtMs` | 66 | 锁步 15Hz |
| `SimDtMs` / `RollbackDtMs` | 16 | 状态同步 / 回滚 ~60Hz |
| `RedundantFrames` | 3 | 锁步下行与回滚 P2P 冗余窗 |
| `MaxPacketBytes` | 1200 | 单数据报上限（含 24 字节头） |
| `MaxInboxAhead` | 64 | 状态同步 Inbox 相对 `LastSeq` 的上限 |
| `PacketMagic` | `0x54414E53` | 头 magic，小端 `53 4E 41 54` |

`FNsWorld::Step`：每槽 `X += clamp(dx,-1,1) * Speed`，然后
`Rng = (Rng * 1103515245 + 12345) & 0x7FFFFFFF`。
`Checksum` 用 `int64` 再截断到 `uint32`，保证负数包装与各端一致。

硬规则：

1. 逻辑只用整数毫秒步长，禁止 `1/15` 浮点累加。
2. 先单机两份 `World` 喂同一输入，checksum 必须相同。
3. 先 drop=0，再加 80ms RTT，再加丢包。
4. 插值只读 `World`，不准写回。
5. 超 1200 字节由 `NsSplitForMtu` 拆成多个完整包，各自独立丢包。

## 传输契约

开发期用 `FNsFakeNet`。同一 `INsNet` 可换成 `FNsUdpNet`。

| 能力 | 约定 |
| --- | --- |
| 不可靠 | `Send` 先盖 seq 再按 `Drop` 丢弃；丢掉的序号仍消耗 |
| 乱序 | jitter 改 `DeliverAt`；接收端 32-bit 窗去重，不重排玩法 |
| 身份 | `Src`/`Dst` 不进字节；UDP 用 `SetPeer` 的 IP:port 反查 |
| 解析失败 | `MakeDest` 返回 false，不回落到 loopback |
| Bind | 已有 peer 端口时不覆盖成「自己」 |

地址：`Sv` / `C0` / `C1`。`BindLoopback` 开三端口。
`ANsNetManager` 勾选 `bUseUdp`：`LocalMesh` 走三端口；`Host` / `Client` 只绑本端，对端填 `UdpRemoteHost`。
`UdpBasePort=0` 时默认 27000、27001、27002。`bUdpLan` 绑 `0.0.0.0`。

| `ENsMsg` | 方向 | 用途 |
| --- | --- | --- |
| `C2SInput` | 客 → 服 | 锁步最新 dx，或状态同步输入窗口 |
| `S2CFrame` | 服 → 客 | 锁步输入帧 + 前 3 拍 |
| `S2CSnapshot` | 服 → 客 | 全量或相对 ACK 基的增量 |
| `C2SSnapAck` | 客 → 服 | 已应用 tick；`Tick=0` 请求全量 |
| `P2PInput` | 对等 | 回滚输入 + 前 3 拍 |
| `C2SChecksum` | 客 → 服 | 锁步每 15 拍校验 |
| `S2CJoinSnap` | 服 → 客 | 锁步重连：世界 + 快照之后的输入 |
| `S2CDoorOpen` | 服 → 客 | 锁步加门开关，不带 pawn `X` |
| `C2SFrameNack` | 客 → 服 | 锁步点名缺拍；回复 `S2CFrame` 或 Join |
| `C2SFire` | 客 → 服 | 状态同步倒带开火；`Tick` 为上报 RTT |

逐字节布局以 [impl/packet-format.md](impl/packet-format.md) 为准。

## 四套协议

每套只列不变量。主循环与包字段见对应 `impl/*.md`。

### 乐观帧同步

服务器按墙钟 15Hz 广播，不等齐新包。缺输入沿用 `Latest`。

| 不变量 | 含义 |
| --- | --- |
| 禁止跳帧 | 缺 `ExecFrame` 时不得执行 `+1`；乱序只进 `Buf` |
| 冗余 | 每个 `S2CFrame` 带本拍与前 3 拍 |
| 身份 | `OnInput(NsPlayerIdFromAddr(Src), dx)` |
| 加入 | 每 75 拍存 `SnapWorld`；每 4 拍 `SendJoin` 两次 |
| 按号 NACK | `Buf` 有未来拍而缺 `ExecFrame` 时发 `C2SFrameNack`；`OnNack` 不进 `Tick` |
| ApplyJoin | 仅当 `Tick > ExecFrame` 时跳世界；只丢掉 `< Tick` 的 `Buf`，保留未来帧 |
| 校验 | 每 15 拍 `C2SChecksum`；对不上则 `bDesync`；缺记录则忽略（迟到） |

停拍拉齐不得改 `ApplyJoin`，也不得在 `Tick` 里看 `bDesync`。见 [impl/hybrid/resync.md](impl/hybrid/resync.md)。

中途加入会掺状态快照，不再是纯输入锁步。
代码：`NsLockstep.*`。规格：[impl/lockstep.md](impl/lockstep.md)。
四支内核：乐观 `NsLockstep.*`、等齐 `NsLockstepWait.*`、通信回合 `NsLockstepTurn.*`（含 Speed Control）、delay `NsLockstepDelay.*`。见 [impl/lockstep-kinds.md](impl/lockstep-kinds.md)。
与状态同步结合按包拆开，见 [impl/hybrid/README.md](impl/hybrid/README.md)。不要在乐观 `Tick` 里双写 `X`。

### 权威状态同步

服务器 60Hz 模拟，每 3 拍发快照（20Hz）。客户端预测自己、插值别人。

| 不变量 | 含义 |
| --- | --- |
| 有序消费 | Inbox 只应用 `LastSeq+1`，禁止 latest-wins 跳号 |
| Inbox 上限 | `seq > LastSeq + MaxInboxAhead` 丢弃 |
| 增量基 | `BaseTick` 必须是客户端 ACK 过的 tick |
| 缺基 | 连发 `C2SSnapAck Tick=0`；服务器把 `LastAck` 置 0，下一份全量 |
| 旧快照 | `P.Tick <= LastAckedTick` 则忽略 |
| 插值 | `t_show = now - 100ms`；禁止外推；自己走 `PredX` |
| 倒带 | `RewindX`：`ping/2+100`，超过 220ms 用当前 x |
| 开火 | `C2SFire` Drain `OnFire`：倒带受害者，`|ΔX|<=HitRange` 则 `Hits[shooter]++` |

不要把这套和解叫回滚。这里有服务器真值。
代码：`NsStateSync.*`。规格：[impl/state_sync.md](impl/state_sync.md)。

### GGPO 式回滚

两端 60Hz。本地输入（+1 拍延迟）立刻进逻辑；远程先猜上一拍，猜错从存档重演。

| 不变量 | 含义 |
| --- | --- |
| 真值 | 输入磁带，不是服务器世界 |
| Confirmed | 从 `InputDelay-1` 起连续到达的最大远程帧；禁止跳过空洞 |
| WAIT | `Frame - Confirmed > 8` 则不 Step，只重传 |
| 失败回滚 | `RollbackFrom` 失败（太远或无存档）则不抬 Confirmed |
| 渲染 | 本轮 `Advance` / `OnRemote` 全部结束后画一次 |

两端最终 `X[0],X[1],Rng` 必须一致。
代码：`NsRollback.*`。规格：[impl/rollback.md](impl/rollback.md)。

### UE 对象复制

不自研快照。走 `UPROPERTY(Replicated)` + Server RPC。概念见 [unreal.md](unreal.md)。

| Actor | 复制内容 | 输入 |
| --- | --- | --- |
| `ANsReplicatedActor` | `Counter` | 各端按 `E` → 自己的 `ANsInputProxy` Server RPC |
| `ANsDoor` | `bOpen` | 各端按 `F` → 同上 |
| `ANsInputProxy` | 无属性 | 每名 PC 一份，Owner 才能调 Server RPC |

只在 `HasAuthority` 时 spawn Counter / Door / Proxy。Counter / Door 不 `SetOwner`。
本插件不接入角色移动（Mover / CMC）。复制只演示属性和 RPC。

规格：[impl/replication_ue.md](impl/replication_ue.md)。

## 运行时

`ANsNetManager` 每套协议用自己的固定步累加器，不跟渲染帧 1:1 步进。
运行中改 `Scheme` 会 `ApplyScheme`：重置协议对象、`INsNet::ResetSession`（假网络队列与序号、`Now=0`）、若勾选 UDP 则按新方案重新 `BindUdp`。
切到 Replication 会 spawn 演示 Actor；切走会 Destroy。

| Scheme | 玩家 0 | 玩家 1 | 画面 |
| --- | --- | --- | --- |
| Lockstep | A / D | 方向键 | 两球，锁步 lerp |
| StateSync | A / D | 方向键 | 自己 PredX，别人插值 |
| Rollback | A / D | 方向键 | 回滚结束后的 World |
| Replication | E / F | 观察复制 | Counter + 门 |

控制台：

1. `ns.SelfTest` — 假网络三套协议、编解码、UDP、压力长跑；日志含 `NetworkSync self-test OK`。
2. `ns.DropRate [0-1] [count]` — 标定假网络丢包率。
3. `ns.SpawnDemo` — 生成 Manager；改 `Scheme`，可勾选 `bUseUdp` 与 `UdpRole`。
4. `ns.RendezvousHub [port]` — 第三进程会合助手（默认 3479）；`ns.RendezvousHubStop` 停。
5. Session Frontend：`NetworkSync.*`。

引擎：UE 5.8。`TA-Playground.uproject` 的 `EngineAssociation=5.8`。

## 测试金字塔

自动化过滤器 `NetworkSync`。`ns.SelfTest` 跑同一批自测函数。RNG 种子为 1。

| 层 | 前缀 | 覆盖 |
| --- | --- | --- |
| 内核 | `World.*` | 确定性、clamp、Reset |
| 编解码 | `Codec.*` | 往返（含 Frame/Checksum/JoinSnap）、拒收、MTU 拆包（S2C/Join/C2S/P2P） |
| 传输 | `FakeNet.*`、`Udp.*`、`Stun.*` | 序号窗、丢包延迟、**丢包率标定**、环回、对等、分进程锁步/状态同步/回滚、突发、STUN Binding 编解码与环回、已知对端 Indication 打洞、会合换映射地址、对端 Binding 连通检查、TURN Allocate 查中继地址、CreatePermission 放行对端 IP、ChannelBind / ChannelData 转发 TANS、直连失败时 `Send` / `Drain` 走 TURN ChannelData、ICE 候选清单（host / srflx / relay）、会合交换 `NSIC`、host/srflx 配对检查、Host 先检查再 USE-CANDIDATE 提名、进程内 `FNsRendezvousHub` 转发 `NSIC` / `NSRV`、`ns.RendezvousHub` 后台会合 |
| 锁步 | `Lockstep.*` | 乐观：干净、Drop、Join、空洞、按号 NACK、分叉。等齐：`Lockstep.Wait.*`（含 Join、按号 NACK、停拍拉齐、超时踢人）。通信回合：`Lockstep.Turn.*`（含 Speed / Recovery / 停拍拉齐）。delay：`Lockstep.Delay.*`（含停拍拉齐、按 RTT 调 `d`、按号 NACK） |
| 结合 | `Lockstep.Resync.*` / `Lockstep.Wait.Resync.*` / `Lockstep.Turn.Resync.*` / `Lockstep.Delay.Resync.*` / `LockstepDoor.*` | 四支锁步停拍强制回跳与恢复（含包驱动 Host/Client 与 `*.Resync.Udp`）；四支停拍均可改踢分叉槽（`*.Resync.Kick*`）；假网络四支锁步走各自停拍泵并叠门；FakeNet 门；检查点用 `Lockstep.Join*`；切段 `SchemeSwitch` / `SchemeApply` |
| 状态同步 | `StateSync.*` | 和解、倒带、倒带开火、nack 全量、Inbox 空洞/上限、长断线排空、旧快照忽略、Src 身份 |
| 回滚 | `Rollback.*` | 干净、WAIT、Confirmed 不跳空洞（前缀/中间）、冲突输入终止态 |
| 运行时 | `Runtime.SchemeSwitch` / `Runtime.SchemeApply` | 热切后锁步不追 `Now`、队列清空；`ApplyScheme` 重建协议且锁步不继承 `PredX` |
| 复制 | `Actors.Cdo` | Door / Counter RPC / InputProxy |
| 压力 | `Stress.*` | 长跑限时（World / Codec / FakeNet / 三套协议） |

改 `Drop` 后若偶发失败，先看冷却循环是否排空在途包。
锁步压力在比对 `World` 前必须两端 `ExecFrame` 对齐。

## 已知边界

已闭环：两人整数世界、假网络字节头、本机与分进程 UDP、STUN Binding 查询映射地址、已知对端 Binding Indication 打洞、会合换映射地址、对端 Binding 连通检查、TURN Allocate 查询中继地址、CreatePermission 放行对端 IP、ChannelBind / ChannelData 转发 TANS、直连失败时 `Send` / `Drain` 走 TURN ChannelData、ICE 候选清单（host / srflx / relay）、会合交换 `NSIC`、host/srflx 配对检查、Host 先检查再 USE-CANDIDATE 提名、进程内会合助手转发 `NSIC` / `NSRV`、`ns.RendezvousHub` 后台会合、ICE 候选 SDP 编解码、三套协议主循环、checksum、有序 Inbox、增量 nack、回滚空洞、锁步周期 Join、乐观/等齐/delay 按号 NACK、四支停拍踢分叉者、状态同步倒带开火、复制整数与门、Owner 输入代理。

| 未做 | 影响 |
| --- | --- |
| 停拍拉齐 | 四支锁步 Manager 走各自停拍泵；Host/Client 靠 LiveSnap 包。通信回合用空 `S2CFrame` 恢复 |
| 锁步加门 | 四支锁步 Manager 已叠 FakeNet `S2CDoorOpen`；不接 `UNetDriver` / `ANsDoor` |
| NAT / STUN | Binding 可查出映射 IPv4:port；会合可换对端地址；Indication 打洞后对端 Binding Request 确认路径；Allocate 可查出 XOR-RELAYED；CreatePermission 可放行对端 IP；ChannelBind 成功且连通检查失败时 `Send` / `Drain` 走 TURN ChannelData。可列出 host / srflx / relay 候选，会合可换 `NSIC` 清单；`IceCheckPairs` 按 host-host 优先检查 host/srflx 对，通了再发 USE-CANDIDATE；Client `IceWaitNominate` 收到提名才 `SetPeer`。`FNsRendezvousHub` / `ns.RendezvousHub` 转发 `NSIC` / `NSRV`。`NsIceSdpEncode` 可把清单写成 SDP，会合不发。`UdpRendezvousHost` 为空时 `UdpRemoteHost` 仍要填 |
| 多人 / AOI | 地址写死 Sv/C0/C1 |
| 开火命中 | 倒带开火已接；无血量 / 无真正射线 |
| 客户端 Owner RPC | 每名 PC 一份 `ANsInputProxy`；Counter / Door 仍不 SetOwner |
| 角色移动 | 不在本插件；玩法项目自选引擎移动系统 |

后续：ICE-CONTROLLING / ICE-CONTROLLED。

## 索引

| 路径 | 内容 |
| --- | --- |
| [overview.md](overview.md) | 拓扑 / 权威 / 协议 |
| [comparison.md](comparison.md) | 优劣与品类 |
| [schemes/README.md](schemes/README.md) | 四篇协议细则 |
| [impl/README.md](impl/README.md) | 实现规格与如何运行 |
| [impl/lockstep-kinds.md](impl/lockstep-kinds.md) | 四支锁步内核如何互斥落地 |
| [impl/hybrid/README.md](impl/hybrid/README.md) | 结合包：检查点 / 切段 / 停拍拉齐 / 等齐、回合、delay 停拍拉齐 / 锁步加门 |
| [impl/packet-format.md](impl/packet-format.md) | 帧字节格式 |
| [cases/compare-three.md](cases/compare-three.md) | 守望先锋 / Dota 2 / 王者 |
| `Plugins/NetworkSync/` | UE 插件源码 |

| 类 | 文件 |
| --- | --- |
| `FNsWorld` | `Public/NsTypes.h` |
| `FNsPacket` / `ENsMsg` | `Public/NsPacket.h` |
| `INsNet` / `FNsSeqWindow` | `Public/NsNet.h` |
| `NsEncodePacket` | `Public/NsCodec.h` |
| `FNsFakeNet` | `Public/NsFakeNet.h` |
| `FNsUdpNet` | `Public/NsUdpNet.h` |
| `NsStun*` | `Public/NsStun.h` |
| `NsPump*` | `Public/NsPump.h` |
| `FNsLockstepServer` / `Client` | `Public/NsLockstep.h` |
| `FNsLockstepWaitServer` / `Client` | `Public/NsLockstepWait.h` |
| `FNsLockstepTurnServer` / `Client` | `Public/NsLockstepTurn.h` |
| `FNsLockstepDelayServer` / `Client` | `Public/NsLockstepDelay.h` |
| `FNsLockstepResync` | `Public/NsLockstepResync.h` |
| `NsPumpLockstepWaitResync*` | `Public/NsLockstepWaitResync.h` |
| `NsPumpLockstepDelayResync*` | `Public/NsLockstepDelayResync.h` |
| `NsPumpLockstepTurnResync*` | `Public/NsLockstepTurnResync.h` |
| `FNsLockstepDoorServer` / `Client` | `Public/NsLockstepDoor.h` |
| `FNsStateSyncServer` / `Client` | `Public/NsStateSync.h` |
| `FNsRollbackPeer` | `Public/NsRollback.h` |
| `ANsNetManager` | `Public/NsNetManager.h` |
| `ANsReplicatedActor` | `Public/NsReplicatedActor.h` |
| `ANsInputProxy` | `Public/NsInputProxy.h` |
| `ANsDoor` | `Public/NsDoor.h` |
