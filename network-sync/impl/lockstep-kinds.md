# 锁步内核：四种互斥实现

`ENsScheme::Lockstep` 下面还有四支内核。和四套主方案一样：**一次只跑一支，不要合成万能锁步器。**

概念轴见 [../schemes/lockstep-variants.md](../schemes/lockstep-variants.md)。
已实现：[lockstep.md](lockstep.md)（乐观 15Hz）、[lockstep-conservative.md](lockstep-conservative.md)（等齐）、[lockstep-comm-turn.md](lockstep-comm-turn.md)（通信回合 + Speed Control）、[lockstep-delay.md](lockstep-delay.md)（固定 `d`）。

## 可独立实现的四支

| `ENsLockstepKind` | 节拍 | 规格 | 代码 |
| --- | --- | --- | --- |
| `Optimistic` | 墙钟到点就广播 | [lockstep.md](lockstep.md) | `NsLockstep.*`（已做） |
| `Conservative` | 收齐本拍再 `F` | [lockstep-conservative.md](lockstep-conservative.md) | `NsLockstepWait.*` |
| `CommTurn` | 通信回合 + 两回合提前 | [lockstep-comm-turn.md](lockstep-comm-turn.md) | `NsLockstepTurn.*` |
| `DelayBased` | 等齐 + 固定 `d` 拍缓冲 | [lockstep-delay.md](lockstep-delay.md) | `NsLockstepDelay.*` |

Speed Control 是 `CommTurn` 的第二里程碑，不是第五支内核。
等齐 Join 是 `Conservative` 的第二里程碑，同样不是新 Kind。
等齐停拍拉齐是第三里程碑：独立泵 `NsPumpLockstepWaitResync*`，规格 [hybrid/wait-resync.md](hybrid/wait-resync.md)。
等齐超时踢人是第四里程碑：`KickAfterStalls`，仍改 `NsLockstepWait.*`，不是新 Kind。
delay 停拍拉齐同样是第三里程碑：独立泵 `NsPumpLockstepDelayResync*`，规格 [hybrid/delay-resync.md](hybrid/delay-resync.md)。
delay 按 RTT 调 `d` 是第四里程碑：`DelayFrames` 字段 + `NsLockstepDelayFromRtt`，不是新 Kind。
乐观按号 NACK 是乐观类后续字段：`C2SFrameNack` + 泵 Drain `OnNack`，不是新 Kind，也不进 `Tick`。
乐观停拍踢分叉者是停拍包第三里程碑：`bKickDesyncer`，泵 Drain，不是新 Kind，也不进 `Tick`。等齐 / 通信回合 / delay 停拍泵同样认这个字段。
通信回合停拍拉齐同样是第三里程碑：独立泵 `NsPumpLockstepTurnResync*`，规格 [hybrid/turn-resync.md](hybrid/turn-resync.md)。
空输入 vs `Latest`、追帧限流，是某支内核里的字段，不是新 Kind。

## 禁止并进 Lockstep 的

| 东西 | 去哪 |
| --- | --- |
| 回滚 / Quantum 默认 | `ENsScheme::Rollback` |
| 表现层先动 | 不要做 |
| P2P 锁步 | 先用 Sv+C0+C1 把 Kind 跑通；P2P 是同一 Kind 换泵，仍不要改乐观类 |
| 与状态同步结合 | [hybrid/README.md](hybrid/README.md)；不要在本 Kind 的 `Tick` 里写快照坐标 |

## 共享、禁止改乐观类

新 Kind **复制清单，另开文件**。不要在 `FNsLockstepServer::Tick` 里 `if (Kind)`。

| 共用 | 各 Kind 自己写 |
| --- | --- |
| `FNsWorld` / `FNsInputs` / `LogicDtMs` | 何时 `Step`、何时广播 |
| `INsNet`、`NsCodec`、现有 `ENsMsg` 1/2/6/7/9 | 自己的 `*Server` / `*Client` / `NsPump*` |
| 身份 `NsPlayerIdFromAddr` | 自测 `NetworkSync.Lockstep.<Kind>.*` |
| Join 快照语义（等齐已做 `Wait.Join`） | 缺包策略 |

`C2SInput` 乐观：`win=0`，只带最新 dx。
等齐型：`win=1`，窗口 `seq=目标拍`，不必改字节格式。

## 落地顺序

1. 读本页 + 目标 Kind 那一篇。不要改 `NsLockstep.cpp`。
2. 新头/源、新泵、新自测。`ANsNetManager` 只在 `TickLockstep` 里按 Kind 调新泵。
3. FakeNet Drop=0 对齐 checksum，再加 RTT，再加丢包。
4. 热切 Kind 与热切 Scheme 一样：`InitProtocols` + `ResetWire`。

Manager 上未实现的 Kind 不跑乐观循环，避免「选了保守却在走王者」。
