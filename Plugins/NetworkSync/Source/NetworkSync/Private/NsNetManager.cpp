// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsNetManager.h"
#include "NsReplicatedActor.h"
#include "NsDoor.h"
#include "NsMoverPawn.h"
#include "NsTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

ANsNetManager::ANsNetManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	NetLink = &Fake;
}

void ANsNetManager::BeginPlay()
{
	Super::BeginPlay();

	Fake.RttMs = 80.f;
	Fake.Drop = 0.05f;
	Fake.JitterMs = 6.f;
	Fake.Rng.Initialize(1);
	NetLink = &Fake;
	if (bUseUdp)
	{
		if (BindUdp())
		{
			NetLink = &Udp;
		}
	}

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

INsNet& ANsNetManager::Wire()
{
	return *NetLink;
}

bool ANsNetManager::RunsServer() const
{
	return !bUseUdp || UdpRole == ENsUdpRole::LocalMesh || UdpRole == ENsUdpRole::Host;
}

bool ANsNetManager::RunsC0() const
{
	return !bUseUdp || UdpRole == ENsUdpRole::LocalMesh || UdpRole == ENsUdpRole::Host;
}

bool ANsNetManager::RunsC1() const
{
	return !bUseUdp || UdpRole == ENsUdpRole::LocalMesh || UdpRole == ENsUdpRole::Client;
}

bool ANsNetManager::BindUdp()
{
	Udp.Close();
	if (UdpRole == ENsUdpRole::LocalMesh)
	{
		return Udp.BindLoopback(UdpBasePort);
	}
	const int32 Base = (UdpBasePort > 0) ? UdpBasePort : 27000;
	const int32 RemoteBase = (UdpRemoteBasePort > 0) ? UdpRemoteBasePort : Base;
	const TCHAR* Remote = UdpRemoteHost.IsEmpty() ? TEXT("127.0.0.1") : *UdpRemoteHost;
	if (Scheme == ENsScheme::Rollback)
	{
		if (UdpRole == ENsUdpRole::Host)
		{
			return Udp.Bind(ENsAddr::C0, Base + 1, bUdpLan)
				&& Udp.SetPeer(ENsAddr::C1, Remote, RemoteBase + 2);
		}
		return Udp.Bind(ENsAddr::C1, Base + 2, bUdpLan)
			&& Udp.SetPeer(ENsAddr::C0, Remote, RemoteBase + 1);
	}
	if (UdpRole == ENsUdpRole::Host)
	{
		return Udp.Bind(ENsAddr::Sv, Base, bUdpLan)
			&& Udp.Bind(ENsAddr::C0, Base + 1, bUdpLan)
			&& Udp.SetPeer(ENsAddr::C1, Remote, RemoteBase + 2);
	}
	return Udp.Bind(ENsAddr::C1, Base + 2, bUdpLan)
		&& Udp.SetPeer(ENsAddr::Sv, Remote, RemoteBase)
		&& Udp.SetPeer(ENsAddr::C0, Remote, RemoteBase + 1);
}

void ANsNetManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Udp.Close();
	NetLink = &Fake;
	Super::EndPlay(EndPlayReason);
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
		if (bUseUdp && UdpRole == ENsUdpRole::Client)
		{
			X = FMath::Lerp(static_cast<float>(LsC1.PrevX[PlayerId]),
				static_cast<float>(LsC1.World.X[PlayerId]), Alpha);
		}
		else if (bUseUdp && UdpRole == ENsUdpRole::Host && PlayerId == 1)
		{
			X = static_cast<float>(LsSv.World.X[1]);
		}
		else
		{
			const FNsLockstepClient& C = (PlayerId == 0) ? LsC0 : LsC1;
			X = FMath::Lerp(static_cast<float>(C.PrevX[PlayerId]),
				static_cast<float>(C.World.X[PlayerId]), Alpha);
		}
		break;
	}
	case ENsScheme::StateSync:
		if (bUseUdp && UdpRole == ENsUdpRole::Client)
		{
			X = (PlayerId == 1)
				? static_cast<float>(SsC1.PredX)
				: static_cast<float>(SsC1.bHasRemote ? SsC1.RemoteDrawn : 0);
		}
		else if (bUseUdp && UdpRole == ENsUdpRole::Host && PlayerId == 1)
		{
			X = static_cast<float>(SsSv.Pawns[1].X);
		}
		else if (PlayerId == 0)
		{
			X = static_cast<float>(SsC0.PredX);
		}
		else
		{
			X = static_cast<float>(SsC0.bHasRemote ? SsC0.RemoteDrawn : SsC1.PredX);
		}
		break;
	case ENsScheme::Rollback:
		if (bUseUdp && UdpRole == ENsUdpRole::Client)
		{
			if (!RbB.bInRollback)
			{
				X = static_cast<float>(RbB.World.X[PlayerId]);
			}
		}
		else if (!RbA.bInRollback && (UdpRole != ENsUdpRole::LocalMesh || !RbB.bInRollback))
		{
			if (bUseUdp && UdpRole == ENsUdpRole::Host)
			{
				X = static_cast<float>(RbA.World.X[PlayerId]);
			}
			else
			{
				X = static_cast<float>((PlayerId == 0) ? RbA.World.X[0] : RbB.World.X[1]);
			}
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

		const bool bClientOnly = bUseUdp && UdpRole == ENsUdpRole::Client;
		if (RunsC0())
		{
			LsC0.SendInput(Wire(), ReadDx(true));
		}
		if (RunsC1())
		{
			LsC1.SendInput(Wire(), ReadDx(bClientOnly));
		}

		if (RunsServer())
		{
			TArray<FNsPacket> ToSv;
			Wire().Drain(ENsAddr::Sv, ToSv);
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
			LsSv.Tick(Wire());
		}

		FNsLockstepClient* Clients[2] = {&LsC0, &LsC1};
		for (FNsLockstepClient* C : Clients)
		{
			if ((C == &LsC0 && !RunsC0()) || (C == &LsC1 && !RunsC1()))
			{
				continue;
			}
			TArray<FNsPacket> ToC;
			Wire().Drain(C->Addr, ToC);
			for (const FNsPacket& P : ToC)
			{
				if (P.Type == ENsMsg::S2CJoinSnap)
				{
					C->ApplyJoin(P);
				}
				else if (P.Type == ENsMsg::S2CFrame)
				{
					C->OnS2C(P.Frames);
				}
			}
			C->Logic(Wire());
		}

		Wire().Advance(Ns::LogicDtMs);
	}
}

