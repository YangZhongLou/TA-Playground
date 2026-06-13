// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HexDataLayer.h"
#include "HexDataLayerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHexCellAdded, const FHexCellRecord&, Cell);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHexCellRemoved, const FHexCoord&, Coord);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHexCellUpdated, const FHexCellRecord&, Cell);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHexGridDataReset);

/**
 * An actor component that owns a hex grid data layer and exposes
 * Blueprint-accessible queries, mutations, and change-notification events.
 *
 * Attach this to any AActor to give it hex-grid data capabilities.
 * Use with AHexGrid or AHexTerrain for auto-sync, or standalone for
 * game-logic data like faction ownership, building maps, etc.
 */
UCLASS(ClassGroup = (Hexagon), meta = (BlueprintSpawnableComponent))
class HEXAGON_API UHexDataLayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHexDataLayerComponent(const FObjectInitializer& ObjectInitializer);

	// ---- C++ Data Access ----

	/** Direct mutable access to the underlying grid data. */
	FHexGridData& GetGridData() { return GridData; }

	/** Direct read-only access to the underlying grid data. */
	const FHexGridData& GetGridDataConst() const { return GridData; }

	// ---- Properties ----

	/** Cell circumradius for coordinate conversion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Data")
	float CellRadius = 100.0f;

	// ---- Blueprint Queries ----

	/** Get a cell by coordinate. Returns false if not found. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Data")
	bool GetCell(FHexCoord Coord, FHexCellRecord& OutCell) const;

	/** Whether a cell exists at the given coordinate. */
	UFUNCTION(BlueprintPure, Category = "Hexagon|Data")
	bool HasCell(FHexCoord Coord) const;

	/** Total number of cells. */
	UFUNCTION(BlueprintPure, Category = "Hexagon|Data")
	int32 GetCellCount() const { return GridData.Num(); }

	/** Get all existing cells within hex-distance Radius of Center. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Data")
	TArray<FHexCellRecord> GetCellsInRadius(FHexCoord Center, int32 Radius) const;

	/** Get the direct neighbors that exist in the grid (max 6). */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Data")
	TArray<FHexCellRecord> GetNeighbors(FHexCoord Coord) const;

	/** Get all existing cells on a ring at the given distance. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Data")
	TArray<FHexCellRecord> GetRing(FHexCoord Center, int32 Radius) const;

	/** Get all existing cells from center to MaxRadius in spiral order. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Data")
	TArray<FHexCellRecord> GetSpiral(FHexCoord Center, int32 MaxRadius) const;

	// ---- Blueprint Mutation ----

	/** Add or update a cell. Broadcasts OnCellAdded or OnCellUpdated. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Data")
	void SetCell(FHexCellRecord Cell);

	/** Remove a cell. Broadcasts OnCellRemoved. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Data")
	void RemoveCell(FHexCoord Coord);

	/** Remove all cells. Broadcasts OnGridReset. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Data")
	void ClearAll();

	/** Populate a spiral grid from center. Removes all existing cells first. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Data")
	void GenerateSpiralGrid(int32 GridRadius, float InCellRadius, float CellHeight);

	// ---- Events ----

	/** Broadcast when a cell is added (coordinate was not previously present). */
	UPROPERTY(BlueprintAssignable, Category = "Hexagon|Data|Events")
	FOnHexCellAdded OnCellAdded;

	/** Broadcast when a cell is removed. */
	UPROPERTY(BlueprintAssignable, Category = "Hexagon|Data|Events")
	FOnHexCellRemoved OnCellRemoved;

	/** Broadcast when an existing cell's data is updated. */
	UPROPERTY(BlueprintAssignable, Category = "Hexagon|Data|Events")
	FOnHexCellUpdated OnCellUpdated;

	/** Broadcast when the entire grid is cleared. */
	UPROPERTY(BlueprintAssignable, Category = "Hexagon|Data|Events")
	FOnHexGridDataReset OnGridReset;

	// ---- Serialization ----

	/** Serialize to FArchive (for save-game integration). */
	void SerializeData(FArchive& Ar);

	/** Binary serialize to bytes. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Data|Save")
	TArray<uint8> SerializeToBytes() const;

	/** Binary deserialize from bytes. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon|Data|Save")
	bool DeserializeFromBytes(const TArray<uint8>& Data);

private:
	/** The actual hex grid data. */
	FHexGridData GridData;
};
