# 等齐停拍拉齐

等齐 checksum 对不上时，停拍并把两端拉到**当前服务器世界**。
不要改 `FNsLockstepWaitServer::Tick`，也不要复用乐观 `NsPumpLockstepResync*`。

包语义与乐观停拍拉齐相同，见 [resync.md](resync.md)。本包只换等齐内核与泵。

规格：[../../impl/hybrid/wait-resync.md](../../impl/hybrid/wait-resync.md)。
代码：`NsLockstepWaitResync.*`。`ANsNetManager` 等齐走这套泵。不新增 `ENsScheme` / `ENsLockstepKind`。

| 平时 | 分叉后 | 对齐 ack 后 |
| --- | --- | --- |
| 收齐再 `F` | 停拍；广播当前 `World`；两端强制回跳 | 清 `bDesync`，重开超时钟，再等齐打拍 |

不要和乐观停拍泵合成一个 `if (Kind)`。门叠在等齐停拍客户端泵上，不进 `World.Step`。
