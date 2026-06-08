// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#if WITH_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HexGeometry.h"
#include "HexTerrainGenerator.h"
#include "HexPrismGenerator.h"

// ============================================================================
// FHexCoord Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCoord_Basic, "TA.Hexagon.FHexCoord.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexCoord_Basic::RunTest(const FString& Parameters)
{
	FHexCoord A(3, -1);
	TestEqual("Q", A.Q, 3);
	TestEqual("R", A.R, -1);
	TestEqual("S", A.S(), -2);

	FHexCoord B(3, -1);
	TestTrue("Equality", A == B);
	TestFalse("Inequality", A != B);

	FHexCoord C(1, 2);
	FHexCoord Sum = A + C;
	TestEqual("Add Q", Sum.Q, 4);
	TestEqual("Add R", Sum.R, 1);

	FHexCoord Diff = A - C;
	TestEqual("Sub Q", Diff.Q, 2);
	TestEqual("Sub R", Diff.R, -3);

	FHexCoord Scaled = C * 3;
	TestEqual("Mul Q", Scaled.Q, 3);
	TestEqual("Mul R", Scaled.R, 6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCoord_Hash, "TA.Hexagon.FHexCoord.Hash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexCoord_Hash::RunTest(const FString& Parameters)
{
	TMap<FHexCoord, int32> Map;
	Map.Add(FHexCoord(0, 0), 1);
	Map.Add(FHexCoord(1, 0), 2);
	Map.Add(FHexCoord(-1, 1), 3);
	Map.Add(FHexCoord(0, 0), 4); // Overwrite

	TestEqual("Map count", Map.Num(), 3);
	TestEqual("Lookup (0,0)", Map[FHexCoord(0, 0)], 4);
	TestEqual("Lookup (1,0)", Map[FHexCoord(1, 0)], 2);
	TestEqual("Lookup (-1,1)", Map[FHexCoord(-1, 1)], 3);

	// Verify different coords don't collide
	TSet<FHexCoord> Set;
	for (int32 Q = -5; Q <= 5; ++Q)
	{
		for (int32 R = -5; R <= 5; ++R)
		{
			Set.Add(FHexCoord(Q, R));
		}
	}
	TestEqual("Unique coords in set", Set.Num(), 121); // 11x11

	return true;
}

// ============================================================================
// FHexGeometry Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGeometry_Roundtrip, "TA.Hexagon.FHexGeometry.Roundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGeometry_Roundtrip::RunTest(const FString& Parameters)
{
	const float Radius = 100.0f;

	// Test a range of hex coords: World → Hex → World should be consistent
	for (int32 Q = -5; Q <= 5; ++Q)
	{
		for (int32 R = -5; R <= 5; ++R)
		{
			FHexCoord Original(Q, R);
			FVector WorldPos = FHexGeometry::HexToWorld(Original, Radius);
			FHexCoord RoundTripped = FHexGeometry::WorldToHex(WorldPos, Radius);

			TestEqual(
				FString::Printf(TEXT("Roundtrip (%d,%d)"), Q, R),
				RoundTripped, Original
			);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGeometry_Distance, "TA.Hexagon.FHexGeometry.Distance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGeometry_Distance::RunTest(const FString& Parameters)
{
	TestEqual("Distance to self", FHexGeometry::Distance(FHexCoord(0,0), FHexCoord(0,0)), 0);
	TestEqual("Adjacent E", FHexGeometry::Distance(FHexCoord(0,0), FHexCoord(1,0)), 1);
	TestEqual("Adjacent NW", FHexGeometry::Distance(FHexCoord(0,0), FHexCoord(-1,1)), 1);
	TestEqual("Two steps", FHexGeometry::Distance(FHexCoord(0,0), FHexCoord(2,-1)), 2);
	TestEqual("Diagonal", FHexGeometry::Distance(FHexCoord(0,0), FHexCoord(3,-3)), 3);
	TestEqual("Far", FHexGeometry::Distance(FHexCoord(-3,1), FHexCoord(2,-4)), 5);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGeometry_Spiral, "TA.Hexagon.FHexGeometry.Spiral",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGeometry_Spiral::RunTest(const FString& Parameters)
{
	// Center only
	{
		TArray<FHexCoord> Result = FHexGeometry::GetSpiral(FHexCoord(0, 0), 0);
		TestEqual("Radius 0 count", Result.Num(), 1);
		TestEqual("Radius 0 element", Result[0], FHexCoord(0, 0));
	}

	// Radius 1: center + 6 neighbors = 7
	{
		TArray<FHexCoord> Result = FHexGeometry::GetSpiral(FHexCoord(0, 0), 1);
		TestEqual("Radius 1 count", Result.Num(), 7);
	}

	// Radius 2: 1 + 6 + 12 = 19
	{
		TArray<FHexCoord> Result = FHexGeometry::GetSpiral(FHexCoord(0, 0), 2);
		TestEqual("Radius 2 count", Result.Num(), 19);
	}

	// Radius 3: 1 + 6 + 12 + 18 = 37
	{
		TArray<FHexCoord> Result = FHexGeometry::GetSpiral(FHexCoord(0, 0), 3);
		TestEqual("Radius 3 count", Result.Num(), 37);
	}

	// Verify no duplicates
	{
		TArray<FHexCoord> Result = FHexGeometry::GetSpiral(FHexCoord(0, 0), 5);
		TSet<FHexCoord> Unique;
		for (const auto& C : Result) Unique.Add(C);
		TestEqual("No duplicates radius 5", Result.Num(), Unique.Num());

		int32 Expected = 1 + 5 * (5 + 1) * 3; // 1 + 3*n*(n+1)
		TestEqual("Expected count radius 5", Result.Num(), Expected); // 91
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGeometry_Neighbors, "TA.Hexagon.FHexGeometry.Neighbors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGeometry_Neighbors::RunTest(const FString& Parameters)
{
	TArray<FHexCoord> N = FHexGeometry::GetNeighbors(FHexCoord(0, 0));
	TestEqual("Neighbor count", N.Num(), 6);

	// All 6 directions should be at distance 1
	for (const FHexCoord& Neighbor : N)
	{
		TestEqual("Neighbor distance",
			FHexGeometry::Distance(FHexCoord(0, 0), Neighbor), 1);
	}

	// Each direction index should give a neighbor at distance 1
	for (int32 Dir = 0; Dir < 6; ++Dir)
	{
		FHexCoord D = FHexGeometry::GetNeighbor(FHexCoord(5, 3), Dir);
		TestEqual(
			FString::Printf(TEXT("Direction %d distance"), Dir),
			FHexGeometry::Distance(FHexCoord(5, 3), D), 1);
	}

	return true;
}

// ============================================================================
// FHexTerrainGenerator Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexTerrainGen_Classify, "TA.Hexagon.FHexTerrainGen.Classify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexTerrainGen_Classify::RunTest(const FString& Parameters)
{
	FHexTerrainConfig Config;
	Config.WaterLevel = 0.20f;
	Config.SandLevel = 0.35f;
	Config.GrassLevel = 0.60f;
	Config.RockLevel = 0.80f;

	TestEqual("Water low", FHexTerrainGenerator::ClassifyTerrain(0.05f, Config), EHexTerrainType::Water);
	TestEqual("Water boundary below", FHexTerrainGenerator::ClassifyTerrain(0.19f, Config), EHexTerrainType::Water);
	TestEqual("Sand low", FHexTerrainGenerator::ClassifyTerrain(0.21f, Config), EHexTerrainType::Sand);
	TestEqual("Sand high", FHexTerrainGenerator::ClassifyTerrain(0.34f, Config), EHexTerrainType::Sand);
	TestEqual("Grass low", FHexTerrainGenerator::ClassifyTerrain(0.36f, Config), EHexTerrainType::Grass);
	TestEqual("Grass high", FHexTerrainGenerator::ClassifyTerrain(0.59f, Config), EHexTerrainType::Grass);
	TestEqual("Rock low", FHexTerrainGenerator::ClassifyTerrain(0.61f, Config), EHexTerrainType::Rock);
	TestEqual("Rock high", FHexTerrainGenerator::ClassifyTerrain(0.79f, Config), EHexTerrainType::Rock);
	TestEqual("Snow", FHexTerrainGenerator::ClassifyTerrain(0.81f, Config), EHexTerrainType::Snow);
	TestEqual("Snow max", FHexTerrainGenerator::ClassifyTerrain(1.00f, Config), EHexTerrainType::Snow);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexTerrainGen_Color, "TA.Hexagon.FHexTerrainGen.Color",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexTerrainGen_Color::RunTest(const FString& Parameters)
{
	// Each type should return a valid (non-white-default) color
	FLinearColor WaterColor = FHexTerrainGenerator::GetTerrainColor(EHexTerrainType::Water);
	FLinearColor SandColor = FHexTerrainGenerator::GetTerrainColor(EHexTerrainType::Sand);
	FLinearColor GrassColor = FHexTerrainGenerator::GetTerrainColor(EHexTerrainType::Grass);
	FLinearColor RockColor = FHexTerrainGenerator::GetTerrainColor(EHexTerrainType::Rock);
	FLinearColor SnowColor = FHexTerrainGenerator::GetTerrainColor(EHexTerrainType::Snow);

	// All colors should differ
	TestNotEqual("Water vs Sand", WaterColor.R, SandColor.R);
	TestNotEqual("Sand vs Grass", SandColor.G, GrassColor.G);
	TestNotEqual("Rock vs Snow", RockColor.R, SnowColor.R);

	// Water should be blue-ish
	TestTrue("Water is blue", WaterColor.B > WaterColor.R);

	// Snow should be bright
	TestTrue("Snow is bright", SnowColor.R > 0.9f && SnowColor.G > 0.9f && SnowColor.B > 0.9f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexTerrainGen_Generate, "TA.Hexagon.FHexTerrainGen.Generate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexTerrainGen_Generate::RunTest(const FString& Parameters)
{
	// GridRadius=0: single cell at origin
	{
		TArray<FHexTerrainCellData> Cells = FHexTerrainGenerator::Generate(0, FHexTerrainConfig());
		TestEqual("Radius 0 cell count", Cells.Num(), 1);
		TestEqual("Radius 0 coord", Cells[0].Axial, FHexCoord(0, 0));
		TestEqual("Radius 0 coord check", Cells[0].Axial, FHexCoord(0, 0));
	}

	// GridRadius=3: 37 cells
	{
		TArray<FHexTerrainCellData> Cells = FHexTerrainGenerator::Generate(3, FHexTerrainConfig());
		TestEqual("Radius 3 cell count", Cells.Num(), 37);
	}

	// All cells should have valid data
	{
		FHexTerrainConfig Config;
		Config.WaterLevel = 0.2f;
		Config.SandLevel = 0.4f;
		Config.GrassLevel = 0.6f;
		Config.RockLevel = 0.8f;

		TArray<FHexTerrainCellData> Cells = FHexTerrainGenerator::Generate(5, Config);
		for (const FHexTerrainCellData& Cell : Cells)
		{
			TestTrue("NormalizedHeight in [0,1]",
				Cell.NormalizedHeight >= 0.0f && Cell.NormalizedHeight <= 1.0f);

			TestTrue("TerrainType valid",
				static_cast<uint8>(Cell.TerrainType) < static_cast<uint8>(EHexTerrainType::Count));

			TestTrue("Color valid",
				Cell.Color.R >= 0.0f && Cell.Color.A > 0.0f);
		}
	}

	// Verify deterministic output (same seed → same cells)
	{
		FHexTerrainConfig Config;
		Config.NoiseSeed = 42;

		TArray<FHexTerrainCellData> A = FHexTerrainGenerator::Generate(3, Config);
		TArray<FHexTerrainCellData> B = FHexTerrainGenerator::Generate(3, Config);

		TestEqual("Deterministic count", A.Num(), B.Num());
		for (int32 i = 0; i < A.Num(); ++i)
		{
			TestEqual(FString::Printf(TEXT("Deterministic cell %d"), i), A[i].Axial, B[i].Axial);
			TestEqual(FString::Printf(TEXT("Deterministic height %d"), i), A[i].NormalizedHeight, B[i].NormalizedHeight);
		}
	}

	// Different seed → different layout
	{
		FHexTerrainConfig ConfigA; ConfigA.NoiseSeed = 100;
		FHexTerrainConfig ConfigB; ConfigB.NoiseSeed = 999;

		TArray<FHexTerrainCellData> A = FHexTerrainGenerator::Generate(4, ConfigA);
		TArray<FHexTerrainCellData> B = FHexTerrainGenerator::Generate(4, ConfigB);

		int32 DiffCount = 0;
		for (int32 i = 0; i < A.Num(); ++i)
		{
			if (!FMath::IsNearlyEqual(A[i].NormalizedHeight, B[i].NormalizedHeight, 0.001f))
				++DiffCount;
		}
		TestTrue("Different seeds produce different heights", DiffCount > 10);
	}

	return true;
}

// ============================================================================
// FHexPrismGenerator Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexPrismGen_Basic, "TA.Hexagon.FHexPrismGen.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexPrismGen_Basic::RunTest(const FString& Parameters)
{
	FHexPrismMeshData Mesh;

	// Generate a full prism
	FHexPrismGenerator::Generate(Mesh, 100.0f, 200.0f, true, true, 2, FColor::Green);

	// Should have vertices
	TestTrue("Has vertices", Mesh.Vertices.Num() > 0);
	TestTrue("Has triangles", Mesh.Triangles.Num() > 0);
	TestTrue("Has UVs", Mesh.UVs.Num() == Mesh.Vertices.Num());
	TestTrue("Has normals", Mesh.Normals.Num() == Mesh.Vertices.Num());
	TestTrue("Has vertex colors", Mesh.VertexColors.Num() == Mesh.Vertices.Num());
	TestTrue("Has tangents", Mesh.Tangents.Num() == Mesh.Vertices.Num());

	// Triangle count should be multiple of 3
	TestTrue("Triangles divisible by 3", Mesh.Triangles.Num() % 3 == 0);

	// All normals should be valid unit vectors
	for (const FVector& N : Mesh.Normals)
	{
		TestTrue("Normal normalized", FMath::IsNearlyEqual(N.Size(), 1.0f, 0.001f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexPrismGen_FlatHex, "TA.Hexagon.FHexPrismGen.FlatHex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexPrismGen_FlatHex::RunTest(const FString& Parameters)
{
	FHexPrismMeshData Mesh;

	// Generate a flat hexagon (height=0, true 2D plane)
	FHexPrismGenerator::GenerateFlatHexagon(Mesh, 100.0f, 0.0f, FColor::Red);

	TestTrue("Flat hex has vertices", Mesh.Vertices.Num() > 0);
	TestTrue("Flat hex has triangles", Mesh.Triangles.Num() > 0);

	// Flat hexagon (height=0) should be a single layer: 1 center + 6 corners = 7 vertices, 6 triangles
	TestEqual("Flat hex vertices", Mesh.Vertices.Num(), 7);
	TestEqual("Flat hex triangles", Mesh.Triangles.Num(), 6 * 3); // 6 triangles × 3 indices

	// All normals should be FVector::UpVector
	for (const FVector& N : Mesh.Normals)
	{
		TestTrue("Flat hex normals up",
			FMath::IsNearlyEqual(N.X, 0.0f, 0.001f) &&
			FMath::IsNearlyEqual(N.Y, 0.0f, 0.001f) &&
			FMath::IsNearlyEqual(N.Z, 1.0f, 0.001f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexPrismGen_WorldUV, "TA.Hexagon.FHexPrismGen.WorldUV",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexPrismGen_WorldUV::RunTest(const FString& Parameters)
{
	FHexPrismMeshData MeshA, MeshB;

	const FVector Offset(500.0f, 300.0f, 0.0f);
	const float TileSize = 200.0f;

	// Generate two prisms at different world positions
	FHexPrismGenerator::Generate(MeshA, 100.0f, 100.0f, true, true, 1, FColor::White,
		FVector::ZeroVector, TileSize);

	FHexPrismGenerator::Generate(MeshB, 100.0f, 100.0f, true, true, 1, FColor::White,
		Offset, TileSize);

	TestEqual("World UV same vertex count", MeshA.Vertices.Num(), MeshB.Vertices.Num());

	// UVs should differ because world offsets differ
	bool bUVsDiffer = false;
	for (int32 i = 0; i < MeshA.Vertices.Num(); ++i)
	{
		if (!MeshA.UVs[i].Equals(MeshB.UVs[i], 0.01f))
		{
			bUVsDiffer = true;
			break;
		}
	}
	TestTrue("World UVs differ by offset", bUVsDiffer);

	// With UVTileSize=0, UVs should be the same (default local-space)
	FHexPrismMeshData MeshC, MeshD;
	FHexPrismGenerator::Generate(MeshC, 100.0f, 100.0f, true, true, 1, FColor::White,
		FVector::ZeroVector, 0.0f);
	FHexPrismGenerator::Generate(MeshD, 100.0f, 100.0f, true, true, 1, FColor::White,
		Offset, 0.0f);

	bool bDefaultUVsDiffer = false;
	for (int32 i = 0; i < MeshC.Vertices.Num(); ++i)
	{
		if (!MeshC.UVs[i].Equals(MeshD.UVs[i], 0.01f))
		{
			bDefaultUVsDiffer = true;
			break;
		}
	}
	TestFalse("Default UVs identical regardless of offset", bDefaultUVsDiffer);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexPrismGen_FlatHexWorldUV, "TA.Hexagon.FHexPrismGen.FlatHexWorldUV",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexPrismGen_FlatHexWorldUV::RunTest(const FString& Parameters)
{
	FHexPrismMeshData Mesh;

	// Flat hexagon with world-space UV
	FHexPrismGenerator::GenerateFlatHexagon(Mesh, 100.0f, 0.0f, FColor::White,
		FVector(1000.0f, 500.0f, 0.0f), 200.0f);

	TestEqual("Flat world UV vertices", Mesh.Vertices.Num(), 7);

	// UVs should be in world-space range (1000/200 = 5.0, 500/200 = 2.5 roughly)
	for (const FVector2D& UV : Mesh.UVs)
	{
		TestTrue("UV.X in world range", UV.X > 4.0f && UV.X < 6.0f);
		TestTrue("UV.Y in world range", UV.Y > 2.0f && UV.Y < 3.5f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexPrismGen_LOD, "TA.Hexagon.FHexPrismGen.LOD",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexPrismGen_LOD::RunTest(const FString& Parameters)
{
	// Full prism (caps + sides)
	FHexPrismMeshData Full;
	FHexPrismGenerator::Generate(Full, 100.0f, 200.0f, true, true, 2, FColor::White);

	// Flat hex (LOD 2 equivalent — no sides, no bottom)
	FHexPrismMeshData Flat;
	FHexPrismGenerator::GenerateFlatHexagon(Flat, 100.0f, 0.0f, FColor::White);

	// Flat hex should have fewer vertices than full prism
	TestTrue("Flat hex fewer vertices than full prism", Flat.Vertices.Num() < Full.Vertices.Num());
	TestTrue("Flat hex fewer triangles than full prism", Flat.Triangles.Num() < Full.Triangles.Num());

	return true;
}

#endif // WITH_AUTOMATION_TESTS
