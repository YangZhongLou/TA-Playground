// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HexTerrainChunk.h"
#include "HexPrismGenerator.h"

UHexTerrainChunk::UHexTerrainChunk(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseAsyncCooking = false;
	bUseComplexAsSimpleCollision = true;
}

void UHexTerrainChunk::SetCells(
	const TArray<FHexTerrainCellData>& InCells,
	float EffectiveRadius,
	float Gap,
	const FHexTerrainConfig& Config,
	const TMap<EHexTerrainType, UMaterialInterface*>* LayerMaterials,
	UMaterialInterface* DefaultMaterial,
	float UVTileSize)
{
	Cells = InCells;
	CachedUVTileSize = UVTileSize;

	// Compute chunk center from cell positions
	if (Cells.Num() > 0)
	{
		CachedCenter = FVector::ZeroVector;
		for (const FHexTerrainCellData& Cell : Cells)
		{
			CachedCenter += Cell.WorldPos;
		}
		CachedCenter /= static_cast<float>(Cells.Num());
	}

	// Force full rebuild — cells have changed
	CurrentLOD = -1;
	RebuildMeshForLOD(0, EffectiveRadius, Gap, Config,
		LayerMaterials, DefaultMaterial, UVTileSize);
}

void UHexTerrainChunk::RebuildMeshForLOD(
	int32 LODLevel,
	float EffectiveRadius,
	float Gap,
	const FHexTerrainConfig& Config,
	const TMap<EHexTerrainType, UMaterialInterface*>* LayerMaterials,
	UMaterialInterface* DefaultMaterial,
	float UVTileSize)
{
	CachedUVTileSize = UVTileSize;

	if (LODLevel == CurrentLOD && SectionTerrainTypes.Num() > 0)
	{
		// Already at correct LOD — just re-apply materials if needed
		return;
	}

	// Clear all existing sections
	ClearAllMeshSections();
	SectionTerrainTypes.Empty();

	if (Cells.Num() == 0)
	{
		CurrentLOD = -1;
		return;
	}

	const bool bUsePerLayer = (LayerMaterials != nullptr && LayerMaterials->Num() > 0);

	if (bUsePerLayer)
	{
		// --- Per-layer mode: separate section per terrain type ---
		TMap<EHexTerrainType, FHexPrismMeshData> TerrainMeshes;

		for (const FHexTerrainCellData& Cell : Cells)
		{
			FHexPrismMeshData CellMesh;
			BuildCellMesh(CellMesh, Cell, LODLevel, EffectiveRadius, Config);

			FHexPrismMeshData& Target = TerrainMeshes.FindOrAdd(Cell.TerrainType);

			const int32 BaseVertex = Target.Vertices.Num();
			for (const FVector& V : CellMesh.Vertices)
			{
				Target.Vertices.Add(V + Cell.WorldPos);
			}
			Target.UVs.Append(CellMesh.UVs);
			Target.Normals.Append(CellMesh.Normals);
			Target.VertexColors.Append(CellMesh.VertexColors);
			Target.Tangents.Append(CellMesh.Tangents);

			for (int32 Tri : CellMesh.Triangles)
			{
				Target.Triangles.Add(Tri + BaseVertex);
			}
		}

		int32 SectionIdx = 0;
		for (uint8 TypeIdx = 0; TypeIdx < static_cast<uint8>(EHexTerrainType::Count); ++TypeIdx)
		{
			const EHexTerrainType Type = static_cast<EHexTerrainType>(TypeIdx);
			FHexPrismMeshData* MeshData = TerrainMeshes.Find(Type);
			if (!MeshData || MeshData->Vertices.Num() == 0)
			{
				continue;
			}

			CreateMeshSection(
				SectionIdx,
				MeshData->Vertices,
				MeshData->Triangles,
				MeshData->Normals,
				MeshData->UVs,
				MeshData->VertexColors,
				MeshData->Tangents,
				true  // bCreateCollision
			);

			// Apply material for this terrain type
			UMaterialInterface* Mat = DefaultMaterial;
			if (LayerMaterials)
			{
				if (UMaterialInterface* const* Found = LayerMaterials->Find(Type))
				{
					if (*Found) Mat = *Found;
				}
			}
			if (Mat)
			{
				SetMaterial(SectionIdx, Mat);
			}

			SectionTerrainTypes.Add(SectionIdx, Type);
			++SectionIdx;
		}
	}
	else
	{
		// --- Single-section mode ---
		FHexPrismMeshData CombinedMesh;

		for (const FHexTerrainCellData& Cell : Cells)
		{
			FHexPrismMeshData CellMesh;
			BuildCellMesh(CellMesh, Cell, LODLevel, EffectiveRadius, Config);

			const int32 BaseVertex = CombinedMesh.Vertices.Num();
			for (const FVector& V : CellMesh.Vertices)
			{
				CombinedMesh.Vertices.Add(V + Cell.WorldPos);
			}
			CombinedMesh.UVs.Append(CellMesh.UVs);
			CombinedMesh.Normals.Append(CellMesh.Normals);
			CombinedMesh.VertexColors.Append(CellMesh.VertexColors);
			CombinedMesh.Tangents.Append(CellMesh.Tangents);

			for (int32 Tri : CellMesh.Triangles)
			{
				CombinedMesh.Triangles.Add(Tri + BaseVertex);
			}
		}

		CreateMeshSection(
			0,
			CombinedMesh.Vertices,
			CombinedMesh.Triangles,
			CombinedMesh.Normals,
			CombinedMesh.UVs,
			CombinedMesh.VertexColors,
			CombinedMesh.Tangents,
			true
		);
	}

	CurrentLOD = LODLevel;
}

