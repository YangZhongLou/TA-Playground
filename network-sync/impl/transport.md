# 传输层（先做这个）

三套玩法协议都假设：不可靠、乱序、可重复的 UDP，再在应用层补可靠。
不要用 TCP 做 15～60Hz 游戏循环。

开发期用 `FNsFakeNet`（`NsFakeNet.h`）：内存队列，带延迟、抖动、丢包。
`Send` 把包编成小端字节再解回。`FNsUdpNet`（`NsUdpNet.h`）按地址表把同一字节发到对端 IP:port。
`Bind` 使用 IPv4 数据报（`FNetworkProtocolTypes::IPv4`）。载荷上限仍按 IPv6 最小 MTU 留余量，见 [packet-format.md](packet-format.md)。
`BindLoopback` 用于单进程自测；`Bind` + `SetPeer` 用于 Host/Client 两进程。

## STUN Binding（RFC 5389）

STUN 不是 `FNsPacket`。字节大端，magic `0x2112A442`，与 TANS 小端 `0x54414E53` 分开。
`Drain` 解不出 TANS 就丢（TURN 中继打开时先拆 ChannelData），所以 Binding / 打洞 / 会合 / 连通检查 / TURN Allocate / CreatePermission / ChannelBind / ICE 候选
走 `StunSendBind` / `StunRecvMapped` / `StunSendIndication` / `StunServe` / `RendezvousSendOffer` / `StunSendAllocate` /
`StunRecvRelayed` / `StunSendPermission` / `StunRecvPermission` / `StunSendChannelBind` / `StunSendChannelData` / `IceSendOffer` / `StunSendNominate`，不要塞进 `ENsMsg`。

Binding 问 STUN 服务器“我的映射 IPv4:port 是什么”。Host/Client 在 Binding 之后，对已填的 peer 发 Binding Indication（`0x0011`，无响应）开 NAT，
再发 Binding Request；对端当 STUN 代理回 XOR-MAPPED Success，确认这条路径通。
LocalMesh 跳过。可列出并配对 host / srflx 候选；Host 先检查再对通的那对发 USE-CANDIDATE，Client 收到提名后再 `SetPeer`。
`NsIceSdpEncode` / `NsIceSdpDecode` 把同一清单编成 SDP 文本；会合仍走 `NSIC`，不改助手缓冲。
`UdpStunHost` 为空则跳过 Binding（自动化保持空）。失败只打警告，不拆 socket。
填了 `UdpTurnHost` 时，对每个已绑 socket 打一次 TURN Allocate（`0x0003` + REQUESTED-TRANSPORT UDP），
成功则记下 Relayed* 并打日志，不改 `Mapped*` / `UdpRemoteHost`。
填好 peer 后再对每个对端 IP 打 CreatePermission（`0x0008` + XOR-PEER-ADDRESS），
再 ChannelBind（`0x0009`，channel `0x4000+slot`）。
每个请求有独立 txid，按 socket 和 txid 匹配全部响应；乱序、重复响应不能提前完成其他请求。

ChannelData 可把 TANS 载荷交给 TURN 转发。连通检查失败且 ChannelBind 成功时，`EnableTurnRelay` 让 `Send` 把 TANS 封进 ChannelData 发往 TURN；`Drain` 按 channel `0x4000+slot` 还原 Src。自动化里 `UdpTurnHost` 为空，不走中继。
UDP ChannelData 收发支持最多 1200 字节应用载荷，另加 4 字节头及对齐填充，超限则拒绝。
TURN 向 peer 转发时，从分配的 relay 地址发出裸 TANS；peer 回包到 relay 地址后，TURN 再封装 ChannelData 发给客户端。

无 MESSAGE-INTEGRITY。`UdpTurnHost` 为空则跳过（自动化保持空）。失败只打警告。

填了 `UdpRendezvousHost` 时，两端先用 `IceExchange` 换 `NSIC` 候选清单，`SetPeer` 取对端 host 候选（无 host 则取清单第一条），
再 `IceCheckPairs`（Host，先检查再提名）或 `IceWaitNominate`（Client，收到提名才改 `SetPeer`）。
助手若只懂 12 字节 `NSRV`，再回落到 `RendezvousExchange`。
`FNsRendezvousHub` 在进程内绑定一个 UDP 口，记下每 slot 最后一包 `NSIC` 或 `NSRV`，回给对端；TANS / STUN 丢弃。
第三进程跑 `ns.RendezvousHub [port]`（默认 3479），Host/Client 把 `UdpRendezvousHost` 指过去；`ns.RendezvousHubStop` 停。自动化保持 `UdpRendezvousHost` 为空。
助手不是 TANS，`Drain` 会丢。助手为空则仍人手填 `UdpRemoteHost`。自动化保持空。

