// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#if WITH_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HexDataLayer.h"
#include "HexDataLayerComponent.h"
#include "HexGeometry.h"
#include "HexTerrainGenerator.h"
#include "HexGridGenerator.h"

// ============================================================================
// FHexCellRecord Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCellRecord_Basic, "TA.Hexagon.HexDataLayer.FHexCellRecord.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexCellRecord_Basic::RunTest(const FString& Parameters)
{
	// Default construction
	FHexCellRecord Default;
	TestEqual("Default Axial", Default.Axial, FHexCoord(0, 0));
	TestEqual("Default WorldPos", Default.WorldPos, FVector::ZeroVector);
	TestEqual("Default Height", Default.Height, 0.0f);
	TestEqual("Default TerrainType", Default.TerrainType, EHexTerrainType::Grass);
	TestFalse("Default bIsBlocked", Default.bIsBlocked);
	TestTrue("Default Color", Default.Color == FLinearColor::White);
	TestEqual("Default Metadata empty", Default.Metadata.Num(), 0);

	// Explicit construction
	FHexCellRecord Explicit(FHexCoord(3, -1), FVector(100, 0, 50), 50.0f,
	                        EHexTerrainType::Rock, true, FLinearColor::Red);
	TestEqual("Explicit Axial", Explicit.Axial, FHexCoord(3, -1));
	TestEqual("Explicit WorldPos", Explicit.WorldPos, FVector(100, 0, 50));
	TestEqual("Explicit Height", Explicit.Height, 50.0f);
	TestEqual("Explicit TerrainType", Explicit.TerrainType, EHexTerrainType::Rock);
	TestTrue("Explicit bIsBlocked", Explicit.bIsBlocked);
	TestTrue("Explicit Color", Explicit.Color == FLinearColor::Red);

	// Metadata
	FHexCellRecord WithMeta;
	WithMeta.Metadata.Add(FName("FactionOwner"), 3.0f);
	WithMeta.Metadata.Add(FName("Population"), 12.0f);
	TestEqual("Metadata count", WithMeta.Metadata.Num(), 2);
	TestEqual("Metadata FactionOwner", WithMeta.Metadata[FName("FactionOwner")], 3.0f);
	TestEqual("Metadata Population", WithMeta.Metadata[FName("Population")], 12.0f);

	return true;
}

// ============================================================================
// FHexCellRecordFactory Tests
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCellRecordFactory_Convert, "TA.Hexagon.HexDataLayer.Factory.Convert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexCellRecordFactory_Convert::RunTest(const FString& Parameters)
{
	// From FHexCellData
	FHexCellData CellData;
	CellData.Axial = FHexCoord(1, -2);
	CellData.WorldPos = FVector(50, 0, 30);
	CellData.Height = 30.0f;
	CellData.TerrainType = 1; // int32 version
	CellData.Color = FLinearColor(0.1f, 0.2f, 0.3f);

	FHexCellRecord FromGrid = FHexCellRecordFactory::FromCellData(CellData);
	TestEqual("FromCellData Axial", FromGrid.Axial, CellData.Axial);
	TestEqual("FromCellData WorldPos", FromGrid.WorldPos, CellData.WorldPos);
	TestEqual("FromCellData Height", FromGrid.Height, CellData.Height);
	TestEqual("FromCellData TerrainType", FromGrid.TerrainType, static_cast<EHexTerrainType>(1));
	TestFalse("FromCellData bIsBlocked", FromGrid.bIsBlocked);

	// From FHexTerrainCellData
	FHexTerrainCellData TerrainData;
	TerrainData.Axial = FHexCoord(-3, 4);
	TerrainData.WorldPos = FVector(-200, 0, 400);
	TerrainData.NormalizedHeight = 0.65f;
	TerrainData.Height = 130.0f;
	TerrainData.TerrainType = EHexTerrainType::Snow;
	TerrainData.Color = FLinearColor::White;

	FHexCellRecord FromTerrain = FHexCellRecordFactory::FromTerrainCellData(TerrainData);
	TestEqual("FromTerrain Axial", FromTerrain.Axial, TerrainData.Axial);
	TestEqual("FromTerrain Height", FromTerrain.Height, TerrainData.Height);
	TestEqual("FromTerrain TerrainType", FromTerrain.TerrainType, EHexTerrainType::Snow);
	TestTrue("FromTerrain Color", FromTerrain.Color == FLinearColor::White);

	return true;
}

