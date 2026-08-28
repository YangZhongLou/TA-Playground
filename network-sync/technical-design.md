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
         FNsUdpNet（本机三端口 UDP）
         或 UNetDriver（仅 Replication）
```

| 模块 | 职责 |
| --- | --- |
| `FNsWorld` | 两人一维坐标 + LCG，纯函数 `Step` / `Checksum` |
| `NsEncodePacket` / `NsDecodePacket` | 20 字节头 + payload，小端；未知 type / 长度不符则丢弃 |
| `FNsFakeNet` | 内存 UDP：RTT、jitter、drop、Seq/Ack 去重，收发走编解码 |
| `FNsUdpNet` | 三个 `FSocket` 绑 127.0.0.1；协议仍用 `INsNet` |
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
| `S2CJoinSnap` | 服务器 → 客户端 | 锁步重连：世界快照 + 快照之后的输入 |

`FNsFakeNet::Send` 会把 `FNsPacket` 编成字节再解回，Src/Dst 仍由地址表提供。
逐字节布局见 [impl/packet-format.md](impl/packet-format.md)。
单数据报不超过 1200 字节（IPv6 最小 MTU 下也不触发 IP 分片）；超长由 `NsSplitForMtu` 拆成多个完整包，各自独立丢包。
`FNsUdpNet` 把同一字节发到对端 `SetPeer` 的 IP:port。`BindLoopback` 仍是单进程三端口。
`ANsNetManager` 勾选 `bUseUdp`：`LocalMesh` 走三端口；`Host` / `Client` 只绑本端，对端填 `UdpRemoteHost`。
`UdpBasePort=0` 时 Host/Client 默认 27000、27001、27002。`bUdpLan` 绑 `0.0.0.0`。

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

重连：每 75 拍（约 5 秒）存一份 `SnapWorld`。`SendJoin` 下发 `S2CJoinSnap`
（`ExecFrame = SnapFrame+1`、坐标、Rng、之后的 `Hist`）。客户端 `ApplyJoin` 后快进。
无快照时从第 0 拍重放全部 `Hist`。

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
| `ANsMoverPawn` | Mover 同步状态 | WASD / 方向键移动，空格跳 |

构造：`bReplicates=true`，`bAlwaysRelevant=true`。
`GetLifetimeReplicatedProps` 里 `DOREPLIFETIME`。
Listen 主机改属性时 **OnRep 不会跑**，权威端要自己改表现（本原型用 Tick 里 `DrawDebug`）。

Server RPC 只能由 Owner 客户端调用。演示把 Owner 设成 Listen 的 FirstPlayerController。
远端客户端按键可能被丢，这是引擎规则，不是 bug。验证复制：主机按 `E`/`F`，客户端看到值变。

角色移动走 `ANsMoverPawn`（`APawn` + `UCharacterMoverComponent`），不接 CMC，不手写 `SetActorLocation`。
`SetReplicatingMovement(false)`，由 Network Prediction 驱动。控制台 `ns.SpawnMover`，或 Replication 方案里一并生成。

规格：[impl/replication_ue.md](impl/replication_ue.md)。

## 演示层与运行方式

`ANsNetManager` 每套协议用自己的固定步累加器，不跟渲染帧 1:1 步进。

| Scheme | 玩家 0 | 玩家 1 | 画面 |
| --- | --- | --- | --- |
| Lockstep | A / D | 方向键 | 两球，锁步 lerp |
| StateSync | A / D | 方向键 | 自己 PredX，别人插值 |
| Rollback | A / D | 方向键 | 回滚结束后的 World |
| Replication | E / F | （观察复制） | 绿球 Counter + 门；并 Possess Mover pawn |

控制台：

1. 控制台 `ns.SelfTest` — 假网络三套协议、编解码、UDP、压力长跑，日志 `NetworkSync self-test OK`。
2. `ns.SpawnDemo` — 生成 Manager；改 `Scheme`，可勾选 `bUseUdp` 与 `UdpRole`。
3. `ns.SpawnMover` — 生成 `ANsMoverPawn` 并 Possess。WASD 移动，空格跳。
4. Session Frontend：`TA.NetworkSync.*`。

引擎：UE 5.8。关联 `TA-Playground.uproject` 的 `EngineAssociation=5.8`。

## 测试与验收

| 测试 | 断言 |
| --- | --- |
| `TA.NetworkSync.World.Determinism` | 200 拍两份 World checksum 相同 |
| `TA.NetworkSync.World.Contract` | clamp、1000 拍、不同输入分叉、Reset |
| `TA.NetworkSync.Lockstep.Drop10` | Drop=0.1，帧数足够、两端相等、checksum 有成功比对 |
| `TA.NetworkSync.Lockstep.Clean` | Drop=0，两端对齐且有 checksum |
| `TA.NetworkSync.Lockstep.HighDrop` | Drop=0.15，冷却后仍追上、不卡超过约 1 秒 |
| `TA.NetworkSync.Lockstep.Join` | 抹掉一端后 `SendJoin`，ExecFrame 与 World 追上 |
| `TA.NetworkSync.Lockstep.Desync` | checksum 对不上则 `bDesync` |
| `TA.NetworkSync.StateSync.Drop05` | 预测与服务器 x 一致、有远程插值、ACK 与增量发生 |
| `TA.NetworkSync.StateSync.Clean` | Drop=0 和解 |
| `TA.NetworkSync.StateSync.Rewind` | `RewindX` 回看；超时 RTT 返回当前 x |
| `TA.NetworkSync.Rollback.Drop05` | 冷却后两端相等、不再 WAIT |
| `TA.NetworkSync.Rollback.Clean` | Drop=0 两端相等 |
| `TA.NetworkSync.Rollback.Wait` | 对端缺失超过 `MaxRollback` 进入 WAIT，补包后恢复 |
| `TA.NetworkSync.FakeNet.Seq` | 连续发送序号递增 |
| `TA.NetworkSync.FakeNet.SeqWindow` | 同一 seq 第二次拒绝；Stamp 递增 |
| `TA.NetworkSync.FakeNet.DropDelay` | Drop=1 全丢；半 RTT 前不到、到点必到 |
| `TA.NetworkSync.Codec.RoundTrip` | 帧包与加入包编解码后字段一致 |
| `TA.NetworkSync.Codec.RejectsBad` | 坏 magic / 未知 type / 长度不符丢弃 |
| `TA.NetworkSync.Codec.Contract` | 其余消息类型往返；空包、截断、256 帧、尾随字节拒绝 |
| `TA.NetworkSync.Codec.Mtu` | 典型包长；超 1200 拆成多个 ≤1200 的数据报；Join 切片仍为 JoinSnap |
| `TA.NetworkSync.Udp.Loopback` | 本机 UDP 发出 `C2SInput` 后能解出同一 payload |
| `TA.NetworkSync.Udp.Lockstep` | 三端口 UDP 上锁步两端 World 一致 |
| `TA.NetworkSync.Udp.Peers` | 两个 `FNsUdpNet` 用地址表互发 |
| `TA.NetworkSync.Udp.Split` | Host 绑 Sv+C0、Client 绑 C1，锁步 World 一致 |
| `TA.NetworkSync.Udp.Burst` | 环回连续 48 个数据报全部收到 |
| `TA.NetworkSync.Actors.Cdo` | Door / Counter RPC 实现改状态；Mover 复制关、有胶囊与组件 |
| `TA.NetworkSync.Stress.World` | 10000 拍确定性，限时 250ms |
| `TA.NetworkSync.Stress.Codec` | 10000 次编解码，限时 1500ms |
| `TA.NetworkSync.Stress.FakeNet` | 2500 包 Drop=0 全送达，限时 2000ms |
| `TA.NetworkSync.Stress.Lockstep` | 600 帧 Drop=0.1，限时 3000ms |
| `TA.NetworkSync.Stress.StateSync` | 800 tick Drop=0.05，限时 3000ms |
| `TA.NetworkSync.Stress.Rollback` | 400 帧 Drop=0.05，限时 3000ms |

自测 RNG 种子为 1，结果应稳定。改 `Drop` 后若偶发失败，先看冷却循环是否排空在途包。

## 已知边界与后续

已闭环：两人整数世界、假网络字节头、本机与分进程 UDP、三套协议主循环、checksum、增量快照、回滚延迟与 WAIT、
锁步重连快照、复制整数与门、Mover pawn（非 CMC）。

| 未做 | 原因与影响 |
| --- | --- |
| NAT / STUN | 地址表是手工填 IP:port，没有打洞 |
| 多人 / AOI | 地址写死三人（含服务器） |
| 开火命中 | `RewindX` 已有，未接武器 |

后续优先顺序：Listen+Client 验收 Mover 跟手，或射击倒带；跨机先填 `UdpRemoteHost`。

## 文档与代码索引

| 路径 | 内容 |
| --- | --- |
| [overview.md](overview.md) | 拓扑 / 权威 / 协议三层模型 |
| [comparison.md](comparison.md) | 优劣与品类 |
| [schemes/README.md](schemes/README.md) | 四篇协议细则入口 |
| [impl/README.md](impl/README.md) | 实现规格与如何运行 |
| [impl/packet-format.md](impl/packet-format.md) | 帧字节格式 |
| [cases/compare-three.md](cases/compare-three.md) | 守望先锋 / Dota 2 / 王者 |
| `Plugins/NetworkSync/` | UE 插件源码 |

类到文件：

| 类 | 文件 |
| --- | --- |
| `FNsWorld` | `Public/NsTypes.h` |
| `NsEncodePacket` | `Public/NsCodec.h` |
| `FNsFakeNet` / `INsNet` | `Public/NsFakeNet.h` |
| `FNsUdpNet` | `Public/NsUdpNet.h` |
| `FNsLockstepServer` / `Client` | `Public/NsLockstep.h` |
| `FNsStateSyncServer` / `Client` | `Public/NsStateSync.h` |
| `FNsRollbackPeer` | `Public/NsRollback.h` |
| `ANsNetManager` | `Public/NsNetManager.h` |
| `ANsReplicatedActor` | `Public/NsReplicatedActor.h` |
| `ANsDoor` | `Public/NsDoor.h` |
| `ANsMoverPawn` | `Public/NsMoverPawn.h` |
