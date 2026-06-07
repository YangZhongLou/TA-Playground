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
