// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsMoverPawn.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoverDataModelTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"

ANsMoverPawn::ANsMoverPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(42.f, 96.f);
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	SetRootComponent(Capsule);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Capsule);
	SpringArm->TargetArmLength = 320.f;
	SpringArm->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	Mover = CreateDefaultSubobject<UCharacterMoverComponent>(TEXT("Mover"));
}

void ANsMoverPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	DrawDebugCapsule(World, GetActorLocation(), Capsule->GetScaledCapsuleHalfHeight(),
		Capsule->GetScaledCapsuleRadius(), GetActorQuat(), FColor::Cyan, false, -1.f, 0, 1.5f);
}

void ANsMoverPawn::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	(void)SimTimeMs;
	FCharacterDefaultInputs& Inputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		if (GetLocalRole() == ROLE_Authority && GetRemoteRole() == ROLE_SimulatedProxy)
		{
			static const FCharacterDefaultInputs DoNothing;
			Inputs = DoNothing;
		}
		return;
	}

	FVector Intent = FVector::ZeroVector;
	if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))
	{
		Intent.X += 1.f;
	}
	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))
	{
		Intent.X -= 1.f;
	}
	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right))
	{
		Intent.Y += 1.f;
	}
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))
	{
		Intent.Y -= 1.f;
	}
	Intent = Intent.GetClampedToMaxSize(1.f);

	Inputs.ControlRotation = PC->GetControlRotation();
	const FVector WorldIntent = Inputs.ControlRotation.RotateVector(Intent);
	Inputs.SetMoveInput(EMoveInputType::DirectionalIntent, WorldIntent);
	Inputs.OrientationIntent = WorldIntent.GetSafeNormal();
	Inputs.SuggestedMovementMode = NAME_None;

	const bool bJump = PC->IsInputKeyDown(EKeys::SpaceBar);
	Inputs.bIsJumpPressed = bJump;
	Inputs.bIsJumpJustPressed = bJump && !bWasJumpPressed;
	bWasJumpPressed = bJump;
}

ANsMoverPawn* ANsMoverPawn::SpawnAndPossess(UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ANsMoverPawn* Pawn = World->SpawnActor<ANsMoverPawn>(Location, FRotator::ZeroRotator, Params);
	if (!Pawn)
	{
		return nullptr;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (PC && PC->IsLocalController())
	{
		APawn* Old = PC->GetPawn();
		PC->Possess(Pawn);
		if (Old && Old != Pawn)
		{
			Old->Destroy();
		}
	}
	return Pawn;
}