`RendezvousExchange` 要求调用方传入 `RequiredPeers`，收齐才返回成功；超时未收齐则由 Manager 回落到手动地址。
Host 要求 C1，Rollback Client 要求 C0，其他 Client 要求 Sv 和 C0；先收到 C0 不能跳过服务器地址。

会合线上的 ICE 候选是 `NSIC`，不是 SDP。小端 magic `0x4E534943`，6 字节头（slot + count）加每条 7 字节 `(type, port, ipv4)`。
type：0 host、1 srflx、2 relay。每 socket 最多 3 条，地址端口重复则去重。
`GatherIceCandidates` 从本机端口、STUN 映射、TURN 中继拼清单。
`IceSendOffer` / `IceRecvPeer` / `IceExchange` 经会合助手交换清单；`IceRecvPeer` 拒收 `NSRV`。
`NsIceFormPairs` 把本端 host/srflx 与对端 host/srflx 排成对（host-host 优先，不含 relay）。
`IceCheckPairs` 按序对每对发普通 Binding（Host 控制方）；先通的地址 `SetPeer`，再用独立 txid 对该地址发 USE-CANDIDATE。
`IceWaitNominate` 只在收到 USE-CANDIDATE 时 `SetPeer`（Client）。
`NsIceSdpEncode` 写出 RFC 4566 会话加 `a=candidate` 行；`o=- <slot>` 带 NSIC 槽位。`NsIceSdpDecode` 认 CRLF / LF，只收 UDP 的 host / srflx / relay，最多 3 条。无 ice-ufrag。会合不发这段文本。
无远程 ICE 清单时仍走 `StunCheckPeers`。relay 对仍靠 TURN ChannelData。

| 项 | 值 |
| --- | --- |
| Binding Request | `0x0001` |
| Binding Indication | `0x0011`（无响应；peer 不核对 txid） |
| Success Response | `0x0101` |
| Allocate Request | `0x0003` |
| Allocate Success | `0x0103` |
| CreatePermission Request | `0x0008` |
| CreatePermission Success | `0x0108` |
| ChannelBind Request | `0x0009` |
| ChannelBind Success | `0x0109` |
| CHANNEL-NUMBER | `0x000C`（`0x4000`–`0x7FFF`） |
| ChannelData | 4 字节头（channel + length）+ 载荷，4 字节对齐填充 |
| XOR-MAPPED-ADDRESS | `0x0020`（IPv4 family `0x01`） |
| XOR-PEER-ADDRESS | `0x0012` |
| XOR-RELAYED-ADDRESS | `0x0016` |
| REQUESTED-TRANSPORT | `0x0019`（UDP=17） |
| 会合 | `NsRendezvousEncode`，12 字节，slot + port + ipv4 |
| ICE 候选 | `NsIceEncode`，`NSIC` + slot + count + 每条 type/port/ipv4 |
| ICE SDP | `NsIceSdpEncode` / `NsIceSdpDecode`，`o=- <slot>` + `a=candidate` |
| ICE 配对 | `NsIceFormPairs`，host/srflx 笛卡尔积，host-host 优先 |
| 会合助手 | `FNsRendezvousHub` / `ns.RendezvousHub`，转发 `NSIC` / `NSRV` |
| USE-CANDIDATE | `0x0025`（length 0），Host 在检查成功后才带；Client `IceWaitNominate` 认 |
| 编解码 | `NsStun.h` |

