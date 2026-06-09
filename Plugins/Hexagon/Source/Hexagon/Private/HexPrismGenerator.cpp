// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HexPrismGenerator.h"

void FHexPrismGenerator::Generate(
	FHexPrismMeshData& OutData,
	float Radius,
	float Height,
	bool bCapTop,
	bool bCapBottom,
	int32 HeightSegments,
	FColor BaseColor,
	FVector WorldOffset,
	float UVTileSize
)
{
	OutData.Reset();

	// Helper: compute UV — world-space planar projection when TileSize > 0,
	// otherwise use the provided default UV.
	auto MakeUV = [&](float LocalX, float LocalY, float LocalZ, const FVector2D& DefaultUV) -> FVector2D
	{
		if (UVTileSize > 0.0f)
		{
			return FVector2D(
				(LocalX + WorldOffset.X) / UVTileSize,
				(LocalY + WorldOffset.Y) / UVTileSize
			);
		}
		return DefaultUV;
	};

	HeightSegments = FMath::Max(1, HeightSegments);
	const float HalfHeight = Height * 0.5f;
	const TArray<FVector> Corners = FHexGeometry::GetHexCorners(Radius);

	// --- Bottom cap: center + ring ---
	const int32 BottomCenterIndex = OutData.Vertices.Num();
	OutData.Vertices.Add(FVector(0.0f, 0.0f, -HalfHeight));
	OutData.UVs.Add(MakeUV(0.0f, 0.0f, -HalfHeight, FVector2D(0.5f, 0.5f)));
	OutData.Normals.Add(FVector::DownVector);
	OutData.VertexColors.Add(BaseColor);
	OutData.Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));

	TArray<int32> BottomCapRingIndices;
	for (int32 i = 0; i < 6; ++i)
	{
		const FVector& Corner = Corners[i];
		BottomCapRingIndices.Add(OutData.Vertices.Num());
		OutData.Vertices.Add(FVector(Corner.X, Corner.Y, -HalfHeight));

		const float Angle = FHexGeometry::CornerAnglesRad[i];
		const FVector2D DefaultUV(
			0.5f + 0.5f * FMath::Cos(Angle),
			0.5f + 0.5f * FMath::Sin(Angle)
		);
		OutData.UVs.Add(MakeUV(Corner.X, Corner.Y, -HalfHeight, DefaultUV));
		OutData.Normals.Add(FVector::DownVector);
		OutData.VertexColors.Add(BaseColor);
		OutData.Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
	}

	// --- Side faces ---
	for (int32 Side = 0; Side < 6; ++Side)
	{
		const int32 NextSide = (Side + 1) % 6;
		const FVector SideNormal = (Corners[Side] + Corners[NextSide]).GetSafeNormal();

		for (int32 Seg = 0; Seg < HeightSegments; ++Seg)
		{
			const float BottomAlpha = static_cast<float>(Seg) / static_cast<float>(HeightSegments);
			const float TopAlpha    = static_cast<float>(Seg + 1) / static_cast<float>(HeightSegments);
			const float BottomZ = FMath::Lerp(-HalfHeight, HalfHeight, BottomAlpha);
			const float TopZ    = FMath::Lerp(-HalfHeight, HalfHeight, TopAlpha);

			// Bottom-left
			const int32 BL = OutData.Vertices.Num();
			OutData.Vertices.Add(FVector(Corners[Side].X, Corners[Side].Y, BottomZ));
			OutData.UVs.Add(MakeUV(Corners[Side].X, Corners[Side].Y, BottomZ,
				FVector2D(static_cast<float>(Side) / 6.0f, BottomAlpha)));
			OutData.Normals.Add(SideNormal);
			OutData.VertexColors.Add(BaseColor);
			OutData.Tangents.Add(FProcMeshTangent(0.0f, 0.0f, 0.0f));

			// Bottom-right
			const int32 BR = OutData.Vertices.Num();
			OutData.Vertices.Add(FVector(Corners[NextSide].X, Corners[NextSide].Y, BottomZ));
			OutData.UVs.Add(MakeUV(Corners[NextSide].X, Corners[NextSide].Y, BottomZ,
				FVector2D(static_cast<float>(Side + 1) / 6.0f, BottomAlpha)));
			OutData.Normals.Add(SideNormal);
			OutData.VertexColors.Add(BaseColor);
			OutData.Tangents.Add(FProcMeshTangent(0.0f, 0.0f, 0.0f));

			// Top-right
			const int32 TR = OutData.Vertices.Num();
			OutData.Vertices.Add(FVector(Corners[NextSide].X, Corners[NextSide].Y, TopZ));
			OutData.UVs.Add(MakeUV(Corners[NextSide].X, Corners[NextSide].Y, TopZ,
				FVector2D(static_cast<float>(Side + 1) / 6.0f, TopAlpha)));
			OutData.Normals.Add(SideNormal);
			OutData.VertexColors.Add(BaseColor);
			OutData.Tangents.Add(FProcMeshTangent(0.0f, 0.0f, 0.0f));

			// Top-left
			const int32 TL = OutData.Vertices.Num();
			OutData.Vertices.Add(FVector(Corners[Side].X, Corners[Side].Y, TopZ));
			OutData.UVs.Add(MakeUV(Corners[Side].X, Corners[Side].Y, TopZ,
				FVector2D(static_cast<float>(Side) / 6.0f, TopAlpha)));
			OutData.Normals.Add(SideNormal);
			OutData.VertexColors.Add(BaseColor);
			OutData.Tangents.Add(FProcMeshTangent(0.0f, 0.0f, 0.0f));

			AddQuad(OutData, BL, TL, TR, BR);
		}
	}

	// --- Top cap: center + ring ---
	const int32 TopCenterIndex = OutData.Vertices.Num();
	OutData.Vertices.Add(FVector(0.0f, 0.0f, HalfHeight));
	OutData.UVs.Add(MakeUV(0.0f, 0.0f, HalfHeight, FVector2D(0.5f, 0.5f)));
	OutData.Normals.Add(FVector::UpVector);
	OutData.VertexColors.Add(BaseColor);
	OutData.Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));

	TArray<int32> TopCapRingIndices;
	for (int32 i = 0; i < 6; ++i)
	{
		const FVector& Corner = Corners[i];
		TopCapRingIndices.Add(OutData.Vertices.Num());
		OutData.Vertices.Add(FVector(Corner.X, Corner.Y, HalfHeight));

		const float Angle = FHexGeometry::CornerAnglesRad[i];
		const FVector2D DefaultUV(
			0.5f + 0.5f * FMath::Cos(Angle),
			0.5f + 0.5f * FMath::Sin(Angle)
		);
		OutData.UVs.Add(MakeUV(Corner.X, Corner.Y, HalfHeight, DefaultUV));
		OutData.Normals.Add(FVector::UpVector);
		OutData.VertexColors.Add(BaseColor);
		OutData.Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
	}

	// --- Triangles ---

	// Bottom cap
	if (bCapBottom)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			const int32 Next = (i + 1) % 6;
			AddTriangle(OutData, BottomCenterIndex, BottomCapRingIndices[i], BottomCapRingIndices[Next]);
		}
	}

	// Top cap
	if (bCapTop)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			const int32 Next = (i + 1) % 6;
			AddTriangle(OutData, TopCenterIndex, TopCapRingIndices[Next], TopCapRingIndices[i]);
		}
	}

	CalculateNormals(OutData);
	CalculateTangents(OutData);
}

