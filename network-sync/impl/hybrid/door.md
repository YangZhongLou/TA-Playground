# 锁步加门包

独立类型 `FNsLockstepDoorServer` / `Client`，泵 `NsPumpLockstepDoor*`。
内部组合 `FNsLockstep*`，不复制乐观循环，不改 `NsLockstep.cpp`。
概念：[../../schemes/hybrid/door.md](../../schemes/hybrid/door.md)。

不要命名 `NsHybrid`。不要新增 `ENsScheme`。第一版入口是自测泵，Manager 仍四套方案。

## 所有权

```cpp
struct FNsDoorOpen
{
    int32 Open = 0;
};
```

| 字段 | 谁写 | 谁读（表现） |
| --- | --- | --- |
| `FNsWorld.X` | 只锁步泵 | `GetPawnLocation` |
| `FNsDoorOpen.Open` | 只本包 `S2CDoorOpen` | 画门 |

`Open` 不得传入 `World.Step`。禁止把 `S2CSnapshot` 的 `x0/x1` 应用到锁步 `World`。
禁止实例化 `FNsStateSync*`。

## 传输（第一版）

只走 `INsNet` / FakeNet。`ENsMsg::S2CDoorOpen=8`，布局见 [packet-format.md](../packet-format.md)。
payload：一个 `int32 Open`。服务器每个泵周期把当前值发给 C0、C1；无 ACK 第一版。

不走 `UNetDriver`，不生成 `ANsDoor`。`ApplyScheme` 离开 Replication 仍销毁复制门，本包不依赖它。

默认架在 `Optimistic` 上。其它 Kind 要加门：先完成那支 Kind，再组合，仍不改 Kind 的 `Tick`。

锁步泵仍发周期 Join。本包不另写 Join / 切 Scheme / 停拍拉齐。

## 禁令

- 不要调用 `FNsStateSyncClient`。
- 不要做表现层先动。
- 不要让门挡住或推动 pawn，除非把门改成锁步输入（那就离开本包）。

## 验收

前缀 `NetworkSync.LockstepDoor.`。

1. Drop=0：两端 `X` 与 `NetworkSync.Lockstep.Clean` 同类；`Open` 相同。
2. 只丢门包：pawn 仍对齐，门可停旧值。
3. 只丢输入帧：门仍对齐，pawn 与 `Lockstep.Drop10` 同类。
4. 若测试里把快照坐标写进 `World.X`，必须失败。
5. `Open` 变化时 `World.X` 与纯锁步对照局相同（门不进 `F`）。
