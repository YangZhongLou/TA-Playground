// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NsFakeNet.h"
#include "NsLockstep.h"
#include "NsLockstepResync.h"
#include "NsLockstepDoor.h"
#include "NsLockstepWait.h"
#include "NsLockstepTurn.h"
#include "NsLockstepDelay.h"
#include "NsStateSync.h"
#include "NsRollback.h"
#include "NsUdpNet.h"
#include "NsSelfTest.h"
#include "NsNetManager.generated.h"

UENUM(BlueprintType)
enum class ENsUdpRole : uint8
{
	LocalMesh UMETA(DisplayName = "Local Mesh"),
	Host      UMETA(DisplayName = "Host Sv+C0"),
	Client    UMETA(DisplayName = "Client C1"),
};

UENUM(BlueprintType)
enum class ENsScheme : uint8
{
	Lockstep     UMETA(DisplayName = "Lockstep"),
	StateSync    UMETA(DisplayName = "State Sync"),
	Rollback     UMETA(DisplayName = "Rollback"),
	Replication  UMETA(DisplayName = "UE Replication"),
};

UENUM(BlueprintType)
enum class ENsLockstepKind : uint8
{
	Optimistic   UMETA(DisplayName = "Optimistic 15Hz"),
	Conservative UMETA(DisplayName = "Conservative wait-all"),
	CommTurn     UMETA(DisplayName = "AoE comm turn"),
	DelayBased   UMETA(DisplayName = "Delay-based"),
};

UCLASS()
class NETWORKSYNC_API ANsNetManager : public AActor
{
	GENERATED_BODY()

public:
	ANsNetManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NetworkSync")
	ENsScheme Scheme = ENsScheme::Lockstep;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NetworkSync", meta = (EditCondition = "Scheme == ENsScheme::Lockstep"))
	ENsLockstepKind LockstepKind = ENsLockstepKind::Optimistic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NetworkSync")
	float DrawScale = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NetworkSync")
	bool bUseUdp = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NetworkSync", meta = (EditCondition = "bUseUdp"))
	int32 UdpBasePort = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NetworkSync", meta = (EditCondition = "bUseUdp"))
	ENsUdpRole UdpRole = ENsUdpRole::LocalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NetworkSync", meta = (EditCondition = "bUseUdp"))
	FString UdpRemoteHost = TEXT("127.0.0.1");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NetworkSync", meta = (EditCondition = "bUseUdp"))
	int32 UdpRemoteBasePort = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NetworkSync", meta = (EditCondition = "bUseUdp"))
	bool bUdpLan = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NetworkSync", meta = (EditCondition = "bUseUdp"))
	FString UdpStunHost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NetworkSync", meta = (EditCondition = "bUseUdp", ClampMin = "1", ClampMax = "65535"))
	int32 UdpStunPort = 3478;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NetworkSync", meta = (EditCondition = "bUseUdp"))
	FString UdpRendezvousHost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NetworkSync", meta = (EditCondition = "bUseUdp", ClampMin = "1", ClampMax = "65535"))
	int32 UdpRendezvousPort = 3479;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "NetworkSync")
	FVector GetPawnLocation(int32 PlayerId) const;

	friend FNsSelfTestResult NsRunSchemeApplySelfTest();
	friend void NsSchemeApplyDirty(ANsNetManager& Manager);

private:
	int8 ReadDx(bool bPlayer0) const;
	void TickLockstep();
	void TickStateSync();
	void TickRollback();
	void DrawPawns() const;
	void DrawLockstepDoor() const;
	void SpawnReplicatedDemo();
	void DestroyReplicatedDemo();
	void EnsureInputProxies();
	void InitProtocols();
	void ResetWire();
	void ApplyScheme(ENsScheme NewScheme);
	INsNet& Wire();
	bool RunsServer() const;
	bool RunsC0() const;
	bool RunsC1() const;
	bool BindUdp();
	void QueryStunIfNeeded();

	FNsFakeNet Fake;
	FNsUdpNet Udp;
	INsNet* NetLink = nullptr;
	FNsLockstepServer LsSv;
	FNsLockstepClient LsC0;
	FNsLockstepClient LsC1;
	FNsLockstepResync LsResync;
	FNsLockstepResyncClient LsResyncC0;
	FNsLockstepResyncClient LsResyncC1;
	FNsDoorOpen DoorSv;
	FNsDoorOpen DoorC0;
	FNsDoorOpen DoorC1;
	FNsLockstepWaitServer WaitSv;
	FNsLockstepWaitClient WaitC0;
	FNsLockstepWaitClient WaitC1;
	FNsLockstepTurnServer TurnSv;
	FNsLockstepTurnClient TurnC0;
	FNsLockstepTurnClient TurnC1;
	FNsLockstepDelayServer DelaySv;
	FNsLockstepDelayClient DelayC0;
	FNsLockstepDelayClient DelayC1;
	FNsStateSyncServer SsSv;
	FNsStateSyncClient SsC0;
	FNsStateSyncClient SsC1;
	FNsRollbackPeer RbA;
	FNsRollbackPeer RbB;
	TWeakObjectPtr<AActor> RepActor;
	TWeakObjectPtr<AActor> DoorActor;
	double AccumMs = 0.0;
	ENsScheme AppliedScheme = ENsScheme::Lockstep;
	ENsLockstepKind AppliedLockstepKind = ENsLockstepKind::Optimistic;
};
