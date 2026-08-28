# 传输层（先做这个）

三套玩法协议都假设：不可靠、乱序、可重复的 UDP，再在应用层补可靠。
不要用 TCP 做 15～60Hz 游戏循环。

开发期用 `FNsFakeNet`（`NsFakeNet.h`）：内存队列，带延迟、抖动、丢包。
真 UDP 头可以后接；自测不解析字节，只传 `FNsPacket`。

## 包头（所有类型共用）

按小端。`magic` 用来扔掉非本游戏的包。接真 socket 时再序列化。

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

payload 紧跟 20 字节头。大于 MTU（按 1200 字节安全值）就在应用层拆片，这里第一版禁止拆片：超了就缩小冗余。

## 发送端状态

接真 UDP 时再加 seq/ack 窗。假网络阶段靠应用层冗余（锁步多帧、状态窗口、回滚前 3 拍）。

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

`FNsFakeNet::Send` 会填 `Seq` / `Ack` / `AckBits`。`Drain` 按目的端去重同一 `Seq`。
乱序：jitter 用随机即可。地址：`ENsAddr::Sv / C0 / C1`。

第一周把 `Drop` 设 0、`RttMs` 设 80，只验证逻辑。第二周 `Drop=0.05`。
`ns.SelfTest` 三套协议分别开了 0.1 / 0.05 / 0.05 丢包。

## 验收

- 同一 `seq` 处理两次必须幂等（用 `last_seq` 集合或 1024 大小的位窗）。
- `payload_len` 与实际长度不符则丢弃整包。
- 未知 `type` 丢弃，不要断连接（版本滚动时有用）。
