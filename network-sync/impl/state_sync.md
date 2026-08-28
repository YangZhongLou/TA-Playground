# 状态同步：实现规格

目标：服务器权威，20Hz 快照，远程插值，本地预测和解。整数坐标。
概念对照 [../schemes/state-sync.md](../schemes/state-sync.md)。
代码：`Plugins/NetworkSync/.../NsStateSync.*`、`NsSelfTest.cpp`。

## 常量

```cpp
constexpr int32 SimDtMs = 16;      // 1000/60 的近似
constexpr int32 SendEvery = 3;     // 60/3 = 20Hz 快照
constexpr int32 StateSpeed = 4;    // 每模拟拍
constexpr int32 InputWindow = 8;   // 未确认输入重传窗口
```

第一版不做真正的射线命中。预留 `history[tick] = positions` 即可。

## 状态

```cpp
struct FNsPawn
{
    int32 X = 0;
    int32 LastSeq = 0;
};

class FNsStateSyncServer
{
    int32 Tick = 0;
    FNsPawn Pawns[Ns::PlayerCount];
    // PendingSeq / PendingDx / bHasPending：本拍该玩家最新输入
};
```

服务器：`World` 一份。客户端：`PredX` 一份用于预测自己；另有 `SnapTick` / `SnapX0` / `SnapX1` 用于画别人。

## 包 payload

### `C2S_INPUT`

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| player_id | u8 | |
| seq | u32 | 单调，重传可重复同一 seq |
| dx | i8 | |

客户端每个模拟拍最多发 1 次。丢包：重复发「尚未被快照确认的 seq 起」的窗口（最多 8 个 Input）。
实现里一次打包 `SeqWindow` + `DxWindow`。

### `S2C_SNAPSHOT`

第一版发全量，不做增量。增量在两端全量跑通后再加。

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| tick | u32 | 服务器模拟拍号 |
| base_tick | u32 | 增量基；全量填 0 |
| player_count | u8 | |
| 每玩家：x, last_processed_seq | i32, u32 | |

### `C2S_SNAP_ACK`

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| tick | u32 | 已收到并应用的最大快照 tick |

服务器按连接记住 `last_ack_tick`。第二版增量：只发 `x` 与基快照的差。基必须是 ACK 过的 tick，不是「上次发出的 tick」。

## 服务器循环

见 `FNsStateSyncServer::Sim`。

```cpp
void Sim(FNsFakeNet& Net)
{
    ++Tick;
    for (int32 i = 0; i < Ns::PlayerCount; ++i)
    {
        if (bHasPending[i] && PendingSeq[i] > Pawns[i].LastSeq)
        {
            Pawns[i].X += static_cast<int32>(PendingDx[i]) * Ns::StateSpeed;
            Pawns[i].LastSeq = PendingSeq[i];
        }
        bHasPending[i] = false;
    }
    if (Tick % Ns::SendEvery == 0)
    {
        // broadcast SnapX / SnapSeq
    }
}
```

同一 `seq` 处理两次：第二次 `seq > LastSeq` 为假，忽略。这是幂等。

## 客户端：别人怎么画

见 `FNsStateSyncClient::OnSnap` / `UpdateRemoteDraw`。
收到快照后发 `C2SSnapAck`。服务器按 ACK 基发增量（`BaseTick != 0` 时 `SnapX` 是差）。
远程按墙钟：`t_show = now - InterpDelayMs`，两份快照之间 lerp，不外推。

```cpp
void OnSnap(int32 Tick, const int32 Xs[2], const int32 LastSeqs[2])
{
    // 丢掉 seq <= LastSeqs[PlayerId] 的 Unacked，从 Xs[PlayerId] 重放剩余
    // 别人：两份快照则 RemoteDrawn = (A+B)/2，否则画最新一份
}
```

**自己的 player_id 不要走 lerp。** 走下一节预测。

没有两份快照时：画最新一份，或停在最后。不要外推，等第二份到了再开 lerp。

## 客户端：自己怎么预测

```cpp
void LocalTick(FNsFakeNet& Net, int8 Dx)
{
    ++Seq;
    UnackedSeq.Add(Seq);
    UnackedDx.Add(NsClampDx(Dx));
    PredX += NsClampDx(Dx) * Ns::StateSpeed;
    // 发送最近 InputWindow 条
}

void OnSnap(...)
{
    PredX = Xs[PlayerId];
    for (int8 D : UnackedDx)  // 仅 seq > last_processed_seq
    {
        PredX += D * Ns::StateSpeed;
    }
}
```

和解用快照里的 `last_processed_seq`，不要用「我发出去多久了」去猜服务器处理到哪。

## 滞后补偿（骨架，可先空实现）

开火时：`RewindX(player, ping_ms)`。`back_ms = ping/2 + interp_delay`，`back_ms > 220` 则用当前坐标。

## 实现顺序

1. 服务器模拟 + 全量快照，客户端无预测、无插值，直接画最新快照（会抖）。
2. 远程插值，本地仍直接画最新（自己会感到延迟）。
3. 本地预测 + `last_processed_seq` 和解。自己跟手，别人平滑。
4. `C2S_SNAP_ACK` + 增量。已做。
5. 历史缓冲 + `RewindX` 命中倒带。已做（无开火，只留倒带查询）。

## 验收

```text
ns.SelfTest
```

日志必须含 `state-sync tick=`。自动化：`TA.NetworkSync.StateSync`。
含义：远端插值有值、本地预测在快照到达后与服务器 x 和解一致。