自动化：`NetworkSync.Stun.Bind`（编解码，含 Indication / 会合 / Request / Allocate / CreatePermission / ChannelBind / ChannelData / USE-CANDIDATE）、
`NetworkSync.Stun.Loopback`（进程内假 STUN + `FNsUdpNet` C0）、
`NetworkSync.Stun.Punch`（两 socket Indication 后 TANS 仍通）、
`NetworkSync.Stun.Rendezvous`（假助手换地址后打洞再 TANS）、
`NetworkSync.Stun.Check`（对端 Binding Request/Response 后 TANS）、
`NetworkSync.Stun.Turn`（进程内假 TURN Allocate）、
`NetworkSync.Stun.Permit`（进程内假 TURN CreatePermission）、
`NetworkSync.Stun.Channel`（假 TURN ChannelBind 后转发 ChannelData 里的 TANS）、
`NetworkSync.Stun.Relay`（假 TURN 上 `Send` / `Drain` 走 ChannelData）、
`NetworkSync.Stun.Ice`（候选清单编解码与 host/srflx/relay gather）、
`NetworkSync.Stun.IceExchange`（假助手换 `NSIC` 后打洞再 TANS）、
`NetworkSync.Stun.IcePairs`（host-host 优先配对，host 不通则改 srflx）、
`NetworkSync.Stun.IceNominate`（先检查后提名；Client 只在 USE-CANDIDATE 后改 `SetPeer`）、
`NetworkSync.Stun.Hub`（进程内 `FNsRendezvousHub` 转发 `NSIC` / `NSRV` 后再 TANS）、
`NetworkSync.Stun.HubProcess`（后台 `NsStartRendezvousHub` 供两端 `IceExchange`）、
`NetworkSync.Stun.IceSdp`（SDP 候选清单编解码）。不打公网 STUN / TURN。

回归：`NetworkSync.Stun.RendezvousOrder` 覆盖乱序、缺地址和 Host / Rollback 必需 peer；
`.ChannelPeers` / `.PermitPeers` 覆盖多请求、跨 socket、乱序、重复和缺失响应；
`.ChannelMtu` 覆盖 508 / 509 / 1200 字节及超限拒绝。
`.Channel` 用独立 relay socket 验证近 MTU 的裸 TANS 转发和回程 ChannelData 封装。
`.Relay` 验证直连失败后 `Send` 封装、对端 `Drain` 裸 TANS、回程 `Drain` 拆 ChannelData。
`.Ice` 验证 `NSIC` 往返、拒收会合包，以及 Binding / Allocate 之后 gather 出 host / srflx / relay。
`.IceExchange` 验证助手转发清单后 `SetPeer` 用 host 候选，并拒收 `NSRV`。
`.IcePairs` 验证假 host 候选不通时改用 srflx，再 TANS。
`.IceNominate` 验证两端假 host 不通时，控制方先检查 srflx 再发 USE-CANDIDATE，受控方按提名 `SetPeer`，再双向 TANS。
`.Hub` 验证助手线程 `Serve` 时两端 `IceExchange` / `RendezvousExchange` 都能换到对端 host 并 TANS。
`.HubProcess` 验证 `NsStartRendezvousHub` 后台转发后可停。
`.IceSdp` 验证 host / srflx / relay 往返、LF、以及坏 slot / TCP / prflx / 超条数拒绝。会合仍走 `NSIC`。

## 包头（所有类型共用）

按小端。`magic` 用来扔掉非本游戏的包。`NsEncodePacket` / `NsDecodePacket` 实现这份布局。

| 偏移 | 类型 | 字段 | 含义 |
| --- | --- | --- | --- |
| 0 | u32 | magic | 固定 `0x54414E53`（随意，但两端一致） |
| 4 | u8 | type | 见下表 |
| 5 | u8 | reserved | 默认 `0`。通信回合 `S2CFrame`：`(ClosedLen<<4)\|NextFpt`（2–6），或仅 NextFpt |
| 6 | u16 | payload_len | 头之后的字节数 |
| 8 | u32 | session | 发送进程启动或重置时生成的非零随机 epoch |
| 12 | u32 | seq | 当前 session 内递增，从 1 起 |
| 16 | u32 | ack | 已收到的对端最大 seq |
| 20 | u32 | ack_bits | 对 ack-1 .. ack-32 的选择性确认 |

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
| 9 | `C2SFrameNack` | 锁步按号补发 |
| 10 | `C2SFire` | 状态同步倒带开火 |

payload 紧跟 24 字节头。逐字段宽度、示例和长度公式见 [packet-format.md](packet-format.md)。
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
| `C2SFrameNack` | `2 + 4×count` | 缺的逻辑拍号，count 通常 ≤8 |
| `C2SFire` | 5 | `player_id` + 上报 RTT（毫秒） |

