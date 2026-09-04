// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NetworkSyncModule.h"
#include "NsSelfTest.h"
#include "NsNetManager.h"
#include "NsUdpNet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNetworkSyncModule, Log, All);

static UWorld* NsActiveWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
	{
		UWorld* World = Ctx.World();
		if (World && (Ctx.WorldType == EWorldType::PIE || Ctx.WorldType == EWorldType::Game))
		{
			return World;
		}
	}
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
	{
		UWorld* World = Ctx.World();
		if (World && Ctx.WorldType == EWorldType::Editor)
		{
			return World;
		}
	}
	return nullptr;
}

static void NsSpawnDemo()
{
	UWorld* World = NsActiveWorld();
	if (!World)
	{
		UE_LOG(LogNetworkSyncModule, Warning, TEXT("ns.SpawnDemo: no world"));
		return;
	}
	ANsNetManager* Mgr = World->SpawnActor<ANsNetManager>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (Mgr)
	{
		UE_LOG(LogNetworkSyncModule, Display,
			TEXT("Spawned ANsNetManager. A/D = player 0, arrows = player 1. Scheme on the actor."));
	}
}

static void NsDropRateCmd(const TArray<FString>& Args)
{
	float Drop = 0.1f;
	int32 Count = 2000;
	if (Args.Num() >= 1)
	{
		Drop = FCString::Atof(*Args[0]);
	}
	if (Args.Num() >= 2)
	{
		Count = FCString::Atoi(*Args[1]);
	}
	NsLogFakeNetDropRate(Drop, Count);
}

static void NsRendezvousHubCmd(const TArray<FString>& Args)
{
	int32 Port = 3479;
	if (Args.Num() >= 1)
	{
		Port = FCString::Atoi(*Args[0]);
	}
	if (Port < 0 || Port > 65535)
	{
		UE_LOG(LogNetworkSyncModule, Warning, TEXT("ns.RendezvousHub: port 0-65535"));
		return;
	}
	if (!NsStartRendezvousHub(Port))
	{
		UE_LOG(LogNetworkSyncModule, Warning, TEXT("ns.RendezvousHub: bind failed port=%d"), Port);
		return;
	}
	UE_LOG(LogNetworkSyncModule, Display, TEXT("ns.RendezvousHub bound port=%d"), NsRendezvousHubBoundPort());
}

static void NsRendezvousHubStopCmd()
{
	NsStopRendezvousHub();
	UE_LOG(LogNetworkSyncModule, Display, TEXT("ns.RendezvousHub stopped"));
}

void FNetworkSyncModule::StartupModule()
{
	SelfTestCmd = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ns.SelfTest"),
		TEXT("Run NetworkSync lockstep / state-sync / rollback fake-net tests"),
		FConsoleCommandDelegate::CreateStatic(&NsRunSelfTestAndLog),
		ECVF_Default);
	DropRateCmd = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ns.DropRate"),
		TEXT("Measure FakeNet drop rate. Usage: ns.DropRate [0-1] [count]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&NsDropRateCmd),
		ECVF_Default);
	SpawnCmd = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ns.SpawnDemo"),
		TEXT("Spawn ANsNetManager (A/D vs arrow keys, debug spheres)"),
		FConsoleCommandDelegate::CreateStatic(&NsSpawnDemo),
		ECVF_Default);
	HubCmd = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ns.RendezvousHub"),
		TEXT("Run ICE/NSRV rendezvous hub. Usage: ns.RendezvousHub [port]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&NsRendezvousHubCmd),
		ECVF_Default);
	HubStopCmd = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ns.RendezvousHubStop"),
		TEXT("Stop ns.RendezvousHub"),
		FConsoleCommandDelegate::CreateStatic(&NsRendezvousHubStopCmd),
		ECVF_Default);
	UE_LOG(LogNetworkSyncModule, Log,
		TEXT("NetworkSync started. Console: ns.SelfTest, ns.DropRate, ns.SpawnDemo, ns.RendezvousHub"));
}

void FNetworkSyncModule::ShutdownModule()
{
	if (SelfTestCmd)
	{
		IConsoleManager::Get().UnregisterConsoleObject(SelfTestCmd);
		SelfTestCmd = nullptr;
	}
	if (DropRateCmd)
	{
		IConsoleManager::Get().UnregisterConsoleObject(DropRateCmd);
		DropRateCmd = nullptr;
	}
	if (SpawnCmd)
	{
		IConsoleManager::Get().UnregisterConsoleObject(SpawnCmd);
		SpawnCmd = nullptr;
	}
	if (HubCmd)
	{
		IConsoleManager::Get().UnregisterConsoleObject(HubCmd);
		HubCmd = nullptr;
	}
	if (HubStopCmd)
	{
		IConsoleManager::Get().UnregisterConsoleObject(HubStopCmd);
		HubStopCmd = nullptr;
	}
	NsStopRendezvousHub();
}

IMPLEMENT_MODULE(FNetworkSyncModule, NetworkSync)
