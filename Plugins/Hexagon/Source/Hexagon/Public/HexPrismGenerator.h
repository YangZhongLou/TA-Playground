// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HexGeometry.h"
#include "ProceduralMeshComponent.h"

/**
 * Per-vertex color override for a hexagonal prism.
 * Center + 6 corners (counterclockwise from east/angle-0°).
 */
struct HEXAGON_API FHexVertexColors
{
	FColor Center;
	FColor Corners[6];

	FHexVertexColors()
		: Center(FColor::White)
	{
		for (int32 i = 0; i < 6; ++i) Corners[i] = FColor::White;
	}

	static FHexVertexColors Uniform(FColor C)
	{
		FHexVertexColors VC;
		VC.Center = C;
		for (int32 i = 0; i < 6; ++i) VC.Corners[i] = C;
		return VC;
	}

	bool IsUniform() const
	{
		for (int32 i = 0; i < 6; ++i)
			if (Corners[i] != Center) return false;
		return true;
	}
};

/**
 * Generates mesh data for a hexagonal prism (pointy-topped).
 * Outputs raw vertex/triangle/UV/normal data that can be fed into any mesh component.
 */
struct HEXAGON_API FHexPrismMeshData
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FVector> Normals;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	void Reset()
	{
		Vertices.Reset();
		Triangles.Reset();
		UVs.Reset();
		Normals.Reset();
		VertexColors.Reset();
		Tangents.Reset();
	}
};

/**
 * Procedural mesh generator for hexagonal prisms.
 */
class HEXAGON_API FHexPrismGenerator
{
public:
	/**
	 * Generate mesh data for a hexagonal prism.
	 *
	 * @param OutData      Output mesh data.
	 * @param Radius       Circumradius (distance from center to any vertex).
	 * @param Height       Total height of the prism.
	 * @param bCapTop      Whether to generate the top face.
	 * @param bCapBottom   Whether to generate the bottom face.
	 * @param HeightSegments Number of vertical subdivisions per side face (>= 1).
	 * @param BaseColor    Optional vertex color tint.
	 */
	/**
	 * @param UVScale Multiplier applied to UV coordinates (1.0 = default tiling).
	 *        Higher values = more tiling (smaller texture repeats).
	 * @param UVOffset Random offset added to UVs to break up tiling patterns.
	 * @param VertexColors Optional per-vertex color overrides. When provided, corner/center
	 *        vertices use these colors instead of BaseColor. Side-face vertices blend between
	 *        adjacent corners.
	 */
	static void Generate(
		FHexPrismMeshData& OutData,
		float Radius,
		float Height,
		bool bCapTop = true,
		bool bCapBottom = true,
		int32 HeightSegments = 1,
		FColor BaseColor = FColor::White,
		FVector WorldOffset = FVector::ZeroVector,
		float UVTileSize = 0.0f,
		const FHexVertexColors* VertexColors = nullptr,
		float UVScale = 1.0f,
		FVector2D UVOffset = FVector2D::ZeroVector
	);

	/**
	 * Generate a flat hexagon (2D, no side faces, only top/bottom caps).
	 * Height controls the thickness; set to 0 for a true 2D plane.
	 */
	static void GenerateFlatHexagon(
		FHexPrismMeshData& OutData,
		float Radius,
		float Height = 0.0f,
		FColor BaseColor = FColor::White,
		FVector WorldOffset = FVector::ZeroVector,
		float UVTileSize = 0.0f,
		const FHexVertexColors* VertexColors = nullptr,
		float UVScale = 1.0f,
		FVector2D UVOffset = FVector2D::ZeroVector
	);

private:
	static void AddTriangle(FHexPrismMeshData& OutData, int32 A, int32 B, int32 C);
	static void AddQuad(FHexPrismMeshData& OutData, int32 A, int32 B, int32 C, int32 D);
	static void CalculateNormals(FHexPrismMeshData& OutData);
	static void CalculateTangents(FHexPrismMeshData& OutData);
};