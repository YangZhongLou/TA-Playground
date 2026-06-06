// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HexGeometry.h"

/**
 * Data for a single hex cell in the grid.
 */
struct TAPLAYGROUND_API FHexCellData
{
	/** Axial coordinate of this cell. */
	FHexCoord Axial;

	/** World-space position of the cell center. */
	FVector WorldPos;

	/** Height of the cell (for terrain, Phase 3). */
	float Height = 0.0f;

	/** Terrain type index (for Phase 3). */
	int32 TerrainType = 0;

	/** Visual color for this cell. */
	FLinearColor Color = FLinearColor::White;

	FHexCellData() = default;
};

/**
 * Generates hexagonal grid layouts (cell positions and metadata).
 */
class TAPLAYGROUND_API FHexGridGenerator
{
public:
	/**
	 * Generate a spiral hex grid: center + rings 1..GridRadius.
	 *
	 * @param CellRadius   Hex circumradius.
	 * @param GridRadius   Number of rings (0 = just center).
	 * @param CellHeight   Base height of each cell.
	 * @param BaseColor    Default cell color.
	 */
	static TArray<FHexCellData> GenerateSpiral(
		float CellRadius,
		int32 GridRadius,
		float CellHeight = 0.0f,
		const FLinearColor& BaseColor = FLinearColor::White
	);

	/**
	 * Generate a rectangular hex grid within axial bounds.
	 *
	 * @param CellRadius   Hex circumradius.
	 * @param MinQ, MaxQ   Q-axis inclusive bounds.
	 * @param MinR, MaxR   R-axis inclusive bounds.
	 * @param CellHeight   Base height of each cell.
	 * @param BaseColor    Default cell color.
	 */
	static TArray<FHexCellData> GenerateRect(
		float CellRadius,
		int32 MinQ, int32 MaxQ,
		int32 MinR, int32 MaxR,
		float CellHeight = 0.0f,
		const FLinearColor& BaseColor = FLinearColor::White
	);

	/**
	 * Apply pseudo-random height variation to cells.
	 * Uses UE's Perlin noise (deterministic for same seed).
	 */
	static void ApplyHeightNoise(
		TArray<FHexCellData>& Cells,
		float Scale = 0.1f,
		float Amplitude = 50.0f
	);
};
