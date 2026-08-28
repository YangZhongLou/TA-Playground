// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NsFakeNet.h"
#include "NsLockstep.h"
#include "NsStateSync.h"
#include "NsRollback.h"
#include "NsNetManager.generated.h"

UENUM(BlueprintType)
enum class ENsScheme : uint8
{
	Lockstep     UMETA(DisplayName = "Lockstep"),
	StateSync    UMETA(DisplayName = "State Sync"),
	Rollback     UMETA(DisplayName = "Rollback"),
	Replication  UMETA(DisplayName = "UE Replication"),
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NetworkSync")
	float DrawScale = 10.f;

	UFUNCTION(BlueprintPure, Category = "NetworkSync")
	FVector GetPawnLocation(int32 PlayerId) const;

private:
	int8 ReadDx(bool bPlayer0) const;
	void TickLockstep();
	void TickStateSync();
	void TickRollback();
	void DrawPawns() const;
	void SpawnReplicatedDemo();

	FNsFakeNet Net;
	FNsLockstepServer LsSv;
	FNsLockstepClient LsC0;
	FNsLockstepClient LsC1;
	FNsStateSyncServer SsSv;
	FNsStateSyncClient SsC0;
	FNsStateSyncClient SsC1;
	FNsRollbackPeer RbA;
	FNsRollbackPeer RbB;
	TWeakObjectPtr<AActor> RepActor;
	TWeakObjectPtr<AActor> DoorActor;
	double AccumMs = 0.0;
};
