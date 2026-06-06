// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HexPrismGenerator.h"

void FHexPrismGenerator::Generate(
	FHexPrismMeshData& OutData,
	float Radius,
	float Height,
	bool bCapTop,
	bool bCapBottom,
	int32 HeightSegments,
	FColor BaseColor
)
{
	OutData.Reset();

	HeightSegments = FMath::Max(1, HeightSegments);
	const float HalfHeight = Height * 0.5f;
	const TArray<FVector> Corners = FHexGeometry::GetHexCorners(Radius);

	// --- Bottom cap: center + ring (independent vertices, not shared with sides) ---
	const int32 BottomCenterIndex = OutData.Vertices.Num();
	OutData.Vertices.Add(FVector(0.0f, 0.0f, -HalfHeight));
	OutData.UVs.Add(FVector2D(0.5f, 0.5f));
	OutData.Normals.Add(FVector::DownVector);
	OutData.VertexColors.Add(BaseColor);
	OutData.Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));

	TArray<int32> BottomCapRingIndices;
	for (int32 i = 0; i < 6; ++i)
	{
		const FVector& Corner = Corners[i];
		BottomCapRingIndices.Add(OutData.Vertices.Num());
		OutData.Vertices.Add(FVector(Corner.X, Corner.Y, -HalfHeight));

		// Radial UV for cap faces
		const float Angle = FHexGeometry::CornerAnglesRad[i];
		OutData.UVs.Add(FVector2D(
			0.5f + 0.5f * FMath::Cos(Angle),
			0.5f + 0.5f * FMath::Sin(Angle)
		));
		OutData.Normals.Add(FVector::DownVector);
		OutData.VertexColors.Add(BaseColor);
		OutData.Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
	}

	// --- Side faces: each quad uses 4 independent vertices ---
	// UV layout: U wraps around the cylinder [0, 1], V goes up [0, 1]
	// Side 5 right edge U = 1.0 for seamless texture wrapping
	for (int32 Side = 0; Side < 6; ++Side)
	{
		const int32 NextSide = (Side + 1) % 6;

		for (int32 Seg = 0; Seg < HeightSegments; ++Seg)
		{
			const float BottomAlpha = static_cast<float>(Seg) / static_cast<float>(HeightSegments);
			const float TopAlpha    = static_cast<float>(Seg + 1) / static_cast<float>(HeightSegments);
			const float BottomZ = FMath::Lerp(-HalfHeight, HalfHeight, BottomAlpha);
			const float TopZ    = FMath::Lerp(-HalfHeight, HalfHeight, TopAlpha);

			// Bottom-left (Side corner, bottom height)
			const int32 BL = OutData.Vertices.Num();
			OutData.Vertices.Add(FVector(Corners[Side].X, Corners[Side].Y, BottomZ));
			OutData.UVs.Add(FVector2D(static_cast<float>(Side) / 6.0f, BottomAlpha));
			OutData.Normals.Add(FVector::ZeroVector);      // calculated by CalculateNormals
			OutData.VertexColors.Add(BaseColor);
			OutData.Tangents.Add(FProcMeshTangent(0.0f, 0.0f, 0.0f));

			// Bottom-right (NextSide corner, bottom height)
			const int32 BR = OutData.Vertices.Num();
			OutData.Vertices.Add(FVector(Corners[NextSide].X, Corners[NextSide].Y, BottomZ));
			OutData.UVs.Add(FVector2D(static_cast<float>(Side + 1) / 6.0f, BottomAlpha));
			OutData.Normals.Add(FVector::ZeroVector);
			OutData.VertexColors.Add(BaseColor);
			OutData.Tangents.Add(FProcMeshTangent(0.0f, 0.0f, 0.0f));

			// Top-right (NextSide corner, top height)
			const int32 TR = OutData.Vertices.Num();
			OutData.Vertices.Add(FVector(Corners[NextSide].X, Corners[NextSide].Y, TopZ));
			OutData.UVs.Add(FVector2D(static_cast<float>(Side + 1) / 6.0f, TopAlpha));
			OutData.Normals.Add(FVector::ZeroVector);
			OutData.VertexColors.Add(BaseColor);
			OutData.Tangents.Add(FProcMeshTangent(0.0f, 0.0f, 0.0f));

			// Top-left (Side corner, top height)
			const int32 TL = OutData.Vertices.Num();
			OutData.Vertices.Add(FVector(Corners[Side].X, Corners[Side].Y, TopZ));
			OutData.UVs.Add(FVector2D(static_cast<float>(Side) / 6.0f, TopAlpha));
			OutData.Normals.Add(FVector::ZeroVector);
			OutData.VertexColors.Add(BaseColor);
			OutData.Tangents.Add(FProcMeshTangent(0.0f, 0.0f, 0.0f));

			AddQuad(OutData, BL, BR, TR, TL);
		}
	}

	// --- Top cap: center + ring (independent vertices, not shared with sides) ---
	const int32 TopCenterIndex = OutData.Vertices.Num();
	OutData.Vertices.Add(FVector(0.0f, 0.0f, HalfHeight));
	OutData.UVs.Add(FVector2D(0.5f, 0.5f));
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
		OutData.UVs.Add(FVector2D(
			0.5f + 0.5f * FMath::Cos(Angle),
			0.5f + 0.5f * FMath::Sin(Angle)
		));
		OutData.Normals.Add(FVector::UpVector);
		OutData.VertexColors.Add(BaseColor);
		OutData.Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));
	}

	// --- Triangles ---

	// Bottom cap (counter-clockwise when viewed from below)
	if (bCapBottom)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			const int32 Next = (i + 1) % 6;
			AddTriangle(OutData, BottomCenterIndex, BottomCapRingIndices[Next], BottomCapRingIndices[i]);
		}
	}

	// Top cap (counter-clockwise when viewed from above)
	if (bCapTop)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			const int32 Next = (i + 1) % 6;
			AddTriangle(OutData, TopCenterIndex, TopCapRingIndices[i], TopCapRingIndices[Next]);
		}
	}

	// Recalculate side face normals and tangents
	// Cap normals remain fixed at +/-Z since their vertices are not shared with sides
	CalculateNormals(OutData);
	CalculateTangents(OutData);
}

