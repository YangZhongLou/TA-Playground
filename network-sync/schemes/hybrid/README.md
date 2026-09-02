# 结合包

帧同步和状态同步的结合**不是一个协议**。每一类切法一个包：自己的概念文、规格、日后的类型和测试前缀。
禁止做成 `NsHybrid` 万能模块，也禁止在 `FNsLockstepServer::Tick` 里分支去写快照。

硬规则（各包共用）：每个逻辑字段只有一个写入者。pawn `X` 若由 `F` 推进，快照不得再写它。

| 包 | 概念 | 规格 | 代码 |
| --- | --- | --- | --- |
| 检查点 | [checkpoint.md](checkpoint.md) | [../../impl/hybrid/checkpoint.md](../../impl/hybrid/checkpoint.md) | 已在 `NsLockstep.*`（JoinSnap） |
| 会话切段 | [session.md](session.md) | [../../impl/hybrid/session.md](../../impl/hybrid/session.md) | 已在 `ApplyScheme` / `ResetWire` |
| 停拍拉齐 | [resync.md](resync.md) | [../../impl/hybrid/resync.md](../../impl/hybrid/resync.md) | `NsLockstepResync.*` |
| 等齐停拍拉齐 | [wait-resync.md](wait-resync.md) | [../../impl/hybrid/wait-resync.md](../../impl/hybrid/wait-resync.md) | `NsLockstepWaitResync.*` |
| 锁步加门 | [door.md](door.md) | [../../impl/hybrid/door.md](../../impl/hybrid/door.md) | `NsLockstepDoor.*`（FakeNet 门） |

不要开「英雄快照 + 小兵锁步」那种双 `F` 包。那是同一战场两套结算。
等齐停拍拉齐不是第五套主方案，也不是新 Kind：同一类切法叠在等齐内核上，单独成包。

落地入口：[../../impl/hybrid/README.md](../../impl/hybrid/README.md)。
锁步主线 [lockstep.md](../lockstep.md)。状态主线 [state-sync.md](../state-sync.md)。
