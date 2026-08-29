// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsNetManager.h"
#include "NsReplicatedActor.h"
#include "NsDoor.h"
#include "NsPump.h"
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

void ANsNetManager::InitProtocols()
{
	LsSv = FNsLockstepServer();
	LsC0 = FNsLockstepClient();
	LsC1 = FNsLockstepClient();
	WaitSv = FNsLockstepWaitServer();
	WaitC0 = FNsLockstepWaitClient();
	WaitC1 = FNsLockstepWaitClient();
	TurnSv = FNsLockstepTurnServer();
	TurnC0 = FNsLockstepTurnClient();
	TurnC1 = FNsLockstepTurnClient();
	DelaySv = FNsLockstepDelayServer();
	DelayC0 = FNsLockstepDelayClient();
	DelayC1 = FNsLockstepDelayClient();
	SsSv = FNsStateSyncServer();
	SsC0 = FNsStateSyncClient();
	SsC1 = FNsStateSyncClient();
	RbA = FNsRollbackPeer();
	RbB = FNsRollbackPeer();
	AccumMs = 0.0;

	LsC0.PlayerId = 0;
	LsC0.Addr = ENsAddr::C0;
	LsC1.PlayerId = 1;
	LsC1.Addr = ENsAddr::C1;

	WaitC0.PlayerId = 0;
	WaitC0.Addr = ENsAddr::C0;
	WaitC1.PlayerId = 1;
	WaitC1.Addr = ENsAddr::C1;

	TurnC0.PlayerId = 0;
	TurnC0.Addr = ENsAddr::C0;
	TurnC1.PlayerId = 1;
	TurnC1.Addr = ENsAddr::C1;

	DelayC0.PlayerId = 0;
	DelayC0.Addr = ENsAddr::C0;
	DelayC1.PlayerId = 1;
	DelayC1.Addr = ENsAddr::C1;

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
}

void ANsNetManager::ResetWire()
{
	Fake.ResetSession();
	NetLink = &Fake;
	if (bUseUdp && BindUdp())
	{
		NetLink = &Udp;
	}
	LsSv.NextMs = Wire().Now;
}

void ANsNetManager::ApplyScheme(ENsScheme NewScheme)
{
	InitProtocols();
	AppliedScheme = NewScheme;
	AppliedLockstepKind = LockstepKind;
	ResetWire();
	if (NewScheme == ENsScheme::Replication)
	{
		SpawnReplicatedDemo();
	}
	else
	{
		DestroyReplicatedDemo();
	}
}

