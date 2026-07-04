// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HexGridGenerator.h"
#include "AHexGrid.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

/**
 * Hexagonal grid actor — renders multiple hex cells as a single merged mesh.
 *
 * Supports spiral (radius-based) and rectangular grid layouts.
 * Each cell can have an independent color via vertex colors.
 */
UCLASS(ClassGroup = (Hexagon), meta = (BlueprintSpawnableComponent))
class HEXAGON_API AHexGrid : public AActor
{
	GENERATED_BODY()

public:
	AHexGrid(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

public:
	/** Cell circumradius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Grid", meta = (ClampMin = "1.0", ClampMax = "5000.0"))
	float CellRadius = 100.0f;

	/** Number of rings around center (0 = center only, 1 = center + 1 ring, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Grid", meta = (ClampMin = "0", ClampMax = "50"))
	int32 GridRadius = 3;

	/** Height of each individual cell. Set to 0 for flat tiles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Grid", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
	float CellHeight = 10.0f;

	/** Gap between cells as a fraction of cell radius (0 = touching, 0.1 = 10% gap). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Grid", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float Gap = 0.0f;

	/** If true, each cell gets a distinct rainbow color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Grid")
	bool bRainbowColors = true;

	/** Optional material override. If null, uses engine default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Material")
	TObjectPtr<UMaterialInterface> GridMaterial;

public:
	/** Regenerate the entire grid mesh from current parameters. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Grid")
	void RegenerateMesh();

	/** Get world position of a cell by axial coordinate. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Grid")
	FVector GetCellWorldPosition(const FHexCoord& Coord) const;

	/** Convert world position to nearest axial coordinate. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Grid")
	FHexCoord GetCellFromWorldPosition(const FVector& Pos) const;

	/** Get the 6 neighbors of a cell. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Grid")
	TArray<FHexCoord> GetNeighbors(const FHexCoord& Coord) const;

	/** Get the distance between two cells in grid steps. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Grid")
	int32 GetCellDistance(const FHexCoord& A, const FHexCoord& B) const;

	/** Get all cell data (read-only, C++ only — not exposed to Blueprint). */
	const TArray<FHexCellData>& GetCellData() const { return CellData; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hexagon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProceduralMeshComponent> MeshComponent;

	/** Cached cell data for the current grid. */
	TArray<FHexCellData> CellData;

	void BuildMeshFromCellData();
	void ApplyMaterial();
};
