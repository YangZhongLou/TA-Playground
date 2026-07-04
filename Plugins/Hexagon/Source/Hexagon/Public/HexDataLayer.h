// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HexGeometry.h"
#include "HexTerrainGenerator.h"
#include "HexDataLayer.generated.h"

struct FHexCellData;
struct FHexTerrainCellData;

/**
 * Generic game-agnostic hex cell record.
 *
 * Replaces the earlier FHexCellData / FHexTerrainCellData split with a single
 * structure that holds terrain, gameplay, and extensible metadata in one place.
 *
 * Metadata (TMap<FName, float>) is the extensibility hook — game code can attach
 * "FactionOwner", "Population", "ResourceYield", etc. without modifying the plugin.
 */
USTRUCT(BlueprintType)
struct HEXAGON_API FHexCellRecord
{
	GENERATED_BODY()

	/** Axial hex coordinate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Data")
	FHexCoord Axial;

	/** World-space position of the cell center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Data")
	FVector WorldPos = FVector::ZeroVector;

	/** Cell height in world units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Data")
	float Height = 0.0f;

	/** Terrain classification type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Data")
	EHexTerrainType TerrainType = EHexTerrainType::Grass;

	/** Walkability / pathfinding flag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Data")
	bool bIsBlocked = false;

	/** Visual color override (vertex color tint). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Data")
	FLinearColor Color = FLinearColor::White;

	/**
	 * Extensible key-value store for game-specific metadata.
	 * Examples: "FactionOwner", "Population", "BuildingLevel", "ResourceYield".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Data")
	TMap<FName, float> Metadata;

	FHexCellRecord() = default;

	FHexCellRecord(FHexCoord InAxial, FVector InWorldPos, float InHeight = 0.0f,
	               EHexTerrainType InType = EHexTerrainType::Grass,
	               bool bInBlocked = false,
	               const FLinearColor& InColor = FLinearColor::White)
		: Axial(InAxial), WorldPos(InWorldPos), Height(InHeight),
		  TerrainType(InType), bIsBlocked(bInBlocked), Color(InColor)
	{}
};

/**
 * Static utilities to convert from the legacy cell data types.
 * Follows the same static-utility pattern as FHexGridGenerator / FHexTerrainGenerator.
 */
struct HEXAGON_API FHexCellRecordFactory
{
	/** Convert from legacy FHexCellData (grid generator output). */
	static FHexCellRecord FromCellData(const FHexCellData& CellData);

	/** Convert from legacy FHexTerrainCellData (terrain generator output). */
	static FHexCellRecord FromTerrainCellData(const FHexTerrainCellData& CellData);
};

/**
 * Collection / bag of FHexCellRecord with efficient spatial queries.
 *
 * Cells are stored in a TMap<FHexCoord, FHexCellRecord> for O(1) lookup.
 * This is NOT a UObject / UStruct — it's a standalone data container designed
 * to be held by UHexDataLayerComponent (which handles Blueprint exposure).
 *
 * All spatial queries delegate to FHexGeometry for coordinate generation,
 * then filter by map containment so only existing cells are returned.
 */
struct HEXAGON_API FHexGridData
{
	/** Map from axial coordinate to cell record. O(1) lookup. */
	TMap<FHexCoord, FHexCellRecord> Cells;

	/** Cell circumradius for coordinate conversion. Must be set before spatial queries. */
	float CellRadius = 100.0f;

	// ---- Construction ----

	FHexGridData() = default;

	/** Pre-allocate space for N cells. */
	void Reserve(int32 ExpectedCount) { Cells.Reserve(ExpectedCount); }

	/** Remove all cells. */
	void Reset() { Cells.Reset(); }

	// ---- Mutation ----

	/** Add or update a cell. Returns true if the cell was already present. */
	bool SetCell(const FHexCellRecord& Record);

	/** Remove a cell by coordinate. Returns true if it existed. */
	bool RemoveCell(const FHexCoord& Coord);

	/** Bulk-insert cells from a generator array. */
	void AddCells(const TArray<FHexCellRecord>& Records);

	/** Populate from a spiral: call Factory(Coord) for each coord and store the result. */
	void GenerateSpiralGrid(int32 GridRadius, float InCellRadius, float CellHeight);

	// ---- Queries ----

	/** O(1) read-only lookup. Returns nullptr if not found. */
	const FHexCellRecord* GetCell(const FHexCoord& Coord) const;

	/** O(1) mutable lookup. Returns nullptr if not found. */
	FHexCellRecord* GetCellMutable(const FHexCoord& Coord);

	/** Whether a cell exists at the given coordinate. */
	bool HasCell(const FHexCoord& Coord) const;

	/** Total number of cells in the grid. */
	int32 Num() const { return Cells.Num(); }

	/** Is the grid empty? */
	bool IsEmpty() const { return Cells.Num() == 0; }

	// ---- Spatial Queries ----

	/** Get cells within hex-distance Radius of Center. Returns only existing cells. */
	TArray<const FHexCellRecord*> GetCellsInRadius(const FHexCoord& Center, int32 Radius) const;

	/** Get the 6 direct neighbors. Only returns neighbors that exist in the grid. */
	TArray<const FHexCellRecord*> GetNeighbors(const FHexCoord& Coord) const;

	/** Get all existing cells on a ring at the given radius. */
	TArray<const FHexCellRecord*> GetRing(const FHexCoord& Center, int32 Radius) const;

	/** Get cells from center outward to MaxRadius (inclusive). */
	TArray<const FHexCellRecord*> GetSpiral(const FHexCoord& Center, int32 MaxRadius) const;

	/** Get all existing cells on a line from Start to End. */
	TArray<const FHexCellRecord*> GetLine(const FHexCoord& Start, const FHexCoord& End) const;

	/** Find cells matching a predicate (e.g. all Grass cells, all blocked cells). */
	TArray<const FHexCellRecord*> FindCells(TFunctionRef<bool(const FHexCellRecord&)> Predicate) const;

	// ---- Iteration ----

	/** Iterate over all cells. */
	void ForEachCell(TFunctionRef<void(const FHexCellRecord&)> Func) const;

	/** Iterate over cells within a radius (outer to inner). */
	void ForEachInRadius(const FHexCoord& Center, int32 Radius,
	                     TFunctionRef<void(const FHexCellRecord&)> Func) const;

	// ---- Serialization ----

	/** Serialize all data to an FArchive. */
	void Serialize(FArchive& Ar);

	/** Binary serialize to byte buffer (for save games / network). */
	TArray<uint8> ToBytes() const;

	/** Binary deserialize from byte buffer. */
	void FromBytes(const TArray<uint8>& Bytes);

private:
	/** Helper: filter TArray<FHexCoord> → TArray<const FHexCellRecord*> by map containment. */
	TArray<const FHexCellRecord*> FilterByContainment(const TArray<FHexCoord>& Coords) const;
};
