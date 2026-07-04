// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "HexTerrainGenerator.h"
#include "HexTerrainChunk.h"
#include "AHexTerrain.h"
#include "HexGeometry.h"

// ============================================================================
// Terrain classification
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainClassifyWater, "Hexagon.Terrain.Classify.Water",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTerrainClassifyWater::RunTest(const FString&)
{
	FHexTerrainConfig Config;
	Config.WaterLevel = 0.25f;
	Config.SandLevel  = 0.40f;
	Config.GrassLevel = 0.65f;
	Config.RockLevel  = 0.85f;
	TestEqual("0.0→Water",  FHexTerrainGenerator::ClassifyTerrain(0.0f, Config),  EHexTerrainType::Water);
	TestEqual("0.10→Water", FHexTerrainGenerator::ClassifyTerrain(0.10f, Config), EHexTerrainType::Water);
	TestEqual("0.24→Water", FHexTerrainGenerator::ClassifyTerrain(0.24f, Config), EHexTerrainType::Water);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainClassifySand, "Hexagon.Terrain.Classify.Sand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTerrainClassifySand::RunTest(const FString&)
{
	FHexTerrainConfig Config;
	Config.WaterLevel = 0.25f;
	Config.SandLevel  = 0.40f;
	Config.GrassLevel = 0.65f;
	Config.RockLevel  = 0.85f;
	TestEqual("0.25→Sand", FHexTerrainGenerator::ClassifyTerrain(0.25f, Config), EHexTerrainType::Sand);
	TestEqual("0.30→Sand", FHexTerrainGenerator::ClassifyTerrain(0.30f, Config), EHexTerrainType::Sand);
	TestEqual("0.39→Sand", FHexTerrainGenerator::ClassifyTerrain(0.39f, Config), EHexTerrainType::Sand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainClassifyAllFive, "Hexagon.Terrain.Classify.AllFive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTerrainClassifyAllFive::RunTest(const FString&)
{
	FHexTerrainConfig Config;
	Config.WaterLevel = 0.20f;
	Config.SandLevel  = 0.40f;
	Config.GrassLevel = 0.60f;
	Config.RockLevel  = 0.80f;
	TestEqual("0.05→Water", FHexTerrainGenerator::ClassifyTerrain(0.05f, Config), EHexTerrainType::Water);
	TestEqual("0.30→Sand",  FHexTerrainGenerator::ClassifyTerrain(0.30f, Config), EHexTerrainType::Sand);
	TestEqual("0.50→Grass", FHexTerrainGenerator::ClassifyTerrain(0.50f, Config), EHexTerrainType::Grass);
	TestEqual("0.70→Rock",  FHexTerrainGenerator::ClassifyTerrain(0.70f, Config), EHexTerrainType::Rock);
	TestEqual("0.95→Snow",  FHexTerrainGenerator::ClassifyTerrain(0.95f, Config), EHexTerrainType::Snow);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainClassifyEdgeCases, "Hexagon.Terrain.Classify.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTerrainClassifyEdgeCases::RunTest(const FString&)
{
	FHexTerrainConfig Config;
	Config.WaterLevel = 0.0f;  // no water
	Config.SandLevel  = 0.0f;  // no sand
	Config.GrassLevel = 1.0f;
	Config.RockLevel  = 1.0f;
	TestEqual("all grass", FHexTerrainGenerator::ClassifyTerrain(0.5f, Config), EHexTerrainType::Grass);

	// Everything above grass level but below rock = rock
	Config.GrassLevel = 0.5f;
	TestEqual("0.6→Rock", FHexTerrainGenerator::ClassifyTerrain(0.6f, Config), EHexTerrainType::Rock);

	// Everything above 0 = snow (when all other thresholds are 0)
	Config.WaterLevel = 0.0f; Config.SandLevel = 0.0f; Config.GrassLevel = 0.0f; Config.RockLevel = 0.0f;
	TestEqual("0.01→Snow", FHexTerrainGenerator::ClassifyTerrain(0.01f, Config), EHexTerrainType::Snow);
	return true;
}

// ============================================================================
// Terrain color mapping
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainColorAllTypes, "Hexagon.Terrain.Color.AllTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTerrainColorAllTypes::RunTest(const FString&)
{
	for (uint8 i = 0; i < static_cast<uint8>(EHexTerrainType::Count); ++i)
	{
		FLinearColor Color = FHexTerrainGenerator::GetTerrainColor(static_cast<EHexTerrainType>(i));
		TestTrue(FString::Printf(TEXT("type %d has color"), i), Color.A > 0.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainColorUnique, "Hexagon.Terrain.Color.Unique",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTerrainColorUnique::RunTest(const FString&)
{
	TSet<FLinearColor> Colors;
	for (uint8 i = 0; i < static_cast<uint8>(EHexTerrainType::Count); ++i)
		Colors.Add(FHexTerrainGenerator::GetTerrainColor(static_cast<EHexTerrainType>(i)));
	// All 5 terrain types have distinct colors
	// (Water, Sand, Grass, Rock, Snow = 5 types)
	TestEqual("5 unique colors", Colors.Num(), 5);
	return true;
}

// ============================================================================
// Terrain generation
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainGenerateZeroRadius, "Hexagon.Terrain.Generate.ZeroRadius",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTerrainGenerateZeroRadius::RunTest(const FString&)
{
	FHexTerrainConfig Config;
	Config.CellRadius = 100.0f;
	TArray<FHexTerrainCellData> Cells = FHexTerrainGenerator::Generate(0, Config);
	TestEqual("single cell at origin", Cells.Num(), 1);
	TestEqual("origin Q", Cells[0].Axial.Q, 0);
	TestEqual("origin R", Cells[0].Axial.R, 0);
	TestEqual("origin pos", Cells[0].WorldPos, FVector::ZeroVector);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainGenerateRadius5, "Hexagon.Terrain.Generate.Radius5",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTerrainGenerateRadius5::RunTest(const FString&)
{
	FHexTerrainConfig Config;
	Config.CellRadius = 100.0f;
	TArray<FHexTerrainCellData> Cells = FHexTerrainGenerator::Generate(5, Config);
	const int32 Expected = 1 + 3 * 5 * (5 + 1); // = 91
	TestEqual("91 cells", Cells.Num(), Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainGenerateNoDuplicates, "Hexagon.Terrain.Generate.NoDuplicates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTerrainGenerateNoDuplicates::RunTest(const FString&)
{
	FHexTerrainConfig Config; Config.CellRadius = 100.0f;
	TArray<FHexTerrainCellData> Cells = FHexTerrainGenerator::Generate(8, Config);
	TSet<FHexCoord> Unique;
	for (const auto& C : Cells) Unique.Add(C.Axial);
	TestEqual("no duplicate coords", Cells.Num(), Unique.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainGenerateHeightsInRange, "Hexagon.Terrain.Generate.HeightsInRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTerrainGenerateHeightsInRange::RunTest(const FString&)
{
	FHexTerrainConfig Config;
	Config.HeightScale = 200.0f;
	Config.SeaLevel = 0.0f;
	TArray<FHexTerrainCellData> Cells = FHexTerrainGenerator::Generate(10, Config);
	for (const auto& Cell : Cells)
	{
		TestTrue("height >= 0", Cell.Height >= 0.0f);
		TestTrue("height <= HeightScale", Cell.Height <= Config.HeightScale);
		TestTrue("NormalizedHeight 0-1", Cell.NormalizedHeight >= 0.0f && Cell.NormalizedHeight <= 1.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainGenerateHasWorldPos, "Hexagon.Terrain.Generate.HasWorldPos",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTerrainGenerateHasWorldPos::RunTest(const FString&)
{
	FHexTerrainConfig Config; Config.CellRadius = 100.0f;
	TArray<FHexTerrainCellData> Cells = FHexTerrainGenerator::Generate(3, Config);
	for (const auto& Cell : Cells)
	{
		FVector ExpectedPos = FHexGeometry::HexToWorld(Cell.Axial, Config.CellRadius);
		TestEqual("world pos matches", Cell.WorldPos, ExpectedPos);
	}
	return true;
}

// ============================================================================
// Noise determinism
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainNoiseDeterministic, "Hexagon.Terrain.Noise.Deterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTerrainNoiseDeterministic::RunTest(const FString&)
{
	FHexTerrainConfig Config;
	Config.NoiseSeed = 42;
	Config.CellRadius = 100.0f;

	TArray<FHexTerrainCellData> Cells1 = FHexTerrainGenerator::Generate(5, Config);
	TArray<FHexTerrainCellData> Cells2 = FHexTerrainGenerator::Generate(5, Config);

	TestEqual("same seed same count", Cells1.Num(), Cells2.Num());
	for (int32 i = 0; i < Cells1.Num(); ++i)
	{
		TestTrue(FString::Printf(TEXT("cell %d height"), i), FMath::IsNearlyEqual(Cells1[i].Height, Cells2[i].Height, 0.001));
		TestEqual(FString::Printf(TEXT("cell %d type"), i),  Cells1[i].TerrainType, Cells2[i].TerrainType);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainNoiseSeedChanges, "Hexagon.Terrain.Noise.SeedChanges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTerrainNoiseSeedChanges::RunTest(const FString&)
{
	FHexTerrainConfig Config1; Config1.NoiseSeed = 1; Config1.CellRadius = 100.0f;
	FHexTerrainConfig Config2; Config2.NoiseSeed = 2; Config2.CellRadius = 100.0f;

	TArray<FHexTerrainCellData> Cells1 = FHexTerrainGenerator::Generate(10, Config1);
	TArray<FHexTerrainCellData> Cells2 = FHexTerrainGenerator::Generate(10, Config2);

	// Different seeds should produce different heights (not all the same)
	int32 DiffCount = 0;
	for (int32 i = 0; i < Cells1.Num(); ++i)
		if (!FMath::IsNearlyEqual(Cells1[i].Height, Cells2[i].Height))
			++DiffCount;
	TestTrue("different seeds give different heights", DiffCount > Cells1.Num() / 2);
	return true;
}

// ============================================================================
// LOD settings
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLODSettingsDefaults, "Hexagon.Terrain.LOD.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLODSettingsDefaults::RunTest(const FString&)
{
	FHexTerrainLODSettings Settings;
	TestEqual("LOD1 default",  Settings.LOD1Distance, 2000.0f);
	TestEqual("LOD2 default",  Settings.LOD2Distance, 5000.0f);
	TestEqual("LOD0 segs",     Settings.LOD0HeightSegments, 2);
	TestEqual("LOD1 segs",     Settings.LOD1HeightSegments, 1);
	TestEqual("hysteresis",    Settings.Hysteresis, 0.1f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLODSettingsValidRange, "Hexagon.Terrain.LOD.ValidRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLODSettingsValidRange::RunTest(const FString&)
{
	FHexTerrainLODSettings Settings;
	// Default values should be within valid ranges
	TestTrue("LOD1Distance > 0", Settings.LOD1Distance > 0.0f);
	TestTrue("LOD2Distance > LOD1Distance", Settings.LOD2Distance > Settings.LOD1Distance);
	TestTrue("LOD0HeightSegments >= 1", Settings.LOD0HeightSegments >= 1);
	TestTrue("LOD1HeightSegments >= 1", Settings.LOD1HeightSegments >= 1);
	TestTrue("Hysteresis >= 0", Settings.Hysteresis >= 0.0f);
	TestTrue("Hysteresis <= 0.5", Settings.Hysteresis <= 0.5f);
	return true;
}

// ============================================================================
// CHUNK_SIZE constant
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChunkSizeConstant, "Hexagon.Terrain.Chunk.SizeConstant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FChunkSizeConstant::RunTest(const FString&)
{
	TestEqual("CHUNK_SIZE = 16", AHexTerrain::CHUNK_SIZE, 16);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChunkCoordMapping, "Hexagon.Terrain.Chunk.CoordMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FChunkCoordMapping::RunTest(const FString&)
{
	// A cell at (0,0) should be in chunk (0,0)
	// A cell at (16,0) should be in chunk (1,0)
	// A cell at (-1,-1) should be in chunk (-1,-1)
	const int32 CS = AHexTerrain::CHUNK_SIZE;

	auto ToChunk = [CS](int32 Q, int32 R) -> FIntPoint {
		return FIntPoint(
			FMath::FloorToInt32(static_cast<float>(Q) / CS),
			FMath::FloorToInt32(static_cast<float>(R) / CS));
	};

	TestEqual("(0,0)",   ToChunk(0, 0),   FIntPoint(0, 0));
	TestEqual("(16,0)",  ToChunk(16, 0),  FIntPoint(1, 0));
	TestEqual("(-1,-1)", ToChunk(-1, -1), FIntPoint(-1, -1));
	TestEqual("(-16,0)", ToChunk(-16, 0), FIntPoint(-1, 0));
	TestEqual("(15,15)", ToChunk(15, 15), FIntPoint(0, 0));
	TestEqual("(32,32)", ToChunk(32, 32), FIntPoint(2, 2));
	return true;
}
