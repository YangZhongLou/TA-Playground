# 固定延迟停拍拉齐

delay checksum 对不上时，停拍并把两端拉到**当前服务器世界**。
不要改 `FNsLockstepDelayServer::Tick`，也不要复用乐观 / 等齐停拍泵。

包语义与乐观停拍拉齐相同，见 [resync.md](resync.md)。本包只换 delay 内核与泵。
周期恢复 JoinSnap 带 Hist 尾巴，空包专留给 LiveSnap。

规格：[../../impl/hybrid/delay-resync.md](../../impl/hybrid/delay-resync.md)。
代码：`NsLockstepDelayResync.*`。`ANsNetManager` delay 走这套泵。不新增 `ENsScheme` / `ENsLockstepKind`。

| 平时 | 分叉后 | 对齐 ack 后 |
| --- | --- | --- |
| 输入提前 `d` 拍，收齐再 `F` | 停拍；广播当前 `World`；两端强制回跳 | 清 `bDesync`，重开超时钟，补回 `d` 管线再打拍 |

不要和乐观 / 等齐停拍泵合成一个 `if (Kind)`。门叠在 delay 停拍客户端泵上，不进 `World.Step`。
