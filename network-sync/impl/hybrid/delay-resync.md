# 固定延迟停拍拉齐包

独立泵 `NsPumpLockstepDelayResync*`。不要改 `NsLockstepDelay.cpp` 的 `Tick`。
不要改 delay `ApplyJoin` 的「仅当 `Tick > ExecFrame`」守卫。
概念：[../../schemes/hybrid/delay-resync.md](../../schemes/hybrid/delay-resync.md)。
LiveSnap 组包、强制回跳、客户端 `HaltTick` 与乐观相同，见 [resync.md](resync.md)。

周期恢复快照必须带刚完成拍的 Hist（`count=1`）。空 `S2CJoinSnap` 只留给 LiveSnap。
`OnChecksum` 对不上只置 `bDesync`。禁止在 `Tick` 里写 `if (bDesync)`。
不要按 Hist 窗口删 `Checksums`：冗余只有 3 拍，第 15 拍的记录会被丢掉，人为分叉会失效。

## 泵

`NsPumpLockstepDelayResyncServer`：Drain 走 delay `OnInput(Id, seq, dx)`，再分支。

| 条件 | 做 | 不做 |
| --- | --- | --- |
| `!bDesync` | 现有 `NsPumpLockstepDelayServer` 的 Tick 路径 | — |
| `bDesync` | 捕获当前世界并 `SendLiveSnap` | **不**调用 `FNsLockstepDelayServer::Tick` |

复用 `FNsLockstepResync` / `FNsLockstepResyncClient`。`CaptureLive(World, Frame)`，并清空 `Inbox`。
lookahead 是停拍前填进当前拍的，留着会让 ack 当拍立刻 `FinishFrame`。
delay 没有 `NextMs`：两槽 ack 后清 `bDesync`、`FinishResume()`，并把 `FrameStartMs = Now`，避免停拍期间攒下的墙钟立刻超时填空。

回跳后 `KnownFrame = Tick - DelayFrames`（客户端字段），使下一发 `C2SInput` 的 seq 等于 `LiveSnapTick`。
客户端另发 `Tick+1 .. Tick+d-1` 的空输入，填回 delay 管线；不要把 `Tick` 本身算进这批空输入，否则 ack 当拍会立刻 `FinishFrame`。

客户端泵：空 `S2CJoinSnap` 且 `Tick>0` 走 `NsApplyDelayResyncSnap`，停拍期间不 `Logic`。
带 `frame >= HaltTick` 的 `S2CFrame` 清停拍。带 Hist 的周期 Join 在停拍期间丢掉。可选 `FNsDoorOpen*`：halt 期间仍 `NsApplyDoorOpen`，不改 delay `Tick`。

`bKickDesyncer` 与乐观停拍包相同。打开后泵把当前拍 Inbox 缺槽标齐并 `dx=0` 再 `Tick`。不要改 delay `Tick`。

## 禁令

- 不要用空 JoinSnap 做 delay 周期恢复。
- 不要调用 delay `ApplyJoin` 做强制回跳。
- 不要在 `FNsLockstepDelayServer::Tick` 里看 `bDesync`。
- 不要和乐观 / 等齐停拍泵合成一支泵。

## 验收

前缀 `NetworkSync.Lockstep.Delay.Resync.`。

1. 人为 checksum 失败后停拍，四份世界相等，`Frame` 不再增加。
2. 客户端已经跑在快照前面仍被拉回。
3. 停拍期间注入 `S2CFrame`，客户端不得再 `Step`。
4. 两槽 ack 后 `bResumed`；ack 当拍 `Frame` 仍等于 `LiveSnapTick`；再收齐输入后继续打拍，两端同位。
5. Resume 后再人为 checksum 失败：新的 `LiveSnapTick`，再次停拍，ack 后再 Resume。
6. 客户端 View 不读服务器 `bCaptured`；空 `S2CJoinSnap` 停拍，随后 `S2CFrame` 恢复。
7. `NetworkSync.Lockstep.Delay.Resync.Udp`：Host Sv+C0 / Client C1 分进程 UDP，C1 只靠包停拍拉齐并恢复。
8. `NetworkSync.Lockstep.Delay.Resync.KickOff`：默认仍停拍。
9. `NetworkSync.Lockstep.Delay.Resync.Kick`：`bKickDesyncer` 时踢分叉槽，继续打拍，迟到输入改不了 `X`。
