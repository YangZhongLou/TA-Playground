# 结合包：落地

概念入口 [../../schemes/hybrid/README.md](../../schemes/hybrid/README.md)。
一支一个目录文件、一套类型、一个测试前缀。不要 `NsHybrid.cpp` 用枚举切各支。

| 包 | 规格 | 类型 | 测试前缀 |
| --- | --- | --- | --- |
| 检查点 | [checkpoint.md](checkpoint.md) | 已在 `NsLockstep.*` | `NetworkSync.Lockstep.Join*` |
| 会话切段 | [session.md](session.md) | 已在 `ANsNetManager` | `NetworkSync.Runtime.SchemeSwitch` / `SchemeApply` |
| 停拍拉齐 | [resync.md](resync.md) | `NsLockstepResync.*` | `NetworkSync.Lockstep.Resync.*` |
| 等齐停拍拉齐 | [wait-resync.md](wait-resync.md) | `NsLockstepWaitResync.*` | `NetworkSync.Lockstep.Wait.Resync.*` |
| 固定延迟停拍拉齐 | [delay-resync.md](delay-resync.md) | `NsLockstepDelayResync.*` | `NetworkSync.Lockstep.Delay.Resync.*` |
| 通信回合停拍拉齐 | [turn-resync.md](turn-resync.md) | `NsLockstepTurnResync.*` | `NetworkSync.Lockstep.Turn.Resync.*` |
| 锁步加门 | [door.md](door.md) | `NsLockstepDoor.*` | `NetworkSync.LockstepDoor.*` |

共用 `INsNet` 与编码。各包自己写泵。禁止 `NsPumpLockstep` 调用 `OnSnap`。
停拍后不调用 `Tick`，直到两槽对 `LiveSnap` checksum ack。
乐观停拍包 `bKickDesyncer` 打开时改为踢分叉槽并继续打拍，默认仍停拍拉齐。等齐 / 通信回合 / delay 停拍泵同样认这个字段。
乐观 `ANsNetManager` 走停拍拉齐泵；等齐走 `NsPumpLockstepWaitResync*`；通信回合走 `NsPumpLockstepTurnResync*`；delay 走 `NsPumpLockstepDelayResync*`。
锁步加门不新增 `ENsScheme`。四支锁步的停拍客户端泵都可叠 `FNsDoorOpen*`。
新线上 type 先改 [packet-format.md](../packet-format.md)。门用 `S2CDoorOpen=8`；停拍拉齐复用 `S2CJoinSnap`。按号补发用 `C2SFrameNack=9`（乐观 / 等齐 / delay；通信回合仍 Resend）。状态同步开火用 `C2SFire=10`。
