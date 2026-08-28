# NetworkSync 技术方案

本仓库用 UE 5.8 插件 `Plugins/NetworkSync` 对照实现四套实时对战同步方案。
本文是总设计：回答选哪套、数据怎么走、代码落在哪、做到哪一步。
概念细则见 `schemes/`，逐行规格见 `impl/`，案例见 `cases/`。

## 文档目的

给实现者和后续扩展提供一份可执行的蓝图，而不是概念综述。

读者应能在 30 秒内知道：插件里有四套可切换协议、逻辑是整数固定步、演示走假网络、复制走引擎。
动手时按「公共内核 → 对应主循环 → 自测」的顺序读后面各节。

## 问题与约束

实时对战要在延迟、丢包、带宽和不可信客户端下，让多台机器对同一场模拟达成可玩的一致。

| 约束 | 对本原型的含义 |
| --- | --- |
| 延迟 | 假网络默认 RTT 80ms；手感靠乐观执行或预测，不靠 TCP |
| 丢包 | 应用层冗余或 ACK，不在传输层做可靠流 |
| 带宽 | 两人一维整数世界；目标是把协议走通，不是压测千人 |
| 信任 | 锁步/回滚各端自模拟；状态同步与复制以服务器为真 |

品类约束决定协议，不反过来。MOBA 兵线适合锁步；射击适合权威快照；格斗适合回滚；UE 关卡原型优先复制。

## 选型结论

四套方案同时保留，用 `ENsScheme` 切换。不要合成「万能同步器」。

| 方案 | 传什么 | 权威 | 拓扑（本插件） | 对标 |
| --- | --- | --- | --- | --- |
| 乐观帧同步 | 输入帧 | 各端同一份确定性 `FNsWorld` | 逻辑服务器 + 两客户端 | 王者荣耀 15Hz |
| 权威状态同步 | 快照 / 增量 | 服务器 pawn | 逻辑服务器 + 两客户端 | Source / 守望先锋一类 |
| 回滚 | 对等输入 | 输入磁带，无服务器世界 | P2P 两端 | GGPO |
| UE 复制 | 属性 + RPC | `ROLE_Authority` | Listen + Client | 引擎默认路径 |

守望先锋口头上的 rollback 是预测和解，不是 GGPO。本插件的 `Rollback` 分支只实现后者。

本仓库默认联机产品路径仍是：**Dedicated / Listen 权威 + Actor 复制**。
锁步与回滚只在明确做 RTS/MOBA 或格斗时启用，且逻辑必须离开 Chaos / 动画图。

## 系统架构

分三层，禁止把渲染 `DeltaSeconds` 写进逻辑 `Step`。

```text
表现层   ANsNetManager  debug sphere / lerp
         ANsReplicatedActor  Counter    ANsDoor  bOpen
           | 只读逻辑状态，或走引擎复制
协议层   FNsLockstep*   FNsStateSync*   FNsRollbackPeer
           | 固定步长，整数状态
传输层   FNsFakeNet（延迟/抖动/丢包/序号）
         或 UNetDriver（仅 Replication）
```

| 模块 | 职责 |
| --- | --- |
| `FNsWorld` | 两人一维坐标 + LCG，纯函数 `Step` / `Checksum` |
| `FNsFakeNet` | 内存 UDP：RTT、jitter、drop、Seq/Ack 去重 |
| 三套协议类 | 锁步 / 快照 / 回滚的主循环与包语义 |
| `ANsNetManager` | PIE 可视化：同一进程模拟两端 |
| `ANsReplicatedActor` / `ANsDoor` | 真复制：整数与 bool |
| `ns.SelfTest` | 假网络闭环，不依赖 PIE 联机 |

锁步、状态同步、回滚在 PIE 里是**单进程双端**。Replication 才走引擎 `UNetDriver`。

## 公共模拟内核

所有自研协议共用 `NsTypes.h`。逻辑状态禁止 float。

```cpp
namespace Ns {
    constexpr int32 PlayerCount = 2;
    constexpr int32 LogicDtMs = 66;      // 锁步 15Hz
    constexpr int32 SimDtMs = 16;        // 状态同步 / 回滚 ~60Hz
}

struct FNsWorld {
    int32 X[2];
    uint32 Rng;
    void Step(const int8 Dxs[2], int32 Speed);
    uint32 Checksum() const;
};
```

`Step`：每槽 `X += clamp(dx,-1,1) * Speed`，然后
`Rng = (Rng * 1103515245 + 12345) & 0x7FFFFFFF`。
`Checksum` 用 `int64` 再截断到 `uint32`，保证负数包装与各端一致。

硬规则：

1. 逻辑只用整数毫秒步长，禁止 `1/15` 浮点累加。
2. 先单机两份 `World` 喂同一输入，checksum 必须相同。
3. 先 drop=0，再加 80ms RTT，再加丢包。
4. 插值只读 `World`，不准写回。

## 假网络与消息

开发期不接 socket。`FNsFakeNet` 模拟不可靠、乱序、可重复的 UDP。

