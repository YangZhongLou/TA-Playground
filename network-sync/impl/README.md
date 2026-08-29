# 实现规格

概念看 `schemes/`。框架看 [../technical-design.md](../technical-design.md)。
这里写到可以直接开写：常量、结构体、主循环、包语义、验收。
线上逐字节只以 [packet-format.md](packet-format.md) 为准。

实现落在 UE 插件 `Plugins/NetworkSync`。逻辑用整数和固定步长，假网络自测不依赖 PIE 联机。

## 做哪一套

| 目标 | 读 | 代码 |
| --- | --- | --- |
| 锁步哪一支 | [lockstep-kinds.md](lockstep-kinds.md) | `ENsLockstepKind`；乐观是 `NsLockstep.*` |
| 手游 MOBA（乐观 15Hz） | [lockstep.md](lockstep.md) | `NsLockstep.h` / `NsLockstep.cpp` |
| 等齐 | [lockstep-conservative.md](lockstep-conservative.md) | `NsLockstepWait.*`，禁止改乐观类 |
| 通信回合 | [lockstep-comm-turn.md](lockstep-comm-turn.md) | 未做：`NsLsTurn.*` |
| delay | [lockstep-delay.md](lockstep-delay.md) | 未做：`NsLsDelay.*` |
| 射击 / 命令式端游 | [state_sync.md](state_sync.md) | `NsStateSync.h` / `NsStateSync.cpp` |
| 格斗 | [rollback.md](rollback.md) | `NsRollback.h` / `NsRollback.cpp` |
| 检查点 | [hybrid/checkpoint.md](hybrid/checkpoint.md) | 已在 `NsLockstep.*` JoinSnap |
| 会话切段 | [hybrid/session.md](hybrid/session.md) | 已在 `ApplyScheme` |
| 停拍拉齐 | [hybrid/resync.md](hybrid/resync.md) | `NsLockstepResync.*` |
| 锁步加门 | [hybrid/door.md](hybrid/door.md) | `NsLockstepDoor.*`，FakeNet 门，禁止双写 `X` |
| UE 联机原型 | [replication_ue.md](replication_ue.md) | `NsReplicatedActor` / `NsDoor` |

公共传输：[transport.md](transport.md)、[packet-format.md](packet-format.md)、`NsPacket.h`、`NsNet.h`、`NsFakeNet.h`、`NsCodec.h`、`NsPump.h`。
共享世界：`NsTypes.h`（`FNsWorld::Step` / `Checksum`）。

## 硬规则（所有方案共用）

1. 逻辑只用固定步长，禁止用渲染 `deltaTime` 推伤害和位移。
2. 逻辑状态用整数（或定点）。float 只出现在表现层。
3. 先单机跑同一份 `World.Step` 两遍，checksum 必须相同，再接网络。
4. 先做丢包为 0 的假网络，再加延迟，再加丢包。
5. 表现层只读逻辑状态做插值，不准写回。
6. 每个 UDP 数据报 ≤ 1200 字节，禁止让 IP 分片。超长用 `NsSplitForMtu` 拆成多个完整包。

## 建议工期

1. 用 `FNsFakeNet` 让两个地址能互发包。
2. 把 `FNsWorld::Step` 写成纯函数：`(state, inputs) -> state`。
3. 接对应主循环（锁步 / 快照 / 回滚）。
4. 加冗余或 ACK，用 `Drop=0.1` 验收不卡死、不久分叉。
5. 再加表现插值。不要第一步就插值。

## 怎么跑

插件已写入 `TA-Playground.uproject`。编译后：

1. 控制台 `ns.SelfTest` — 单元 + 三套协议 + UDP + 压力长跑，日志 `NetworkSync self-test OK`。
2. `ns.DropRate [0-1] [count]` — 测量假网络实际丢包率（默认 0.1 / 2000 包）。
3. 自动化：`NetworkSync.*`（Session Frontend）。
4. PIE 控制台 `ns.SpawnDemo` — 生成 `ANsNetManager`。A/D 控玩家 0，方向键控玩家 1。
5. 在 Actor 上改 `Scheme`：Lockstep / StateSync / Rollback / Replication（热切会重置协议、时钟、假网络队列，并按方案重绑 UDP）。
   Lockstep 再选 `LockstepKind`。未实现的 Kind（通信回合 / delay）不会偷偷跑乐观循环。
6. 勾选 `bUseUdp`：`LocalMesh` 本机三端口；`Host`/`Client` 填 `UdpRemoteHost` 做两进程。
7. Replication：Listen Server 下按 `E` 增加 `Counter`，按 `F` 开关门。

源码根目录：`Plugins/NetworkSync/Source/NetworkSync/`。
