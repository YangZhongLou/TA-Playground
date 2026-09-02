// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsInputProxy.h"
#include "NsDoor.h"
#include "NsReplicatedActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

ANsInputProxy::ANsInputProxy()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bOnlyRelevantToOwner = true;
}

void ANsInputProxy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::E))
	{
		ServerBumpCounter();
	}
	if (PC->WasInputKeyJustPressed(EKeys::F))
	{
		ServerToggleDoor();
	}
}

void ANsInputProxy::ServerBumpCounter_Implementation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<ANsReplicatedActor> It(World); It; ++It)
	{
		It->ServerBump_Implementation();
		return;
	}
}

void ANsInputProxy::ServerToggleDoor_Implementation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<ANsDoor> It(World); It; ++It)
	{
		It->ServerSetOpen_Implementation(!It->bOpen);
		return;
	}
}
