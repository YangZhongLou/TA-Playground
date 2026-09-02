# 通信回合停拍拉齐包

独立泵 `NsPumpLockstepTurnResync*`。不要改 `NsLockstepTurn.cpp` 的 `Tick`。
概念：[../../schemes/hybrid/turn-resync.md](../../schemes/hybrid/turn-resync.md)。
LiveSnap 仍是空 `S2CJoinSnap` 且 `Tick>0`，见 [resync.md](resync.md)。

通信回合的 `S2CFrame` key 是回合号，且每拍 Resend 窗口含当前关闭回合。
禁止用 `key >= HaltTick` 当恢复条件：在途 Resend 会提前清停拍。
恢复令牌是 **空 `S2CFrame`**（`count=0`）。平时 `NsTurnSend` 在窗口为空时不发包，这条信号不会和 Resend 撞车。

`OnChecksum` 对不上只置 `bDesync`。禁止在 `Tick` 里写 `if (bDesync)`。
不要按 Cmds 窗口删 `Checksums`。

## 泵

`NsPumpLockstepTurnResyncServer`：Drain 走回合 `OnInput(Id, turn, dx, Now)`，再分支。

| 条件 | 做 | 不做 |
| --- | --- | --- |
| `!bDesync` | 现有 `NsPumpLockstepTurnServer` 的 Tick 路径 | — |
| `bDesync` | 捕获当前世界并 `SendLiveSnap` | **不**调用 `FNsLockstepTurnServer::Tick`，因此也不 Resend |

两槽 ack 后清 `bDesync`、`FinishResume()`，`TurnStartMs = Now`，发空 `S2CFrame`。
ack 当拍 **不要** `Tick`：已关闭的 `Cmds` 足够 `TryStep`，会让 `Frame` 立刻 +1。

回跳后用 `NsLockstepTurnSyncCursor` 从 `TurnLen` 重建 `ExecTurn` / `ExecTurnStart`，并清空 `Cmds`。
`SendTurn` 保持，下一发仍对准 `CollectTurn`。不叠门。

客户端泵：空 `S2CJoinSnap` 且 `Tick>0` 走 `NsApplyTurnResyncSnap`，停拍期间不 `Logic`。
空 `S2CFrame` 清停拍。带回合的 Resend 在停拍期间丢掉。

## 禁令

- 不要用 `NsS2CResumesHalt`（拍号比较）恢复通信回合。
- 不要在 `FNsLockstepTurnServer::Tick` 里看 `bDesync`。
- 不要和乐观 / 等齐 / delay 停拍泵合成一支泵。

## 验收

前缀 `NetworkSync.Lockstep.Turn.Resync.`。

1. 人为 checksum 失败后停拍，四份世界相等，`Frame` 不再增加。
2. 客户端已经跑在快照前面仍被拉回。
3. 停拍期间注入带回合的 `S2CFrame`，客户端不得再 `Step`。
4. 两槽 ack 后 `bResumed`；ack 当拍 `Frame` 仍等于 `LiveSnapTick`；再打拍后两端同位。
5. Resume 后再人为 checksum 失败：新的 `LiveSnapTick`，再次停拍，ack 后再 Resume。
6. 客户端 View 不读服务器 `bCaptured`；空 `S2CJoinSnap` 停拍，空 `S2CFrame` 恢复。
