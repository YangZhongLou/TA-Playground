# 结合包：落地

概念入口 [../../schemes/hybrid/README.md](../../schemes/hybrid/README.md)。
一支一个目录文件、一套类型、一个测试前缀。不要 `NsHybrid.cpp` 用枚举切各支。

| 包 | 规格 | 类型 | 测试前缀 |
| --- | --- | --- | --- |
| 检查点 | [checkpoint.md](checkpoint.md) | 已在 `NsLockstep.*` | `NetworkSync.Lockstep.Join*` |
| 会话切段 | [session.md](session.md) | 已在 `ANsNetManager` | `NetworkSync.Runtime.SchemeSwitch`（仅时钟/队列） |
| 停拍拉齐 | [resync.md](resync.md) | `NsLockstepResync.*` | `NetworkSync.Lockstep.Resync.*` |
| 等齐停拍拉齐 | [wait-resync.md](wait-resync.md) | `NsLockstepWaitResync.*` | `NetworkSync.Lockstep.Wait.Resync.*` |
| 锁步加门 | [door.md](door.md) | `NsLockstepDoor.*` | `NetworkSync.LockstepDoor.*` |

共用 `INsNet` 与编码。各包自己写泵。禁止 `NsPumpLockstep` 调用 `OnSnap`。
停拍后不调用 `Tick`，直到两槽对 `LiveSnap` checksum ack。
乐观 `ANsNetManager` 走停拍拉齐泵；等齐走 `NsPumpLockstepWaitResync*`。
锁步加门不新增 `ENsScheme`，也不叠到等齐。
新线上 type 先改 [packet-format.md](../packet-format.md)。门用 `S2CDoorOpen=8`；停拍拉齐复用 `S2CJoinSnap`。
