# 检查点包

本包**不新开类型**。乐观锁步已经带 `S2CJoinSnap`。这里只钉所有权和禁令，避免重写一份 Join。

循环：[../lockstep.md](../lockstep.md)。概念：[../../schemes/hybrid/checkpoint.md](../../schemes/hybrid/checkpoint.md)。

## 所有权

| 字段 | 谁写 |
| --- | --- |
| `X` / `Rng` | `World.Step`。`ApplyJoin` 仅当 `Packet.Tick > ExecFrame` |
| 尾巴输入 | Join payload 里 `frame >= Tick` 的拍，进 `Buf` |

世界快照每 `JoinSnapEvery=75` 拍更新。Join **包**每 `RedundantFrames+1=4` 拍发一次。两计时器不要当成一个。

## 禁令

- 不要把 Join 改成每拍。那是双写主线。
- 不要在本包实现停拍拉齐。停拍拉齐是 [resync.md](resync.md)。停拍拉齐禁止改本包的 `ApplyJoin` 守卫。
- 不要在本包实现大厅切段。切段是 [session.md](session.md)。

## 验收（已有，勿复制测试）

`NetworkSync.Lockstep.Join`、`.LateJoin`、`.JoinFrag`。
LateJoin：丢掉早期包后靠周期 Join 追上；`player_id` 伪造不得改槽。