void FHexPrismGenerator::GenerateFlatHexagon(
	FHexPrismMeshData& OutData,
	float Radius,
	float Height,
	FColor BaseColor
)
{
	if (Height <= KINDA_SMALL_NUMBER)
	{
		// True flat plane (single face)
		OutData.Reset();

		const TArray<FVector> Corners = FHexGeometry::GetHexCorners(Radius);

		// Center vertex
		OutData.Vertices.Add(FVector::ZeroVector);
		OutData.UVs.Add(FVector2D(0.5f, 0.5f));
		OutData.Normals.Add(FVector::UpVector);
		OutData.VertexColors.Add(BaseColor);
		OutData.Tangents.Add(FProcMeshTangent(1.0f, 0.0f, 0.0f));

		TArray<int32> RingIndices;
		for (int32 i = 0; i < 6; ++i)
		{
			RingIndices.Add(OutData.Vertices.Num());
			OutData.Vertices.Add(Corners[i]);

			const float Angle = FHexGeometry::CornerAnglesRad[i];
			OutData.UVs.Add(FVector2D(
				0.5f + 0.5f * FMath::Cos(Angle),
				0.5f + 0.5f * FMath::Sin(Angle)
			));
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
		// Thin prism (both caps + sides)
		Generate(OutData, Radius, Height, true, true, 1, BaseColor);
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
	// Triangle 1: A-B-C
	OutData.Triangles.Add(A);
	OutData.Triangles.Add(B);
	OutData.Triangles.Add(C);

	// Triangle 2: A-C-D
	OutData.Triangles.Add(A);
	OutData.Triangles.Add(C);
	OutData.Triangles.Add(D);
}

void FHexPrismGenerator::CalculateNormals(FHexPrismMeshData& OutData)
{
	// Accumulate face normals per vertex, but skip vertices that already have a fixed normal
	// (cap vertices have +/-Z and should not be averaged)
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

			// Only accumulate to vertices with zero normals (side faces)
			if (OutData.Normals[I0].IsNearlyZero()) OutData.Normals[I0] += FaceNormal;
			if (OutData.Normals[I1].IsNearlyZero()) OutData.Normals[I1] += FaceNormal;
			if (OutData.Normals[I2].IsNearlyZero()) OutData.Normals[I2] += FaceNormal;
		}
	}

	// Normalize only the zero-initialized normals (side faces)
	for (FVector& Normal : OutData.Normals)
	{
		if (Normal.IsNearlyZero())
		{
			Normal = FVector::UpVector;
		}
		else
		{
			Normal.Normalize();
		}
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

			// Only update tangents that are still default (side faces)
			if (OutData.Tangents[I0].TangentX.IsNearlyZero())
				OutData.Tangents[I0] = FProcMeshTangent(Tangent.GetSafeNormal(), false);
			if (OutData.Tangents[I1].TangentX.IsNearlyZero())
				OutData.Tangents[I1] = FProcMeshTangent(Tangent.GetSafeNormal(), false);
			if (OutData.Tangents[I2].TangentX.IsNearlyZero())
				OutData.Tangents[I2] = FProcMeshTangent(Tangent.GetSafeNormal(), false);
		}
	}
}
