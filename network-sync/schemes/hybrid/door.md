# 锁步加门结合

同一局里：锁步推进 pawn 的 `X`；另一块状态推与战斗解耦的字段（本包是门开关）。
表现层按对象选时钟。球认 `ExecFrame`，门认门包时间。

门**不得**进入 `World.Step`。若开关挡住走路，它是锁步输入，不是本包字段。

不要实例化 `FNsStateSync*`。那份快照带着 pawn `X`。
第一版只走 `INsNet` 上的门整数，不走 `UNetDriver` / `ANsDoor`。Manager 仍是四套 `ENsScheme`，不新增方案枚举。四支锁步演示把 `S2CDoorOpen` 叠在各自停拍拉齐泵上。

规格：[../../impl/hybrid/door.md](../../impl/hybrid/door.md)。
代码：`NsLockstepDoor.*`。组合 `FNsLockstep*`。不要叫 `NsHybrid`。

| 字段 | 写入者 |
| --- | --- |
| `World.X` | 只锁步 |
| `Door.Open` | 只本包 `S2CDoorOpen` |

禁止：同一 pawn 既 `Step` 又吃快照坐标；表现层先动；英雄快照 + 小兵锁步。

失败：角色发飘、死人还能动 → 两套主线写了同一 Transform。
