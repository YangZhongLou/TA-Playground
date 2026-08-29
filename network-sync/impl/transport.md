# 传输层（先做这个）

三套玩法协议都假设：不可靠、乱序、可重复的 UDP，再在应用层补可靠。
不要用 TCP 做 15～60Hz 游戏循环。

开发期用 `FNsFakeNet`（`NsFakeNet.h`）：内存队列，带延迟、抖动、丢包。
`Send` 把包编成小端字节再解回。`FNsUdpNet`（`NsUdpNet.h`）按地址表把同一字节发到对端 IP:port。
`Bind` 使用 IPv4 数据报（`FNetworkProtocolTypes::IPv4`）。载荷上限仍按 IPv6 最小 MTU 留余量，见 [packet-format.md](packet-format.md)。
`BindLoopback` 用于单进程自测；`Bind` + `SetPeer` 用于 Host/Client 两进程。

## 包头（所有类型共用）

按小端。`magic` 用来扔掉非本游戏的包。`NsEncodePacket` / `NsDecodePacket` 实现这份布局。

| 偏移 | 类型 | 字段 | 含义 |
| --- | --- | --- | --- |
| 0 | u32 | magic | 固定 `0x54414E53`（随意，但两端一致） |
| 4 | u8 | type | 见下表 |
| 5 | u8 | reserved | 编码器写 `0`。解码读掉，不解释 |
| 6 | u16 | payload_len | 头之后的字节数 |
| 8 | u32 | seq | 发送端递增，从 1 起 |
| 12 | u32 | ack | 已收到的对端最大 seq |
| 16 | u32 | ack_bits | 对 ack-1 .. ack-32 的选择性确认 |

type（`ENsMsg`）：

| 值 | 名字 | 谁发 |
| --- | --- | --- |
| 1 | `C2SInput` | 客户端 → 服务器，锁步/预测用 |
| 2 | `S2CFrame` | 服务器 → 客户端，锁步输入帧 |
| 3 | `S2CSnapshot` | 服务器 → 客户端，状态快照 |
| 4 | `C2SSnapAck` | 客户端 → 服务器，确认快照 tick |
| 5 | `P2PInput` | 回滚对等输入 |
| 6 | `C2SChecksum` | 锁步校验 |
| 7 | `S2CJoinSnap` | 锁步重连快照 |
| 8 | `S2CDoorOpen` | 锁步加门开关 |

payload 紧跟 20 字节头。逐字段宽度、示例和长度公式见 [packet-format.md](packet-format.md)。
单数据报 UDP 载荷 ≤ 1200 字节，避免 IP 分片把丢包放大。超长在应用层拆成多个完整 Ns 包，见 `NsSplitForMtu`。Src/Dst 不进字节。

## Payload 摘要

| type | payload 长度 | 内容 |
| --- | --- | --- |
| `C2SInput` | `3 + 5×win` | 锁步 `win=0`；状态同步最多 8 条 `(seq,dx)` |
| `S2CFrame` | `5 + 6×count` | `latest` + 每拍两人 dx，count 通常 4 |
| `S2CSnapshot` | 21 | `tick`、`base_tick`、两人 `x` 与 `last_seq` |
| `C2SSnapAck` | 5 | `player_id` + 已应用 tick |
| `P2PInput` | `1 + 5×count` | 自己的 `(frame, dx)`，count 通常 4 |
| `C2SChecksum` | 9 | `player_id` + 拍号 + hash |
| `S2CJoinSnap` | `17 + 6×count` | `exec_frame`、x0、x1、rng、之后的输入拍 |
| `S2CDoorOpen` | 4 | `open` |

## 发送端状态

`FNsFakeNet` 已维护每源 `NextSeq` 和每对 `(Dst, Src)` 的 `RecvMax` / `RecvBits`（`FNsSeqWindow`，`NsNet.h`）。接真 socket 时沿用同一套窗。

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

`FNsFakeNet::Send` 先 `Stamp` 再按 `Drop` 丢弃：丢掉的包仍消耗 seq。
再 `NsEncodePacket` / `NsDecodePacket`。`FNsUdpNet::Bind` 只开本端数据报；`SetPeer` 登记对端主机和端口。
已有 `PeerPorts[i] > 0` 时 Bind 不得把 peer 改成自己。
`MakeDest` 解析失败返回 false，禁止回落到 loopback。
`FindPeer`：空 host 视为未设置，不当通配。`BindLoopback` 仍为三人各开一端口。
`Drain` 用来源 IP:port 反查 `ENsAddr`。假网络乱序用 jitter；真 UDP 由内核排队。

两份编辑器：都勾 `bUseUdp`，一份 `Host`、一份 `Client`，同一 `UdpBasePort`（如 27000），
`UdpRemoteHost` 填对端 IPv4。局域网勾 `bUdpLan`。
锁步 / 状态同步的 Host 绑 Sv+C0；回滚 Host 只绑 C0，对端 C1。
自动化：`NetworkSync.Udp.Split`（锁步）、`.SplitState`、`.SplitRollback`。

第一周把 `Drop` 设 0、`RttMs` 设 80，只验证逻辑。第二周 `Drop=0.05`。
`ns.SelfTest` 三套协议分别开了 0.1 / 0.05 / 0.05 丢包。

## 验收

- 同一 `seq` 处理两次必须幂等（用 `last_seq` 集合或 1024 大小的位窗）。
- `payload_len` 与实际长度不符则丢弃整包。
- 未知 `type` 丢弃，不要断连接（版本滚动时有用）。
- 序号窗口按 **(接收端, 发送端)** 记账，两个客户端发往服务器的 seq=1 互不打架。
- 发出的每个 UDP 数据报 ≤ 1200 字节。超长必须先 `NsSplitForMtu`，禁止靠 IP 分片。
- 玩法层用 `Src` 认玩家，不信任 payload `player_id`。
- `NsMeasureFakeNetDrop` / `ns.DropRate`：配置的 `Drop` 与实测丢包率在 0/1 时精确，中间档偏差 ≤ 0.03（2000 包）。
  自动化：`NetworkSync.FakeNet.DropRate`。
