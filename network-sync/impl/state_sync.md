# 状态同步：实现规格

目标：服务器权威，20Hz 快照，远程插值，本地预测和解。整数坐标。
概念对照 [../schemes/state-sync.md](../schemes/state-sync.md)。
代码：`Plugins/NetworkSync/.../NsStateSync.*`、`NsSelfTest.cpp`。

## 常量

```cpp
constexpr int32 SimDtMs = 16;      // 1000/60 的近似
constexpr int32 SendEvery = 3;     // 60/3 = 20Hz 快照
constexpr int32 StateSpeed = 4;    // 每模拟拍
constexpr int32 HistoryTicks = 64;
constexpr int32 MaxInboxAhead = HistoryTicks;
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
    TMap<int32, int8> Inbox[Ns::PlayerCount];  // seq -> dx，按 LastSeq+1 顺序消费
    int32 LastAck[Ns::PlayerCount] = {};
};
```

服务器：`World` 一份。客户端：`PredX` 一份用于预测自己；另有 `SnapTick` / `SnapX0` / `SnapX1` 用于画别人。

## 包 payload

字节级布局见 [packet-format.md](packet-format.md)。快照带权威 x。

### `C2SInput`

standalone `dx` 字节写 0。真输入在窗口：最多 8 个未确认 `(seq, dx)`。
服务器把 `seq > LastSeq` 且 `seq <= LastSeq + MaxInboxAhead` 的条目放进 Inbox；更远的序号直接丢弃，防止空洞把 Inbox 撑爆。模拟时只应用 `LastSeq+1`，禁止跳号 latest-wins。

### `S2CSnapshot`

每 3 个模拟拍发一次。`base_tick=0` 为全量 x；否则 x 是相对已 ACK 快照的差。
每人还带 `last_processed_seq`，客户端用来丢掉已确认预测。

### `C2SSnapAck`

`player_id` + 已应用的快照 tick。连发两次。
基必须是 ACK 过的 tick，不是「上次发出的 tick」。

## 服务器循环

见 `FNsStateSyncServer::Sim`。

```cpp
void Sim(INsNet& Net)
{
    ++Tick;
    for (int32 i = 0; i < Ns::PlayerCount; ++i)
    {
        for (;;)
        {
            const int32 Next = Pawns[i].LastSeq + 1;
            const int8* Found = Inbox[i].Find(Next);
            if (!Found)
            {
                break;
            }
            Pawns[i].X += static_cast<int32>(*Found) * Ns::StateSpeed;
            Pawns[i].LastSeq = Next;
            Inbox[i].Remove(Next);
        }
        HistX[i][Tick % Ns::HistoryTicks] = Pawns[i].X;
    }
    if (Tick % Ns::SendEvery == 0)
    {
        // 按 LastAck 发全量或增量；AckTick<=0 视为请求全量
    }
}
```

`OnInput`：`seq <= LastSeq` 直接忽略。`seq > LastSeq + MaxInboxAhead`（`MaxInboxAhead = HistoryTicks`）也忽略。同号再来则覆盖 Inbox。模拟只走 `LastSeq+1`，因此跳号不会被「最新一条」吞掉。
缺 `LastSeq+1` 时该玩家本拍不加位移，直到缺号到达。

## 客户端：别人怎么画

见 `FNsStateSyncClient::OnSnap` / `UpdateRemoteDraw`。
`P.Tick <= LastAckedTick` 的旧快照直接忽略。
收到可用快照后发 `C2SSnapAck`（连发两次）。服务器按 ACK 基发增量。
解不出 `BaseTick`：连发 `C2SSnapAck Tick=0`，等下一份全量。不要静默死等。
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
void LocalTick(INsNet& Net, int8 Dx)
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

日志必须含 `state-sync tick=`。自动化：`NetworkSync.StateSync.Drop05`、`.Clean`、`.Rewind`、`.Nack`。
含义：远端插值有值、本地预测在快照到达后与服务器 x 和解一致；丢掉增量基后 nack 0 能恢复全量。