void FHexPrismGenerator::GenerateFlatHexagon(
	FHexPrismMeshData& OutData,
	float Radius,
	float Height,
	FColor BaseColor,
	FVector WorldOffset,
	float UVTileSize
)
{
	auto MakeUV = [&](float LocalX, float LocalY, float LocalZ, const FVector2D& DefaultUV) -> FVector2D
	{
		if (UVTileSize > 0.0f)
		{
			return FVector2D(
				(LocalX + WorldOffset.X) / UVTileSize,
				(LocalY + WorldOffset.Y) / UVTileSize
			);
		}
		return DefaultUV;
	};

	if (Height <= KINDA_SMALL_NUMBER)
	{
		OutData.Reset();

		const TArray<FVector> Corners = FHexGeometry::GetHexCorners(Radius);

		OutData.Vertices.Add(FVector::ZeroVector);
		OutData.UVs.Add(MakeUV(0.0f, 0.0f, 0.0f, FVector2D(0.5f, 0.5f)));
		OutData.Normals.Add(FVector::UpVector);
		OutData.VertexColors.Add(BaseColor);
		OutData.Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));

		TArray<int32> RingIndices;
		for (int32 i = 0; i < 6; ++i)
		{
			RingIndices.Add(OutData.Vertices.Num());
			OutData.Vertices.Add(Corners[i]);

			const float Angle = FHexGeometry::CornerAnglesRad[i];
			const FVector2D DefaultUV(
				0.5f + 0.5f * FMath::Cos(Angle),
				0.5f + 0.5f * FMath::Sin(Angle)
			);
			OutData.UVs.Add(MakeUV(Corners[i].X, Corners[i].Y, 0.0f, DefaultUV));
			OutData.Normals.Add(FVector::UpVector);
			OutData.VertexColors.Add(BaseColor);
			OutData.Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
		}

		for (int32 i = 0; i < 6; ++i)
		{
			const int32 Next = (i + 1) % 6;
			AddTriangle(OutData, 0, RingIndices[i], RingIndices[Next]);
		}
	}
	else
	{
		Generate(OutData, Radius, Height, true, true, 1, BaseColor, WorldOffset, UVTileSize);
	}
}

