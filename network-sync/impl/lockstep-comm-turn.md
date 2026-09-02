# 通信回合锁步（帝国时代）

独立内核。不要改 `NsLockstep.cpp`，也不要继承等待类。类名 `FNsLockstepTurnServer` / `FNsLockstepTurnClient`，泵 `NsPumpLockstepTurn*`。

对标：1500 Archers。概念见 [../schemes/lockstep-variants.md](../schemes/lockstep-variants.md)「通信回合」。

Speed Control 是本 Kind 的第二里程碑：主机按回合完成时间改 `FramesPerTurn`（2～6），随 `S2CFrame` 广播。

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
constexpr int32 ResendTurns = 16;    // 已确认进度之前仍保留的安全冗余
constexpr int32 CatchupTurns = 128;  // 单包可装下的最大追赶窗口
```

指令延迟 ≈ `FramesPerTurn * LeadTurns * LogicDtMs` = 396ms。

## 回合与拍

`LogicFrame` 从 0 涨。回合长度可动态变化，不能再用一次除法求回合。
服务端与客户端都维护 `(ExecTurn, ExecTurnStart)` 游标；逻辑帧跨过当前回合长度时只向前推进游标，禁止每拍从回合 0 重扫。
回合 T 内采样到的本机 dx，记为 `Cmd[T][player]`（每回合每槽一个 dx，取该回合最后一次按键）。

执行逻辑拍 k 时：

```text
while k >= ExecTurnStart + TurnLen[ExecTurn]:
    ExecTurnStart += TurnLen[ExecTurn]
    ExecTurn += 1
SrcTurn  = ExecTurn - LeadTurns
I = (SrcTurn >= 0) ? Cmd[SrcTurn] : {0,0}
World.Step(I)
```

回合 0、1 没有两回合前的指令，喂空输入。

## 服务器

汇聚点按**回合**等齐：回合 T 结束前必须收到两人的 `Cmd[T]`（或超时填 0）。
然后广播「回合 T 的 Cmd」，带回合号。

`CollectTurn` 不得跑到 `ExecTurn` 前面无限预取。客户端的 `SendTurn` 也是它最早缺少的命令回合，
服务器按玩家记录 `ClientNeedTurn`，分别补发 `ClientNeedTurn-ResendTurns .. ClosedTurn`。
`Cmds` / `TurnLen` 只能裁掉两个客户端都已越过、且服务器执行游标也不再需要的部分。

某客户端落后达到 `CatchupTurns` 时，服务器置 `bCatchupBlocked`，暂停关闭回合和 `Step`，但继续补发。
客户端收到历史、用下一份 `C2SInput` 推进 `ClientNeedTurn` 后自动恢复。这样离线客户端不会导致历史无限增长，
也不会因为固定 16 回合窗口永久缺帧。128 条追赶历史加 16 条安全冗余仍能装进单个 1200 字节数据报，
代码用 `static_assert` 守住这个条件。

不要每 66ms 广播一次最新摇杆。那是乐观。

上行仍用 `C2SInput`：`win=1`，`seq=Turn`，`dx=本回合命令`。客户端在回合内可重发，后到覆盖。

## 客户端

本地只显示已执行的 `World`。点按到单位移动，要等两回合，这是本 Kind 的手感，不要用表现层先动去藏。

`Logic` 按 `LogicDtMs` 连 `Step`，输入取 `Cmd[ExecTurn-2]`。缺 `Cmd` 则停等，与等齐相同，停在回合边界而不是停在 66ms 边界。
`ExecTurn>=2` 后若当前回合的最终 `TurnLen` 尚未收到也必须停，禁止用本地最新 FPT 猜历史长度。

## 验收

前缀 `NetworkSync.Lockstep.Turn.`。

1. Drop=0：回合 T 发出的 dx，要到逻辑拍 `(T+2)*FramesPerTurn` 才改变 `X`。
2. 拍 `(T+2)*FramesPerTurn - 1` 的坐标仍是旧值。
3. 一人迟交回合命令：全场停在该回合边界。
4. 长跑后 `Cmds` / `TurnLen` 只保留有界窗口，执行回合查询总成本随总帧数线性增长。
5. C1 连续丢失 `CatchupTurns` 个回合：服务器有界停拍；恢复下行后 C1 补齐到同帧同世界，ACK 后服务器继续推进。

## 第二里程碑：Speed Control

仍在 `NsLockstepTurn.*` 里加，不要新 Kind。

主机用各端回合完成时间改 `FramesPerTurn`（整数，范围 2～6）。
变差时立刻加大，好转时每回合最多减 1。全场同一下一回合长度。
`S2CFrame` reserved = `(ClosedLen<<4)|NextFpt`（两档都是 2–6）。内存 `Tick=NextFpt`，
`BaseTick=ClosedLen`。打包窗口每条再带该回合长度。客户端用包里的长度覆盖
`TurnLen[Closed]`，禁止用本地 `FramesPerTurn` 填已关闭回合。
只带 NextFpt 的 reserved（高四位 0）仍解码。
验收：`NetworkSync.Lockstep.Turn.Speed` — 人为把 C1 的处理变慢，全场 `FramesPerTurn` 上升，且两端 `World` 仍同位。
`NetworkSync.Lockstep.Turn.LenDrop` — 升 FPT 后丢掉 C1 的 `S2CFrame`，每回合 dx 不同，Resend 后世界同位且 `TurnLen[Closed]` 与主机一致。

## 第三里程碑：停拍拉齐

另开 `NsLockstepTurnResync.*`，不要新 Kind，不要改 `Tick`。
内核补 `Checksums` / `OnChecksum` / `bDesync`，供停拍泵读取。
恢复用空 `S2CFrame`，不要用回合号 `>= HaltTick`。
规格：[hybrid/turn-resync.md](hybrid/turn-resync.md)。验收：`NetworkSync.Lockstep.Turn.Resync.*`。
