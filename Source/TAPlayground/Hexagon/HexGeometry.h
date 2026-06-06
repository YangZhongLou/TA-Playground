// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HexGeometry.generated.h"

/**
 * Pure math utilities for hexagonal coordinate systems.
 * Pointy-topped hexagons with Axial (q, r) coordinates.
 * The third cube coordinate is implicit: s = -q - r.
 */
USTRUCT(BlueprintType)
struct TAPLAYGROUND_API FHexCoord
{
	GENERATED_BODY()

	/** Axial coordinate column (pointing right). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Q = 0;

	/** Axial coordinate row (pointing down-right). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 R = 0;

	FHexCoord() = default;
	FHexCoord(int32 InQ, int32 InR) : Q(InQ), R(InR) {}

	/** Implicit cube coordinate s = -q - r. */
	int32 S() const { return -Q - R; }

	bool operator==(const FHexCoord& Other) const
	{
		return Q == Other.Q && R == Other.R;
	}

	bool operator!=(const FHexCoord& Other) const
	{
		return !(*this == Other);
	}

	FHexCoord operator+(const FHexCoord& Other) const
	{
		return FHexCoord(Q + Other.Q, R + Other.R);
	}

	FHexCoord operator-(const FHexCoord& Other) const
	{
		return FHexCoord(Q - Other.Q, R - Other.R);
	}

	FHexCoord operator*(int32 Scale) const
	{
		return FHexCoord(Q * Scale, R * Scale);
	}
};

/** Hash for use in TMap/TSet. */
inline uint32 GetTypeHash(const FHexCoord& Coord)
{
	return HashCombine(GetTypeHash(Coord.Q), GetTypeHash(Coord.R));
}

/**
 * Static utility class for hexagonal grid math.
 */
class TAPLAYGROUND_API FHexGeometry
{
public:
	/** Six axial directions: E, NE, NW, W, SW, SE (clockwise from East). */
	static const FHexCoord Directions[6];

	/** Corner angles in radians for pointy-topped hexagons (0°, 60°, 120°, 180°, 240°, 300°). */
	static const float CornerAnglesRad[6];

public:
	/** Convert axial coordinates to world position (XZ plane, Y up). */
	static FVector HexToWorld(const FHexCoord& Hex, float CellRadius);

	/** Convert world position to axial coordinates (nearest hex). */
	static FHexCoord WorldToHex(const FVector& WorldPos, float CellRadius);

	/** Convert world position to axial coordinates with fractional result. */
	static FVector HexToWorldFractional(float Q, float R, float CellRadius);

	/** Hex distance in grid steps (number of edges to traverse). */
	static int32 Distance(const FHexCoord& A, const FHexCoord& B);

	/** Get the six neighboring hex coordinates. */
	static TArray<FHexCoord> GetNeighbors(const FHexCoord& Hex);

	/** Get neighbor in a specific direction (0-5, clockwise from East). */
	static FHexCoord GetNeighbor(const FHexCoord& Hex, int32 DirectionIndex);

	/**
	 * Get all hex coordinates in a ring at distance Radius from Center.
	 * Radius 0 returns just Center.
	 */
	static TArray<FHexCoord> GetRing(const FHexCoord& Center, int32 Radius);

	/**
	 * Get all hex coordinates within a spiral from 0 to MaxRadius (inclusive).
	 * Ordered from center outward.
	 */
	static TArray<FHexCoord> GetSpiral(const FHexCoord& Center, int32 MaxRadius);

	/**
	 * Get all hex coordinates on a line from Start to End (inclusive).
	 * Uses cube coordinate linear interpolation.
	 */
	static TArray<FHexCoord> GetLine(const FHexCoord& Start, const FHexCoord& End);

	/** Get world-space positions of the 6 vertices of a single hex (XZ plane, Y=0). */
	static TArray<FVector> GetHexCorners(float Radius);

	/** Get world-space positions of the 6 vertices of a single hex at a given center position. */
	static TArray<FVector> GetHexCornersAt(const FVector& Center, float Radius);

	/** Width of a pointy-topped hex (distance between leftmost and rightmost points). */
	static float GetHexWidth(float Radius) { return Radius * UE_SQRT_3; }

	/** Height of a pointy-topped hex (distance between topmost and bottommost points). */
	static float GetHexHeight(float Radius) { return Radius * 2.0f; }

	/** Horizontal spacing between adjacent hex centers in the same row. */
	static float GetHorizontalSpacing(float Radius) { return Radius * UE_SQRT_3; }

	/** Vertical spacing between adjacent hex rows. */
	static float GetVerticalSpacing(float Radius) { return Radius * 1.5f; }
};