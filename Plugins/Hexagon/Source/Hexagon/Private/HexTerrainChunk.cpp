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
	float UVTileSize,
	FHexCellLookup CellLookup,
	const TMap<EHexTerrainType, float>* LayerUVScales)
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
		LayerMaterials, DefaultMaterial, UVTileSize, CellLookup, LayerUVScales);
}

void UHexTerrainChunk::RebuildMeshForLOD(
	int32 LODLevel,
	float EffectiveRadius,
	float Gap,
	const FHexTerrainConfig& Config,
	const TMap<EHexTerrainType, UMaterialInterface*>* LayerMaterials,
	UMaterialInterface* DefaultMaterial,
	float UVTileSize,
	FHexCellLookup CellLookup,
	const TMap<EHexTerrainType, float>* LayerUVScales)
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
			float UVScale = 1.0f;
			if (LayerUVScales)
			{
				const float* FS = LayerUVScales->Find(Cell.TerrainType);
				if (FS) UVScale = *FS;
			}
			BuildCellMesh(CellMesh, Cell, LODLevel, EffectiveRadius, Config, CellLookup, UVScale);

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
			float UVScale = 1.0f;
			if (LayerUVScales)
			{
				const float* FS = LayerUVScales->Find(Cell.TerrainType);
				if (FS) UVScale = *FS;
			}
			BuildCellMesh(CellMesh, Cell, LODLevel, EffectiveRadius, Config, CellLookup, UVScale);

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
	const FHexTerrainConfig& Config,
	FHexCellLookup CellLookup),
	float LayerUVScale)
{
	const FColor CellColor = (DebugColor.A > 0) ? DebugColor : Cell.Color.ToFColor(true);

	// Compute per-vertex blend colors from neighbor cells when a lookup is provided
	FHexVertexColors BlendColors;
	const bool bUseBlend = (CellLookup != nullptr) && (DebugColor.A == 0);
	if (bUseBlend)
	{
		// Center vertex uses the cell's own color
		BlendColors.Center = CellColor;

		// Each corner blends the 3 cells meeting at that corner
		for (int32 i = 0; i < 6; ++i)
		{
			FHexCoord SelfCoord, NeighborA, NeighborB;
			FHexTerrainGenerator::GetCornerCells(Cell.Axial, i, SelfCoord, NeighborA, NeighborB);

			FLinearColor Sum = Cell.Color;
			int32 Count = 1;

			if (const FHexTerrainCellData* DataA = (*CellLookup)(NeighborA))
			{
				Sum += DataA->Color;
				++Count;
			}
			if (const FHexTerrainCellData* DataB = (*CellLookup)(NeighborB))
			{
				Sum += DataB->Color;
				++Count;
			}

			Sum /= static_cast<float>(Count);
			BlendColors.Corners[i] = Sum.ToFColor(true);
		}
	}

	const FHexVertexColors* VC = bUseBlend ? &BlendColors : nullptr;

	// Deterministic per-cell UV offset to break up tiling patterns
	uint32 UVHash = static_cast<uint32>(Cell.Axial.Q * 2654435761u) ^ static_cast<uint32>(Cell.Axial.R * 0x9e3779b9u);
	FVector2D UVOffset(
		(static_cast<float>(UVHash & 0xFFFF) / 65535.0f - 0.5f) * 0.3f,
		(static_cast<float>((UVHash >> 16) & 0xFFFF) / 65535.0f - 0.5f) * 0.3f
	);

	// LOD 2: flat hexagon
	if (LODLevel >= 2)
	{
		FHexPrismGenerator::GenerateFlatHexagon(OutMesh, EffectiveRadius, 0.0f, CellColor,
			Cell.WorldPos, CachedUVTileSize, VC, LayerUVScale, UVOffset);
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
			Cell.WorldPos, CachedUVTileSize, VC, LayerUVScale, UVOffset
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
			Cell.WorldPos, CachedUVTileSize, VC, LayerUVScale, UVOffset
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
	float UVTileSize,
	FHexCellLookup CellLookup,
	const TMap<EHexTerrainType, float>* LayerUVScales)
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
			LayerMaterials, DefaultMaterial, UVTileSize, CellLookup, LayerUVScales);
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

	SectionMIDs.Empty();

	for (int32 i = 0; i < GetNumSections(); ++i)
	{
		SetMaterial(i, Material);

		// Create MID for runtime parameter changes
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
		if (MID)
		{
			SetMaterial(i, MID);
			SectionMIDs.Add(i, MID);
		}
	}
}

void UHexTerrainChunk::ApplyLayerMaterials(
	const TMap<EHexTerrainType, UMaterialInterface*>& LayerMaterials,
	UMaterialInterface* DefaultMaterial)
{
	SectionMIDs.Empty();

	if (SectionTerrainTypes.Num() == 0)
	{
		if (DefaultMaterial)
		{
			SetMaterial(0, DefaultMaterial);
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(DefaultMaterial, this);
			if (MID)
			{
				SetMaterial(0, MID);
				SectionMIDs.Add(0, MID);
			}
		}
		return;
	}

	for (const auto& Pair : SectionTerrainTypes)
	{
		const int32 SectionIdx = Pair.Key;
		const EHexTerrainType Type = Pair.Value;

		UMaterialInterface* Mat = DefaultMaterial;
		if (UMaterialInterface* const* Found = LayerMaterials.Find(Type))
		{
			if (*Found) Mat = *Found;
		}

		if (!Mat) continue;

		// Create MID so texture/scalar params can be changed at runtime
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
		if (MID)
		{
			SetMaterial(SectionIdx, MID);
			SectionMIDs.Add(SectionIdx, MID);
		}
		else
		{
			SetMaterial(SectionIdx, Mat);
		}
	}
}

int32 UHexTerrainChunk::FindSectionByType(EHexTerrainType Type) const
{
	for (const auto& Pair : SectionTerrainTypes)
	{
		if (Pair.Value == Type)
		{
			return Pair.Key;
		}
	}
	return INDEX_NONE;
}

void UHexTerrainChunk::SetLayerTexture(EHexTerrainType Type, FName ParameterName, UTexture* Texture)
{
	const int32 SectionIdx = FindSectionByType(Type);
	if (SectionIdx == INDEX_NONE) return;

	UMaterialInstanceDynamic* const* Found = SectionMIDs.Find(SectionIdx);
	if (!Found || !*Found) return;

	(*Found)->SetTextureParameterValue(ParameterName, Texture);
	MarkRenderStateDirty();
}

void UHexTerrainChunk::SetLayerScalarParameter(EHexTerrainType Type, FName ParameterName, float Value)
{
	const int32 SectionIdx = FindSectionByType(Type);
	if (SectionIdx == INDEX_NONE) return;

	UMaterialInstanceDynamic* const* Found = SectionMIDs.Find(SectionIdx);
	if (!Found || !*Found) return;

	(*Found)->SetScalarParameterValue(ParameterName, Value);
	MarkRenderStateDirty();
}
