# 锁步内核：四种互斥实现

`ENsScheme::Lockstep` 下面还有四支内核。和四套主方案一样：**一次只跑一支，不要合成万能锁步器。**

概念轴见 [../schemes/lockstep-variants.md](../schemes/lockstep-variants.md)。
已实现：[lockstep.md](lockstep.md)（乐观 15Hz）、[lockstep-conservative.md](lockstep-conservative.md)（等齐）、[lockstep-comm-turn.md](lockstep-comm-turn.md)（通信回合第一版）、[lockstep-delay.md](lockstep-delay.md)（固定 `d`）。

## 可独立实现的四支

| `ENsLockstepKind` | 节拍 | 规格 | 代码 |
| --- | --- | --- | --- |
| `Optimistic` | 墙钟到点就广播 | [lockstep.md](lockstep.md) | `NsLockstep.*`（已做） |
| `Conservative` | 收齐本拍再 `F` | [lockstep-conservative.md](lockstep-conservative.md) | `NsLockstepWait.*` |
| `CommTurn` | 通信回合 + 两回合提前 | [lockstep-comm-turn.md](lockstep-comm-turn.md) | `NsLockstepTurn.*`（Speed Control 未做） |
| `DelayBased` | 等齐 + 固定 `d` 拍缓冲 | [lockstep-delay.md](lockstep-delay.md) | `NsLockstepDelay.*` |

Speed Control 是 `CommTurn` 的第二里程碑，不是第五支内核。
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
| `INsNet`、`NsCodec`、现有 `ENsMsg` 1/2/6/7 | 自己的 `*Server` / `*Client` / `NsPump*` |
| 身份 `NsPlayerIdFromAddr` | 自测 `NetworkSync.Lockstep.<Kind>.*` |
| Join 快照语义（可选；等齐型可第一版不做 Join） | 缺包策略 |

`C2SInput` 乐观：`win=0`，只带最新 dx。
等齐型：`win=1`，窗口 `seq=目标拍`，不必改字节格式。

## 落地顺序

1. 读本页 + 目标 Kind 那一篇。不要改 `NsLockstep.cpp`。
2. 新头/源、新泵、新自测。`ANsNetManager` 只在 `TickLockstep` 里按 Kind 调新泵。
3. FakeNet Drop=0 对齐 checksum，再加 RTT，再加丢包。
4. 热切 Kind 与热切 Scheme 一样：`InitProtocols` + `ResetWire`。

Manager 上未实现的 Kind 不跑乐观循环，避免「选了保守却在走王者」。
