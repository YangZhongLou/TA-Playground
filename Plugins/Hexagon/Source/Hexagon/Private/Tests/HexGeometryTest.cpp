// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "HexGeometry.h"

// ============================================================================
// FHexCoord basic operations
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCoordEquality, "Hexagon.Geometry.Coord.Equality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexCoordEquality::RunTest(const FString&)
{
	FHexCoord A(3, -2), B(3, -2), C(0, 0);
	TestEqual("equality", A, B);
	TestNotEqual("inequality", A, C);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCoordAddition, "Hexagon.Geometry.Coord.Addition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexCoordAddition::RunTest(const FString&)
{
	FHexCoord A(2, 3), B(1, -1);
	FHexCoord Sum = A + B;
	TestEqual("Q sum", Sum.Q, 3);
	TestEqual("R sum", Sum.R, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCoordSubtraction, "Hexagon.Geometry.Coord.Subtraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexCoordSubtraction::RunTest(const FString&)
{
	FHexCoord A(5, 2), B(1, 3);
	FHexCoord Diff = A - B;
	TestEqual("Q diff", Diff.Q, 4);
	TestEqual("R diff", Diff.R, -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCoordScalarMultiply, "Hexagon.Geometry.Coord.ScalarMultiply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexCoordScalarMultiply::RunTest(const FString&)
{
	FHexCoord A(2, -1);
	FHexCoord Scaled = A * 3;
	TestEqual("Q*3", Scaled.Q, 6);
	TestEqual("R*3", Scaled.R, -3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCoordCubicS, "Hexagon.Geometry.Coord.CubicS",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexCoordCubicS::RunTest(const FString&)
{
	FHexCoord A(3, -5);
	TestEqual("S = -Q-R", A.S(), 2);
	FHexCoord B(0, 0);
	TestEqual("S at origin", B.S(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCoordHash, "Hexagon.Geometry.Coord.Hash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexCoordHash::RunTest(const FString&)
{
	FHexCoord A(3, -2), B(3, -2), C(4, -2);
	TestEqual("same coord same hash", GetTypeHash(A), GetTypeHash(B));
	TestNotEqual("different coord different hash", GetTypeHash(A), GetTypeHash(C));
	return true;
}

// ============================================================================
// Directions
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexDirectionsCount, "Hexagon.Geometry.Directions.Count",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexDirectionsCount::RunTest(const FString&)
{
	TestEqual("six directions", static_cast<int32>(UE_ARRAY_COUNT(FHexGeometry::Directions)), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexDirectionsRoundTrip, "Hexagon.Geometry.Directions.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexDirectionsRoundTrip::RunTest(const FString&)
{
	// Sum of all 6 directions should be (0,0)
	FHexCoord Sum(0, 0);
	for (int32 i = 0; i < 6; ++i) Sum = Sum + FHexGeometry::Directions[i];
	TestEqual("sum Q=0", Sum.Q, 0);
	TestEqual("sum R=0", Sum.R, 0);
	return true;
}

// ============================================================================
// World ↔ Hex coordinate conversion
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexWorldRoundTrip, "Hexagon.Geometry.World.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexWorldRoundTrip::RunTest(const FString&)
{
	const float Radius = 100.0f;
	FHexCoord Original(-2, 5);
	FVector WorldPos = FHexGeometry::HexToWorld(Original, Radius);
	FHexCoord Converted = FHexGeometry::WorldToHex(WorldPos, Radius);
	TestEqual("round-trip Q", Converted.Q, Original.Q);
	TestEqual("round-trip R", Converted.R, Original.R);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexWorldRoundTripsMany, "Hexagon.Geometry.World.RoundTripMany",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexWorldRoundTripsMany::RunTest(const FString&)
{
	const float Radius = 100.0f;
	TArray<FHexCoord> TestCoords = {
		FHexCoord(0,0), FHexCoord(5,0), FHexCoord(-3,4), FHexCoord(2,-7),
		FHexCoord(10,-10), FHexCoord(-8,-3), FHexCoord(7,5), FHexCoord(-1,-1)
	};
	for (const FHexCoord& Orig : TestCoords)
	{
		FVector World = FHexGeometry::HexToWorld(Orig, Radius);
		FHexCoord Converted = FHexGeometry::WorldToHex(World, Radius);
		TestEqual(FString::Printf(TEXT("(%d,%d)"), Orig.Q, Orig.R), Converted, Orig);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexWorldOrigin, "Hexagon.Geometry.World.Origin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexWorldOrigin::RunTest(const FString&)
{
	FVector Origin = FHexGeometry::HexToWorld(FHexCoord(0,0), 100.0f);
	TestEqual("origin X", Origin.X, 0.0);
	TestEqual("origin Y", Origin.Y, 0.0);
	TestEqual("origin Z", Origin.Z, 0.0);
	return true;
}

// ============================================================================
// Distance
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexDistanceOrigin, "Hexagon.Geometry.Distance.FromOrigin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexDistanceOrigin::RunTest(const FString&)
{
	TestEqual("dist(0,0->0,0)", FHexGeometry::Distance(FHexCoord(0,0), FHexCoord(0,0)), 0);
	TestEqual("dist(0,0->1,0)", FHexGeometry::Distance(FHexCoord(0,0), FHexCoord(1,0)), 1);
	TestEqual("dist(0,0->3,-2)", FHexGeometry::Distance(FHexCoord(0,0), FHexCoord(3,-2)), 3);
	TestEqual("dist(0,0->0,-5)", FHexGeometry::Distance(FHexCoord(0,0), FHexCoord(0,-5)), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexDistanceSymmetric, "Hexagon.Geometry.Distance.Symmetric",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexDistanceSymmetric::RunTest(const FString&)
{
	FHexCoord A(5, -8), B(-3, 4);
	TestEqual("dist(A,B)=dist(B,A)", FHexGeometry::Distance(A, B), FHexGeometry::Distance(B, A));
	return true;
}

// ============================================================================
// Neighbors
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexNeighborsCount, "Hexagon.Geometry.Neighbors.Count",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexNeighborsCount::RunTest(const FString&)
{
	TArray<FHexCoord> Neighbors = FHexGeometry::GetNeighbors(FHexCoord(0,0));
	TestEqual("6 neighbors", Neighbors.Num(), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexNeighborsAllDistanceOne, "Hexagon.Geometry.Neighbors.AllDistanceOne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexNeighborsAllDistanceOne::RunTest(const FString&)
{
	FHexCoord Center(3, -4);
	TArray<FHexCoord> Neighbors = FHexGeometry::GetNeighbors(Center);
	for (const FHexCoord& N : Neighbors)
	{
		TestEqual(FString::Printf(TEXT("neighbor (%d,%d) dist=1"), N.Q, N.R),
			FHexGeometry::Distance(Center, N), 1);
	}
	return true;
}

// ============================================================================
// Rings
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexRingZero, "Hexagon.Geometry.Ring.Zero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexRingZero::RunTest(const FString&)
{
	TArray<FHexCoord> Ring = FHexGeometry::GetRing(FHexCoord(0,0), 0);
	TestEqual("ring 0 size", Ring.Num(), 1);
	TestEqual("ring 0 is center", Ring[0], FHexCoord(0,0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexRingSizeFormula, "Hexagon.Geometry.Ring.SizeFormula",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexRingSizeFormula::RunTest(const FString&)
{
	// Ring radius R has max(6*R, 1) cells
	for (int32 R = 1; R <= 10; ++R)
	{
		TArray<FHexCoord> Ring = FHexGeometry::GetRing(FHexCoord(5, -3), R);
		TestEqual(FString::Printf(TEXT("ring %d size = 6*R"), R), Ring.Num(), 6 * R);
	}
	return true;
}

// ============================================================================
// Spiral
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexSpiralTotal, "Hexagon.Geometry.Spiral.Total",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexSpiralTotal::RunTest(const FString&)
{
	// Spiral of radius R has 1 + 3*R*(R+1) cells
	for (int32 R = 0; R <= 10; ++R)
	{
		TArray<FHexCoord> Spiral = FHexGeometry::GetSpiral(FHexCoord(0,0), R);
		const int32 Expected = 1 + 3 * R * (R + 1);
		TestEqual(FString::Printf(TEXT("spiral R=%d"), R), Spiral.Num(), Expected);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexSpiralNoDuplicates, "Hexagon.Geometry.Spiral.NoDuplicates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexSpiralNoDuplicates::RunTest(const FString&)
{
	TArray<FHexCoord> Spiral = FHexGeometry::GetSpiral(FHexCoord(0,0), 5);
	TSet<FHexCoord> Unique(Spiral);
	TestEqual("no duplicates", Spiral.Num(), Unique.Num());
	return true;
}

// ============================================================================
// Line
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexLineStraight, "Hexagon.Geometry.Line.Straight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexLineStraight::RunTest(const FString&)
{
	TArray<FHexCoord> Line = FHexGeometry::GetLine(FHexCoord(0,0), FHexCoord(5,0));
	TestEqual("line size", Line.Num(), 6);
	TestEqual("first", Line[0], FHexCoord(0,0));
	TestEqual("last", Line[5], FHexCoord(5,0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexLineEndpoints, "Hexagon.Geometry.Line.Endpoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexLineEndpoints::RunTest(const FString&)
{
	FHexCoord Start(0, 0), End(3, -2);
	TArray<FHexCoord> Line = FHexGeometry::GetLine(Start, End);
	TestEqual("starts at start", Line[0], Start);
	TestEqual("ends at end", Line.Last(), End);
	return true;
}

// ============================================================================
// Corners
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCornersCount, "Hexagon.Geometry.Corners.Count",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexCornersCount::RunTest(const FString&)
{
	TestEqual("6 corners", FHexGeometry::GetHexCorners(100.0f).Num(), 6);
	TestEqual("6 corners at position", FHexGeometry::GetHexCornersAt(FVector(1,2,3), 100.0f).Num(), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCornersOnCircle, "Hexagon.Geometry.Corners.OnCircle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexCornersOnCircle::RunTest(const FString&)
{
	const float R = 100.0f;
	TArray<FVector> Corners = FHexGeometry::GetHexCorners(R);
	for (const FVector& C : Corners)
	{
		TestTrue("distance to origin = radius", FMath::IsNearlyEqual(C.Size2D(), static_cast<double>(R), 0.01));
		TestTrue("Z is zero", FMath::IsNearlyZero(C.Z, 0.01));
	}
	return true;
}

// ============================================================================
// Spacing helpers
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexSpacingConsistency, "Hexagon.Geometry.Spacing.Consistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHexSpacingConsistency::RunTest(const FString&)
{
	const float R = 100.0f;
	// Two hexagons at (0,0) and (1,0) should be exactly HorizontalSpacing apart in X
	FVector Center0 = FHexGeometry::HexToWorld(FHexCoord(0,0), R);
	FVector Center1 = FHexGeometry::HexToWorld(FHexCoord(1,0), R);
	TestTrue("horizontal spacing", FMath::IsNearlyEqual(Center1.X - Center0.X, static_cast<double>(FHexGeometry::GetHorizontalSpacing(R)), 0.01));
	return true;
}