void FHexPrismGenerator::AddTriangle(FHexPrismMeshData& OutData, int32 A, int32 B, int32 C)
{
	OutData.Triangles.Add(A);
	OutData.Triangles.Add(B);
	OutData.Triangles.Add(C);
}

void FHexPrismGenerator::AddQuad(FHexPrismMeshData& OutData, int32 A, int32 B, int32 C, int32 D)
{
	OutData.Triangles.Add(A);
	OutData.Triangles.Add(B);
	OutData.Triangles.Add(C);

	OutData.Triangles.Add(A);
	OutData.Triangles.Add(C);
	OutData.Triangles.Add(D);
}

void FHexPrismGenerator::CalculateNormals(FHexPrismMeshData& OutData)
{
	for (int32 i = 0; i < OutData.Triangles.Num(); i += 3)
	{
		const int32 I0 = OutData.Triangles[i];
		const int32 I1 = OutData.Triangles[i + 1];
		const int32 I2 = OutData.Triangles[i + 2];

		const FVector& V0 = OutData.Vertices[I0];
		const FVector& V1 = OutData.Vertices[I1];
		const FVector& V2 = OutData.Vertices[I2];

		const FVector Edge1 = V1 - V0;
		const FVector Edge2 = V2 - V0;
		FVector FaceNormal = FVector::CrossProduct(Edge1, Edge2);

		if (!FaceNormal.IsNearlyZero())
		{
			FaceNormal.Normalize();
			if (OutData.Normals[I0].IsNearlyZero()) OutData.Normals[I0] += FaceNormal;
			if (OutData.Normals[I1].IsNearlyZero()) OutData.Normals[I1] += FaceNormal;
			if (OutData.Normals[I2].IsNearlyZero()) OutData.Normals[I2] += FaceNormal;
		}
	}

	for (FVector& Normal : OutData.Normals)
	{
		if (Normal.IsNearlyZero())
			Normal = FVector::UpVector;
		else
			Normal.Normalize();
	}
}

void FHexPrismGenerator::CalculateTangents(FHexPrismMeshData& OutData)
{
	for (int32 i = 0; i < OutData.Triangles.Num(); i += 3)
	{
		const int32 I0 = OutData.Triangles[i];
		const int32 I1 = OutData.Triangles[i + 1];
		const int32 I2 = OutData.Triangles[i + 2];

		const FVector& V0 = OutData.Vertices[I0];
		const FVector& V1 = OutData.Vertices[I1];
		const FVector& V2 = OutData.Vertices[I2];

		const FVector2D& UV0 = OutData.UVs[I0];
		const FVector2D& UV1 = OutData.UVs[I1];
		const FVector2D& UV2 = OutData.UVs[I2];

		const FVector Edge1 = V1 - V0;
		const FVector Edge2 = V2 - V0;

		const FVector2D DUV1 = UV1 - UV0;
		const FVector2D DUV2 = UV2 - UV0;

		const float Denom = DUV1.X * DUV2.Y - DUV1.Y * DUV2.X;
		if (FMath::Abs(Denom) > KINDA_SMALL_NUMBER)
		{
			const float R = 1.0f / Denom;
			const FVector Tangent = (Edge1 * DUV2.Y - Edge2 * DUV1.Y) * R;

			if (OutData.Tangents[I0].TangentX.IsNearlyZero())
				OutData.Tangents[I0] = FProcMeshTangent(Tangent.GetSafeNormal(), false);
			if (OutData.Tangents[I1].TangentX.IsNearlyZero())
				OutData.Tangents[I1] = FProcMeshTangent(Tangent.GetSafeNormal(), false);
			if (OutData.Tangents[I2].TangentX.IsNearlyZero())
				OutData.Tangents[I2] = FProcMeshTangent(Tangent.GetSafeNormal(), false);
		}
	}
}
