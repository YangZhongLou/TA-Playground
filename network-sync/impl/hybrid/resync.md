# 停拍拉齐包

独立类型 `FNsLockstepResync` / `NsPumpLockstepResync*`。不要改 `NsLockstep.cpp` 的 15Hz `Tick`。
不要改 `ApplyJoin` 的「仅当 `Tick > ExecFrame`」守卫。那是重连。
概念：[../../schemes/hybrid/resync.md](../../schemes/hybrid/resync.md)。

`OnChecksum` 对不上只置 `bDesync`。记录已从 `Checksums` 删掉的迟到 Hash 仍忽略，停拍拉齐不会触发。

## 泵

`NsPumpLockstepResyncServer`：先 Drain（输入 / checksum），再分支。

| 条件 | 做 | 不做 |
| --- | --- | --- |
| `!bDesync` | 现有 `NsPumpLockstepServer` 的 Tick 路径 | — |
| `bDesync` | 捕获当前世界并 `SendLiveSnap` | **不**调用 `FNsLockstepServer::Tick` |

禁止在 `Tick` 里写 `if (bDesync)`。

## 捕获与发送

第一次进入 `bDesync`：

| 字段 | 值 |
| --- | --- |
| `LiveSnap` | 当时的 `Sv.World`（刚打完的拍），不是 `SnapWorld` |
| `LiveSnapTick` | 当时的 `Sv.Frame`（客户端下一拍） |

`SendLiveSnap` 组 `S2CJoinSnap`：`Tick = LiveSnapTick`，`SnapX` / `SnapRng` 来自 `LiveSnap`，`Frames` 空。
对 C0、C1 各调用一次。`SendLiveSnap` 内部与现有 `SendJoin` 一样连发两个数据报，泵不要再套一层「发两次」。

之后每个泵周期可再 `SendLiveSnap` 抗丢包。不要另开 type。落地不改 [packet-format.md](../packet-format.md) 的 Join 布局。

## 强制回跳

`NsApplyResyncSnap(Client, Packet)`（写在 `NsLockstepResync.*`，不要改 `ApplyJoin`）：

1. 无论 `Packet.Tick` 与 `ExecFrame` 谁大，都覆盖 `World` / `PrevX`，`ExecFrame = Packet.Tick`。
2. 清空 `Buf`。
3. 不合并 `Frames`（payload 为空）。

停拍期间（`bCaptured && !bResumed`）`NsPumpLockstepResyncClient`：仅当 `Tick == LiveSnapTick` 且 `Frames` 为空时走 `NsApplyResyncSnap`。周期 Join（`Tick=0` / 旧检查点 / 带尾巴）一律忽略。**忽略** `S2CFrame`；**不**调用 `Logic`。回跳成功后发 `C2SChecksum`。`bResumed` 后走平时 `ApplyJoin` / `OnS2C` / `Logic`。

在途的旧输入帧不得在回跳后再执行。

## 收场

对齐成功：`C0.World`、`C1.World`、`Sv.World`、`LiveSnap` 四份相等，且两客户端 `ExecFrame == LiveSnapTick`。
对齐当拍仍停拍：客户端回跳后发 `C2SChecksum`（`Tick=LiveSnapTick`，Hash 为 `LiveSnap`）。服务器两槽都对上后 `Resume`：清 `bDesync`，`bResumed`，`NextMs = Now + LogicDtMs`（禁止用停拍期间攒下的墙钟追帧）。
`bGiveUp` 后不要 Resume。
未对齐前：保持停拍，不要清 `bDesync`、不要 `Tick`。

逻辑 bug 拉齐后仍会分叉；Resume 后若 checksum 再对不上，再次停拍。

第一版不做按视野裁快照，不做踢人替代。恢复打拍是本包第二里程碑，已做。

## 禁令

- 不要调用现有 `SendJoin`（那是 75 拍检查点）。
- 不要调用现有 `ApplyJoin`（晚到才跳）。
- 不要每秒预防性全量快照。
- 不要和锁步加门共用 `NsHybrid`。
- 不要在 `OnChecksum` 里改 `Latest`、`Frame`、`World`。

## 验收

前缀 `NetworkSync.Lockstep.Resync.`。

1. 人为让 C1 在仍有 checksum 记录时上报错误 Hash，`bDesync`。
2. 泵停拍拉齐后四份世界相等，`ExecFrame == LiveSnapTick == Sv.Frame`，`Sv.Frame` 相对置位前不再增加。
3. 客户端 `ExecFrame` 已大于 `LiveSnapTick` 时仍被拉回（强制回跳）。
4. 置位后注入在途 `S2CFrame`，客户端不得再 `Step`。
5. Drop=0.1：靠 `SendLiveSnap` 双发对齐（与 Join 分片无关，payload 无尾巴）。
6. `ApplyJoin` 单测行为不变：`Tick <= ExecFrame` 时不跳世界。
7. 对齐后再注入 `Tick=0` 的空 Join、或同 Tick 但带 `Frames` 的 Join：世界仍是 `LiveSnap`。
8. 丢掉全部 `S2CJoinSnap`：33 个泵后 `bGiveUp`，`Frame` 不再增加。
9. 对齐且两槽 checksum ack 后：`bResumed`，再过一个 `LogicDtMs` 后 `Frame` 增加，两端 `World` 同位。Ack 当拍 `Frame` 仍等于 `LiveSnapTick`。