默认参数：`RttMs=80`，`JitterMs` 4～8，锁步自测 `Drop=0.1`，其余 `0.05`。
发送：`DeliverAt = Now + Rtt/2 + jitter`，以概率 `Drop` 扔掉。
序号：每源地址 `NextSeq` 递增；`Drain` 按目的端 32-bit 窗去重同一 `Seq`。

| `ENsMsg` | 方向 | 用途 |
| --- | --- | --- |
| `C2SInput` | 客户端 → 服务器 | 锁步最新 dx，或状态同步输入窗口 |
| `S2CFrame` | 服务器 → 客户端 | 锁步输入帧 + 前 3 拍冗余 |
| `S2CSnapshot` | 服务器 → 客户端 | 全量或相对 ACK 基的增量快照 |
| `C2SSnapAck` | 客户端 → 服务器 | 已应用的最大快照 tick |
| `P2PInput` | 对等 | 回滚输入 + 前 3 拍 |
| `C2SChecksum` | 客户端 → 服务器 | 锁步每 15 拍校验 |

真 UDP 头（magic、payload_len、ack 窗）规格在 [impl/transport.md](impl/transport.md)，尚未序列化到字节流。

地址：`Sv` / `C0` / `C1`。两人局足够验证协议，不在本阶段做 AOI。

## 乐观帧同步

对标王者荣耀：服务器按墙钟 15Hz 广播，不等齐所有人的新包。缺包沿用上一拍输入。

### 锁步数据流

```text
C0/C1  --C2SInput(dx)-->  Server.Latest[player]
Server 每 66ms: Hist[n]=Latest, World.Step, 广播 n..n-3
C0/C1  Buf 填槽，仅当 ExecFrame 已到才 Step，禁止跳帧
每 15 拍 C2SChecksum；服务器对照自己的 World
```

客户端**禁止**在缺 `ExecFrame` 时执行 `ExecFrame+1`。乱序只进 `Buf`。

冗余：每个 `S2CFrame` 带最新拍和前 3 拍。`Drop=0.1` 时仍应追上，不卡死超过 1 秒。

### 锁步表现与校验

`Logic` 在 `Step` 前保存 `PrevX`。`ANsNetManager` 用
`alpha = AccumMs / LogicDtMs` 画 `lerp(PrevX, X)`。checksum 只来自 `World`。

服务器 `OnChecksum`：对得上则 `ChecksumOk++`，对不上则 `bDesync`。
自测要求两端 `World` 相等、`ChecksumOk > 0`、无分叉。

未做：断线重连（存 5 秒一份世界快照再快进）。中途加入必须掺状态，纯度会下降。

代码：`NsLockstep.h` / `NsLockstep.cpp`。规格：[impl/lockstep.md](impl/lockstep.md)。

## 权威状态同步

服务器 60Hz 模拟，每 3 拍发一份快照（20Hz）。客户端预测自己、插值别人。

### 状态同步数据流

```text
Client 每 16ms: Seq++, PredX += dx*4, 发送最近 8 个 (seq,dx)
Server: 只处理 seq > LastSeq 的最新输入（幂等）
        Tick%3==0 时按 LastAck 发全量或增量
Client OnSnap: 用 last_processed_seq 丢掉已确认输入，从权威 x 重放 Unacked
        立刻 C2SSnapAck（连发两次抗丢包）
```

增量：`BaseTick` 为已 ACK 的快照 tick，`SnapX` 为相对该基的差。
基必须是客户端确认过的 tick，不能是「上次发出去的 tick」。
解不出基则丢弃该包，等下一份全量。

### 插值与滞后补偿

远程：`t_show = now_ms - 100`。找到 `t0 <= t_show <= t1` 的两份快照做整数 lerp。
没有括号则画最新一份，**禁止外推**。自己的 `player_id` 走 `PredX`，不走 lerp。

`RewindX(player, ping_ms)`：`back = ping/2 + 100`，超过 220ms 则用当前坐标。
历史环长 64 tick。本原型无射线开火，只保留倒带查询。

不要把这套和解叫回滚。这里有服务器真值；回滚没有。

代码：`NsStateSync.h` / `NsStateSync.cpp`。规格：[impl/state_sync.md](impl/state_sync.md)。

## GGPO 式回滚

两端 60Hz。本地输入（加 1 拍延迟后）立刻进逻辑；远程先猜上一拍，猜错则从存档重演。

### 回滚数据流

```text
Advance: Local[Frame+1] = pad
         若 Frame-Confirmed > 8 → WAIT：不 Step，只重传 Local
         否则 Pair(真本地, 真远程或猜), Saves[Frame]=World, Step, Frame++
OnRemote: 填 RealRemote；与 Pred 不一致且 F<Frame 则 RollbackFrom(F)
重演区间 [F, Frame)：有真用真，否则保持当时的猜
渲染只在本轮 Advance/OnRemote 全部结束后做一次
```

`Confirmed` 是 `RealRemote` 从 0 起连续到达的最大帧。
`INPUT_DELAY=1` 降低回滚次数：当前拍用的是上一拍读到的本地输入。

