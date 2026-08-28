# 方案细则怎么读

四篇主线写的是协议本身：传什么、一拍怎么走、丢包怎么办、哪些变体、怎样失败。
配套技术在 [../techniques.md](../techniques.md)。横向优劣在 [../comparison.md](../comparison.md)。

| 文件 | 主线 | 先读哪一节把细节钉住 |
| --- | --- | --- |
| [lockstep.md](lockstep.md) | 输入 + 确定性 F | 三种节拍方式；确定性细则；传输必须可靠有序 |
| [state-sync.md](state-sync.md) | 权威世界 + 快照 | 三种频率；增量必须相对 ACK 基；插值窗 |
| [rollback.md](rollback.md) | 输入 + 猜远程 + 存档重演 | 确认帧 / 预测帧；输入延迟旋钮；和预测和解的差别 |
| [replication.md](replication.md) | 状态同步的对象接口 | Role / Owner；属性对 RPC；相关性穷举会先炸 CPU |

读案例前先读对应主线：王者 → lockstep；Dota 2 / 守望先锋 → state-sync；
守望先锋的「rollback」一词仍走 state-sync，不要跳去 rollback.md。

要照着写代码： [../impl/README.md](../impl/README.md)。
