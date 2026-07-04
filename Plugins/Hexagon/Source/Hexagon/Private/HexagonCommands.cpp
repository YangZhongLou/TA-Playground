// Copyright (c) 2026 TA-Playground. All Rights Reserved.
// All hex.* console commands — extracted from HexagonModule.cpp

#include "HexagonCommands.h"

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
#include "HexTerrainGenerator.h"

// ============================================================================
// Helpers
// ============================================================================
static AHexTerrain* FindFirstTerrain()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<AHexTerrain> It(World); It; ++It) return *It;
	return nullptr;
}

static bool ParseTerrainType(const FString& Str, EHexTerrainType& OutType)
{
	const UEnum* Enum = StaticEnum<EHexTerrainType>();
	if (!Enum) return false;
	const int64 Val = Enum->GetValueByNameString(Str);
	if (Val != INDEX_NONE) { OutType = static_cast<EHexTerrainType>(Val); return true; }
	for (uint8 i = 0; i < static_cast<uint8>(EHexTerrainType::Count); ++i)
	{
		const EHexTerrainType T = static_cast<EHexTerrainType>(i);
		if (Enum->GetNameStringByValue(static_cast<int64>(T)).Equals(Str, ESearchCase::IgnoreCase))
			{ OutType = T; return true; }
	}
	UE_LOG(LogTemp, Warning, TEXT("Unknown terrain type '%s'. Use: Water, Sand, Grass, Rock, Snow"), *Str);
	return false;
}

static void CleanupHexActors(UWorld* World)
{
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
		if (It->IsA<AHexPrism>() || It->IsA<AHexGrid>() || It->IsA<AHexTerrain>())
			ToDestroy.Add(*It);
	if (GEditor) GEditor->SelectNone(true, true);
	for (AActor* A : ToDestroy) if (IsValid(A)) A->Destroy();
}

static void LoadAllLayerMaterials(AHexTerrain* Terrain)
{
	auto L = [](const TCHAR* P) { return LoadObject<UMaterialInterface>(nullptr, P); };
	if (auto* M = L(TEXT("/Game/Materials/M_Water.M_Water"))) Terrain->LayerMaterials.Add(EHexTerrainType::Water, M);
	if (auto* M = L(TEXT("/Game/Materials/M_Sand.M_Sand")))     Terrain->LayerMaterials.Add(EHexTerrainType::Sand, M);
	if (auto* M = L(TEXT("/Game/Materials/M_Grass.M_Grass")))   Terrain->LayerMaterials.Add(EHexTerrainType::Grass, M);
	if (auto* M = L(TEXT("/Game/Materials/M_Rock.M_Rock")))     Terrain->LayerMaterials.Add(EHexTerrainType::Rock, M);
	if (auto* M = L(TEXT("/Game/Materials/M_Snow.M_Snow")))     Terrain->LayerMaterials.Add(EHexTerrainType::Snow, M);
}

#define TERRAIN_PROLOGUE(RadiusVar) \
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr; \
	if (!World) { UE_LOG(LogTemp, Warning, TEXT("hex: No world")); return; } \
	CleanupHexActors(World); \
	const int32 RadiusVar = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 12; \
	AHexTerrain* Terrain = World->SpawnActor<AHexTerrain>(FVector::ZeroVector, FRotator::ZeroRotator); \
	if (!Terrain) { UE_LOG(LogTemp, Error, TEXT("Failed to spawn terrain")); return; } \
	LoadAllLayerMaterials(Terrain); \
	Terrain->Gap = 0.02f; \
	Terrain->LODSettings.LOD1Distance = 8000.0f; \
	Terrain->LODSettings.LOD2Distance = 20000.0f

// ============================================================================
// Commands
// ============================================================================

static void HexSpawnPrism(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return;
	const float R = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 100.0f;
	const float H = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 200.0f;
	const float X = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.0f;
	const float Y = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.0f;
	const float Z = Args.Num() > 4 ? FCString::Atof(*Args[4]) : 0.0f;
	AHexPrism* P = World->SpawnActor<AHexPrism>(FVector(X, Y, Z), FRotator::ZeroRotator);
	if (P) { P->Radius = R; P->Height = H; P->RegenerateMesh(); }
}

static void HexSpawnGrid(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return;
	const float CR = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 100.0f;
	const int32 GR = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 3;
	const float CH = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 10.0f;
	AHexGrid* G = World->SpawnActor<AHexGrid>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (G) { G->CellRadius = CR; G->GridRadius = GR; G->CellHeight = CH; G->RegenerateMesh(); }
}