void ANsNetManager::TickStateSync()
{
	AccumMs += GetWorld()->GetDeltaSeconds() * 1000.0;
	while (AccumMs >= Ns::SimDtMs)
	{
		AccumMs -= Ns::SimDtMs;

		const bool bClientOnly = bUseUdp && UdpRole == ENsUdpRole::Client;
		if (RunsC0())
		{
			SsC0.LocalTick(Wire(), ReadDx(true));
		}
		if (RunsC1())
		{
			SsC1.LocalTick(Wire(), ReadDx(bClientOnly));
		}

		if (RunsServer())
		{
			TArray<FNsPacket> ToSv;
			Wire().Drain(ENsAddr::Sv, ToSv);
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
			SsSv.Sim(Wire());
		}

		FNsStateSyncClient* Clients[2] = {&SsC0, &SsC1};
		for (FNsStateSyncClient* C : Clients)
		{
			if ((C == &SsC0 && !RunsC0()) || (C == &SsC1 && !RunsC1()))
			{
				continue;
			}
			TArray<FNsPacket> ToC;
			Wire().Drain(C->Addr, ToC);
			for (const FNsPacket& P : ToC)
			{
				if (P.Type == ENsMsg::S2CSnapshot)
				{
					C->OnSnap(Wire(), P);
				}
			}
			C->UpdateRemoteDraw(Wire().Now);
		}

		Wire().Advance(Ns::SimDtMs);
	}
}

void ANsNetManager::TickRollback()
{
	AccumMs += GetWorld()->GetDeltaSeconds() * 1000.0;
	while (AccumMs >= Ns::RollbackDtMs)
	{
		AccumMs -= Ns::RollbackDtMs;

		const bool bClientOnly = bUseUdp && UdpRole == ENsUdpRole::Client;
		if (RunsC0())
		{
			RbA.Advance(Wire(), ReadDx(true));
		}
		if (RunsC1())
		{
			RbB.Advance(Wire(), ReadDx(bClientOnly));
		}

		if (RunsC0())
		{
			TArray<FNsPacket> ToA;
			Wire().Drain(ENsAddr::C0, ToA);
			for (const FNsPacket& P : ToA)
			{
				if (P.Type == ENsMsg::P2PInput)
				{
					RbA.OnRemote(P.RemoteDx);
				}
			}
		}
		if (RunsC1())
		{
			TArray<FNsPacket> ToB;
			Wire().Drain(ENsAddr::C1, ToB);
			for (const FNsPacket& P : ToB)
			{
				if (P.Type == ENsMsg::P2PInput)
				{
					RbB.OnRemote(P.RemoteDx);
				}
			}
		}

		Wire().Advance(Ns::RollbackDtMs);
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
	const FVector MoverLoc = GetActorLocation() + FVector(80.f, 0.f, 120.f);
	ANsMoverPawn::SpawnAndPossess(World, MoverLoc);
}
