// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "HexTerrainGenerator.h"
#include "HexPrismGenerator.h"
#include "HexTerrainChunk.generated.h"

/**
 * LOD settings for hex terrain chunks.
 */
USTRUCT(BlueprintType)
struct FHexTerrainLODSettings
{
	GENERATED_BODY()

	/** Distance at which LOD 1 begins (reduced side detail). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD")
	float LOD1Distance = 2000.0f;

	/** Distance at which LOD 2 begins (flat hex tiles). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD")
	float LOD2Distance = 5000.0f;

	/** Height segments for LOD 0 (near). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD", meta = (ClampMin = "1", ClampMax = "4"))
	int32 LOD0HeightSegments = 2;

	/** Height segments for LOD 1 (mid). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD", meta = (ClampMin = "1", ClampMax = "4"))
	int32 LOD1HeightSegments = 1;

	/** Hysteresis as fraction of distance — prevents LOD flickering at boundaries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LOD", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float Hysteresis = 0.1f;
};

/**
 * A single chunk of the hex terrain, responsible for CHUNK_SIZE × CHUNK_SIZE hex cells.
 * Inherits from UProceduralMeshComponent directly — the chunk IS its mesh section(s).
 *
 * Each chunk independently builds its mesh, manages its collision, and tracks
 * its LOD level based on distance to camera.  When per-layer materials are provided,
 * cells are split into separate mesh sections by terrain type.
 */
UCLASS(ClassGroup = (Hexagon), meta = (BlueprintSpawnableComponent))
class HEXAGON_API UHexTerrainChunk : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	UHexTerrainChunk(const FObjectInitializer& ObjectInitializer);

	/** Chunk coordinate in chunk-space (not hex axial space). */
	FIntPoint GetChunkCoord() const { return ChunkCoord; }
	void SetChunkCoord(FIntPoint InCoord) { ChunkCoord = InCoord; }

	/** Number of cells in this chunk. */
	int32 GetCellCount() const { return Cells.Num(); }

	/** Whether this chunk has no cells (should be destroyed). */
	bool IsEmpty() const { return Cells.Num() == 0; }

	/** Current LOD level (0 = full, 1 = reduced, 2 = flat). */
	int32 GetCurrentLOD() const { return CurrentLOD; }

	/**
	 * Replace all cells in this chunk and rebuild the mesh at LOD 0.
	 * @param LayerMaterials Optional per-terrain-type materials. When non-null and non-empty,
	 *        separate mesh sections are created per terrain type from the start.
	 * @param DefaultMaterial Fallback material for terrain types not in LayerMaterials.
	 * @param UVTileSize World-space UV tile size (0 = default local-space UVs).
	 */
	void SetCells(
		const TArray<FHexTerrainCellData>& InCells,
		float EffectiveRadius,
		float Gap,
		const FHexTerrainConfig& Config,
		const TMap<EHexTerrainType, UMaterialInterface*>* LayerMaterials = nullptr,
		UMaterialInterface* DefaultMaterial = nullptr,
		float UVTileSize = 0.0f
	);

	/**
	 * Rebuild mesh for a specific LOD level.
	 * @param LODLevel 0 = full detail, 1 = reduced side faces, 2 = flat hexagons.
	 * @param LayerMaterials Optional per-terrain-type materials. If non-empty, separate
	 *        mesh sections are created for each terrain type present in this chunk.
	 * @param DefaultMaterial Fallback material for terrain types not in LayerMaterials.
	 * @param UVTileSize World-space UV tile size (0 = use default local-space UVs).
	 */
	void RebuildMeshForLOD(
		int32 LODLevel,
		float EffectiveRadius,
		float Gap,
		const FHexTerrainConfig& Config,
		const TMap<EHexTerrainType, UMaterialInterface*>* LayerMaterials = nullptr,
		UMaterialInterface* DefaultMaterial = nullptr,
		float UVTileSize = 0.0f
	);

	/**
	 * Update LOD based on camera distance. Returns true if LOD changed.
	 * Pass LayerMaterials + DefaultMaterial to apply per-layer materials after rebuild.
	 */
	bool UpdateLOD(
		const FVector& CameraPosition,
		const FHexTerrainLODSettings& LODSettings,
		float EffectiveRadius,
		float Gap,
		const FHexTerrainConfig& Config,
		const TMap<EHexTerrainType, UMaterialInterface*>* LayerMaterials = nullptr,
		UMaterialInterface* DefaultMaterial = nullptr,
		float UVTileSize = 0.0f
	);

	/** Apply a single material to ALL sections (clears per-layer layout). */
	void ApplySingleMaterial(UMaterialInterface* Material);

	/** Apply per-layer materials based on terrain type map. */
	void ApplyLayerMaterials(
		const TMap<EHexTerrainType, UMaterialInterface*>& LayerMaterials,
		UMaterialInterface* DefaultMaterial
	);

	/** Override vertex color for all cells in this chunk (debug visualization). */
	void SetDebugColor(FColor InColor) { DebugColor = InColor; }
	void ClearDebugColor() { DebugColor = FColor::Transparent; }
	bool HasDebugColor() const { return DebugColor.A > 0; }

	/** Find section index for a terrain type, or INDEX_NONE. */
	int32 FindSectionByType(EHexTerrainType Type) const {
		for (const auto& Pair : SectionTerrainTypes) {
			if (Pair.Value == Type) return Pair.Key;
		}
		return INDEX_NONE;
	}

	/** Set a texture parameter on the material for a terrain type's section. */
	void SetLayerTexture(EHexTerrainType Type, FName ParameterName, UTexture* Texture);

	/** Set a scalar parameter on the material for a terrain type's section. */
	void SetLayerScalarParameter(EHexTerrainType Type, FName ParameterName, float Value);

	/** Get the world-space center of this chunk's bounding box. */
	FVector GetChunkCenter() const;

private:
	/** Chunk coordinate in chunk-space (not hex axial space). */
	FIntPoint ChunkCoord;

	/** Cells assigned to this chunk. */
	TArray<FHexTerrainCellData> Cells;

	/** Debug color override — when A > 0, all cells use this color. */
	FColor DebugColor = FColor::Transparent;

	/** Current LOD level. -1 = never built. */
	int32 CurrentLOD = -1;

	/** Cached chunk center in world space (for distance calc). */
	FVector CachedCenter = FVector::ZeroVector;

	/** Map from section index to terrain type (populated when per-layer materials used). */
	TMap<int32, EHexTerrainType> SectionTerrainTypes;

	/** Cached texture tile size for world-space UVs (0 = use default UVs). */
	float CachedUVTileSize = 0.0f;

	/** Build a single cell mesh for a given LOD. */
	void BuildCellMesh(
		FHexPrismMeshData& OutMesh,
		const FHexTerrainCellData& Cell,
		int32 LODLevel,
		float EffectiveRadius,
		const FHexTerrainConfig& Config
	);
};