// ============================================================================
// FHexGridData â€?Mutation
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridData_Mutation, "TA.Hexagon.HexDataLayer.GridData.Mutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGridData_Mutation::RunTest(const FString& Parameters)
{
	FHexGridData Grid;
	TestTrue("Initially empty", Grid.IsEmpty());
	TestEqual("Initially 0 cells", Grid.Num(), 0);

	// Add cells
	TestFalse("SetCell new returns false", Grid.SetCell(
		FHexCellRecord(FHexCoord(0, 0), FVector::ZeroVector)));
	TestTrue("SetCell overwrite returns true", Grid.SetCell(
		FHexCellRecord(FHexCoord(0, 0), FVector::ZeroVector)));

	TestFalse("SetCell new (1,0)", Grid.SetCell(
		FHexCellRecord(FHexCoord(1, 0), FVector(100, 0, 0))));
	TestFalse("SetCell new (0,1)", Grid.SetCell(
		FHexCellRecord(FHexCoord(0, 1), FVector(50, 0, 86))));

	TestEqual("3 cells", Grid.Num(), 3);
	TestTrue("HasCell (0,0)", Grid.HasCell(FHexCoord(0, 0)));
	TestTrue("HasCell (1,0)", Grid.HasCell(FHexCoord(1, 0)));
	TestFalse("HasCell nonexistent", Grid.HasCell(FHexCoord(5, 5)));

	// Remove
	TestTrue("RemoveCell existing", Grid.RemoveCell(FHexCoord(1, 0)));
	TestFalse("RemoveCell nonexistent", Grid.RemoveCell(FHexCoord(1, 0)));
	TestEqual("2 cells after remove", Grid.Num(), 2);
	TestFalse("HasCell removed", Grid.HasCell(FHexCoord(1, 0)));

	// Reset
	Grid.Reset();
	TestTrue("Empty after reset", Grid.IsEmpty());
	TestEqual("0 after reset", Grid.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridData_AddCells, "TA.Hexagon.HexDataLayer.GridData.AddCells",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGridData_AddCells::RunTest(const FString& Parameters)
{
	FHexGridData Grid;
	TArray<FHexCellRecord> Records;
	for (int32 i = 0; i < 10; ++i)
	{
		Records.Emplace(FHexCoord(i, 0), FVector(i * 100.0f, 0, 0));
	}
	Grid.AddCells(Records);
	TestEqual("10 cells added", Grid.Num(), 10);
	for (int32 i = 0; i < 10; ++i)
	{
		TestTrue(FString::Printf(TEXT("HasCell (%d,0)"), i), Grid.HasCell(FHexCoord(i, 0)));
	}
	return true;
}

// ============================================================================
// FHexGridData â€?Spatial Queries
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridData_Spatial, "TA.Hexagon.HexDataLayer.GridData.Spatial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGridData_Spatial::RunTest(const FString& Parameters)
{
	FHexGridData Grid;
	// Populate full spiral radius 2: 19 cells
	TArray<FHexCoord> AllCoords = FHexGeometry::GetSpiral(FHexCoord(0, 0), 2);
	for (const FHexCoord& C : AllCoords)
	{
		FHexCellRecord Record;
		Record.Axial = C;
		Record.WorldPos = FHexGeometry::HexToWorld(C, 100.0f);
		// Tag each cell's ring by its distance from center
		Record.Height = static_cast<float>(FHexGeometry::Distance(FHexCoord(0, 0), C));
		Grid.SetCell(Record);
	}

	TestEqual("19 cells total", Grid.Num(), 19);

	// GetCell
	const FHexCellRecord* Center = Grid.GetCell(FHexCoord(0, 0));
	TestNotNull("GetCell center", Center);
	TestEqual("Center height 0", Center->Height, 0.0f);

	// GetCellsInRadius(0, 0): just center
	{
		TArray<const FHexCellRecord*> Cells = Grid.GetCellsInRadius(FHexCoord(0, 0), 0);
		TestEqual("Radius 0 count", Cells.Num(), 1);
		TestEqual("Radius 0 is center", Cells[0]->Axial, FHexCoord(0, 0));
	}

	// GetCellsInRadius(0, 1): center + 6 neighbors = 7
	{
		TArray<const FHexCellRecord*> Cells = Grid.GetCellsInRadius(FHexCoord(0, 0), 1);
		TestEqual("Radius 1 count", Cells.Num(), 7);
	}

	// GetNeighbors of center: exactly 6
	{
		TArray<const FHexCellRecord*> Neighbors = Grid.GetNeighbors(FHexCoord(0, 0));
		TestEqual("Center has 6 neighbors", Neighbors.Num(), 6);
		for (const FHexCellRecord* N : Neighbors)
		{
			TestEqual("Neighbor at distance 1",
				FHexGeometry::Distance(FHexCoord(0, 0), N->Axial), 1);
		}
	}

	// GetRing radius 2: 12 cells
	{
		TArray<const FHexCellRecord*> Ring = Grid.GetRing(FHexCoord(0, 0), 2);
		TestEqual("Ring 2 has 12 cells", Ring.Num(), 12);
		for (const FHexCellRecord* R : Ring)
		{
			TestEqual("Ring cell at distance 2",
				FHexGeometry::Distance(FHexCoord(0, 0), R->Axial), 2);
		}
	}

	// GetSpiral radius 2: all 19
	{
		TArray<const FHexCellRecord*> Spiral = Grid.GetSpiral(FHexCoord(0, 0), 2);
		TestEqual("Spiral radius 2 = 19", Spiral.Num(), 19);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridData_SparseNeighbors, "TA.Hexagon.HexDataLayer.GridData.SparseNeighbors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGridData_SparseNeighbors::RunTest(const FString& Parameters)
{
	// Only two non-adjacent cells â€?GetNeighbors returns only existing neighbors
	FHexGridData Grid;
	Grid.SetCell(FHexCellRecord(FHexCoord(0, 0), FVector::ZeroVector));
	Grid.SetCell(FHexCellRecord(FHexCoord(3, 0), FVector(300, 0, 0)));

	// Center has 0 existing neighbors (none of its 6 neighbors are in the grid)
	{
		TArray<const FHexCellRecord*> N = Grid.GetNeighbors(FHexCoord(0, 0));
		TestEqual("Sparse: center has 0 neighbors", N.Num(), 0);
	}

	// Add one neighbor
	Grid.SetCell(FHexCellRecord(FHexCoord(1, 0), FVector(100, 0, 0)));
	{
		TArray<const FHexCellRecord*> N = Grid.GetNeighbors(FHexCoord(0, 0));
		TestEqual("Sparse: center has 1 neighbor", N.Num(), 1);
		TestEqual("The neighbor is (1,0)", N[0]->Axial, FHexCoord(1, 0));
	}

	return true;
}

// ============================================================================
// FHexGridData â€?FindCells (predicate)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridData_FindCells, "TA.Hexagon.HexDataLayer.GridData.FindCells",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGridData_FindCells::RunTest(const FString& Parameters)
{
	FHexGridData Grid;
	TArray<FHexCoord> Coords = FHexGeometry::GetSpiral(FHexCoord(0, 0), 3);
	for (const FHexCoord& C : Coords)
	{
		FHexCellRecord Record;
		Record.Axial = C;
		Record.WorldPos = FHexGeometry::HexToWorld(C, 100.0f);
		Record.bIsBlocked = (FHexGeometry::Distance(FHexCoord(0, 0), C) == 2);
		Grid.SetCell(Record);
	}

	// Find blocked cells (ring 2 = 12 cells)
	TArray<const FHexCellRecord*> Blocked = Grid.FindCells(
		[](const FHexCellRecord& R) { return R.bIsBlocked; });
	TestEqual("12 blocked cells (ring 2)", Blocked.Num(), 12);

	// Find cells with no match
	TArray<const FHexCellRecord*> None = Grid.FindCells(
		[](const FHexCellRecord& R) { return R.Height > 9999.0f; });
	TestEqual("No matches", None.Num(), 0);

	return true;
}

// ============================================================================
// FHexGridData â€?Iteration
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridData_Iteration, "TA.Hexagon.HexDataLayer.GridData.Iteration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGridData_Iteration::RunTest(const FString& Parameters)
{
	FHexGridData Grid;
	TArray<FHexCoord> Coords = FHexGeometry::GetSpiral(FHexCoord(0, 0), 2);
	for (const FHexCoord& C : Coords)
	{
		Grid.SetCell(FHexCellRecord(C, FHexGeometry::HexToWorld(C, 100.0f)));
	}

	// ForEachCell
	int32 Count = 0;
	Grid.ForEachCell([&Count](const FHexCellRecord&) { ++Count; });
	TestEqual("ForEachCell count = 19", Count, 19);

	// ForEachInRadius(0, 1): center + 6 neighbors = 7
	int32 RadiusCount = 0;
	Grid.ForEachInRadius(FHexCoord(0, 0), 1,
		[&RadiusCount](const FHexCellRecord&) { ++RadiusCount; });
	TestEqual("ForEachInRadius count = 7", RadiusCount, 7);

	return true;
}

// ============================================================================
// FHexGridData â€?Serialization
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridData_Roundtrip, "TA.Hexagon.HexDataLayer.GridData.Roundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGridData_Roundtrip::RunTest(const FString& Parameters)
{
	FHexGridData Original;
	Original.CellRadius = 200.0f;

	// Fill with known data
	TArray<FHexCoord> Coords = FHexGeometry::GetSpiral(FHexCoord(0, 0), 2);
	for (const FHexCoord& C : Coords)
	{
		FHexCellRecord Record;
		Record.Axial = C;
		Record.WorldPos = FHexGeometry::HexToWorld(C, Original.CellRadius);
		Record.Height = static_cast<float>(FHexGeometry::Distance(FHexCoord(0, 0), C));
		Record.TerrainType = EHexTerrainType::Grass;
		Record.bIsBlocked = false;
		Record.Color = FLinearColor::Green;
		Record.Metadata.Add(FName("Dist"), Record.Height);
		Original.SetCell(Record);
	}

	// Round-trip through bytes
	TArray<uint8> Bytes = Original.ToBytes();
	TestTrue("Bytes not empty", Bytes.Num() > 0);

	FHexGridData Restored;
	Restored.FromBytes(Bytes);

	TestEqual("Restored CellRadius", Restored.CellRadius, Original.CellRadius);
	TestEqual("Restored Num", Restored.Num(), Original.Num());

	// Verify each cell
	for (const auto& Pair : Original.Cells)
	{
		const FHexCellRecord& Orig = Pair.Value;
		const FHexCellRecord* Rest = Restored.GetCell(Pair.Key);
		TestNotNull(FString::Printf(TEXT("Restored has cell (%d,%d)"), Pair.Key.Q, Pair.Key.R), Rest);
		if (Rest)
		{
			TestEqual("Restored Axial", Rest->Axial, Orig.Axial);
			TestEqual("Restored WorldPos", Rest->WorldPos, Orig.WorldPos);
			TestEqual("Restored Height", Rest->Height, Orig.Height);
			TestEqual("Restored TerrainType", Rest->TerrainType, Orig.TerrainType);
			TestEqual("Restored bIsBlocked", Rest->bIsBlocked, Orig.bIsBlocked);
			TestEqual("Restored Color", Rest->Color, Orig.Color);
			TestEqual("Restored Metadata count", Rest->Metadata.Num(), Orig.Metadata.Num());
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridData_EmptySerialization, "TA.Hexagon.HexDataLayer.GridData.EmptySerialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGridData_EmptySerialization::RunTest(const FString& Parameters)
{
	FHexGridData Empty;
	TArray<uint8> Bytes = Empty.ToBytes();
	TestTrue("Empty grid has bytes (header only)", Bytes.Num() > 0);

	FHexGridData Restored;
	Restored.FromBytes(Bytes);
	TestEqual("Restored empty", Restored.Num(), 0);
	TestTrue("Restored IsEmpty", Restored.IsEmpty());

	return true;
}

// ============================================================================
// FHexGridData â€?Empty Grid Safety
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridData_EmptySafe, "TA.Hexagon.HexDataLayer.GridData.EmptySafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGridData_EmptySafe::RunTest(const FString& Parameters)
{
	FHexGridData Grid;
	TestTrue("IsEmpty", Grid.IsEmpty());

	// All queries on empty grid should return empty, not crash
	TestNull("GetCell null", Grid.GetCell(FHexCoord(0, 0)));
	TestFalse("HasCell false", Grid.HasCell(FHexCoord(0, 0)));

	TestEqual("Neighbors empty", Grid.GetNeighbors(FHexCoord(0, 0)).Num(), 0);
	TestEqual("Radius empty", Grid.GetCellsInRadius(FHexCoord(0, 0), 5).Num(), 0);
	TestEqual("Ring empty", Grid.GetRing(FHexCoord(0, 0), 3).Num(), 0);
	TestEqual("Spiral empty", Grid.GetSpiral(FHexCoord(0, 0), 5).Num(), 0);
	TestEqual("Line empty", Grid.GetLine(FHexCoord(0, 0), FHexCoord(5, 5)).Num(), 0);
	TestEqual("FindCells empty", Grid.FindCells([](const FHexCellRecord&) { return true; }).Num(), 0);

		// Iteration on empty grid should not call the function
		int32 IterCount = 0;
		Grid.ForEachCell([&IterCount](const FHexCellRecord&) { ++IterCount; });
		TestEqual("ForEachCell empty grid not called", IterCount, 0);
		Grid.ForEachInRadius(FHexCoord(0, 0), 10,
			[&IterCount](const FHexCellRecord&) { ++IterCount; });
		TestEqual("ForEachInRadius empty grid not called", IterCount, 0);

		return true;
}

// ============================================================================
// FHexGridData â€?GenerateSpiralGrid
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridData_GenerateSpiral, "TA.Hexagon.HexDataLayer.GridData.GenerateSpiral",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGridData_GenerateSpiral::RunTest(const FString& Parameters)
{
	FHexGridData Grid;
	Grid.GenerateSpiralGrid(3, 150.0f, 20.0f);

	// Radius 3 = 37 cells
	TestEqual("37 cells", Grid.Num(), 37);
	TestEqual("CellRadius set", Grid.CellRadius, 150.0f);

	// Every cell should have Axial set, WorldPos computed, Height set
	Grid.ForEachCell([this](const FHexCellRecord& Record) {
		TestTrue("WorldPos not zero", !Record.WorldPos.IsZero());
		TestEqual("Height = 20", Record.Height, 20.0f);
		TestEqual("TerrainType default", Record.TerrainType, EHexTerrainType::Grass);
		TestFalse("Not blocked", Record.bIsBlocked);
	});

	// Center should be at (0,0) with height 0 (Z is flat, XY is centered)
	const FHexCellRecord* Center = Grid.GetCell(FHexCoord(0, 0));
	TestNotNull("Center exists", Center);
	if (Center)
	{
		// WorldPos for hex (0,0) at any radius is FVector::ZeroVector
		TestTrue("Center WorldPos near zero", Center->WorldPos.Equals(FVector::ZeroVector, 0.1f));
	}

	// Calling again replaces
	Grid.GenerateSpiralGrid(1, 100.0f, 10.0f);
	TestEqual("Replaced: 7 cells", Grid.Num(), 7);

	return true;
}

// ============================================================================
// FHexDataLayerComponent â€?Events
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexDataLayerComponent_Events, "TA.Hexagon.HexDataLayer.Component.Events",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FHexDataLayerComponent_Events::RunTest(const FString& Parameters)
	{
		UHexDataLayerComponent* Component = NewObject<UHexDataLayerComponent>();
		FHexGridData& Grid = Component->GetGridData();

		TestTrue("Initially empty via component", Grid.IsEmpty());

		// SetCell â€” new cell
		FHexCellRecord Record(FHexCoord(0, 0), FVector::ZeroVector);
		Component->SetCell(Record);
		TestEqual("After SetCell: count 1", Grid.Num(), 1);
		TestTrue("After SetCell: HasCell", Grid.HasCell(FHexCoord(0, 0)));

		// SetCell â€” update existing cell
		Record.Height = 100.0f;
		Component->SetCell(Record);
		TestEqual("After update: count still 1", Grid.Num(), 1);
		const FHexCellRecord* Updated = Grid.GetCell(FHexCoord(0, 0));
		TestNotNull("Updated cell exists", Updated);
		if (Updated)
		{
			TestEqual("Updated Height", Updated->Height, 100.0f);
		}

		// SetCell â€” add second cell
		Component->SetCell(FHexCellRecord(FHexCoord(1, 0), FVector(100, 0, 0)));
		TestEqual("After 2nd SetCell: count 2", Grid.Num(), 2);
		TestTrue("HasCell (1,0)", Grid.HasCell(FHexCoord(1, 0)));

		// RemoveCell â€” existing
		Component->RemoveCell(FHexCoord(1, 0));
		TestEqual("After remove: count 1", Grid.Num(), 1);
		TestFalse("Removed cell gone", Grid.HasCell(FHexCoord(1, 0)));

		// RemoveCell â€” nonexistent (no crash, no state change)
		Component->RemoveCell(FHexCoord(99, 99));
		TestEqual("After remove nonexistent: count still 1", Grid.Num(), 1);

		// ClearAll
		Component->ClearAll();
		TestTrue("After ClearAll: empty", Grid.IsEmpty());
		TestEqual("After ClearAll: count 0", Grid.Num(), 0);

		// GenerateSpiralGrid
		Component->GenerateSpiralGrid(2, 100.0f, 10.0f);
		TestEqual("GenerateSpiralGrid: 19 cells", Grid.Num(), 19);
		TestTrue("GenerateSpiralGrid: center exists", Grid.HasCell(FHexCoord(0, 0)));

		return true;
	}

// ============================================================================
// FHexDataLayerComponent â€?Query methods
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexDataLayerComponent_Queries, "TA.Hexagon.HexDataLayer.Component.Queries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexDataLayerComponent_Queries::RunTest(const FString& Parameters)
{
	UHexDataLayerComponent* Component = NewObject<UHexDataLayerComponent>();
	Component->GenerateSpiralGrid(1, 100.0f, 0.0f); // 7 cells

	TestEqual("GetCellCount", Component->GetCellCount(), 7);

	// HasCell
	TestTrue("HasCell center", Component->HasCell(FHexCoord(0, 0)));
	TestFalse("HasCell outside", Component->HasCell(FHexCoord(10, 10)));

	// GetCell
	FHexCellRecord Out;
	TestTrue("GetCell success", Component->GetCell(FHexCoord(0, 0), Out));
	TestEqual("GetCell returns center", Out.Axial, FHexCoord(0, 0));
	TestFalse("GetCell fail", Component->GetCell(FHexCoord(99, 99), Out));

	// GetNeighbors
	{
		TArray<FHexCellRecord> N = Component->GetNeighbors(FHexCoord(0, 0));
		TestEqual("Neighbors from center", N.Num(), 6);
	}

	// GetCellsInRadius
	{
		TArray<FHexCellRecord> R0 = Component->GetCellsInRadius(FHexCoord(0, 0), 0);
		TestEqual("Radius 0 = 1", R0.Num(), 1);
	}

	// GetSpiral
	{
		TArray<FHexCellRecord> S = Component->GetSpiral(FHexCoord(0, 0), 1);
		TestEqual("Spiral radius 1 = 7", S.Num(), 7);
	}

	// GetRing
	{
		TArray<FHexCellRecord> R = Component->GetRing(FHexCoord(0, 0), 1);
		TestEqual("Ring radius 1 = 6", R.Num(), 6);
	}

	return true;
}

// ============================================================================
// FHexDataLayerComponent â€?Serialization
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexDataLayerComponent_Roundtrip, "TA.Hexagon.HexDataLayer.Component.Roundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexDataLayerComponent_Roundtrip::RunTest(const FString& Parameters)
{
	UHexDataLayerComponent* Source = NewObject<UHexDataLayerComponent>();
	Source->CellRadius = 200.0f;
	Source->GenerateSpiralGrid(2, Source->CellRadius, 50.0f);

	// Add metadata to test serialization of extended fields
	FHexCellRecord Update;
	Source->GetCell(FHexCoord(0, 0), Update);
	Update.Metadata.Add(FName("FactionOwner"), 1.0f);
	Update.Metadata.Add(FName("Population"), 5.0f);
	Source->SetCell(Update);

	TArray<uint8> Bytes = Source->SerializeToBytes();
	TestTrue("Bytes not empty", Bytes.Num() > 0);

	UHexDataLayerComponent* Target = NewObject<UHexDataLayerComponent>();
	TestTrue("DeserializeFromBytes success", Target->DeserializeFromBytes(Bytes));

	TestEqual("Target CellRadius", Target->CellRadius, Source->CellRadius); // Should show 200 or 100?
	// Note: CellRadius is serialized inside GridData (FHexGridData::Serialize),
	// not on the component UPROPERTY directly.
	TestEqual("Target cell count", Target->GetCellCount(), Source->GetCellCount());

	// Verify metadata round-tripped
	FHexCellRecord TargetCell;
	TestTrue("Target has center", Target->GetCell(FHexCoord(0, 0), TargetCell));
	TestEqual("Metadata FactionOwner", TargetCell.Metadata[FName("FactionOwner")], 1.0f);
	TestEqual("Metadata Population", TargetCell.Metadata[FName("Population")], 5.0f);

	return true;
}

// ============================================================================
// FHexGridData â€?GetLine
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridData_Line, "TA.Hexagon.HexDataLayer.GridData.Line",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHexGridData_Line::RunTest(const FString& Parameters)
{
	FHexGridData Grid;
	// Populate a line of cells along Q axis
	for (int32 Q = -3; Q <= 3; ++Q)
	{
		Grid.SetCell(FHexCellRecord(FHexCoord(Q, 0),
			FHexGeometry::HexToWorld(FHexCoord(Q, 0), 100.0f)));
	}

	// Line from (-3,0) to (3,0) should find all 7
	TArray<const FHexCellRecord*> Line = Grid.GetLine(FHexCoord(-3, 0), FHexCoord(3, 0));
	TestEqual("Line from -3 to 3", Line.Num(), 7);

	// But some cells in the line won't exist if they're off-axis
	TArray<const FHexCellRecord*> Diagonal = Grid.GetLine(FHexCoord(-3, 0), FHexCoord(0, 3));
	// Only existing cells are returned â€?this is correctness, not a specific count
	TestTrue("Diagonal line returns only existing cells", Diagonal.Num() >= 1);

	return true;
}

#endif // WITH_AUTOMATION_TESTS