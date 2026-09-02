# 通信回合停拍拉齐

通信回合 checksum 对不上时，停拍并把两端拉到**当前服务器世界**。
不要改 `FNsLockstepTurnServer::Tick`，也不要复用乐观 / 等齐 / delay 停拍泵。

LiveSnap 与乐观相同，见 [resync.md](resync.md)。
恢复不能看 `S2CFrame` 的回合号：Resend 窗口一直含当前关闭回合。
恢复令牌是空 `S2CFrame`。

规格：[../../impl/hybrid/turn-resync.md](../../impl/hybrid/turn-resync.md)。
代码：`NsLockstepTurnResync.*`。`ANsNetManager` 通信回合走这套泵。不新增 `ENsScheme` / `ENsLockstepKind`。

| 平时 | 分叉后 | 对齐 ack 后 |
| --- | --- | --- |
| 按回合等齐，指令两回合后进 `F` | 停拍；广播当前 `World`；两端强制回跳 | 清 `bDesync`，发空 `S2CFrame`，下一泵再 `Tick` |

不要和其它停拍泵合成一个 `if (Kind)`。门叠在通信回合停拍客户端泵上，不进 `World.Step`。
