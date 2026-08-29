# 检查点结合

锁步平时只传输入。重连没有「当前世界」，必须从 0 快进，或吃一份逻辑快照再快进尾巴。
快照是**跳世界用的状态**，不是权威移动。平时 `X` 仍只由 `F(S, I(n))` 推进。

不要每拍发检查点。那是付状态同步带宽，还在付确定性税。

规格与代码在乐观锁步包里，本包不另开类型：[../../impl/hybrid/checkpoint.md](../../impl/hybrid/checkpoint.md)。
循环细节：[../../impl/lockstep.md](../../impl/lockstep.md) 的 `S2CJoinSnap`。

王者重连慢，卡在快进尾巴，不是卡在 15Hz。见 [../../cases/honor-of-kings.md](../../cases/honor-of-kings.md)。

| 字段 | 写入者 |
| --- | --- |
| `World.X` / `Rng` | 锁步 `Step`；仅 `ApplyJoin` 且 `Tick > ExecFrame` 时可覆盖 |
| 输入磁带 | `S2CFrame` / Join 尾巴 |

`ApplyJoin` 只向前跳。停拍拉齐的强制回跳不得改这个守卫。

失败：平时飘、只有重连对齐 → 把 Join 当每拍快照在用。
