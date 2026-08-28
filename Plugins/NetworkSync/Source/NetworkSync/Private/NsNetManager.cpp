// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsNetManager.h"
#include "NsReplicatedActor.h"
#include "NsDoor.h"
#include "NsTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

ANsNetManager::ANsNetManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
}

void ANsNetManager::BeginPlay()
{
	Super::BeginPlay();

	Net.RttMs = 80.f;
	Net.Drop = 0.05f;
	Net.JitterMs = 6.f;
	Net.Rng.Initialize(1);

	LsC0.PlayerId = 0;
	LsC0.Addr = ENsAddr::C0;
	LsC1.PlayerId = 1;
	LsC1.Addr = ENsAddr::C1;

	SsC0.PlayerId = 0;
	SsC0.Addr = ENsAddr::C0;
	SsC1.PlayerId = 1;
	SsC1.Addr = ENsAddr::C1;

	RbA.PlayerId = 0;
	RbA.Addr = ENsAddr::C0;
	RbA.Other = ENsAddr::C1;
	RbB.PlayerId = 1;
	RbB.Addr = ENsAddr::C1;
	RbB.Other = ENsAddr::C0;

	if (Scheme == ENsScheme::Replication)
	{
		SpawnReplicatedDemo();
	}
}

void ANsNetManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	switch (Scheme)
	{
	case ENsScheme::Lockstep:
		TickLockstep();
		break;
	case ENsScheme::StateSync:
		TickStateSync();
		break;
	case ENsScheme::Rollback:
		TickRollback();
		break;
	case ENsScheme::Replication:
		break;
	}

	if (Scheme != ENsScheme::Replication)
	{
		DrawPawns();
	}
}

FVector ANsNetManager::GetPawnLocation(int32 PlayerId) const
{
	float X = 0.f;
	const float Alpha = FMath::Clamp(static_cast<float>(AccumMs / Ns::LogicDtMs), 0.f, 1.f);
	switch (Scheme)
	{
	case ENsScheme::Lockstep:
	{
		const FNsLockstepClient& C = (PlayerId == 0) ? LsC0 : LsC1;
		const int32 Idx = PlayerId;
		X = FMath::Lerp(static_cast<float>(C.PrevX[Idx]), static_cast<float>(C.World.X[Idx]), Alpha);
		break;
	}
	case ENsScheme::StateSync:
		if (PlayerId == 0)
		{
			X = static_cast<float>(SsC0.PredX);
		}
		else
		{
			X = static_cast<float>(SsC0.bHasRemote ? SsC0.RemoteDrawn : SsC1.PredX);
		}
		break;
	case ENsScheme::Rollback:
		if (!RbA.bInRollback && !RbB.bInRollback)
		{
			X = static_cast<float>((PlayerId == 0) ? RbA.World.X[0] : RbB.World.X[1]);
		}
		break;
	case ENsScheme::Replication:
		if (const ANsReplicatedActor* Rep = Cast<ANsReplicatedActor>(RepActor.Get()))
		{
			X = static_cast<float>(Rep->Counter);
		}
		break;
	}
	const float Y = (PlayerId == 0) ? 0.f : 80.f;
	return GetActorLocation() + FVector(X * DrawScale, Y, 40.f);
}

int8 ANsNetManager::ReadDx(bool bPlayer0) const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return 0;
	}
	if (bPlayer0)
	{
		if (PC->IsInputKeyDown(EKeys::D))
		{
			return 1;
		}
		if (PC->IsInputKeyDown(EKeys::A))
		{
			return -1;
		}
		return 0;
	}
	if (PC->IsInputKeyDown(EKeys::Right))
	{
		return 1;
	}
	if (PC->IsInputKeyDown(EKeys::Left))
	{
		return -1;
	}
	return 0;
}

