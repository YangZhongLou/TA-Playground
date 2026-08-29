// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NsReplicatedActor.generated.h"

UCLASS()
class NETWORKSYNC_API ANsReplicatedActor : public AActor
{
	GENERATED_BODY()

public:
	ANsReplicatedActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Counter, BlueprintReadOnly, Category = "NetworkSync")
	int32 Counter = 0;

	UFUNCTION(Server, Reliable)
	void ServerBump();

	UFUNCTION()
	void OnRep_Counter();

private:
	void TryBumpFromInput();
};
