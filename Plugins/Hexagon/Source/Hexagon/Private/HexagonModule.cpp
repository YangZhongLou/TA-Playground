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
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Camera/CameraActor.h"
#include "Engine/StaticMeshActor.h"
#include "EditorLevelUtils.h"
#include "LevelEditor.h"

// ============================================================================
// hex.SpawnPrism
// ============================================================================
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

// ============================================================================
// hex.SpawnGrid
// ============================================================================
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

// ============================================================================
// hex.SpawnTerrain
// ============================================================================
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

// ============================================================================
// hex.DestroyAll
// ============================================================================
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

// ============================================================================
// hex.CreateTestScene
// ============================================================================
static void HexCreateTestScene(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) { UE_LOG(LogTemp, Warning, TEXT("hex.CreateTestScene: No world")); return; }

	UE_LOG(LogTemp, Log, TEXT("========== Creating HexTerrain Test Scene =========="));

	// 1. Clean up existing hex actors
	{
		TArray<AActor*> ToDestroy;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->IsA<AHexPrism>() || It->IsA<AHexGrid>() || It->IsA<AHexTerrain>())
				ToDestroy.Add(*It);
		}
		for (AActor* Actor : ToDestroy)
			if (IsValid(Actor)) Actor->Destroy();
		UE_LOG(LogTemp, Log, TEXT("  Cleaned up %d existing hex actors"), ToDestroy.Num());
	}

	// 2. Spawn terrain with good test parameters
	const int32 TestGridRadius = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 15;
	const float TestCellRadius = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 100.0f;

	AHexTerrain* Terrain = World->SpawnActor<AHexTerrain>(
		FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Terrain)
	{
		UE_LOG(LogTemp, Error, TEXT("  Failed to spawn AHexTerrain!"));
		return;
	}

	Terrain->CellRadius = TestCellRadius;
	Terrain->GridRadius = TestGridRadius;
	Terrain->Gap = 0.05f;
	Terrain->TerrainConfig.HeightScale = 400.0f;     // dramatic height variation
	Terrain->TerrainConfig.NoiseScale = 0.04f;        // slightly smoother hills
	Terrain->TerrainConfig.NoiseOctaves = 4;
	Terrain->TerrainConfig.WaterLevel = 0.20f;
	Terrain->TerrainConfig.SandLevel = 0.32f;
	Terrain->TerrainConfig.GrassLevel = 0.55f;
	Terrain->TerrainConfig.RockLevel = 0.75f;
	Terrain->TerrainConfig.NoiseSeed = FMath::RandRange(0, 99999);
	Terrain->LODSettings.LOD1Distance = 10000.0f;     // keep 3D prisms up to 100m
	Terrain->LODSettings.LOD2Distance = 25000.0f;     // flat only beyond 250m

	// Enable debug chunk colors so boundaries are visible
	Terrain->bDebugChunkColors = true;

	Terrain->RegenerateTerrain();

	// 3. Print stats
	const int32 ExpectedCells = 1 + TestGridRadius * (TestGridRadius + 1) * 3;
	const int32 ChunkCount = Terrain->GetChunkCount();
	UE_LOG(LogTemp, Log, TEXT("  Terrain: GridRadius=%d, CellRadius=%.0f"), TestGridRadius, TestCellRadius);
	UE_LOG(LogTemp, Log, TEXT("  Expected cells: %d"), ExpectedCells);
	UE_LOG(LogTemp, Log, TEXT("  Actual chunks:  %d"), ChunkCount);
	UE_LOG(LogTemp, Log, TEXT("  Debug colors:   ON (each chunk has unique color)"));

	// 4. Spawn directional light (sun)
	{
		ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>(
			FVector(500, 500, 2000), FRotator(-45, -30, 0));
		if (Light)
		{
			Light->GetLightComponent()->SetIntensity(8.0f);
			Light->GetLightComponent()->SetLightColor(FLinearColor(1.0f, 0.95f, 0.85f));
			UE_LOG(LogTemp, Log, TEXT("  Directional light: OK"));
		}
	}

	// 5. Spawn SkyLight (ambient/environment lighting)
	{
		ASkyLight* Sky = World->SpawnActor<ASkyLight>(
			FVector(0, 0, 1000), FRotator::ZeroRotator);
		if (Sky)
		{
			Sky->GetLightComponent()->SetIntensity(1.5f);
			Sky->GetLightComponent()->bRealTimeCapture = true;
			UE_LOG(LogTemp, Log, TEXT("  SkyLight: OK"));
		}
	}

	// 6. Print detailed stats
	Terrain->PrintStats();

	UE_LOG(LogTemp, Log, TEXT("========== Test Scene Ready =========="));
	UE_LOG(LogTemp, Log, TEXT("  Try: hex.Stats       — print stats again"));
	UE_LOG(LogTemp, Log, TEXT("  Try: hex.DestroyAll  — clean up"));
	UE_LOG(LogTemp, Log, TEXT("  Drag AHexTerrain in editor to tweak parameters live"));
}

