# 帧同步：实现规格

目标：两个客户端 + 一个服务器，15Hz 逻辑，整数坐标，丢包靠冗余，checksum 发现分叉。
概念对照 [../schemes/lockstep.md](../schemes/lockstep.md)。
代码：`Plugins/NetworkSync/.../NsTypes.*`、`NsLockstep.*`、`NsSelfTest.cpp`。

## 常量

`NsTypes.h` 里的 `Ns::` 命名空间：

```cpp
constexpr int32 LogicDtMs = 66;       // 1000/15，用整数毫秒
constexpr int32 PlayerCount = 2;
constexpr int32 RedundantFrames = 3;  // 每个下行包带上前 3 拍
constexpr int32 ChecksumEvery = 15;   // 每秒一次
constexpr int32 LockstepSpeed = 8;    // 每逻辑拍位移（整数）
```

逻辑拍时长必须整毫秒。不要用 `1/15` 的 float 累加。

## 状态与输入（必须确定）

```cpp
struct FNsInputs
{
    int8 Dx[Ns::PlayerCount] = {0, 0};  // 每槽 -1, 0, +1
};

struct FNsWorld
{
    int32 X[Ns::PlayerCount] = {0, 0};
    uint32 Rng = 1;
    void Step(const int8 Dxs[Ns::PlayerCount], int32 Speed);
    uint32 Checksum() const;
};
```

`Step` 里禁止：float、字典遍历、系统时间、未初始化内存。
单机验收：两个 `FNsWorld` 同种子，喂同一串输入，1000 拍后 `Checksum` 必须相等。
测试名：`TA.NetworkSync.World.Determinism`。

## 包 payload

### `C2S_INPUT`

客户端尽快发。服务器只保留每玩家「最新尚未编入帧的输入」。

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| player_id | u8 | 0 .. PLAYER_COUNT-1 |
| dx | i8 | -1, 0, 1 |

### `S2C_FRAME`

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| frame | u32 | 本包最新拍号 n |
| count | u8 | 后面跟多少拍，= 1 + 冗余（最多 4） |
| 重复 count 次：frame_i, dx[0], dx[1], ... | u32 + i8*N | 从 n 往回 |

客户端用 `frame_i` 填自己的槽。已执行过的拍丢掉。缺的拍继续等。

## 服务器主循环

服务器有墙钟，但只在 `now_ms >= next_logic_ms` 时打一拍。见 `FNsLockstepServer::Tick`。

```cpp
FNsInputs Latest;                 // 缺包则沿用
TMap<int32, FNsInputs> Hist;
int32 Frame = 0;
double NextMs = 0.0;

void OnInput(int32 PlayerId, int8 Dx)
{
    Latest.Dx[PlayerId] = NsClampDx(Dx);
}

void Tick(FNsFakeNet& Net)
{
    while (Net.Now >= NextMs)
    {
        Hist.Add(Frame, Latest);
        World.Step(Latest.Dx, Ns::LockstepSpeed);
        // pack Frame .. Frame-RedundantFrames，广播给 C0/C1
        ++Frame;
        NextMs += Ns::LogicDtMs;
    }
}
```

不等待「所有人本拍都有新包」。到点就广播。这是乐观锁步。
`pack` 把 n、n-1、n-2、n-3 的输入都放进去（n<0 的跳过）。
服务器要保存最近 `RedundantFrames+1` 拍的输入数组，供补发。

补发：客户端发现缺拍 n 且等了超过 `LogicDtMs*2`，发一个「请重发 n」或靠下一包冗余带上。
第一版只靠冗余，不要做复杂请求。

## 客户端主循环

见 `FNsLockstepClient::Logic`。

```cpp
int32 ExecFrame = 0;
TMap<int32, FNsInputs> Buf;
FNsWorld World;

void OnS2C(const TMap<int32, FNsInputs>& Frames)
{
    Buf.Append(Frames);
}

void Logic(FNsFakeNet& Net)
{
    while (const FNsInputs* Found = Buf.Find(ExecFrame))
    {
        PrevX[0] = World.X[0];
        PrevX[1] = World.X[1];
        World.Step(Found->Dx, Ns::LockstepSpeed);
        if (ExecFrame % ChecksumEvery == 0)
        {
            // C2SChecksum(frame, World.Checksum())
        }
        ++ExecFrame;
    }
}
```

**禁止**在缺 `ExecFrame` 时执行 `ExecFrame+1`。乱序到达只进 `Buf`。

渲染：`Step` 前把 `PrevX` 存下来。`ANsNetManager` 用 `AccumMs / LogicDtMs` 做 `lerp(PrevX, World.X)`，只用于画。

## 校验

服务器若跑了同一份 `World`：每 `ChecksumEvery` 拍比对客户端上报。
`FNsLockstepServer::OnChecksum` 对不上则 `bDesync`。`ns.SelfTest` 要求 `ChecksumOk > 0` 且不分叉。

## 实现顺序（按天）

1. 单机两个 `FNsWorld` 喂同一输入，checksum 相同。
2. 假网络 Drop=0，服务器 15Hz 广播，两端 ExecFrame 对齐、checksum 对齐。
3. `RedundantFrames=3`，Drop=0.1，ExecFrame 仍能追上，不卡死超过 1 秒。
4. 表现插值 + 定期 checksum 上报。已做。
5. 重连：服务器存每 5 秒一份 `World` 快照 + 之后输入；客户端加载快照再快进。尚未做。

## 验收命令

编辑器或 PIE 控制台：

```text
ns.SelfTest
```

日志必须含 `lockstep frames=`。自动化：`TA.NetworkSync.Lockstep`。
自测已开 `Drop=0.1` 与冗余。
