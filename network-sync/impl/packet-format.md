# 数据帧字节格式

所有自研协议共用 `NsEncodePacket` / `NsDecodePacket`（`NsCodec.cpp`）。
本文是线上布局的唯一对照表。语义见各方案的 `impl/*.md`。

约定：一律小端；有符号整数是二进制补码；字段之间无对齐填充；`TMap` 按 key 升序写出。
整包长度必须等于 `20 + payload_len`，且不超过 `MaxPacketBytes=1200`。发送前超限则应用层拆片；解码时对不上则丢弃。

## 整包

```text
偏移    0        4    5    6        8        12       16       20
        +--------+----+----+--------+--------+--------+--------+-------
字节    | magic  |type|rsv | plen   | seq    | ack    |ackbits | payload
宽度    |   4    | 1  | 1  |   2    |   4    |   4    |   4    | plen
```

| 绝对偏移 | 宽 | 类型 | 字段 | 线上值 |
| --- | --- | --- | --- | --- |
| 0 | 4 | u32 | magic | `0x54414E53`。小端字节 `53 4E 41 54` |
| 4 | 1 | u8 | type | `ENsMsg` 1–8。其它值丢弃 |
| 5 | 1 | u8 | reserved | 默认 `0`。通信回合 `S2CFrame`：`(ClosedLen<<4)|NextFpt`，两档都是 2–6；仅 NextFpt 时高四位为 0。其它 type 仍为 0 |
| 6 | 2 | u16 | payload_len | payload 字节数。必须等于 `包长 - 20` |
| 8 | 4 | u32 | seq | 发送端对该源递增，从 1 起 |
| 12 | 4 | u32 | ack | 本端作为接收方、对**当前对端**已收到的最大 seq |
| 16 | 4 | u32 | ack_bits | 选择性确认。见下方序号窗 |
| 20 | plen | — | payload | 由 type 决定 |

不进字节的字段：`Src`、`Dst`、`DeliverAt`。由 UDP 地址表或假网络邮箱提供。

`seq` / `ack` / `ack_bits` 按 **(接收端, 发送端)** 记账。两个客户端各自从 1 编号，发往服务器时互不覆盖。

`ack_bits`：若 `RecvMax = M`，bit `k`（`k=0..31`）表示是否已收到 seq `M - (k+1)`。本插件只用来去重，协议逻辑不读 ACK。

## 包长与 IP 分片

IP 若把一个 UDP 拆成多片，丢任意一片则整报作废。单片丢包率 5%、拆成 2 片时，整报损失约 10%。

因此每个 Ns 包必须单独成为一个 IP 包。UDP 载荷上限 `MaxPacketBytes=1200`：
IPv6 最小 MTU 1280，IPv6+UDP 头 48，1200+48=1248 < 1280。IPv4 以太网 1200+28=1228 < 1500。
`FNsUdpNet` 只开 IPv4 socket；1200 仍按 IPv6 下限留余量，避免以后换 dual-stack 时踩分片。

超长内容在应用层拆成多个完整 Ns 包（各带 20 字节头），互不依赖。丢其中一个不影响其它。禁止依赖内核 IP 分片。

| 包 | 典型整包 | 单数据报最多条目 |
| --- | --- | --- |
| `C2SInput` 锁步 | 23 B | 235 |
| `S2CFrame` 4 拍 | 49 B | 195 |
| `S2CSnapshot` | 41 B | — |
| `P2PInput` 4 条 | 41 B | 235 |
| `S2CJoinSnap` 75 拍 | 487 B | 193 |
| `S2CDoorOpen` | 24 B | — |

日常冗余远小于上限。`Send` 路径调用 `NsSplitForMtu`：超限则按帧号切片，每片独立编序号、独立丢包。

`S2CJoinSnap` 切片仍全部是 JoinSnap，快照字段重复，输入拍按 key 升序切开。客户端 `ApplyJoin` 只在 `Tick > ExecFrame` 时跳世界，其余片只合并 `Buf`。

## type 一览

| 值 | 枚举 | 谁发 | 方案 | payload 固定部分 |
| --- | --- | --- | --- | --- |
| 1 | `C2SInput` | 客→服 | 锁步、状态同步 | 3 + 5×win |
| 2 | `S2CFrame` | 服→客 | 锁步 | 5 + 6×count |
| 3 | `S2CSnapshot` | 服→客 | 状态同步 | 21（两人时） |
| 4 | `C2SSnapAck` | 客→服 | 状态同步 | 5 |
| 5 | `P2PInput` | 对等 | 回滚 | 1 + 5×count |
| 6 | `C2SChecksum` | 客→服 | 锁步 | 9 |
| 7 | `S2CJoinSnap` | 服→客 | 锁步重连 / 停拍拉齐 | 17 + 6×count |
| 8 | `S2CDoorOpen` | 服→客 | 锁步加门 | 4 |

下面偏移均相对 **payload 起点**（整包偏移 = 20 + 该列）。

## 1 `C2SInput`

锁步只填最新摇杆；状态同步再带未确认窗口。同一布局。

