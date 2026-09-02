# 方案细则怎么读

四篇主线写的是协议本身：传什么、一拍怎么走、丢包怎么办、哪些变体、怎样失败。
结合不是第五套协议：每一类切法单独成包，见 `hybrid/`。
仓库里怎么落地见 [../technical-design.md](../technical-design.md)。
配套技术在 [../techniques.md](../techniques.md)。横向优劣在 [../comparison.md](../comparison.md)。

| 文件 | 主线 | 先读哪一节把细节钉住 |
| --- | --- | --- |
| [lockstep.md](lockstep.md) | 输入 + 确定性 F | 三种节拍方式；确定性细则；传输必须可靠有序 |
| [lockstep-variants.md](lockstep-variants.md) | 帧同步变种 | 节拍 / 输入 / 追帧；四支内核见 impl/lockstep-kinds |
| [state-sync.md](state-sync.md) | 权威世界 + 快照 | 三种频率；增量必须相对 ACK 基；插值窗 |
| [rollback.md](rollback.md) | 输入 + 猜远程 + 存档重演 | 确认帧 / 预测帧；输入延迟旋钮；和预测和解的差别 |
| [hybrid/README.md](hybrid/README.md) | 结合包索引 | 禁止 `NsHybrid` 万能模块 |
| [hybrid/checkpoint.md](hybrid/checkpoint.md) | 检查点 | 重连快照；平时 `X` 仍只由 `F` 写 |
| [hybrid/session.md](hybrid/session.md) | 会话切段 | 大厅状态 / PvP 锁步，一次硬切 |
| [hybrid/resync.md](hybrid/resync.md) | 停拍拉齐 | 停拍；当前世界强制回跳；不恢复推进 |
| [hybrid/wait-resync.md](hybrid/wait-resync.md) | 等齐停拍拉齐 | 同一切法叠在等齐内核；独立泵 |
| [hybrid/delay-resync.md](hybrid/delay-resync.md) | 固定延迟停拍拉齐 | 同一切法叠在 delay 内核；独立泵 |
| [hybrid/turn-resync.md](hybrid/turn-resync.md) | 通信回合停拍拉齐 | 同一切法叠在回合内核；空 `S2CFrame` 恢复 |
| [hybrid/door.md](hybrid/door.md) | 锁步加门 | pawn 锁步，FakeNet 门；门不进 `F` |
| [replication.md](replication.md) | 状态同步的对象接口 | Role / Owner；属性对 RPC；相关性穷举会先炸 CPU |

读案例前先读对应主线：王者 → lockstep；Dota 2 / 守望先锋 → state-sync；
守望先锋的「rollback」一词仍走 state-sync，不要跳去 rollback.md。

要照着写代码： [../impl/README.md](../impl/README.md)。
