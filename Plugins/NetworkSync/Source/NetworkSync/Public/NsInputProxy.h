// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NsInputProxy.generated.h"

UCLASS()
class NETWORKSYNC_API ANsInputProxy : public AActor
{
	GENERATED_BODY()

public:
	ANsInputProxy();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(Server, Reliable)
	void ServerBumpCounter();

	UFUNCTION(Server, Reliable)
	void ServerToggleDoor();
};
