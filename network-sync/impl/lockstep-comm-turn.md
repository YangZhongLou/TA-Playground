# 通信回合锁步（帝国时代）

独立内核。不要改 `NsLockstep.cpp`，也不要继承等待类。类名建议 `FNsLsTurnServer` / `FNsLsTurnClient`。

对标：1500 Archers。概念见 [../schemes/lockstep-variants.md](../schemes/lockstep-variants.md)「通信回合」。

Speed Control 是本 Kind 的第二里程碑，写在文末。第一版回合长度写死。

## 和乐观 / 等齐的差别

| | 乐观 | 等齐 | 本 Kind |
| --- | --- | --- | --- |
| 时钟 | 每 66ms 一拍逻辑 | 每拍等齐 | 通信回合 ≈ 3 个逻辑拍 |
| 指令何时进 `F` | 本拍广播就执行 | 本拍收齐就执行 | 本回合发出的指令，**两回合之后**才执行 |
| 落后 | 自己追帧 | 全场停 | 第一版全场跟回合走；第二版拉长回合 |

渲染帧仍可 60Hz。逻辑 `Step` 仍用 `LogicDtMs=66`。回合只决定「哪一拍的 dx 生效」。

## 常量

```cpp
constexpr int32 FramesPerTurn = 3;   // 198ms，贴近 200ms，且整除 LogicDtMs
constexpr int32 LeadTurns = 2;       // 回合 T 的指令在回合 T+2 执行
```

指令延迟 ≈ `FramesPerTurn * LeadTurns * LogicDtMs` = 396ms。

## 回合与拍

`LogicFrame` 从 0 涨。`Turn = LogicFrame / FramesPerTurn`。
回合 T 内采样到的本机 dx，记为 `Cmd[T][player]`（每回合每槽一个 dx，取该回合最后一次按键）。

执行逻辑拍 k 时：

```text
ExecTurn = k / FramesPerTurn
SrcTurn  = ExecTurn - LeadTurns
I = (SrcTurn >= 0) ? Cmd[SrcTurn] : {0,0}
World.Step(I)
```

回合 0、1 没有两回合前的指令，喂空输入。

## 服务器

汇聚点按**回合**等齐：回合 T 结束前必须收到两人的 `Cmd[T]`（或超时填 0）。
然后广播「回合 T 的 Cmd」，带回合号，可冗余前 1 个回合。

不要每 66ms 广播一次最新摇杆。那是乐观。

上行仍用 `C2SInput`：`win=1`，`seq=Turn`，`dx=本回合命令`。客户端在回合内可重发，后到覆盖。

## 客户端

本地只显示已执行的 `World`。点按到单位移动，要等两回合，这是本 Kind 的手感，不要用表现层先动去藏。

`Logic` 按 `LogicDtMs` 连 `Step`，输入取 `Cmd[ExecTurn-2]`。缺 `Cmd` 则停等，与等齐相同，停在回合边界而不是停在 66ms 边界。

## 验收

前缀 `NetworkSync.Lockstep.Turn.`。

1. Drop=0：回合 T 发出的 dx，要到逻辑拍 `(T+2)*FramesPerTurn` 才改变 `X`。
2. 拍 `(T+2)*FramesPerTurn - 1` 的坐标仍是旧值。
3. 一人迟交回合命令：全场停在该回合边界。

## 第二里程碑：Speed Control

仍在 `NsLsTurn.*` 里加，不要新 Kind。

主机用各端回合完成时间和 RTT，加权改 `FramesPerTurn`（整数，范围 2～6）。
变差时立刻加大，好转时每回合最多减 1。全场同一 `FramesPerTurn`，随回合广播。
验收：人为把 C1 的处理变慢，全场 `FramesPerTurn` 上升，且两端 `World` 仍同位。
