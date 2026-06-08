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
#include "HexTerrainEdMode.h"
#include "EditorModeRegistry.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "ToolMenuEntry.h"
#include "ToolMenuDelegates.h"
#include "Framework/Commands/UIAction.h"

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

	// Load per-layer materials
	auto LoadMat = [](const TCHAR* Path) { return LoadObject<UMaterialInterface>(nullptr, Path); };
	if (auto* M = LoadMat(TEXT("/Game/Materials/M_Water.M_Water"))) Terrain->LayerMaterials.Add(EHexTerrainType::Water, M);
	if (auto* M = LoadMat(TEXT("/Game/Materials/M_Sand.M_Sand")))   Terrain->LayerMaterials.Add(EHexTerrainType::Sand, M);
	if (auto* M = LoadMat(TEXT("/Game/Materials/M_Grass.M_Grass"))) Terrain->LayerMaterials.Add(EHexTerrainType::Grass, M);
	if (auto* M = LoadMat(TEXT("/Game/Materials/M_Rock.M_Rock")))   Terrain->LayerMaterials.Add(EHexTerrainType::Rock, M);
	if (auto* M = LoadMat(TEXT("/Game/Materials/M_Snow.M_Snow")))   Terrain->LayerMaterials.Add(EHexTerrainType::Snow, M);

	Terrain->RegenerateTerrain();

	UE_LOG(LogTemp, Log, TEXT("========== Grass Terrain Ready =========="));
	UE_LOG(LogTemp, Log, TEXT("  Cells: %d  |  Chunks: %d  |  HeightScale: %.0f"),
		1 + R * (R + 1) * 3, Terrain->GetChunkCount(), 250.0f);
	Terrain->PrintStats();
}

// ============================================================================
// hex.CreateGrassSandTerrain — grass + sand mixed terrain
// ============================================================================
static void HexCreateGrassSandTerrain(const TArray<FString>& Args)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) { UE_LOG(LogTemp, Warning, TEXT("hex.CreateGrassSandTerrain: No world")); return; }

	const int32 R = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 14;

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

	Terrain->CellRadius = 120.0f;
	Terrain->GridRadius = R;
	Terrain->Gap = 0.02f;
	Terrain->TerrainConfig.HeightScale = 300.0f;
	Terrain->TerrainConfig.NoiseScale = 0.05f;
	Terrain->TerrainConfig.NoiseOctaves = 3;
	Terrain->TerrainConfig.WaterLevel = 0.15f;
	Terrain->TerrainConfig.SandLevel = 0.40f;
	Terrain->TerrainConfig.GrassLevel = 10.0f;
	Terrain->TerrainConfig.RockLevel = 1.0f;
	Terrain->TerrainConfig.NoiseSeed = FMath::RandRange(0, 99999);
	Terrain->LODSettings.LOD1Distance = 8000.0f;
	Terrain->LODSettings.LOD2Distance = 20000.0f;

	auto LoadMat = [](const TCHAR* Path) { return LoadObject<UMaterialInterface>(nullptr, Path); };
	if (auto* M = LoadMat(TEXT("/Game/Materials/M_Water.M_Water"))) Terrain->LayerMaterials.Add(EHexTerrainType::Water, M);
	if (auto* M = LoadMat(TEXT("/Game/Materials/M_Sand.M_Sand")))   Terrain->LayerMaterials.Add(EHexTerrainType::Sand, M);
	if (auto* M = LoadMat(TEXT("/Game/Materials/M_Grass.M_Grass"))) Terrain->LayerMaterials.Add(EHexTerrainType::Grass, M);
	if (auto* M = LoadMat(TEXT("/Game/Materials/M_Rock.M_Rock")))   Terrain->LayerMaterials.Add(EHexTerrainType::Rock, M);
	if (auto* M = LoadMat(TEXT("/Game/Materials/M_Snow.M_Snow")))   Terrain->LayerMaterials.Add(EHexTerrainType::Snow, M);

	Terrain->RegenerateTerrain();

	UE_LOG(LogTemp, Log, TEXT("========== Grass+Sand Terrain Ready =========="));
	UE_LOG(LogTemp, Log, TEXT("  Cells: %d  |  Chunks: %d  |  Materials: Grass+Sand"),
		1 + R * (R + 1) * 3, Terrain->GetChunkCount());
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
// Helper: find first terrain actor or report error
// ============================================================================
static AHexTerrain* FindFirstTerrain()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) { UE_LOG(LogTemp, Warning, TEXT("No world")); return nullptr; }

	for (TActorIterator<AHexTerrain> It(World); It; ++It)
	{
		return *It;
	}
	UE_LOG(LogTemp, Warning, TEXT("No AHexTerrain found. Try hex.CreateTestScene first."));
	return nullptr;
}