// ============================================================================
// hex.CreateGrassTerrain — grass-only terrain with rolling hills
// ============================================================================
static void HexCreateGrassTerrain(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) { UE_LOG(LogTemp, Warning, TEXT("hex.CreateGrassTerrain: No world")); return; }

	const int32 R = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 12;

	// Clean up old
	{
		TArray<AActor*> ToDestroy;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->IsA<AHexPrism>() || It->IsA<AHexGrid>() || It->IsA<AHexTerrain>())
				ToDestroy.Add(*It);
		}
		for (AActor* Actor : ToDestroy)
			if (IsValid(Actor)) Actor->Destroy();
	}

	AHexTerrain* Terrain = World->SpawnActor<AHexTerrain>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Terrain) { UE_LOG(LogTemp, Error, TEXT("Failed to spawn terrain")); return; }

	// Grass-only classification: everything is grass
	Terrain->CellRadius = 120.0f;
	Terrain->GridRadius = R;
	Terrain->Gap = 0.02f;
	Terrain->TerrainConfig.HeightScale = 250.0f;
	Terrain->TerrainConfig.NoiseScale = 0.06f;
	Terrain->TerrainConfig.NoiseOctaves = 3;
	Terrain->TerrainConfig.WaterLevel = 0.0f;    // no water
	Terrain->TerrainConfig.SandLevel = 0.0f;     // no sand
	Terrain->TerrainConfig.GrassLevel = 10.0f;   // everything → grass (above max height)
	Terrain->TerrainConfig.RockLevel = 1.0f;     // no rock
	Terrain->TerrainConfig.NoiseSeed = FMath::RandRange(0, 99999);
	Terrain->LODSettings.LOD1Distance = 8000.0f;
	Terrain->LODSettings.LOD2Distance = 20000.0f;

	// Use vertex-color material so grass appears green
	{
		UMaterialInterface* VCMat = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/EngineDebugMaterials/VertexColorViewModeMaterial.VertexColorViewModeMaterial"));
		if (VCMat)
		{
			Terrain->TerrainMaterial = VCMat;
		}
	}

	Terrain->RegenerateTerrain();

	UE_LOG(LogTemp, Log, TEXT("========== Grass Terrain Ready =========="));
	UE_LOG(LogTemp, Log, TEXT("  Cells: %d  |  Chunks: %d  |  HeightScale: %.0f"),
		1 + R * (R + 1) * 3, Terrain->GetChunkCount(), 250.0f);
	Terrain->PrintStats();
}

// ============================================================================
// hex.Stats — print stats for all hex terrain actors in the world
// ============================================================================
static void HexStats(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) { UE_LOG(LogTemp, Warning, TEXT("hex.Stats: No world")); return; }

	int32 Count = 0;
	for (TActorIterator<AHexTerrain> It(World); It; ++It)
	{
		(*It)->PrintStats();
		++Count;
	}

	if (Count == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("hex.Stats: No AHexTerrain actors found. Try hex.CreateTestScene"));
	}
}

// ============================================================================
// Command registration
// ============================================================================
static TArray<IConsoleCommand*> GHexCommands;

static void RegisterHexCommands()
{
	if (GHexCommands.Num() > 0) return;

#define REGISTER_CMD(Name, Help, Func) \
	GHexCommands.Add(IConsoleManager::Get().RegisterConsoleCommand( \
		TEXT(Name), TEXT(Help), \
		FConsoleCommandWithArgsDelegate::CreateStatic(&Func), ECVF_Cheat))

	REGISTER_CMD("hex.SpawnPrism",     "Spawn hex prism. <Radius> <Height> [X] [Y] [Z]",           HexSpawnPrism);
	REGISTER_CMD("hex.SpawnGrid",      "Spawn hex grid. <CellRadius> <GridRadius> [CellHeight]",   HexSpawnGrid);
	REGISTER_CMD("hex.SpawnTerrain",   "Spawn hex terrain. <CellRadius> <GridRadius> <HeightScale> [NoiseScale]", HexSpawnTerrain);
	REGISTER_CMD("hex.DestroyAll",     "Destroy all hex actors in the current world",              HexDestroyAll);
	REGISTER_CMD("hex.CreateTestScene","Create test scene: terrain + light + debug colors. [GridRadius=15] [CellRadius=100]", HexCreateTestScene);
	REGISTER_CMD("hex.CreateGrassTerrain","Create grass-only rolling hills terrain. [GridRadius=12]", HexCreateGrassTerrain);
	REGISTER_CMD("hex.Stats",          "Print statistics for all hex terrain actors",              HexStats);

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
