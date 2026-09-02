# 会话切段包

本包**不新开协议类型**。用已有 `ENsScheme` 热切模拟「出大厅进对战」。
概念：[../../schemes/hybrid/session.md](../../schemes/hybrid/session.md)。
大厅 HTTP 不在本插件。开放世界切格斗场也不在本包验收里。

## 切什么

`ApplyScheme` 必须：`InitProtocols`、`AppliedScheme` / `AppliedLockstepKind`、`ResetWire`（`Now=0`、假网络队列空、UDP 按新方案重绑）。
离开 Replication 时销毁门；进入 Replication 时再生成。`InitProtocols` 重建对象，锁步不得继承状态同步的 `PredX`。

## 禁令

- 不要在切段包里发 `S2CJoinSnap`。重连是 [checkpoint.md](checkpoint.md)。
- 不要同一 `Tick` 里先泵状态同步再泵锁步还写同一 `X`。那是 [door.md](door.md) 的反例，不是切段。
- 未实现的 `ENsLockstepKind` 不得回落跑乐观循环（已做）。

## 验收

`NetworkSync.Runtime.SchemeSwitch` **只**证明：锁步跑一段 → `ResetSession` → 新锁步不得按旧 `Now` 追帧风暴；队列必须空。

`NetworkSync.Runtime.SchemeApply` 走真实 `ANsNetManager::ApplyScheme`：
热切后 `InitProtocols` 重建对象，锁步 `X` / `Frame` 不得继承状态同步 `PredX`，
假网络 `Now` 与队列清空。Kind 热切同样重建等齐内核。
