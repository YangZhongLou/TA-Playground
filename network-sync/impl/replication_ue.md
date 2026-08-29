# UE 对象复制：实现规格

在本仓库（UE5）做联机原型时走这条，不要自研快照。概念见
[../schemes/replication.md](../schemes/replication.md) 与 [../unreal.md](../unreal.md)。

整数复制：`ANsReplicatedActor`，按 `E` 增加 `Counter`。
门：`ANsDoor`，按 `F` 切换 `bOpen`。Scheme 选 Replication 或 `ns.SpawnDemo` 后改 Scheme。
离开 Replication 时 Manager 会 Destroy 这两个 Actor。

下面仍给出门的最小集。先 Listen Server 开两份编辑器。

## 项目开关

1. `Edit > Plugins` 启用所需网络插件（默认 Game 即可）。
2. `DefaultEngine.ini`：

```ini
[/Script/Engine.GameEngine]
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="OnlineSubsystemUtils.IpNetDriver",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")

[/Script/OnlineSubsystemUtils.IpNetDriver]
NetServerMaxTickRate=30
MaxClientRate=15000
MaxInternetClientRate=10000
```

地图：`GameMode` 设成你的 `AMyGameMode`（只服务器跑规则）。
运行：第一份编辑器 `Play as Listen Server`，第二份 `Play as Client`。

## 第一步：复制一个整数

`MyReplicatedActor.h`：

```cpp
UCLASS()
class AMyReplicatedActor : public AActor
{
    GENERATED_BODY()
public:
    AMyReplicatedActor();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

    UPROPERTY(ReplicatedUsing=OnRep_Counter)
    int32 Counter = 0;

    UFUNCTION()
    void OnRep_Counter();

    UFUNCTION(Server, Reliable)
    void ServerBump();
};
```

`MyReplicatedActor.cpp`：

```cpp
#include "Net/UnrealNetwork.h"

AMyReplicatedActor::AMyReplicatedActor()
{
    bReplicates = true;
    SetReplicateMovement(false);
}

void AMyReplicatedActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(AMyReplicatedActor, Counter);
}

void AMyReplicatedActor::OnRep_Counter()
{
    // 只改表现：例如改材质。不要在这里改玩法规则。
}

void AMyReplicatedActor::ServerBump_Implementation()
{
    Counter += 1;
}
```

客户端按键：引擎只允许 Owner 调 `ServerBump()`。非 Owner 调 Server RPC 会被丢。
本插件 **不** `SetOwner`。Listen 主机本地按 `E`/`F` 走 authority 实现；远端客户端按键可能被丢，用主机操作验收复制。

验收：Listen 上按键，Client 的 `Counter` 变。

## 第二步：门用属性，不用每帧 RPC

```cpp
UPROPERTY(ReplicatedUsing=OnRep_Open)
bool bOpen = false;

UFUNCTION(Server, Reliable)
void ServerSetOpen(bool bNewOpen);

void AMyDoor::ServerSetOpen_Implementation(bool bNewOpen)
{
    bOpen = bNewOpen; // OnRep 在模拟端执行；Listen 主机要自己调开关表现
}
```

主机自己改 `bOpen` 时 **OnRep 不会跑**。主机要在 `ServerSetOpen_Implementation` 里同时改表现。

## 第三步：谁复制给谁

| 对象 | 设置 |
| --- | --- |
| GameMode | 不复制 |
| GameState | `bReplicates=true`，比分放这里 |
| PlayerState | 默认复制，分数、名字 |
| PlayerController | 仅 Owner，输入和 UI |
| Pawn | 本插件不提供；玩法项目自选引擎移动 |

`bOnlyRelevantToOwner=true` 用在私有库存。误开在 Pawn 上，别人会看不见你。

## 实现顺序

1. Listen + Client 进同一张图，打 `log LogNet Verbose`。
2. 复制 `int32` + Server RPC。
3. 门的 bool。
4. 再谈休眠、频率、Replication Graph。人数 < 8 不要上 Graph。

## 验收清单

- Client 改本地 `Counter` 而不走 RPC：下一拍被服务器值覆盖。
- 用可靠 RPC 每 Tick 发位置：丢包时卡一下再瞬移——这是反例，不要合并进主分支。
- 断开 Client：服务器 Actor 仍在；重新进来要能再收到当前 `Counter`。