真值是输入磁带，不是服务器世界。两端最终 `X[0],X[1],Rng` 必须一致。

代码：`NsRollback.h` / `NsRollback.cpp`。规格：[impl/rollback.md](impl/rollback.md)。

## UE 对象复制

不自研快照。走 `UPROPERTY(Replicated)` + Server RPC。概念见 [unreal.md](unreal.md)。

| Actor | 复制内容 | 输入 |
| --- | --- | --- |
| `ANsReplicatedActor` | `Counter`，`OnRep_Counter` | 按 `E` → `ServerBump` |
| `ANsDoor` | `bOpen`，`OnRep_Open` | 按 `F` → `ServerSetOpen` |

构造：`bReplicates=true`，`bAlwaysRelevant=true`。
`GetLifetimeReplicatedProps` 里 `DOREPLIFETIME`。
Listen 主机改属性时 **OnRep 不会跑**，权威端要自己改表现（本原型用 Tick 里 `DrawDebug`）。

Server RPC 只能由 Owner 客户端调用。演示把 Owner 设成 Listen 的 FirstPlayerController。
远端客户端按键可能被丢，这是引擎规则，不是 bug。验证复制：主机按 `E`/`F`，客户端看到值变。

角色移动不要手写 `SetActorLocation`。下一步才是 `ACharacter` + `UCharacterMovementComponent`，需要 GameMode 与 Possess，尚未做。

规格：[impl/replication_ue.md](impl/replication_ue.md)。

## 演示层与运行方式

`ANsNetManager` 每套协议用自己的固定步累加器，不跟渲染帧 1:1 步进。

| Scheme | 玩家 0 | 玩家 1 | 画面 |
| --- | --- | --- | --- |
| Lockstep | A / D | 方向键 | 两球，锁步 lerp |
| StateSync | A / D | 方向键 | 自己 PredX，别人插值 |
| Rollback | A / D | 方向键 | 回滚结束后的 World |
| Replication | E / F | （观察复制） | 绿球 Counter + 门 |

控制台：

1. `ns.SelfTest` — 三套假网络协议，日志 `NetworkSync self-test OK`。
2. `ns.SpawnDemo` — 生成 Manager；在细节面板改 `Scheme`。
3. Session Frontend：`TA.NetworkSync.*`。

引擎：UE 5.8。关联 `TA-Playground.uproject` 的 `EngineAssociation=5.8`。

## 测试与验收

| 测试 | 断言 |
| --- | --- |
| `TA.NetworkSync.World.Determinism` | 200 拍两份 World checksum 相同 |
| `TA.NetworkSync.Lockstep` | 帧数足够、两端相等、checksum 有成功比对 |
| `TA.NetworkSync.StateSync` | 预测与服务器 x 一致、有远程插值、ACK 与增量发生 |
| `TA.NetworkSync.Rollback` | 冷却后两端相等、不再 WAIT |
| `TA.NetworkSync.FakeNet.Seq` | 连续发送序号递增 |

自测 RNG 种子为 1，结果应稳定。改 `Drop` 后若偶发失败，先看冷却循环是否排空在途包。

## 已知边界与后续

已闭环：两人整数世界、假网络、三套协议主循环、checksum、增量快照、回滚延迟与 WAIT、复制整数与门。

| 未做 | 原因与影响 |
| --- | --- |
| 锁步重连快照 | 中途加入必须快进或掺状态 |
| 真 UDP 字节包 | 现在是 `FNsPacket` 内存对象 |
| CharacterMovement | 需要 Possess；复制移动应走 CMC |
| 多人 / AOI | 地址写死三人（含服务器） |
| 开火命中 | `RewindX` 已有，未接武器 |

后续优先顺序：真 socket 替换 `FNsFakeNet` → 锁步重连 → 若做射击再接命中倒带；
关卡原型继续复制 + CMC，不要把锁步塞进 Character。

## 文档与代码索引

| 路径 | 内容 |
| --- | --- |
| [overview.md](overview.md) | 拓扑 / 权威 / 协议三层模型 |
| [comparison.md](comparison.md) | 优劣与品类 |
| [schemes/README.md](schemes/README.md) | 四篇协议细则入口 |
| [impl/README.md](impl/README.md) | 实现规格与如何运行 |
| [cases/compare-three.md](cases/compare-three.md) | 守望先锋 / Dota 2 / 王者 |
| `Plugins/NetworkSync/` | UE 插件源码 |

类到文件：

| 类 | 文件 |
| --- | --- |
| `FNsWorld` | `Public/NsTypes.h` |
| `FNsFakeNet` | `Public/NsFakeNet.h` |
| `FNsLockstepServer` / `Client` | `Public/NsLockstep.h` |
| `FNsStateSyncServer` / `Client` | `Public/NsStateSync.h` |
| `FNsRollbackPeer` | `Public/NsRollback.h` |
| `ANsNetManager` | `Public/NsNetManager.h` |
| `ANsReplicatedActor` | `Public/NsReplicatedActor.h` |
| `ANsDoor` | `Public/NsDoor.h` |
