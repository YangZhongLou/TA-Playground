// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HexTerrainGenerator.h"
#include "HexTerrainChunk.h"
#include "AHexTerrain.generated.h"

class UMaterialInterface;
class UHexTerrainChunk;

/**
 * Procedural hexagonal terrain actor.
 * Generates a grid of hex cells with Perlin-noise height variation
 * and automatic terrain type classification (water/sand/grass/rock/snow).
 *
 * The terrain is partitioned into chunks (each CHUNK_SIZE x CHUNK_SIZE cells
 * in axial space). Each chunk owns its mesh and collision independently,
 * enabling frustum culling, LOD, and incremental rebuilds.
 *
 * Per-layer materials: assign a different material per terrain type to have
 * each chunk create separate mesh sections with independent materials.
 */
UCLASS(ClassGroup = (Hexagon), meta = (BlueprintSpawnableComponent))
class HEXAGON_API AHexTerrain : public AActor
{
	GENERATED_BODY()

public:
	AHexTerrain(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Chunk size in axial-coordinate space. Each chunk covers CHUNK_SIZE x CHUNK_SIZE hex cells. */
	static constexpr int32 CHUNK_SIZE = 16;

public:
	/** Number of rings around center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Terrain", meta = (ClampMin = "0", ClampMax = "50"))
	int32 GridRadius = 5;

	/** Cell circumradius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Terrain", meta = (ClampMin = "1.0", ClampMax = "5000.0"))
	float CellRadius = 100.0f;

	/** Gap between cells as fraction of radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Terrain", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float Gap = 0.0f;

	/** Terrain generation config. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Terrain")
	FHexTerrainConfig TerrainConfig;

	/** If true, regenerate on parameter changes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Terrain")
	bool bAutoRegenerate = true;

	/** LOD settings for chunk detail reduction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|LOD")
	FHexTerrainLODSettings LODSettings;

	/**
	 * Per-terrain-type material override.
	 * When populated, each chunk creates separate mesh sections per terrain type
	 * with the assigned material.  Types without an entry fall back to TerrainMaterial.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Material")
	TMap<EHexTerrainType, TObjectPtr<UMaterialInterface>> LayerMaterials;

	/** Default material override (used for terrain types not in LayerMaterials). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Material")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	/** When true, terrain types come from ManualCellTypes instead of noise-based classification. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Manual")
	bool bManualMode = false;

	/** Default terrain type for cells not in ManualCellTypes when bManualMode is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Manual")
	EHexTerrainType DefaultManualType = EHexTerrainType::Grass;

	/** Per-cell terrain type overrides.  Applied only when bManualMode is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Manual")
	TMap<FHexCoord, EHexTerrainType> ManualCellTypes;

	/** Brush radius in hex grid units (0 = single cell, 1 = +1 ring, ...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Brush", meta = (ClampMin = "0.0", ClampMax = "20.0"))
	float BrushRadius = 1.0f;

	/** Terrain type to apply when painting with the brush. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Brush")
	EHexTerrainType BrushTerrainType = EHexTerrainType::Grass;

	/**
	 * World-space UV tile size in world units.
	 * When > 0, all cell UVs use a world-space planar projection so textures
	 * tile seamlessly across adjacent cells.  Set to 0 to use default per-cell UVs.
	 * Typical values: 100–500 (texture repeats every N cm across the terrain).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Texture", meta = (ClampMin = "0.0"))
	float TextureTileSize = 200.0f;

public:
	/** Regenerate terrain from current parameters. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Terrain")
	void RegenerateTerrain();

	/** Get terrain cell at axial coordinate (C++ only). Returns nullptr if not found. */
	const FHexTerrainCellData* GetCell(const FHexCoord& Coord) const;

	/** Get terrain type at world position. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Terrain")
	EHexTerrainType GetTerrainTypeAtPosition(const FVector& Pos) const;

	/** Get all generated cell data (C++ only). */
	const TArray<FHexTerrainCellData>& GetTerrainCells() const { return TerrainCells; }

	/** Get number of active chunks. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Terrain")
	int32 GetChunkCount() const { return ChunkMap.Num(); }

	/** When true, each chunk is tinted a distinct color to visualize chunk boundaries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Debug")
	bool bDebugChunkColors = false;

	/** Set material for a specific terrain type (runtime). */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Material")
	void SetLayerMaterial(EHexTerrainType Type, UMaterialInterface* Material);

	/** Print terrain statistics to log (cell count, chunk count, LOD distribution). */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Debug")
	void PrintStats() const;

	/**
	 * Incremental paint: change a set of cells to a new terrain type.
	 * Only rebuilds the chunks that contain the affected cells.
	 * Automatically enables bManualMode.
	 */
	void PaintCells(const TArray<FHexCoord>& Coords, EHexTerrainType Type);

	/**
	 * Rebuild only the specified chunks (by chunk coordinate).
	 */
	void RebuildDirtyChunks(const TSet<FIntPoint>& DirtyChunkCoords);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hexagon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	/** All generated cell data (flat list). */
	TArray<FHexTerrainCellData> TerrainCells;

	/** Map from chunk coordinate -> chunk component. */
	UPROPERTY()
	TMap<FIntPoint, TObjectPtr<UHexTerrainChunk>> ChunkMap;

	/** Cached effective radius (recalculated on regenerate). */
	float CachedEffectiveRadius = 100.0f;

	/** Cached config used for last build (for LOD rebuilds). */
	FHexTerrainConfig CachedConfig;

	/** Distribute cells to chunks and rebuild each. */
	void BuildAllChunks(const FHexTerrainConfig& Config);

	/** Remove chunks that are no longer needed. */
	void PruneUnusedChunks(const TSet<FIntPoint>& ActiveChunkCoords);

	/** Apply materials to all chunks based on current LayerMaterials / TerrainMaterial. */
	void ApplyMaterialsToAllChunks();

	/** Update LOD on all chunks based on camera distance. */
	void UpdateChunkLODs();
};
