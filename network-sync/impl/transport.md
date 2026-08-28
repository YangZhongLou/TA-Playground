# 传输层（先做这个）

三套玩法协议都假设：不可靠、乱序、可重复的 UDP，再在应用层补可靠。
不要用 TCP 做 15～60Hz 游戏循环。

开发期用 `FNsFakeNet`（`NsFakeNet.h`）：内存队列，带延迟、抖动、丢包。
`Send` 把包编成小端字节再解回。`FNsUdpNet`（`NsUdpNet.h`）把同一字节发到本机三个 UDP 端口。
协议自测默认假网络；`TA.NetworkSync.Udp.*` 走真 socket。

## 包头（所有类型共用）

按小端。`magic` 用来扔掉非本游戏的包。`NsEncodePacket` / `NsDecodePacket` 实现这份布局。

| 偏移 | 类型 | 字段 | 含义 |
| --- | --- | --- | --- |
| 0 | u32 | magic | 固定 `0x54414E53`（随意，但两端一致） |
| 4 | u8 | type | 见下表 |
| 5 | u8 | flags | bit0=请 ACK |
| 6 | u16 | payload_len | 头之后的字节数 |
| 8 | u32 | seq | 发送端递增，从 1 起 |
| 12 | u32 | ack | 已收到的对端最大 seq |
| 16 | u32 | ack_bits | 对 ack-1 .. ack-32 的选择性确认 |

type（`ENsMsg`）：

| 值 | 名字 | 谁发 |
| --- | --- | --- |
| 1 | `C2S_INPUT` | 客户端 → 服务器，锁步/预测用 |
| 2 | `S2C_FRAME` | 服务器 → 客户端，锁步输入帧 |
| 3 | `S2C_SNAPSHOT` | 服务器 → 客户端，状态快照 |
| 4 | `C2S_SNAP_ACK` | 客户端 → 服务器，确认快照 tick |
| 5 | `P2P_INPUT` | 回滚对等输入 |
| 6 | `C2S_CHECKSUM` | 锁步校验 |
| 7 | `S2C_JOIN_SNAP` | 锁步重连快照 |

payload 紧跟 20 字节头。大于 MTU（按 1200 字节安全值）就在应用层拆片，这里第一版禁止拆片：超了就缩小冗余。

## Payload 布局

字段一律小端。`TMap` 按 key 升序写出。

| type | payload |
| --- | --- |
| `C2S_INPUT` | `u8 player_id`、`i8 dx`、`u8 win`、然后 `win` 次 `u32 seq + i8 dx` |
| `S2C_FRAME` | `u32 latest`、`u8 count`、然后 `count` 次 `u32 frame + i8 dx0 + i8 dx1` |
| `S2C_SNAPSHOT` | `u32 tick`、`u32 base_tick`、`u8 2`、每玩家 `i32 x + u32 last_seq` |
| `C2S_SNAP_ACK` | `u8 player_id`、`u32 tick` |
| `P2P_INPUT` | `u8 count`、然后 `count` 次 `u32 frame + i8 dx` |
| `C2S_CHECKSUM` | `u8 player_id`、`u32 tick`、`u32 hash` |
| `S2C_JOIN_SNAP` | `u32 exec_frame`、`i32 x0`、`i32 x1`、`u32 rng`、`u8 count`、然后输入帧 |

锁步的 `C2S_INPUT` 把 `win` 写成 0。Src/Dst 不进字节，由 socket 地址决定。

## 发送端状态

`FNsFakeNet` 已维护每源 `NextSeq` 和每目的 `RecvMax` / `RecvBits`。接真 socket 时沿用同一套窗。

```cpp
int32 Seq = 1;
int32 RecvMax = 0;
uint32 RecvBits = 0;

void OnRecvSeq(int32 S)
{
    if (S > RecvMax)
    {
        const int32 Shift = S - RecvMax;
        RecvBits = (Shift < 32) ? (RecvBits << Shift) : 0u;
        RecvMax = S;
    }
}
```

发出去时填 `seq`，然后 `seq += 1`；`ack=RecvMax`，`ack_bits=RecvBits`。

## 假网络（开发期）

`FNsFakeNet::Send` 会填 `Seq` / `Ack` / `AckBits`，再 `NsEncodePacket` / `NsDecodePacket`。
`FNsUdpNet::BindLoopback` 为 `Sv` / `C0` / `C1` 各开一个 IPv4 数据报，Src/Dst 由端口反查。
`Drain` 按目的端去重同一 `Seq`。假网络乱序用 jitter；真 UDP 由内核排队。

第一周把 `Drop` 设 0、`RttMs` 设 80，只验证逻辑。第二周 `Drop=0.05`。
`ns.SelfTest` 三套协议分别开了 0.1 / 0.05 / 0.05 丢包。

## 验收

- 同一 `seq` 处理两次必须幂等（用 `last_seq` 集合或 1024 大小的位窗）。
- `payload_len` 与实际长度不符则丢弃整包。
- 未知 `type` 丢弃，不要断连接（版本滚动时有用）。
