# 固定帧延迟锁步（delay-based）

独立内核。不要改 `NsLockstep.cpp`。类名 `FNsLockstepDelayServer` / `FNsLockstepDelayClient`，泵 `NsPumpLockstepDelay*`。

对标：回滚普及前的格斗网战。概念见 [../schemes/lockstep-variants.md](../schemes/lockstep-variants.md)「固定帧延迟」。

算法是「等齐 + 输入提前 `d` 拍」。可以抄 [lockstep-conservative.md](lockstep-conservative.md)
的收集循环，但必须是自己的类型和泵，禁止 `FNsLockstepWaitServer` 子类化。

## 和乐观 / 等齐的差别

| | 乐观 | 等齐 | 本 Kind |
| --- | --- | --- | --- |
| 本地键何时进 `F` | 广播回来的那一拍 | 本拍收齐 | 本机在 `n` 采样，进 `F(n+d)` |
| 高 ping | 只有自己迟 | 全场停 | 全员同一 `d`，一起钝；`d` 不够仍会停等 |
| 延迟 | `RTT + T/2` | 最慢 RTT | `d × LogicDtMs`（再加等齐） |

不要做成「乐观服务器 + 客户端多攒几拍再演」。王者试过，卡，且那只是乐观的字段，不是本 Kind。

## 常量

```cpp
constexpr int32 DelayFrames = 3; // 默认约 198ms @ 66ms
```

`DelayFrames` 是内核字段，默认 3。`NsLockstepDelayFromRtt(RttMs)` 按整段 RTT 向上取整到拍数再加 1 拍量化余量，夹在 1～8。客户端用已收 `S2CFrame` 标 `seq`，所以 `d` 必须盖住来回。三端必须同一 `d`，不新开 `ENsMsg`。局中途改 `d` 会让 `seq` 错位，第一版只在开局设。

Manager 默认仍是 3。`HighRtt` 继续用默认 3，证明 `d` 不够会停等。

## 客户端

```text
采样 dx
发 C2SInput：win=1，seq = KnownFrame + DelayFrames，dx
```

`KnownFrame` 是已确认要执行的最大拍（上次 `S2CFrame` 的 latest）。开局从 `DelayFrames` 起标。

前 `d` 拍：服务器用空输入开拍，或客户端约定前 `d` 拍全 0。两端必须同一约定。本规格：**前 `d` 拍强制空输入，不收玩家键。**

## 服务器

与保守相同：只在收到两人针对拍 `n` 的输入（或超时填 0）后 `Step` 并广播。
不要墙钟到点用 `Latest`。

普通 `S2CFrame` 只冗余最近 4 拍，连续丢失后会留下永久缺口。因此服务器每完成 4 拍另发一份
`S2CJoinSnap`，`count=1`，带刚完成拍的 Hist；`exec_frame=Frame+1`、x/rng 是该拍执行后的权威世界。
空 JoinSnap 留给停拍拉齐 LiveSnap。客户端只应用 `exec_frame > ExecFrame` 的新快照，清理更早的缓冲，再从该帧继续；旧快照不得把世界倒退。

`ApplyJoin` 不把 Join 里的 Hist 并进 `Buf`。

## 验收

前缀 `NetworkSync.Lockstep.Delay.`。

1. Drop=0，RTT=0：第 0 拍按下，`X` 在拍 `DelayFrames` 才变。拍 `DelayFrames-1` 仍为 0。
2. 两人对打同一 `d`：两端 `World` 同位，没有「只有高 ping 的人晚结算」。
3. RTT=80、`DelayFrames=3`：多数拍不等超时；把 RTT 加到 400ms 后才频繁 `StallTimeoutMs`。
4. 连续丢掉所有 `S2CFrame`：客户端仍会通过周期快照前进，且两端世界一致。
5. `NsLockstepDelayFromRtt(400)=8`：RTT=400 时 `StallFills=0`，两端同位；按下仍在拍 `d` 才进 `X`。

## 第三里程碑：停拍拉齐

另开 `NsLockstepDelayResync.*`，不要新 Kind，不要改 `Tick`。
内核补 `Checksums` / `OnChecksum` / `bDesync`，供停拍泵读取。
规格：[hybrid/delay-resync.md](hybrid/delay-resync.md)。验收：`NetworkSync.Lockstep.Delay.Resync.*`。

## 第四里程碑：按 RTT 调 `d`

仍在 `NsLockstepDelay.*` 里加，不要新 Kind。
`NsLockstepDelayFromRtt` / `NsLockstepDelayApplyFrames`。验收：`NetworkSync.Lockstep.Delay.FromRtt` / `Adapt`。

格斗手感不够再去 `ENsScheme::Rollback`，不要在本 Kind 里猜远程输入。