// ============================================================================
// Helper: parse terrain type from string
// ============================================================================
static bool ParseTerrainType(const FString& Str, EHexTerrainType& OutType)
{
	const UEnum* Enum = StaticEnum<EHexTerrainType>();
	if (!Enum) return false;

	const int64 Val = Enum->GetValueByNameString(Str);
	if (Val == INDEX_NONE)
	{
		// Try case-insensitive
		for (uint8 i = 0; i < static_cast<uint8>(EHexTerrainType::Count); ++i)
		{
			const EHexTerrainType T = static_cast<EHexTerrainType>(i);
			const FString Name = Enum->GetNameStringByValue(static_cast<int64>(T));
			if (Name.Equals(Str, ESearchCase::IgnoreCase))
			{
				OutType = T;
				return true;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("Unknown terrain type '%s'. Use: Water, Sand, Grass, Rock, Snow"), *Str);
		return false;
	}

	OutType = static_cast<EHexTerrainType>(Val);
	return true;
}

// ============================================================================
// hex.SetCell <Q> <R> <Type>
// Set a single hex cell to the given terrain type (enables manual mode).
// ============================================================================
static void HexSetCell(const TArray<FString>& Args)
{
	if (Args.Num() < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: hex.SetCell <Q> <R> <Type>  — e.g. hex.SetCell 3 0 Sand"));
		return;
	}

	const int32 Q = FCString::Atoi(*Args[0]);
	const int32 R = FCString::Atoi(*Args[1]);

	EHexTerrainType Type;
	if (!ParseTerrainType(Args[2], Type)) return;

	AHexTerrain* Terrain = FindFirstTerrain();
	if (!Terrain) return;

	Terrain->ManualCellTypes.Add(FHexCoord(Q, R), Type);
	Terrain->bManualMode = true;
	Terrain->RegenerateTerrain();

	UE_LOG(LogTemp, Log, TEXT("hex.SetCell: (%d, %d) → %s"), Q, R, *UEnum::GetValueAsString(Type));
}

// ============================================================================
// hex.FillRing <CenterQ> <CenterR> <Radius> <Type>
// Fill all cells within the given ring radius to a terrain type (enables manual mode).
// ============================================================================
static void HexFillRing(const TArray<FString>& Args)
{
	if (Args.Num() < 4)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: hex.FillRing <CenterQ> <CenterR> <Radius> <Type>  — e.g. hex.FillRing 0 0 5 Grass"));
		return;
	}

	const int32 CenterQ = FCString::Atoi(*Args[0]);
	const int32 CenterR = FCString::Atoi(*Args[1]);
	const int32 Radius = FCString::Atoi(*Args[2]);

	EHexTerrainType Type;
	if (!ParseTerrainType(Args[3], Type)) return;

	AHexTerrain* Terrain = FindFirstTerrain();
	if (!Terrain) return;

	// Get all cells within radius using spiral
	const TArray<FHexCoord> Coords = FHexGeometry::GetSpiral(FHexCoord(CenterQ, CenterR), Radius);

	for (const FHexCoord& Coord : Coords)
	{
		Terrain->ManualCellTypes.Add(Coord, Type);
	}
	Terrain->bManualMode = true;
	Terrain->RegenerateTerrain();

	UE_LOG(LogTemp, Log, TEXT("hex.FillRing: center(%d,%d) radius=%d  →  %d cells set to %s"),
		CenterQ, CenterR, Radius, Coords.Num(), *UEnum::GetValueAsString(Type));
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
	REGISTER_CMD("hex.CreateGrassSandTerrain","Create grass+sand mixed terrain. [GridRadius=14]", HexCreateGrassSandTerrain);
	REGISTER_CMD("hex.Stats",          "Print statistics for all hex terrain actors",              HexStats);
	REGISTER_CMD("hex.SetCell",        "Set a single hex cell terrain type. <Q> <R> <Type>  e.g. hex.SetCell 3 0 Sand", HexSetCell);
	REGISTER_CMD("hex.FillRing",       "Fill all cells in a ring to one type. <CenterQ> <CenterR> <Radius> <Type>  e.g. hex.FillRing 0 0 5 Grass", HexFillRing);

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

// ============================================================================
// Editor mode registration (WITH_EDITOR)
// ============================================================================

#if WITH_EDITOR
static void RegisterHexEditorModes()
{
	FEditorModeRegistry::Get().RegisterMode<FHexTerrainEdMode>(
		FHexTerrainEdMode::EM_HexTerrainPaint,
		NSLOCTEXT("Hexagon", "HexTerrainPaintMode", "Hex Terrain Paint"),
		FSlateIcon(),
		true
	);
	UE_LOG(LogTemp, Log, TEXT("Hexagon: registered HexTerrainPaint editor mode"));
}

static void UnregisterHexEditorModes()
{
	FEditorModeRegistry::Get().UnregisterMode(FHexTerrainEdMode::EM_HexTerrainPaint);
}

// ============================================================================
// Editor menu registration (WITH_EDITOR)
// ============================================================================
static bool GHexMenuRegistered = false;

static void RegisterHexEditorMenus()
{
	if (GHexMenuRegistered) return;
	GHexMenuRegistered = true;

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = WindowMenu->FindOrAddSection("WindowLayout");

		Section.AddSubMenu(
			"HexagonMenu",
			NSLOCTEXT("Hexagon", "HexagonMenu", "Hexagon"),
			NSLOCTEXT("Hexagon", "HexagonMenuTooltip", "Hexagon terrain tools"),
			FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
			{
				// --- Terrain Presets ---
				FToolMenuSection& PresetSection = SubMenu->AddSection(
					"HexagonPresets", NSLOCTEXT("Hexagon", "PresetsSection", "Terrain Presets"));

				PresetSection.AddMenuEntry(
					"HexCreateTestScene",
					NSLOCTEXT("Hexagon", "CreateTestScene", "Create Test Scene"),
					NSLOCTEXT("Hexagon", "CreateTestSceneTooltip", "Create test terrain with lights and debug colors (GridRadius=15, CellRadius=100)"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]() {
						TArray<FString> Args;
						HexCreateTestScene(Args);
					}))
				);

				PresetSection.AddMenuEntry(
					"HexCreateGrassTerrain",
					NSLOCTEXT("Hexagon", "CreateGrassTerrain", "Create Grass Terrain"),
					NSLOCTEXT("Hexagon", "CreateGrassTerrainTooltip", "Create grass-only rolling hills terrain (GridRadius=12)"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]() {
						TArray<FString> Args;
						HexCreateGrassTerrain(Args);
					}))
				);

				PresetSection.AddMenuEntry(
					"HexCreateGrassSandTerrain",
					NSLOCTEXT("Hexagon", "CreateGrassSandTerrain", "Create Grass+Sand Terrain"),
					NSLOCTEXT("Hexagon", "CreateGrassSandTerrainTooltip", "Create mixed grass, sand, and water terrain (GridRadius=14)"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]() {
						TArray<FString> Args;
						HexCreateGrassSandTerrain(Args);
					}))
				);

				// --- Utilities ---
				FToolMenuSection& UtilSection = SubMenu->AddSection(
					"HexagonUtilities", NSLOCTEXT("Hexagon", "UtilitiesSection", "Utilities"));

				UtilSection.AddMenuEntry(
					"HexDestroyAll",
					NSLOCTEXT("Hexagon", "DestroyAll", "Destroy All"),
					NSLOCTEXT("Hexagon", "DestroyAllTooltip", "Destroy all hex actors in the level"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]() {
						TArray<FString> Args;
						HexDestroyAll(Args);
					}))
				);

				UtilSection.AddMenuEntry(
					"HexPrintStats",
					NSLOCTEXT("Hexagon", "PrintStats", "Print Stats"),
					NSLOCTEXT("Hexagon", "PrintStatsTooltip", "Print terrain statistics (cell count, chunk count, LOD dist) to Output Log"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([]() {
						TArray<FString> Args;
						HexStats(Args);
					}))
				);
			})
		);
	}));

	UE_LOG(LogTemp, Log, TEXT("Hexagon: registered editor menus"));
}
#endif

void FHexagonModule::StartupModule()
{
#if WITH_EDITOR
	FCoreDelegates::OnPostEngineInit.AddStatic(&RegisterHexCommands);
	FCoreDelegates::OnPostEngineInit.AddStatic(&RegisterHexEditorModes);
	RegisterHexEditorMenus();
#endif
}

void FHexagonModule::ShutdownModule()
{
#if WITH_EDITOR
	UnregisterHexCommands();
	UnregisterHexEditorModes();
#endif
}

IMPLEMENT_MODULE(FHexagonModule, Hexagon)