## 发送端状态

`FNsFakeNet` 已维护每源 `SendSession` / `NextSeq` 和每对 `(Dst, Src)` 的
`RecvSession` / `RecvMax` / `RecvBits`（`FNsSeqWindow`，`NsNet.h`）。接真 socket 时沿用同一套窗。

```cpp
uint32 SendSession = NewNonZeroSession();
int32 Seq = 1;
uint32 RecvSession = 0;
int32 RecvMax = 0;
uint32 RecvBits = 0;

void OnRecvSeq(uint32 Session, int32 S)
{
    if (Session != RecvSession)
    {
        Retire(RecvSession);
        RecvSession = Session;
        RecvMax = 0;
        RecvBits = 0;
    }
    if (S > RecvMax)
    {
        const int32 Shift = S - RecvMax;
        RecvBits = (Shift < 32) ? (RecvBits << Shift) : 0u;
        RecvMax = S;
    }
}
```

发出去时填 `session` 与 `seq`，然后 `seq += 1`；`ack=RecvMax`，`ack_bits=RecvBits`。
进程重启或 `ResetSession` 会换 session 并把 seq 置回 1。接收端切到新 session 后保留最近退役 epoch，迟到的旧包不能把窗口切回去。

## 假网络（开发期）

`FNsFakeNet::Send` 先 `Stamp` 再按 `Drop` 丢弃：丢掉的包仍消耗 seq。
再 `NsEncodePacket` / `NsDecodePacket`。`FNsUdpNet::Bind` 只开本端数据报；`SetPeer` 登记对端主机和端口。
已有 `PeerPorts[i] > 0` 时 Bind 不得把 peer 改成自己。
`MakeDest` 解析失败返回 false，禁止回落到 loopback。
`FindPeer`：空 host 视为未设置，不当通配。`BindLoopback` 仍为三人各开一端口。
`Drain` 用来源 IP:port 反查 `ENsAddr`。假网络乱序用 jitter；真 UDP 由内核排队。

两份编辑器：都勾 `bUseUdp`，一份 `Host`、一份 `Client`，同一 `UdpBasePort`（如 27000），
`UdpRemoteHost` 填对端 IPv4。局域网勾 `bUdpLan`。
可选填 `UdpStunHost`（点分 IPv4，不解析 DNS）在 Bind 后打一次 Binding。可选填 `UdpTurnHost` 打一次 Allocate。
可选填 `UdpRendezvousHost` 用会合助手换对端地址；空则仍填 `UdpRemoteHost`。不要靠 STUN / TURN 自动改 `UdpRemoteHost`。
锁步 / 状态同步的 Host 绑 Sv+C0；回滚 Host 只绑 C0，对端 C1。
自动化：`NetworkSync.Udp.Split`（锁步）、`.SplitState`、`.SplitRollback`。

第一周把 `Drop` 设 0、`RttMs` 设 80，只验证逻辑。第二周 `Drop=0.05`。
`ns.SelfTest` 三套协议分别开了 0.1 / 0.05 / 0.05 丢包。

## 验收

- 同一 `seq` 处理两次必须幂等（用 `last_seq` 集合或 1024 大小的位窗）。
- `payload_len` 与实际长度不符则丢弃整包。
- 未知 `type` 丢弃，不要断连接（版本滚动时有用）。
- `session=0`、`seq<=0`、重复 seq 或退役 session 的迟到包丢弃。
- 序号窗口按 **(接收端, 发送端)** 记账，两个客户端发往服务器的 seq=1 互不打架。
- 发出的每个 UDP 数据报 ≤ 1200 字节。超长必须先 `NsSplitForMtu`，禁止靠 IP 分片。
- 玩法层用 `Src` 认玩家，不信任 payload `player_id`。
- 玩法层只接受合法方向：C2S 只能客户端到服务器，S2C 只能服务器到客户端，P2P 只能 C0/C1 互发。
- `NsMeasureFakeNetDrop` / `ns.DropRate`：配置的 `Drop` 与实测丢包率在 0/1 时精确，中间档偏差 ≤ 0.03（2000 包）。
  自动化：`NetworkSync.FakeNet.DropRate`。