void ANsNetManager::TickLockstep()
{
	AccumMs += GetWorld()->GetDeltaSeconds() * 1000.0;
	while (AccumMs >= Ns::LogicDtMs)
	{
		AccumMs -= Ns::LogicDtMs;

		LsC0.SendInput(Net, ReadDx(true));
		LsC1.SendInput(Net, ReadDx(false));

		TArray<FNsPacket> ToSv;
		Net.Drain(ENsAddr::Sv, ToSv);
		for (const FNsPacket& P : ToSv)
		{
			if (P.Type == ENsMsg::C2SInput)
			{
				LsSv.OnInput(P.PlayerId, P.Dx);
			}
			else if (P.Type == ENsMsg::C2SChecksum)
			{
				LsSv.OnChecksum(P.Tick, P.Hash);
			}
		}
		LsSv.Tick(Net);

		FNsLockstepClient* Clients[2] = {&LsC0, &LsC1};
		for (FNsLockstepClient* C : Clients)
		{
			TArray<FNsPacket> ToC;
			Net.Drain(C->Addr, ToC);
			for (const FNsPacket& P : ToC)
			{
				if (P.Type == ENsMsg::S2CFrame)
				{
					C->OnS2C(P.Frames);
				}
			}
			C->Logic(Net);
		}

		Net.Advance(Ns::LogicDtMs);
	}
}

void ANsNetManager::TickStateSync()
{
	AccumMs += GetWorld()->GetDeltaSeconds() * 1000.0;
	while (AccumMs >= Ns::SimDtMs)
	{
		AccumMs -= Ns::SimDtMs;

		SsC0.LocalTick(Net, ReadDx(true));
		SsC1.LocalTick(Net, ReadDx(false));

		TArray<FNsPacket> ToSv;
		Net.Drain(ENsAddr::Sv, ToSv);
		for (const FNsPacket& P : ToSv)
		{
			if (P.Type == ENsMsg::C2SInput)
			{
				for (int32 i = 0; i < P.SeqWindow.Num(); ++i)
				{
					SsSv.OnInput(P.PlayerId, P.SeqWindow[i], P.DxWindow[i]);
				}
			}
			else if (P.Type == ENsMsg::C2SSnapAck)
			{
				SsSv.OnAck(P.PlayerId, P.Tick);
			}
		}
		SsSv.Sim(Net);

		FNsStateSyncClient* Clients[2] = {&SsC0, &SsC1};
		for (FNsStateSyncClient* C : Clients)
		{
			TArray<FNsPacket> ToC;
			Net.Drain(C->Addr, ToC);
			for (const FNsPacket& P : ToC)
			{
				if (P.Type == ENsMsg::S2CSnapshot)
				{
					C->OnSnap(Net, P);
				}
			}
			C->UpdateRemoteDraw(Net.Now);
		}

		Net.Advance(Ns::SimDtMs);
	}
}

void ANsNetManager::TickRollback()
{
	AccumMs += GetWorld()->GetDeltaSeconds() * 1000.0;
	while (AccumMs >= Ns::RollbackDtMs)
	{
		AccumMs -= Ns::RollbackDtMs;

		RbA.Advance(Net, ReadDx(true));
		RbB.Advance(Net, ReadDx(false));

		TArray<FNsPacket> ToA;
		Net.Drain(ENsAddr::C0, ToA);
		for (const FNsPacket& P : ToA)
		{
			if (P.Type == ENsMsg::P2PInput)
			{
				RbA.OnRemote(P.RemoteDx);
			}
		}
		TArray<FNsPacket> ToB;
		Net.Drain(ENsAddr::C1, ToB);
		for (const FNsPacket& P : ToB)
		{
			if (P.Type == ENsMsg::P2PInput)
			{
				RbB.OnRemote(P.RemoteDx);
			}
		}

		Net.Advance(Ns::RollbackDtMs);
	}
}

void ANsNetManager::DrawPawns() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	DrawDebugSphere(World, GetPawnLocation(0), 24.f, 8, FColor::Cyan, false, -1.f, 0, 1.5f);
	DrawDebugSphere(World, GetPawnLocation(1), 24.f, 8, FColor::Orange, false, -1.f, 0, 1.5f);
}

void ANsNetManager::SpawnReplicatedDemo()
{
	UWorld* World = GetWorld();
	if (!World || RepActor.IsValid())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = World->GetFirstPlayerController();
	ANsReplicatedActor* Spawned = World->SpawnActor<ANsReplicatedActor>(
		GetActorLocation() + FVector(0.f, 0.f, 40.f),
		FRotator::ZeroRotator,
		Params);
	RepActor = Spawned;
	ANsDoor* Door = World->SpawnActor<ANsDoor>(
		GetActorLocation() + FVector(0.f, 160.f, 40.f),
		FRotator::ZeroRotator,
		Params);
	DoorActor = Door;
}
