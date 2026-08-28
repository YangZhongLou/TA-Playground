# 回滚：实现规格

目标：两人 60Hz，本地输入立刻进逻辑，远程先猜「上一拍」，猜错则倒回重演。
概念对照 [../schemes/rollback.md](../schemes/rollback.md)。
代码：`Plugins/NetworkSync/.../NsRollback.*`、`NsSelfTest.cpp`。

不要把守望先锋的预测和解抄到这里。这里没有服务器世界真值，真值是输入磁带。

## 常量

```cpp
constexpr int32 RollbackDtMs = 16;
constexpr int32 InputDelay = 1;
constexpr int32 MaxRollback = 8;
constexpr int32 RollbackSpeed = 3;
```

`INPUT_DELAY` 已设为 1。本地输入写入 `Frame + InputDelay`，当前拍用已排队的本地输入。

## 状态

共用 `FNsWorld`（`X[0]`、`X[1]`、`Rng`）。`Step` 必须深拷贝进 `Saves`。漏掉 `Rng` 一定会在回滚后分叉。

## 每端保存的窗口

下标用逻辑帧号 `f`。见 `FNsRollbackPeer`。

```cpp
TMap<int32, FNsWorld> Saves;     // f -> 执行完 f 之前的状态，即 Step 前
TMap<int32, FNsInputs> Pred;     // 执行 f 时用的输入对
TMap<int32, int8> RealRemote;    // 对端已到达的真输入
TMap<int32, int8> Local;
int32 Frame = 0;                 // 当前正在往前演的预测帧
```

本端是玩家 0 时，`Dx[0]` 永远是真的（加 INPUT_DELAY 后）；`Dx[1]` 可能是猜的。

## 一帧（60Hz）

见 `FNsRollbackPeer::AdvanceLocal`。

```cpp
void AdvanceLocal(int8 Dx, TMap<int32, int8>& OutPacked)
{
    Local.Add(Frame + InputDelay, NsClampDx(Dx));
    // OutPacked = 发送拍 + 前 3 拍冗余
    // Frame - Confirmed > MaxRollback 则 WAIT：不 Step，只重传 Local
    const FNsInputs In = Pair(Frame);  // 本地真 + 远程真或上一拍猜测
    Saves.Add(Frame, World);
    Pred.Add(Frame, In);
    World.Step(In.Dx, Ns::RollbackSpeed);
    ++Frame;
    Trim();
}
```

## 收到对端输入

```cpp
void OnRemote(const TMap<int32, int8>& Packed)
{
    for (const TPair<int32, int8>& Kv : Packed)
    {
        RealRemote.Add(Kv.Key, NsClampDx(Kv.Value));
        if (Pred 里该帧猜的远程输入 != 真输入 && Kv.Key < Frame)
        {
            RollbackFrom(Kv.Key);
        }
    }
}
```

`RollbackFrom(F)`：`World = Saves[F]`，再对 `K in [F, Frame)` 用「有真用真，否则保持当时的猜」重演。
重演过程**不要渲染、不要播声音**。渲染只在本轮 `AdvanceLocal` 结束后做一次。

若 `Frame - F > MaxRollback`：不要继续猜，停住等输入（减速），或断开。

## 包

`P2P_INPUT` payload：

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| frame | u32 | |
| dx | i8 | |
| 再附带前 3 拍的 (frame, dx) | 冗余 | |

乱序：按 `frame` 填 `RealRemote`，不要按到达顺序 `Step`。

## 实现顺序

1. 单机：`INPUT_DELAY=0`，两个 World 喂相同输入，不回滚，checksum 同。
2. 人为让对端输入晚 3 拍到达，猜「上一拍」，对了不回滚，错了 `RollbackFrom`。
3. 接假网络 rtt=80、Drop=0。
4. `INPUT_DELAY=1`，回滚次数应下降。已做。
5. Drop=0.05，靠冗余填洞。超过 `MaxRollback` 置 `bWaiting` 并暂停 `Advance`。已做。

## 验收

```text
ns.SelfTest
```

日志必须含 `rollback frame=`。自动化：`TA.NetworkSync.Rollback`。
含义：故意错猜若干拍后，两端最终 `X[0],X[1],Rng` 一致。
