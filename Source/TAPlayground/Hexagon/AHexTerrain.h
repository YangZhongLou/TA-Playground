// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HexTerrainGenerator.h"
#include "AHexTerrain.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

/**
 * Procedural hexagonal terrain actor.
 * Generates a grid of hex cells with Perlin-noise height variation
 * and automatic terrain type classification (water/sand/grass/rock/snow).
 */
UCLASS(ClassGroup = (Hexagon), meta = (BlueprintSpawnableComponent))
class TAPLAYGROUND_API AHexTerrain : public AActor
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

	/** Optional material override. */
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

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hexagon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProceduralMeshComponent> MeshComponent;

	TArray<FHexTerrainCellData> TerrainCells;

	void BuildTerrainMesh();
	void ApplyMaterial();
};
