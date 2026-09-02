# 会话切段结合

大厅、匹配、商城、战报走 HTTP / 普通状态。进了 PvP 房间才走输入帧。
这是**两个会话、两套序列化、一次硬切**，不是同一条 UDP 上的混合协议。

王者即此。见 [../../cases/honor-of-kings.md](../../cases/honor-of-kings.md)。
问「是不是帧同步」时，只问 PvP 战斗。

规格：[../../impl/hybrid/session.md](../../impl/hybrid/session.md)。
本插件用 `ENsScheme` 热切模拟切段，不实现大厅服。
`SchemeSwitch` 覆盖时钟和队列；`SchemeApply` 覆盖 `ApplyScheme` 的 `InitProtocols` 与 `PredX` 隔离。

| 时段 | 主线 | pawn `X` |
| --- | --- | --- |
| 大厅 | 状态 / HTTP | 非对战，本插件不模拟 |
| PvP | 锁步 | 只认 `F` |

切的时候必须丢掉上一会话的时钟、队列、预测残差。不切干净会出现进房瞬移或锁步风暴。

不要把切段和检查点写进同一个类。检查点发生在 PvP 会话内部。
