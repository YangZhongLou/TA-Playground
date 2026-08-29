# 固定帧延迟锁步（delay-based）

独立内核。不要改 `NsLockstep.cpp`。类名建议 `FNsLsDelayServer` / `FNsLsDelayClient`。

对标：回滚普及前的格斗网战。概念见 [../schemes/lockstep-variants.md](../schemes/lockstep-variants.md)「固定帧延迟」。

算法是「等齐 + 输入提前 `d` 拍」。可以抄 [lockstep-conservative.md](lockstep-conservative.md) 的收集循环，但必须是自己的类型和泵，禁止 `FNsLockstepWaitServer` 子类化。

## 和乐观 / 等齐的差别

| | 乐观 | 等齐 | 本 Kind |
| --- | --- | --- | --- |
| 本地键何时进 `F` | 广播回来的那一拍 | 本拍收齐 | 本机在 `n` 采样，进 `F(n+d)` |
| 高 ping | 只有自己迟 | 全场停 | 全员同一 `d`，一起钝；`d` 不够仍会停等 |
| 延迟 | `RTT + T/2` | 最慢 RTT | `d × LogicDtMs`（再加等齐） |

不要做成「乐观服务器 + 客户端多攒几拍再演」。王者试过，卡，且那只是乐观的字段，不是本 Kind。

## 常量

```cpp
constexpr int32 DelayFrames = 3; // 约 198ms @ 66ms
```

`d` 写死。按 RTT 调 `d` 是后续字段，仍在本 Kind，不是新枚举。

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

## 验收

前缀 `NetworkSync.Lockstep.Delay.`。

1. Drop=0，RTT=0：第 0 拍按下，`X` 在拍 `DelayFrames` 才变。拍 `DelayFrames-1` 仍为 0。
2. 两人对打同一 `d`：两端 `World` 同位，没有「只有高 ping 的人晚结算」。
3. RTT=80、`DelayFrames=3`：多数拍不等超时；把 RTT 加到 400ms 后才频繁 `StallTimeoutMs`。

格斗手感不够再去 `ENsScheme::Rollback`，不要在本 Kind 里猜远程输入。
