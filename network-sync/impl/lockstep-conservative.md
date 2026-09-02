# 保守锁步（等齐）

独立内核。不要改 `NsLockstep.cpp`。类名 `FNsLockstepWaitServer` / `FNsLockstepWaitClient`，泵 `NsPumpLockstepWait*`。

对标：早期 RTS 囚徒。概念见 [../schemes/lockstep-variants.md](../schemes/lockstep-variants.md)「保守锁步」。

## 和乐观的差别

| | 乐观（已做） | 本 Kind |
| --- | --- | --- |
| 何时 `F(n)` | 墙钟到点 | 两槽都有 `I(n)`，或超时填空 |
| 上行 | `C2SInput` win=0 最新 dx | win=1，`seq=n`，`dx` |
| 缺人 | 沿用 `Latest`，全场继续 | 全场停在 n |
| 延迟 | `RTT + T/2` | 最慢端 RTT + 对齐等待 |

拓扑仍是 Sv+C0+C1。P2P 等齐是同一算法换泵，另开任务。

## 常量

沿用 `PlayerCount`、`LogicDtMs`、`LockstepSpeed`、`ChecksumEvery`。
本 Kind 另加：

```cpp
constexpr int32 NsLockstepWaitStallMs = 500; // 超时后缺槽填 0 再开拍，避免测试挂死
```

`KickAfterStalls` 默认 0：一直超时填空，不踢。大于 0 时，同一槽连续缺席这么多次后从**等待集**拿掉；`World` 仍是两人，该槽之后永远填 0，迟到输入丢掉。不是新 Kind，也不新开 `ENsMsg`。

Join 是本 Kind 的第二里程碑：落后超过冗余窗时靠 `S2CJoinSnap` 追上，不要改 `NsLockstep.cpp`。

## 服务器

```cpp
int32 Frame = 0;
bool Got[2] = {};
bool Alive[2] = {true, true};
int32 MissStreak[2] = {};
int32 KickAfterStalls = 0;
FNsInputs Slot;
double FrameStartMs = 0.0;
FNsWorld World;

void OnInput(int32 Id, int32 Tick, int8 Dx)
{
    if (Id < 0 || Id >= 2 || Tick != Frame || !Alive[Id]) return;
    Slot.Dx[Id] = NsClampDx(Dx);
    Got[Id] = true;
}

void Tick(INsNet& Net)
{
    bool bWaiting = false;
    bool bAll = true;
    for (int32 Id = 0; Id < 2; ++Id)
    {
        if (!Alive[Id]) continue;
        bWaiting = true;
        if (!Got[Id]) bAll = false;
    }
    if (!bWaiting) return;
    const bool bStall = (Net.Now - FrameStartMs) >= NsLockstepWaitStallMs;
    if (!bAll && !bStall) return;
    for (int32 Id = 0; Id < 2; ++Id)
    {
        if (!Alive[Id]) continue;
        if (Got[Id]) { MissStreak[Id] = 0; continue; }
        Slot.Dx[Id] = 0;
        ++MissStreak[Id];
        if (KickAfterStalls > 0 && MissStreak[Id] >= KickAfterStalls) Alive[Id] = false;
    }
    World.Step(Slot.Dx, Ns::LockstepSpeed);
    // S2CFrame：只带本拍 n，可仍打包前 3 拍冗余
    Got[0] = Got[1] = false;
    Slot = FNsInputs();
    ++Frame;
    FrameStartMs = Net.Now;
}
```

禁止用墙钟 15Hz 在没人齐时 `Step`。那是乐观。

## 客户端

本地采样 dx，发 `C2SInput`：`win=1`，`seq=ExecFrame`（尚未执行的下一拍）。
收到 `S2CFrame` 的拍 n 写入 `Buf`，`Logic` 与乐观相同：`while (Buf.Find(ExecFrame)) Step`，每 `ChecksumEvery` 拍上报 `C2SChecksum`。
缺 `ExecFrame` 时禁止跳。

Join 与乐观同一套 `S2CJoinSnap`：`Tick > ExecFrame` 才跳世界，尾巴进 `Buf`。周期 Join 每 `RedundantFrames+1` 拍；世界快照每 `JoinSnapEvery` 拍。`Hist` 只在快照时裁到 `SnapFrame - RedundantFrames`，不要每拍丢掉更早输入，否则 Join 没有尾巴。

客户端落后超过 `RedundantFrames+1` 拍、且服务器仍在超时推进时，冗余窗盖不住；没有 Join 就追不上。

## 泵

新泵。Drain Sv：`C2SInput` 用窗口第一条的 `seq` 当 Tick，不要走 `FNsLockstepServer::OnInput(Id, Dx)`。

## 验收

测试名前缀 `NetworkSync.Lockstep.Wait.`。

1. Drop=0，RTT=0：两边每拍都有输入，`Frame` 与两端 `ExecFrame` 对齐，checksum 相同。
2. C1 停发：全场 `Frame` 停住，直到 `NsLockstepWaitStallMs` 后才进一步；缺槽填 0，不是沿用上一拍。
3. Drop=0.1：靠冗余仍不跳拍；允许因停等变慢，不允许分叉。
4. C1 收不到包、服务器靠超时推进超过冗余窗：`S2CFrame` 追不上；`SendJoin` 后 `ExecFrame` 对齐且 `World` 同位。
5. `KickAfterStalls=2`：两次超时后不再等该槽；被踢槽迟到输入改不了 `X`。
6. 一次超时后对方恢复：缺席计数清零，再缺席仍要等 `StallMs`。

不要复用 `NetworkSync.Lockstep.Drop10`：那条假定到点就走。
不要复用 `NetworkSync.Lockstep.Join*`：那是乐观泵。

## 第二里程碑：Join

仍在 `NsLockstepWait.*` 里加，不要新 Kind，不要改乐观 `ApplyJoin`。

`SendJoin` / `ApplyJoin` 字段与 [lockstep.md](lockstep.md) 重连节相同。验收：`NetworkSync.Lockstep.Wait.Join`。

## 第三里程碑：停拍拉齐

另开 `NsLockstepWaitResync.*`，不要新 Kind，不要改 `Tick`。
规格：[hybrid/wait-resync.md](hybrid/wait-resync.md)。验收：`NetworkSync.Lockstep.Wait.Resync.*`。

## 第四里程碑：超时踢人

仍在 `NsLockstepWait.*` 的 `Tick` 里加，不要新 Kind，不要新消息。
`KickAfterStalls=0`（默认）保持验收 2 的填空。大于 0 时从等待集拿掉槽，不缩小 `FNsWorld`。
验收：`NetworkSync.Lockstep.Wait.Kick` / `KickResume`。

## 第五里程碑：按号 NACK

仍在 `NsLockstepWait.*` 里加，不要新 Kind，不要改 `Tick`。
`Buf` 有未来拍而缺 `ExecFrame` 时发 `C2SFrameNack`。泵 Drain `OnNack`：`Hist` 命中单播 `S2CFrame`，已裁则 `SendJoin`。停拍时忽略。
验收：`NetworkSync.Lockstep.Wait.Nack` / `.NackJoin`。