| 偏移 | 宽 | 类型 | 字段 | 约束 |
| --- | --- | --- | --- | --- |
| 0 | 1 | u8 | player_id | 0 或 1。其它值服务器忽略 |
| 1 | 1 | i8 | dx | 锁步用：-1 / 0 / 1。状态同步此字节写 0，真输入在窗口里 |
| 2 | 1 | u8 | win | 后面条目数。`>255` 编码失败。乐观锁步为 0；等齐为 1，`seq` 是目标拍 |
| 3 | 5×win | — | 窗口 | 每条 `u32 seq` + `i8 dx` |

一条窗口：

| 相对条目起点 | 宽 | 类型 | 字段 |
| --- | --- | --- | --- |
| 0 | 4 | u32 | 客户端输入序号，单调递增 |
| 4 | 1 | i8 | 该序号的 dx，-1 / 0 / 1 |

payload 长度：`3 + 5×win`。乐观锁步整包 23 字节。等齐 `win=1` 整包 28 字节。状态同步 `win≤8` 时整包最多 63 字节。

锁步示例（player=0，dx=+1，win=0，seq=9，ack=4，ack_bits=0）：

```text
53 4E 41 54  01 00  03 00  09 00 00 00  04 00 00 00  00 00 00 00
00 01 00
```

状态同步窗口由近到远最多 8 个尚未被快照 `last_processed_seq` 确认的 `(seq, dx)`。
服务器把 `LastSeq < seq <= LastSeq + MaxInboxAhead` 写入 Inbox，模拟时按 `LastSeq+1` 顺序应用。

## 2 `S2CFrame`

锁步的数据帧。网上是两人输入，不是坐标。

| 偏移 | 宽 | 类型 | 字段 | 约束 |
| --- | --- | --- | --- | --- |
| 0 | 4 | u32 | latest | 本包最大 frame。等于下面 keys 的最后一个。解码读掉但不单独存 |
| 4 | 1 | u8 | count | 后面拍数。`>255` 失败。实现里 1–4 |
| 5 | 6×count 或 7×count | — | 拍 | 每拍 `u32 frame + i8 dx0 + i8 dx1`；通信回合再加 `u8` 回合长度 |

一拍：

| 相对拍起点 | 宽 | 类型 | 字段 |
| --- | --- | --- | --- |
| 0 | 4 | u32 | 逻辑拍号 n（通信回合里是回合号） |
| 4 | 1 | i8 | 玩家 0 的 dx |
| 5 | 1 | i8 | 玩家 1 的 dx |
| 6 | 1 | u8 | 仅当 reserved 表示通信回合 FPT：该 key 的回合长度 2–6 |

keys 升序写出。实现打包 `n-RedundantFrames .. n`（缺的 Hist 槽不写）。`RedundantFrames=3`，故 count 通常为 4；开局不足 4 拍则更少。

payload 长度：`5 + 6×count`。count=4 时整包 49 字节。

通信回合把 `Tick` 设为 2–6（下一回合 `FramesPerTurn`）。此时每条多 1 字节 `u8` 该回合长度（2–6），payload 为 `5 + 7×count`。reserved 为 `(ClosedLen<<4)|NextFpt`；只带 NextFpt 的旧包（reserved 2–6）仍解码。`BaseTick` 在内存里是 ClosedLen。

示例（latest=4，两拍：frame 3 为 `(1,-1)`，frame 4 为 `(0,1)`）：

```text
头 20 字节 type=2
04 00 00 00  02
03 00 00 00  01 FF
04 00 00 00  00 01
```

客户端用 `frame` 填 `Buf`。已执行的拍丢掉。没有 `ExecFrame` 这一槽就停，禁止跳帧。

## 3 `S2CSnapshot`

状态同步的数据帧。网上是权威坐标。

| 偏移 | 宽 | 类型 | 字段 | 约束 |
| --- | --- | --- | --- | --- |
| 0 | 4 | u32 | tick | 服务器模拟拍号，发出该快照时的 `Tick` |
| 4 | 4 | u32 | base_tick | 0 = 全量 x。非 0 = 相对该 tick 已 ACK 快照的差 |
| 8 | 1 | u8 | player_count | 必须为 `2`，否则丢弃 |
| 9 | 8 | — | 玩家 0 | `i32 x` + `u32 last_processed_seq` |
| 17 | 8 | — | 玩家 1 | 同上 |

每名玩家 8 字节：

| 相对玩家块 | 宽 | 类型 | 字段 |
| --- | --- | --- | --- |
| 0 | 4 | i32 | 全量时为世界 x。增量时为 `当前x - 基快照x` |
| 4 | 4 | u32 | 服务器已消费的该玩家最大输入 seq |

payload 固定 21 字节。整包 41 字节。

服务器每 3 个模拟拍发一次（约 20Hz）。基必须是客户端 ACK 过的 tick。
客户端解不出基则连发 `C2SSnapAck` 且 `tick=0`，服务器下一份改发全量。`tick <= LastAckedTick` 的旧快照丢弃。

## 4 `C2SSnapAck`

