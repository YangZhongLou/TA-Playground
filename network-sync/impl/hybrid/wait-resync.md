# 等齐停拍拉齐包

独立泵 `NsPumpLockstepWaitResync*`。不要改 `NsLockstepWait.cpp` 的 `Tick`。
不要改等齐 `ApplyJoin` 的「仅当 `Tick > ExecFrame`」守卫。
概念：[../../schemes/hybrid/wait-resync.md](../../schemes/hybrid/wait-resync.md)。
LiveSnap 组包、强制回跳、客户端 `HaltTick` 与乐观相同，见 [resync.md](resync.md)。

`OnChecksum` 对不上只置 `bDesync`。禁止在 `Tick` 里写 `if (bDesync)`。

## 泵

`NsPumpLockstepWaitResyncServer`：Drain 走等齐 `OnInput(Id, seq, dx)`，再分支。

| 条件 | 做 | 不做 |
| --- | --- | --- |
| `!bDesync` | 现有 `NsPumpLockstepWaitServer` 的 Tick 路径 | — |
| `bDesync` | 捕获当前世界并 `SendLiveSnap` | **不**调用 `FNsLockstepWaitServer::Tick` |

复用 `FNsLockstepResync` / `FNsLockstepResyncClient`。`CaptureLive(World, Frame)`。
等齐没有 `NextMs`：两槽 ack 后清 `bDesync`、`FinishResume()`，并把 `FrameStartMs = Now`，避免停拍期间攒下的墙钟立刻超时填空。

客户端泵：空 `S2CJoinSnap` 且 `Tick>0` 走 `NsApplyWaitResyncSnap`，停拍期间不 `Logic`。带 `frame >= HaltTick` 的 `S2CFrame` 清停拍。可选 `FNsDoorOpen*`：halt 期间仍 `NsApplyDoorOpen`，不改等齐 `Tick`。

`bKickDesyncer` 与乐观停拍包相同：默认仍停拍；打开后 checksum 分叉槽置 `Resync.Alive` 与 `WaitSv.Alive` 为 false，继续 `Tick`。不要改等齐 `Tick`。

## 禁令

- 不要调用等齐 `SendJoin` 当 LiveSnap。
- 不要调用等齐 `ApplyJoin` 做强制回跳。
- 不要在 `FNsLockstepWaitServer::Tick` 里看 `bDesync`。
- 不要和乐观 `NsPumpLockstepResync*` 合成一支泵。

## 验收

前缀 `NetworkSync.Lockstep.Wait.Resync.`。

1. 人为 checksum 失败后停拍，四份世界相等，`Frame` 不再增加。
2. 客户端已经跑在快照前面仍被拉回。
3. 停拍期间注入 `S2CFrame`，客户端不得再 `Step`。
4. 两槽 ack 后 `bResumed`；ack 当拍 `Frame` 仍等于 `LiveSnapTick`；再收齐输入后继续打拍，两端同位。
5. Resume 后再人为 checksum 失败：新的 `LiveSnapTick`，再次停拍，ack 后再 Resume。
6. 客户端 View 不读服务器 `bCaptured`；空 `S2CJoinSnap` 停拍，随后 `S2CFrame` 恢复。
7. `NetworkSync.Lockstep.Wait.Resync.Udp`：Host Sv+C0 / Client C1 分进程 UDP，C1 只靠包停拍拉齐并恢复。
8. `NetworkSync.Lockstep.Wait.Resync.KickOff`：默认仍停拍。
9. `NetworkSync.Lockstep.Wait.Resync.Kick`：`bKickDesyncer` 时踢分叉槽，继续打拍，迟到输入改不了 `X`。
