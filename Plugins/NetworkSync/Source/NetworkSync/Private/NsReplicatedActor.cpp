// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsReplicatedActor.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ANsReplicatedActor::ANsReplicatedActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ANsReplicatedActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Loc = GetActorLocation() + FVector(static_cast<float>(Counter) * 10.f, 0.f, 0.f);
	DrawDebugSphere(World, Loc, 28.f, 8, FColor::Green, false, -1.f, 0, 2.f);
	DrawDebugString(World, Loc + FVector(0.f, 0.f, 40.f),
		FString::Printf(TEXT("Counter=%d"), Counter), nullptr, FColor::White, 0.f, false, 1.2f);
}

void ANsReplicatedActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANsReplicatedActor, Counter);
}

void ANsReplicatedActor::ServerBump_Implementation()
{
	++Counter;
}

void ANsReplicatedActor::OnRep_Counter()
{
}
