// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Editor.h"
#include "EngineUtils.h"
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
	if (Prism)
	{
		Prism->Radius = Radius;
		Prism->Height = Height;
		Prism->RegenerateMesh();
		UE_LOG(LogTemp, Log, TEXT("hex.SpawnPrism: spawned %s at (%.0f,%.0f,%.0f) R=%.0f H=%.0f"),
			*Prism->GetName(), X, Y, Z, Radius, Height);
	}
}

static void HexSpawnGrid(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) { UE_LOG(LogTemp, Warning, TEXT("hex.SpawnGrid: No world")); return; }

	const float CellRadius = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 100.0f;
	const int32 GridRadius = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 3;
	const float CellHeight = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 10.0f;

	AHexGrid* Grid = World->SpawnActor<AHexGrid>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (Grid)
	{
		Grid->CellRadius = CellRadius;
		Grid->GridRadius = GridRadius;
		Grid->CellHeight = CellHeight;
		Grid->RegenerateMesh();
		UE_LOG(LogTemp, Log, TEXT("hex.SpawnGrid: spawned %s radius=%d cellR=%.0f"),
			*Grid->GetName(), GridRadius, CellRadius);
	}
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
	if (Terrain)
	{
		Terrain->CellRadius = CellRadius;
		Terrain->GridRadius = GridRadius;
		Terrain->TerrainConfig.HeightScale = HeightScale;
		Terrain->TerrainConfig.NoiseScale = NoiseScale;
		Terrain->RegenerateTerrain();
		UE_LOG(LogTemp, Log, TEXT("hex.SpawnTerrain: spawned %s gridR=%d cellR=%.0f height=%.0f"),
			*Terrain->GetName(), GridRadius, CellRadius, HeightScale);
	}
}

static void HexDestroyAll(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) { UE_LOG(LogTemp, Warning, TEXT("hex.DestroyAll: No world")); return; }

	// Collect first to avoid modifying the actor list during iteration,
	// which can trigger TypedElement framework crashes.
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->IsA<AHexPrism>() || It->IsA<AHexGrid>() || It->IsA<AHexTerrain>())
		{
			ToDestroy.Add(*It);
		}
	}

	// Clear editor selection to avoid dangling TypedElement references
	if (GEditor)
	{
		GEditor->SelectNone(true, true);
	}

	int32 Count = 0;
	for (AActor* Actor : ToDestroy)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
			++Count;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("hex.DestroyAll: destroyed %d hex actors"), Count);
}

static FAutoConsoleCommand GCmdSpawnPrism(
	TEXT("hex.SpawnPrism"),
	TEXT("Spawn a hex prism. Args: <Radius> <Height> [X] [Y] [Z]"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&HexSpawnPrism),
	ECVF_Cheat
);

static FAutoConsoleCommand GCmdSpawnGrid(
	TEXT("hex.SpawnGrid"),
	TEXT("Spawn a hex grid. Args: <CellRadius> <GridRadius> [CellHeight]"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&HexSpawnGrid),
	ECVF_Cheat
);

static FAutoConsoleCommand GCmdSpawnTerrain(
	TEXT("hex.SpawnTerrain"),
	TEXT("Spawn hex terrain. Args: <CellRadius> <GridRadius> <HeightScale> [NoiseScale]"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&HexSpawnTerrain),
	ECVF_Cheat
);

static FAutoConsoleCommand GCmdDestroyAll(
	TEXT("hex.DestroyAll"),
	TEXT("Destroy all hex actors in the world"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&HexDestroyAll),
	ECVF_Cheat
);