static void HexSpawnTerrain(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return;
	const float CR = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 100.0f;
	const int32 GR = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 5;
	const float H = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 150.0f;
	const float N = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.08f;
	AHexTerrain* T = World->SpawnActor<AHexTerrain>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (T) { T->CellRadius = CR; T->GridRadius = GR; T->TerrainConfig.HeightScale = H; T->TerrainConfig.NoiseScale = N; T->RegenerateTerrain(); }
}

static void HexDestroyAll(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return;
	CleanupHexActors(World);
}

static void HexCreateTestScene(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return;
	CleanupHexActors(World);

	const int32 GR = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 15;
	const float CR = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 100.0f;

	AHexTerrain* T = World->SpawnActor<AHexTerrain>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!T) return;
	T->CellRadius = CR; T->GridRadius = GR; T->Gap = 0.05f;
	T->TerrainConfig.HeightScale = 400; T->TerrainConfig.NoiseScale = 0.04f; T->TerrainConfig.NoiseOctaves = 4;
	T->TerrainConfig.WaterLevel = 0.20f; T->TerrainConfig.SandLevel = 0.32f;
	T->TerrainConfig.GrassLevel = 0.55f; T->TerrainConfig.RockLevel = 0.75f;
	T->TerrainConfig.NoiseSeed = FMath::RandRange(0, 99999);
	T->LODSettings.LOD1Distance = 10000; T->LODSettings.LOD2Distance = 25000;
	T->bDebugChunkColors = true;
	T->RegenerateTerrain();

	// Lights
	auto* Sun = World->SpawnActor<ADirectionalLight>(FVector(500, 500, 2000), FRotator(-45, -30, 0));
	if (Sun) { Sun->GetLightComponent()->SetIntensity(8); Sun->GetLightComponent()->SetLightColor(FLinearColor(1,0.95f,0.85f)); }
	auto* Sky = World->SpawnActor<ASkyLight>(FVector(0, 0, 1000), FRotator::ZeroRotator);
	if (Sky) { Sky->GetLightComponent()->SetIntensity(1.5f); Sky->GetLightComponent()->bRealTimeCapture = true; }
	T->PrintStats();
	UE_LOG(LogTemp, Log, TEXT("========== Test Scene Ready =========="));
}

static void HexCreateGrassTerrain(const TArray<FString>& Args)
{
	TERRAIN_PROLOGUE(R);
	Terrain->CellRadius = 120; Terrain->GridRadius = R;
	Terrain->TerrainConfig.HeightScale = 250; Terrain->TerrainConfig.NoiseScale = 0.06f; Terrain->TerrainConfig.NoiseOctaves = 3;
	Terrain->TerrainConfig.WaterLevel = 0; Terrain->TerrainConfig.SandLevel = 0;
	Terrain->TerrainConfig.GrassLevel = 10; Terrain->TerrainConfig.RockLevel = 1;
	Terrain->TerrainConfig.NoiseSeed = FMath::RandRange(0, 99999);
	Terrain->RegenerateTerrain();
	UE_LOG(LogTemp, Log, TEXT("========== Grass Terrain Ready =========="));
	Terrain->PrintStats();
}

static void HexCreateGrassSandTerrain(const TArray<FString>& Args)
{
	TERRAIN_PROLOGUE(R);
	Terrain->CellRadius = 120; Terrain->GridRadius = R;
	Terrain->TerrainConfig.HeightScale = 300; Terrain->TerrainConfig.NoiseScale = 0.05f; Terrain->TerrainConfig.NoiseOctaves = 3;
	Terrain->TerrainConfig.WaterLevel = 0.30f; Terrain->TerrainConfig.SandLevel = 0.60f;
	Terrain->TerrainConfig.GrassLevel = 10; Terrain->TerrainConfig.RockLevel = 1;
	Terrain->TerrainConfig.NoiseSeed = FMath::RandRange(0, 99999);
	Terrain->RegenerateTerrain();
	UE_LOG(LogTemp, Log, TEXT("========== Grass+Sand Terrain Ready =========="));
	Terrain->PrintStats();
}

static void HexCreateFullTerrain(const TArray<FString>& Args)
{
	TERRAIN_PROLOGUE(R);
	Terrain->CellRadius = 100; Terrain->GridRadius = R;
	Terrain->TerrainConfig.HeightScale = 500; Terrain->TerrainConfig.NoiseScale = 0.06f; Terrain->TerrainConfig.NoiseOctaves = 3;
	Terrain->TerrainConfig.WaterLevel = 0.22f; Terrain->TerrainConfig.SandLevel = 0.40f;
	Terrain->TerrainConfig.GrassLevel = 0.60f; Terrain->TerrainConfig.RockLevel = 0.78f;
	Terrain->TerrainConfig.NoiseSeed = FMath::RandRange(0, 99999);
	Terrain->RegenerateTerrain();
	const int32 N = 1 + R * (R + 1) * 3;
	UE_LOG(LogTemp, Log, TEXT("========== Full Terrain Ready ==========  GridRadius=%d  Cells=%d  Chunks=%d"), R, N, Terrain->GetChunkCount());
	Terrain->PrintStats();
}