void ANsNetManager::BeginPlay()
{
	Super::BeginPlay();

	Fake.RttMs = 80.f;
	Fake.Drop = 0.05f;
	Fake.JitterMs = 6.f;
	Fake.Rng.Initialize(1);
	ApplyScheme(Scheme);
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
	if (AppliedScheme == ENsScheme::Rollback)
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

	if (Scheme != AppliedScheme
		|| (Scheme == ENsScheme::Lockstep && LockstepKind != AppliedLockstepKind))
	{
		ApplyScheme(Scheme);
	}

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
		if (AppliedLockstepKind == ENsLockstepKind::Conservative)
		{
			if (bUseUdp && UdpRole == ENsUdpRole::Client)
			{
				X = FMath::Lerp(static_cast<float>(WaitC1.PrevX[PlayerId]),
					static_cast<float>(WaitC1.World.X[PlayerId]), Alpha);
			}
			else if (bUseUdp && UdpRole == ENsUdpRole::Host && PlayerId == 1)
			{
				X = static_cast<float>(WaitSv.World.X[1]);
			}
			else
			{
				const FNsLockstepWaitClient& C = (PlayerId == 0) ? WaitC0 : WaitC1;
				X = FMath::Lerp(static_cast<float>(C.PrevX[PlayerId]),
					static_cast<float>(C.World.X[PlayerId]), Alpha);
			}
			break;
		}
		if (AppliedLockstepKind == ENsLockstepKind::CommTurn)
		{
			if (bUseUdp && UdpRole == ENsUdpRole::Client)
			{
				X = FMath::Lerp(static_cast<float>(TurnC1.PrevX[PlayerId]),
					static_cast<float>(TurnC1.World.X[PlayerId]), Alpha);
			}
			else if (bUseUdp && UdpRole == ENsUdpRole::Host && PlayerId == 1)
			{
				X = static_cast<float>(TurnSv.World.X[1]);
			}
			else
			{
				const FNsLockstepTurnClient& C = (PlayerId == 0) ? TurnC0 : TurnC1;
				X = FMath::Lerp(static_cast<float>(C.PrevX[PlayerId]),
					static_cast<float>(C.World.X[PlayerId]), Alpha);
			}
			break;
		}
		if (AppliedLockstepKind == ENsLockstepKind::DelayBased)
		{
			if (bUseUdp && UdpRole == ENsUdpRole::Client)
			{
				X = FMath::Lerp(static_cast<float>(DelayC1.PrevX[PlayerId]),
					static_cast<float>(DelayC1.World.X[PlayerId]), Alpha);
			}
			else if (bUseUdp && UdpRole == ENsUdpRole::Host && PlayerId == 1)
			{
				X = static_cast<float>(DelaySv.World.X[1]);
			}
			else
			{
				const FNsLockstepDelayClient& C = (PlayerId == 0) ? DelayC0 : DelayC1;
				X = FMath::Lerp(static_cast<float>(C.PrevX[PlayerId]),
					static_cast<float>(C.World.X[PlayerId]), Alpha);
			}
			break;
		}
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
	if (AppliedLockstepKind == ENsLockstepKind::Conservative)
	{
		AccumMs += GetWorld()->GetDeltaSeconds() * 1000.0;
		while (AccumMs >= Ns::LogicDtMs)
		{
			AccumMs -= Ns::LogicDtMs;

			const bool bClientOnly = bUseUdp && UdpRole == ENsUdpRole::Client;
			if (RunsC0())
			{
				WaitC0.SendInput(Wire(), ReadDx(true));
			}
			if (RunsC1())
			{
				WaitC1.SendInput(Wire(), ReadDx(bClientOnly));
			}

			if (RunsServer())
			{
				NsPumpLockstepWaitServer(Wire(), WaitSv);
			}
			if (RunsC0())
			{
				NsPumpLockstepWaitClient(Wire(), WaitC0);
			}
			if (RunsC1())
			{
				NsPumpLockstepWaitClient(Wire(), WaitC1);
			}

			Wire().Advance(Ns::LogicDtMs);
		}
		return;
	}
	if (AppliedLockstepKind == ENsLockstepKind::CommTurn)
	{
		AccumMs += GetWorld()->GetDeltaSeconds() * 1000.0;
		while (AccumMs >= Ns::LogicDtMs)
		{
			AccumMs -= Ns::LogicDtMs;

			const bool bClientOnly = bUseUdp && UdpRole == ENsUdpRole::Client;
			if (RunsC0())
			{
				TurnC0.SendInput(Wire(), ReadDx(true));
			}
			if (RunsC1())
			{
				TurnC1.SendInput(Wire(), ReadDx(bClientOnly));
			}

			if (RunsServer())
			{
				NsPumpLockstepTurnServer(Wire(), TurnSv);
			}
			if (RunsC0())
			{
				NsPumpLockstepTurnClient(Wire(), TurnC0);
			}
			if (RunsC1())
			{
				NsPumpLockstepTurnClient(Wire(), TurnC1);
			}

			Wire().Advance(Ns::LogicDtMs);
		}
		return;
	}
	if (AppliedLockstepKind == ENsLockstepKind::DelayBased)
	{
		AccumMs += GetWorld()->GetDeltaSeconds() * 1000.0;
		while (AccumMs >= Ns::LogicDtMs)
		{
			AccumMs -= Ns::LogicDtMs;

			const bool bClientOnly = bUseUdp && UdpRole == ENsUdpRole::Client;
			if (RunsC0())
			{
				DelayC0.SendInput(Wire(), ReadDx(true));
			}
			if (RunsC1())
			{
				DelayC1.SendInput(Wire(), ReadDx(bClientOnly));
			}

			if (RunsServer())
			{
				NsPumpLockstepDelayServer(Wire(), DelaySv);
			}
			if (RunsC0())
			{
				NsPumpLockstepDelayClient(Wire(), DelayC0);
			}
			if (RunsC1())
			{
				NsPumpLockstepDelayClient(Wire(), DelayC1);
			}

			Wire().Advance(Ns::LogicDtMs);
		}
		return;
	}
	if (AppliedLockstepKind != ENsLockstepKind::Optimistic)
	{
		return;
	}
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
			NsPumpLockstepServer(Wire(), LsSv);
		}
		if (RunsC0())
		{
			NsPumpLockstepClient(Wire(), LsC0);
		}
		if (RunsC1())
		{
			NsPumpLockstepClient(Wire(), LsC1);
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
			NsPumpStateServer(Wire(), SsSv);
		}
		if (RunsC0())
		{
			NsPumpStateClient(Wire(), SsC0);
		}
		if (RunsC1())
		{
			NsPumpStateClient(Wire(), SsC1);
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
			NsPumpRollbackPeer(Wire(), RbA);
		}
		if (RunsC1())
		{
			NsPumpRollbackPeer(Wire(), RbB);
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
	if (!HasAuthority() || !World || RepActor.IsValid())
	{
		return;
	}
	FActorSpawnParameters Params;
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

void ANsNetManager::DestroyReplicatedDemo()
{
	if (AActor* Rep = RepActor.Get())
	{
		Rep->Destroy();
	}
	RepActor.Reset();
	if (AActor* Door = DoorActor.Get())
	{
		Door->Destroy();
	}
	DoorActor.Reset();
}
