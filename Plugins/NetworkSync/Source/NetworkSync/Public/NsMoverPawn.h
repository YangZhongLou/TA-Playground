// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MoverSimulationTypes.h"
#include "NsMoverPawn.generated.h"

class UCapsuleComponent;
class USpringArmComponent;
class UCameraComponent;
class UCharacterMoverComponent;

UCLASS()
class NETWORKSYNC_API ANsMoverPawn : public APawn, public IMoverInputProducerInterface
{
	GENERATED_BODY()

public:
	ANsMoverPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

	static ANsMoverPawn* SpawnAndPossess(UWorld* World, const FVector& Location);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NetworkSync")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NetworkSync")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NetworkSync")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NetworkSync")
	TObjectPtr<UCharacterMoverComponent> Mover;

	bool bWasJumpPressed = false;
};