static void HexPaintCell(const TArray<FString>& Args)
{
	if (Args.Num() < 3) { UE_LOG(LogTemp, Warning, TEXT("Usage: hex.PaintCell <Q> <R> <Type>")); return; }
	AHexTerrain* T = FindFirstTerrain(); if (!T) return;
	EHexTerrainType Ty; if (!ParseTerrainType(Args[2], Ty)) return;
	T->PaintCells({ FHexCoord(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1])) }, Ty);
}

static void HexSetCell(const TArray<FString>& Args)
{
	if (Args.Num() < 3) { UE_LOG(LogTemp, Warning, TEXT("Usage: hex.SetCell <Q> <R> <Type>")); return; }
	AHexTerrain* T = FindFirstTerrain(); if (!T) return;
	EHexTerrainType Ty; if (!ParseTerrainType(Args[2], Ty)) return;
	T->ManualCellTypes.Add(FHexCoord(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1])), Ty);
	T->bManualMode = true; T->RegenerateTerrain();
}

static void HexFillRing(const TArray<FString>& Args)
{
	if (Args.Num() < 4) { UE_LOG(LogTemp, Warning, TEXT("Usage: hex.FillRing <CenterQ> <CenterR> <Radius> <Type>")); return; }
	AHexTerrain* T = FindFirstTerrain(); if (!T) return;
	EHexTerrainType Ty; if (!ParseTerrainType(Args[3], Ty)) return;
	const TArray<FHexCoord> Coords = FHexGeometry::GetSpiral(FHexCoord(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1])), FCString::Atoi(*Args[2]));
	for (const auto& C : Coords) T->ManualCellTypes.Add(C, Ty);
	T->bManualMode = true; T->RegenerateTerrain();
}

static void HexStats(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return;
	int32 C = 0;
	for (TActorIterator<AHexTerrain> It(World); It; ++It) { (*It)->PrintStats(); ++C; }
	if (C == 0) UE_LOG(LogTemp, Log, TEXT("hex.Stats: No AHexTerrain found. Try hex.CreateTestScene"));
}

#undef TERRAIN_PROLOGUE

// ============================================================================
// Registration
// ============================================================================
static TArray<IConsoleCommand*> GHexCommands;

void RegisterHexCommands()
{
	if (GHexCommands.Num() > 0) return;

#define REG(Name, Help, Func) \
	GHexCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(TEXT(Name), TEXT(Help), \
		FConsoleCommandWithArgsDelegate::CreateStatic(&Func), ECVF_Cheat))

	REG("hex.SpawnPrism",            "Spawn hex prism. <Radius> <Height> [X] [Y] [Z]",            HexSpawnPrism);
	REG("hex.SpawnGrid",             "Spawn hex grid. <CellRadius> <GridRadius> [CellHeight]",     HexSpawnGrid);
	REG("hex.SpawnTerrain",          "Spawn hex terrain. <CellRadius> <GridRadius> <HeightScale> [NoiseScale]", HexSpawnTerrain);
	REG("hex.DestroyAll",            "Destroy all hex actors in the current world",                HexDestroyAll);
	REG("hex.CreateTestScene",       "Create test scene: terrain + light + debug colors. [GridRadius=15]", HexCreateTestScene);
	REG("hex.CreateGrassTerrain",    "Create grass-only rolling hills terrain. [GridRadius=12]",   HexCreateGrassTerrain);
	REG("hex.CreateGrassSandTerrain","Create grass+sand mixed terrain. [GridRadius=14]",           HexCreateGrassSandTerrain);
	REG("hex.CreateFullTerrain",     "Create large terrain with all 5 types. [GridRadius=31]",     HexCreateFullTerrain);
	REG("hex.PaintCell",             "Paint a cell: hex.PaintCell <Q> <R> <Water/Sand/Grass/Rock/Snow>", HexPaintCell);
	REG("hex.SetCell",               "Set cell terrain type (enables manual mode). <Q> <R> <Type>", HexSetCell);
	REG("hex.FillRing",              "Fill ring area to one type. <CenterQ> <CenterR> <Radius> <Type>", HexFillRing);
	REG("hex.Stats",                 "Print statistics for all hex terrain actors",                HexStats);

#undef REG

	UE_LOG(LogTemp, Log, TEXT("Hexagon: registered %d console commands"), GHexCommands.Num());
}

void UnregisterHexCommands()
{
	for (IConsoleCommand* C : GHexCommands) IConsoleManager::Get().UnregisterConsoleObject(C);
	GHexCommands.Empty();
}
#endif // WITH_EDITOR
