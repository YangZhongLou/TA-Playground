// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HexGeometry.h"

const FHexCoord FHexGeometry::Directions[6] = {
	FHexCoord(+1,  0),  // East
	FHexCoord(+1, -1),  // North-East
	FHexCoord( 0, -1),  // North-West
	FHexCoord(-1,  0),  // West
	FHexCoord(-1, +1),  // South-West
	FHexCoord( 0, +1),  // South-East
};

const float FHexGeometry::CornerAnglesRad[6] = {
	0.0f,                              // 0°   — East
	UE_PI / 3.0f,                      // 60°  — North-East
	2.0f * UE_PI / 3.0f,               // 120° — North-West
	UE_PI,                             // 180° — West
	4.0f * UE_PI / 3.0f,               // 240° — South-West
	5.0f * UE_PI / 3.0f,               // 300° — South-East
};

FVector FHexGeometry::HexToWorld(const FHexCoord& Hex, float CellRadius)
{
	const float X = CellRadius * UE_SQRT_3 * (Hex.Q + Hex.R * 0.5f);
	const float Y = CellRadius * 1.5f * Hex.R;
	return FVector(X, Y, 0.0f);
}

FHexCoord FHexGeometry::WorldToHex(const FVector& WorldPos, float CellRadius)
{
	const float Q = (UE_SQRT_3 / 3.0f * WorldPos.X - 1.0f / 3.0f * WorldPos.Y) / CellRadius;
	const float R = (2.0f / 3.0f * WorldPos.Y) / CellRadius;
	const float S = -Q - R;

	float Rq = FMath::RoundToFloat(Q);
	float Rr = FMath::RoundToFloat(R);
	float Rs = FMath::RoundToFloat(S);

	const float DiffQ = FMath::Abs(Rq - Q);
	const float DiffR = FMath::Abs(Rr - R);
	const float DiffS = FMath::Abs(Rs - S);

	// Cube coordinate rounding: correct the axis with the largest deviation
	if (DiffQ > DiffR && DiffQ > DiffS)
	{
		Rq = -Rr - Rs;
	}
	else if (DiffR > DiffS)
	{
		Rr = -Rq - Rs;
	}
	else
	{
		Rs = -Rq - Rr;
	}

	return FHexCoord(static_cast<int32>(Rq), static_cast<int32>(Rr));
}

FVector FHexGeometry::HexToWorldFractional(float Q, float R, float CellRadius)
{
	const float X = CellRadius * UE_SQRT_3 * (Q + R * 0.5f);
	const float Y = CellRadius * 1.5f * R;
	return FVector(X, Y, 0.0f);
}

int32 FHexGeometry::Distance(const FHexCoord& A, const FHexCoord& B)
{
	return (FMath::Abs(A.Q - B.Q) + FMath::Abs(A.Q + A.R - B.Q - B.R) + FMath::Abs(A.R - B.R)) / 2;
}

TArray<FHexCoord> FHexGeometry::GetNeighbors(const FHexCoord& Hex)
{
	TArray<FHexCoord> Neighbors;
	Neighbors.Reserve(6);
	for (int32 i = 0; i < 6; ++i)
	{
		Neighbors.Add(Hex + Directions[i]);
	}
	return Neighbors;
}

FHexCoord FHexGeometry::GetNeighbor(const FHexCoord& Hex, int32 DirectionIndex)
{
	check(DirectionIndex >= 0 && DirectionIndex < 6);
	return Hex + Directions[DirectionIndex];
}

TArray<FHexCoord> FHexGeometry::GetRing(const FHexCoord& Center, int32 Radius)
{
	TArray<FHexCoord> Result;

	if (Radius == 0)
	{
		Result.Add(Center);
		return Result;
	}

	Result.Reserve(Radius * 6);

	// Start at the hex Radius steps in direction 4 (South-West), then walk the ring
	FHexCoord Hex = Center + Directions[4] * Radius;

	for (int32 Side = 0; Side < 6; ++Side)
	{
		for (int32 Step = 0; Step < Radius; ++Step)
		{
			Result.Add(Hex);
			Hex = Hex + Directions[Side];
		}
	}

	return Result;
}

TArray<FHexCoord> FHexGeometry::GetSpiral(const FHexCoord& Center, int32 MaxRadius)
{
	TArray<FHexCoord> Result;
	Result.Reserve(1 + MaxRadius * (MaxRadius + 1) * 3);

	Result.Add(Center);

	for (int32 Radius = 1; Radius <= MaxRadius; ++Radius)
	{
		Result.Append(GetRing(Center, Radius));
	}

	return Result;
}

TArray<FHexCoord> FHexGeometry::GetLine(const FHexCoord& Start, const FHexCoord& End)
{
	const int32 Dist = Distance(Start, End);
	TArray<FHexCoord> Result;
	Result.Reserve(Dist + 1);

	for (int32 i = 0; i <= Dist; ++i)
	{
		const float T = (Dist == 0) ? 0.0f : static_cast<float>(i) / static_cast<float>(Dist);

		// Cube coordinate lerp
		const float LerpQ = FMath::Lerp(static_cast<float>(Start.Q), static_cast<float>(End.Q), T);
		const float LerpR = FMath::Lerp(static_cast<float>(Start.R), static_cast<float>(End.R), T);
		const float LerpS = FMath::Lerp(static_cast<float>(Start.S()), static_cast<float>(End.S()), T);

		// Round to nearest hex
		float Rq = FMath::RoundToFloat(LerpQ);
		float Rr = FMath::RoundToFloat(LerpR);
		float Rs = FMath::RoundToFloat(LerpS);

		const float DiffQ = FMath::Abs(Rq - LerpQ);
		const float DiffR = FMath::Abs(Rr - LerpR);
		const float DiffS = FMath::Abs(Rs - LerpS);

		if (DiffQ > DiffR && DiffQ > DiffS)
		{
			Rq = -Rr - Rs;
		}
		else if (DiffR > DiffS)
		{
			Rr = -Rq - Rs;
		}
		else
		{
			Rs = -Rq - Rr;
		}

		Result.Add(FHexCoord(static_cast<int32>(Rq), static_cast<int32>(Rr)));
	}

	return Result;
}

TArray<FVector> FHexGeometry::GetHexCorners(float Radius)
{
	TArray<FVector> Corners;
	Corners.Reserve(6);

	for (int32 i = 0; i < 6; ++i)
	{
		const float Angle = CornerAnglesRad[i];
		Corners.Add(FVector(
			Radius * FMath::Cos(Angle),
			Radius * FMath::Sin(Angle),
			0.0f
		));
	}

	return Corners;
}

TArray<FVector> FHexGeometry::GetHexCornersAt(const FVector& Center, float Radius)
{
	TArray<FVector> Corners = GetHexCorners(Radius);
	for (FVector& Corner : Corners)
	{
		Corner += Center;
	}
	return Corners;
}