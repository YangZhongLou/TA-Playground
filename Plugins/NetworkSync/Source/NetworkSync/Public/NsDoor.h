// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NsDoor.generated.h"

UCLASS()
class NETWORKSYNC_API ANsDoor : public AActor
{
	GENERATED_BODY()

public:
	ANsDoor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Open, BlueprintReadOnly, Category = "NetworkSync")
	bool bOpen = false;

	UFUNCTION(Server, Reliable)
	void ServerSetOpen(bool bNewOpen);

	UFUNCTION()
	void OnRep_Open();

private:
	void DrawDoor() const;
};
