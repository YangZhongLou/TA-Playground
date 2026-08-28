// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsDoor.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Net/UnrealNetwork.h"

ANsDoor::ANsDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ANsDoor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			SetOwner(PC);
		}
	}
}

void ANsDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TryToggleFromInput();
	DrawDoor();
}

void ANsDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANsDoor, bOpen);
}

void ANsDoor::ServerSetOpen_Implementation(bool bNewOpen)
{
	bOpen = bNewOpen;
}

void ANsDoor::OnRep_Open()
{
}

void ANsDoor::TryToggleFromInput()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (!PC->WasInputKeyJustPressed(EKeys::F))
	{
		return;
	}
	if (HasAuthority())
	{
		bOpen = !bOpen;
	}
	else
	{
		ServerSetOpen(!bOpen);
	}
}

void ANsDoor::DrawDoor() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Loc = GetActorLocation();
	const FColor Color = bOpen ? FColor::Green : FColor::Red;
	DrawDebugBox(World, Loc, FVector(20.f, 60.f, 80.f), Color, false, -1.f, 0, 2.f);
	DrawDebugString(World, Loc + FVector(0.f, 0.f, 100.f),
		bOpen ? TEXT("Door OPEN (F)") : TEXT("Door CLOSED (F)"),
		nullptr, FColor::White, 0.f, false, 1.1f);
}