| 偏移 | 宽 | 类型 | 字段 |
| --- | --- | --- | --- |
| 0 | 1 | u8 | player_id |
| 1 | 4 | u32 | 已应用的快照 tick |

payload 5 字节。整包 25 字节。客户端收到可用快照后连发两次，抗丢包。
`tick=0` 表示「我解不出增量基，请改发全量」；服务器把该玩家 `LastAck` 置 0。

## 5 `P2PInput`

回滚的数据帧。只带**自己**的 dx 磁带，不带对端，不带 `X` / `Rng`。

| 偏移 | 宽 | 类型 | 字段 | 约束 |
| --- | --- | --- | --- | --- |
| 0 | 1 | u8 | count | 后面条目数。实现里最多 4 |
| 1 | 5×count | — | 条目 | 每条 `u32 frame + i8 dx` |

一条：

| 相对条目 | 宽 | 类型 | 字段 |
| --- | --- | --- | --- |
| 0 | 4 | u32 | 该输入预定执行的逻辑帧 |
| 4 | 1 | i8 | 发送端自己的 dx |

payload 长度：`1 + 5×count`。count=4 时整包 41 字节。

打包 `max(0, EndF-3) .. EndF`。`INPUT_DELAY=1` 时本地写入 `Local[Frame+1]`，第 0 帧不上网。接收端按 `frame` 填洞，不要按到达顺序 `Step`。

## 6 `C2SChecksum`

| 偏移 | 宽 | 类型 | 字段 |
| --- | --- | --- | --- |
| 0 | 1 | u8 | player_id |
| 1 | 4 | u32 | 刚 Step 完的逻辑拍号 |
| 5 | 4 | u32 | `FNsWorld::Checksum()` |

payload 9 字节。整包 29 字节。锁步客户端每 15 拍发一次。服务器对照自己的 `Checksums[frame]`：相同则 `ChecksumOk++`，不同则 `bDesync`。未知拍号忽略。

## 7 `S2CJoinSnap`

锁步中途加入 / 重连。这是自研协议里**唯一常规携带世界状态**的包（正常 `S2CFrame` 不带 x）。

| 偏移 | 宽 | 类型 | 字段 | 含义 |
| --- | --- | --- | --- | --- |
| 0 | 4 | u32 | exec_frame | 客户端下一拍，等于 `SnapFrame+1`；尚无快照则为 0 |
| 4 | 4 | i32 | x0 | 快照里玩家 0 的 X |
| 8 | 4 | i32 | x1 | 快照里玩家 1 的 X |
| 12 | 4 | u32 | rng | 快照里的 LCG |
| 16 | 1 | u8 | count | 快照之后的输入拍数 |
| 17 | 6×count | — | 拍 | 与 `S2CFrame` 相同的 `frame, dx0, dx1` |

payload 长度：`17 + 6×count`。服务器每 75 拍存一份 `SnapWorld`，`SendJoin` 连发两次。
超 MTU 时同一快照切成多个 JoinSnap，每片独立成 UDP 包。
停拍拉齐复用本 type：`count=0`，`exec_frame` / x / rng 来自**当前**服务器世界，见 [hybrid/resync.md](hybrid/resync.md)。

## 8 `S2CDoorOpen`

锁步加门的开关。不带 pawn 坐标。

| 偏移 | 宽 | 类型 | 字段 | 约束 |
| --- | --- | --- | --- | --- |
| 0 | 4 | i32 | open | 0 关，1 开。其它值客户端仍照写 |

payload 长度：4。整包 24 字节。无 ACK。实现里服务器每个锁步加门泵周期重发当前值。

## 拒收规则

解码任一条失败则整包丢弃，不断连接：

- 短于 20 字节
- magic 不对
- type 不是 1–8
- `包长 != 20 + payload_len`
- payload 内部读越界，或读完后 `off != 包长`
- `S2CSnapshot` 的 `player_count != 2`
- `win` / `count` 导致编码时 `>255`，或单数据报 `>1200`（发送前应已拆片）

## 方案对照（线上到底有什么）

| | 锁步数据帧 | 状态同步数据帧 | 回滚数据帧 |
| --- | --- | --- | --- |
| 主 type | `S2CFrame` | `S2CSnapshot` | `P2PInput` |
| 主体 | 两人 `dx` | 两人 `x` + 已处理 seq | 自己的 `(frame, dx)` |
| 位置 | 仅 Join | 每份快照 | 无 |
| 典型整包 | 49 B（4 拍冗余） | 41 B | 41 B（4 条） |

位移由本地 `X += dx * Speed` 算出（Speed 为 8 / 4 / 3）。lerp、胶囊、相机不进包。

UE 复制（`Counter`、`bOpen`）走 `UNetDriver`，不是本格式。

## 代码

| 项 | 位置 |
| --- | --- |
| 常量 `HeaderBytes` / `PacketMagic` / `MaxPacketBytes` | `NsTypes.h` |
| `ENsMsg` / `FNsPacket` | `NsPacket.h` |
| 读写、长度、`NsSplitForMtu` | `NsCodec.cpp` |
| 填 seq/ack、去重 | `FNsSeqWindow`（`NsNet.h`） |
