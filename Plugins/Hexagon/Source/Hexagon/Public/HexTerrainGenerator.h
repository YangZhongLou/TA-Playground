// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HexGeometry.h"
#include "HexTerrainGenerator.generated.h"

/** Terrain layer types, ordered by elevation. */
UENUM(BlueprintType)
enum class EHexTerrainType : uint8
{
	Water  UMETA(DisplayName = "Water"),
	Sand   UMETA(DisplayName = "Sand"),
	Grass  UMETA(DisplayName = "Grass"),
	Rock   UMETA(DisplayName = "Rock"),
	Snow   UMETA(DisplayName = "Snow"),
	Count  UMETA(Hidden),
};

/** Configuration for terrain generation. */
USTRUCT(BlueprintType)
struct FHexTerrainConfig
{
	GENERATED_BODY()

	/** Cell circumradius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	float CellRadius = 100.0f;

	/** Maximum elevation variation (peak-to-valley). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	float HeightScale = 150.0f;

	/** Noise frequency — higher = more detail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	float NoiseScale = 0.08f;

	/** Noise octaves — higher = more detail but slower. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "1", ClampMax = "8"))
	int32 NoiseOctaves = 4;

	/** Height threshold for water [0,1]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WaterLevel = 0.28f;

	/** Height threshold for sand (above water). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SandLevel = 0.38f;

	/** Height threshold for grass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GrassLevel = 0.60f;

	/** Height threshold for rock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RockLevel = 0.78f;

	/** Water cell thickness (how far below 0 the water extends). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	float WaterThickness = 20.0f;

	/** Base height offset (sea level). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	float SeaLevel = 0.0f;

	/** Random seed for noise — change to get different terrain layout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	int32 NoiseSeed = 42;

	/** Click to randomize the seed and regenerate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (MakeEditWidget = ""))
	bool bRandomizeSeed = false;

	/** Lacunarity — frequency multiplier per octave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Advanced", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float Lacunarity = 2.0f;

	/** Persistence — amplitude multiplier per octave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain|Advanced", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float Persistence = 0.5f;
};

/** Per-cell terrain data. */
USTRUCT(BlueprintType)
struct FHexTerrainCellData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FHexCoord Axial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector WorldPos;

	/** Normalized height [0,1]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float NormalizedHeight = 0.0f;

	/** World-space Z height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Height = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EHexTerrainType TerrainType = EHexTerrainType::Grass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor Color = FLinearColor::White;
};

/**
 * Procedural terrain generator for hexagonal grids.
 * Uses layered Perlin noise for height and automatic terrain classification.
 */
class HEXAGON_API FHexTerrainGenerator
{
public:
	/** Generate terrain cell data for a spiral grid. */
	static TArray<FHexTerrainCellData> Generate(
		int32 GridRadius,
		const FHexTerrainConfig& Config
	);

	/** Get the visual color for a terrain type. */
	static FLinearColor GetTerrainColor(EHexTerrainType Type);

	/** Get terrain type from normalized height. */
	static EHexTerrainType ClassifyTerrain(float NormalizedHeight, const FHexTerrainConfig& Config);

	/** Layered Perlin noise [0,1] — exposed for custom cell generation. */
	static float SampleNoise(float Q, float R, float Scale, int32 Octaves, int32 Seed = 42, float Lacunarity = 2.0f, float Persistence = 0.5f);

	/** Get the 6 neighbor hex coordinates for a given cell. Index 0..5 = E, NE, NW, W, SW, SE. */
	static TArray<FHexCoord> GetNeighborCoords(const FHexCoord& Center);

	/** Get the 3 hex coordinates whose cells meet at a given corner of a cell.
	 *  Corner index 0..5 matches hex corner angle order.
	 *  Returns: {cell itself, neighbor in direction i, neighbor in direction (i+1)%6} */
	static void GetCornerCells(const FHexCoord& Cell, int32 CornerIndex,
		FHexCoord& OutSelf, FHexCoord& OutNeighborA, FHexCoord& OutNeighborB);
};