void UHexTerrainChunk::BuildCellMesh(
	FHexPrismMeshData& OutMesh,
	const FHexTerrainCellData& Cell,
	int32 LODLevel,
	float EffectiveRadius,
	const FHexTerrainConfig& Config)
{
	const FColor CellColor = (DebugColor.A > 0) ? DebugColor : Cell.Color.ToFColor(true);

	// LOD 2: flat hexagon
	if (LODLevel >= 2)
	{
		FHexPrismGenerator::GenerateFlatHexagon(OutMesh, EffectiveRadius, 0.0f, CellColor,
			Cell.WorldPos, CachedUVTileSize);
		const float ZOffset = Cell.Height;
		for (FVector& V : OutMesh.Vertices)
		{
			V.Z += ZOffset;
		}
		return;
	}

	// LOD 0 & 1: 3D prism
	const int32 HeightSeg = (LODLevel == 0) ? 2 : 1;
	const bool bCapBottom = (LODLevel == 0);

	if (Cell.TerrainType == EHexTerrainType::Water)
	{
		const float WaterTop = Cell.Height;
		const float WaterBottom = Cell.Height - Config.WaterThickness;
		const float WaterHeight = WaterTop - WaterBottom;

		FHexPrismGenerator::Generate(
			OutMesh, EffectiveRadius, WaterHeight,
			true, bCapBottom, HeightSeg, CellColor,
			Cell.WorldPos, CachedUVTileSize
		);

		const float ZOffset = WaterBottom + WaterHeight * 0.5f;
		for (FVector& V : OutMesh.Vertices)
		{
			V.Z += ZOffset;
		}
	}
	else
	{
		FHexPrismGenerator::Generate(
			OutMesh, EffectiveRadius, Cell.Height,
			true, bCapBottom, HeightSeg, CellColor,
			Cell.WorldPos, CachedUVTileSize
		);

		const float ZOffset = Cell.Height * 0.5f;
		for (FVector& V : OutMesh.Vertices)
		{
			V.Z += ZOffset;
		}
	}
}

bool UHexTerrainChunk::UpdateLOD(
	const FVector& CameraPosition,
	const FHexTerrainLODSettings& LODSettings,
	float EffectiveRadius,
	float Gap,
	const FHexTerrainConfig& Config,
	const TMap<EHexTerrainType, UMaterialInterface*>* LayerMaterials,
	UMaterialInterface* DefaultMaterial,
	float UVTileSize)
{
	if (Cells.Num() == 0)
	{
		return false;
	}

	const float Distance = FVector::Dist(CameraPosition, CachedCenter);

	int32 TargetLOD = 0;

	const float HystMult = 1.0f + LODSettings.Hysteresis;
	const float LOD1Hyst = LODSettings.LOD1Distance * HystMult;
	const float LOD2Hyst = LODSettings.LOD2Distance * HystMult;

	if (CurrentLOD == 0)
	{
		if (Distance > LODSettings.LOD2Distance)  TargetLOD = 2;
		else if (Distance > LODSettings.LOD1Distance) TargetLOD = 1;
	}
	else if (CurrentLOD == 1)
	{
		if (Distance > LOD2Hyst) TargetLOD = 2;
		else if (Distance < LODSettings.LOD1Distance * (1.0f - LODSettings.Hysteresis)) TargetLOD = 0;
	}
	else
	{
		if (Distance < LODSettings.LOD2Distance * (1.0f - LODSettings.Hysteresis))
		{
			TargetLOD = (Distance < LODSettings.LOD1Distance * (1.0f - LODSettings.Hysteresis)) ? 0 : 1;
		}
		else
		{
			TargetLOD = 2;
		}
	}

	if (TargetLOD != CurrentLOD)
	{
		CachedUVTileSize = UVTileSize;
		RebuildMeshForLOD(TargetLOD, EffectiveRadius, Gap, Config,
			LayerMaterials, DefaultMaterial, UVTileSize);
		return true;
	}

	return false;
}

FVector UHexTerrainChunk::GetChunkCenter() const
{
	return CachedCenter;
}

void UHexTerrainChunk::ApplySingleMaterial(UMaterialInterface* Material)
{
	if (!Material) return;
	for (int32 i = 0; i < GetNumSections(); ++i)
	{
		SetMaterial(i, Material);
	}
}

void UHexTerrainChunk::ApplyLayerMaterials(
	const TMap<EHexTerrainType, UMaterialInterface*>& LayerMaterials,
	UMaterialInterface* DefaultMaterial)
{
	if (SectionTerrainTypes.Num() == 0)
	{
		if (DefaultMaterial)
		{
			SetMaterial(0, DefaultMaterial);
		}
		return;
	}

	for (const auto& Pair : SectionTerrainTypes)
	{
		const int32 SectionIdx = Pair.Key;
		const EHexTerrainType Type = Pair.Value;

		if (UMaterialInterface* const* Found = LayerMaterials.Find(Type))
		{
			if (*Found) SetMaterial(SectionIdx, *Found);
		}
		else if (DefaultMaterial)
		{
			SetMaterial(SectionIdx, DefaultMaterial);
		}
	}
}
