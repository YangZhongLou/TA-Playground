// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HexGridGenerator.h"

TArray<FHexCellData> FHexGridGenerator::GenerateSpiral(
	float CellRadius,
	int32 GridRadius,
	float CellHeight,
	const FLinearColor& BaseColor
)
{
	GridRadius = FMath::Max(0, GridRadius);

	TArray<FHexCoord> Coords = FHexGeometry::GetSpiral(FHexCoord(0, 0), GridRadius);
	TArray<FHexCellData> Result;
	Result.Reserve(Coords.Num());

	for (const FHexCoord& Coord : Coords)
	{
		FHexCellData Cell;
		Cell.Axial = Coord;
		Cell.WorldPos = FHexGeometry::HexToWorld(Coord, CellRadius);
		Cell.Height = CellHeight;
		Cell.Color = BaseColor;
		Result.Add(Cell);
	}

	return Result;
}

TArray<FHexCellData> FHexGridGenerator::GenerateRect(
	float CellRadius,
	int32 MinQ, int32 MaxQ,
	int32 MinR, int32 MaxR,
	float CellHeight,
	const FLinearColor& BaseColor
)
{
	TArray<FHexCellData> Result;
	const int32 CountQ = MaxQ - MinQ + 1;
	const int32 CountR = MaxR - MinR + 1;
	Result.Reserve(CountQ * CountR);

	for (int32 Q = MinQ; Q <= MaxQ; ++Q)
	{
		for (int32 R = MinR; R <= MaxR; ++R)
		{
			FHexCellData Cell;
			Cell.Axial = FHexCoord(Q, R);
			Cell.WorldPos = FHexGeometry::HexToWorld(Cell.Axial, CellRadius);
			Cell.Height = CellHeight;
			Cell.Color = BaseColor;
			Result.Add(Cell);
		}
	}

	return Result;
}

void FHexGridGenerator::ApplyHeightNoise(
	TArray<FHexCellData>& Cells,
	float Scale,
	float Amplitude
)
{
	for (FHexCellData& Cell : Cells)
	{
		const float Noise = FMath::PerlinNoise2D(FVector2D(
			Cell.Axial.Q * Scale,
			Cell.Axial.R * Scale
		));
		Cell.Height += Noise * Amplitude;
	}
}
