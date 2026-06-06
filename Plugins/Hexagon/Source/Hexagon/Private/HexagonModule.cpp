// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HexagonModule.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "AHexPrism.h"
#include "AHexGrid.h"
#include "AHexTerrain.h"

static void HexSpawnPrism(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) { UE_LOG(LogTemp, Warning, TEXT("hex.SpawnPrism: No world")); return; }

	const float Radius = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 100.0f;
	const float Height = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 200.0f;
	const float X = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.0f;
	const float Y = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.0f;
	const float Z = Args.Num() > 4 ? FCString::Atof(*Args[4]) : 0.0f;

	AHexPrism* Prism = World->SpawnActor<AHexPrism>(FVector(X, Y, Z), FRotator::ZeroRotator);
	if (Prism) { Prism->Radius = Radius; Prism->Height = Height; Prism->RegenerateMesh(); }
}

static void HexSpawnGrid(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) { UE_LOG(LogTemp, Warning, TEXT("hex.SpawnGrid: No world")); return; }

	const float CellRadius = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 100.0f;
	const int32 GridRadius = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 3;
	const float CellHeight = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 10.0f;

	AHexGrid* Grid = World->SpawnActor<AHexGrid>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (Grid) { Grid->CellRadius = CellRadius; Grid->GridRadius = GridRadius; Grid->CellHeight = CellHeight; Grid->RegenerateMesh(); }
}

static void HexSpawnTerrain(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) { UE_LOG(LogTemp, Warning, TEXT("hex.SpawnTerrain: No world")); return; }

	const float CellRadius = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 100.0f;
	const int32 GridRadius = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 5;
	const float HeightScale = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 150.0f;
	const float NoiseScale = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.08f;

	AHexTerrain* Terrain = World->SpawnActor<AHexTerrain>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (Terrain) { Terrain->CellRadius = CellRadius; Terrain->GridRadius = GridRadius; Terrain->TerrainConfig.HeightScale = HeightScale; Terrain->TerrainConfig.NoiseScale = NoiseScale; Terrain->RegenerateTerrain(); }
}

static void HexDestroyAll(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) { UE_LOG(LogTemp, Warning, TEXT("hex.DestroyAll: No world")); return; }

	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->IsA<AHexPrism>() || It->IsA<AHexGrid>() || It->IsA<AHexTerrain>())
			ToDestroy.Add(*It);
	}
	if (GEditor) GEditor->SelectNone(true, true);

	int32 Count = 0;
	for (AActor* Actor : ToDestroy)
		if (IsValid(Actor)) { Actor->Destroy(); ++Count; }

	UE_LOG(LogTemp, Log, TEXT("hex.DestroyAll: destroyed %d hex actors"), Count);
}

static TArray<IConsoleCommand*> GHexCommands;

static void RegisterHexCommands()
{
	if (GHexCommands.Num() > 0) return; // already registered

#define REGISTER_CMD(Name, Help, Func) \
	GHexCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(TEXT(Name), TEXT(Help), FConsoleCommandWithArgsDelegate::CreateStatic(&Func), ECVF_Cheat))

	REGISTER_CMD("hex.SpawnPrism",   "Spawn hex prism. <Radius> <Height> [X] [Y] [Z]",           HexSpawnPrism);
	REGISTER_CMD("hex.SpawnGrid",    "Spawn hex grid. <CellRadius> <GridRadius> [CellHeight]", HexSpawnGrid);
	REGISTER_CMD("hex.SpawnTerrain", "Spawn hex terrain. <CellRadius> <GridRadius> <HeightScale> [NoiseScale]", HexSpawnTerrain);
	REGISTER_CMD("hex.DestroyAll",   "Destroy all hex actors",                                  HexDestroyAll);

#undef REGISTER_CMD

	UE_LOG(LogTemp, Log, TEXT("Hexagon: registered %d console commands"), GHexCommands.Num());
}

static void UnregisterHexCommands()
{
	for (IConsoleCommand* Cmd : GHexCommands)
		IConsoleManager::Get().UnregisterConsoleObject(Cmd);
	GHexCommands.Empty();
}
#endif // WITH_EDITOR

void FHexagonModule::StartupModule()
{
#if WITH_EDITOR
	// Defer console command registration until engine is fully initialized.
	// Registering during StartupModule can crash because UnrealEd may not be ready yet.
	FCoreDelegates::OnPostEngineInit.AddStatic(&RegisterHexCommands);
#endif
}

void FHexagonModule::ShutdownModule()
{
#if WITH_EDITOR
	UnregisterHexCommands();
#endif
}

IMPLEMENT_MODULE(FHexagonModule, Hexagon)
