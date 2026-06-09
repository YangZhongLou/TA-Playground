// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HexTerrainGenerator.h"

TArray<FHexTerrainCellData> FHexTerrainGenerator::Generate(
	int32 GridRadius,
	const FHexTerrainConfig& Config
)
{
	GridRadius = FMath::Max(0, GridRadius);

	TArray<FHexCoord> Coords = FHexGeometry::GetSpiral(FHexCoord(0, 0), GridRadius);
	TArray<FHexTerrainCellData> Result;
	Result.Reserve(Coords.Num());

	for (const FHexCoord& Coord : Coords)
	{
		FHexTerrainCellData Cell;
		Cell.Axial = Coord;
		Cell.WorldPos = FHexGeometry::HexToWorld(Coord, Config.CellRadius);

		// Sample layered noise
		Cell.NormalizedHeight = SampleNoise(
			static_cast<float>(Coord.Q),
			static_cast<float>(Coord.R),
			Config.NoiseScale,
			Config.NoiseOctaves,
			Config.NoiseSeed,
			Config.Lacunarity,
			Config.Persistence
		);

		// Clamp and map to world height
		Cell.NormalizedHeight = FMath::Clamp(Cell.NormalizedHeight, 0.0f, 1.0f);
		Cell.Height = Config.SeaLevel + Cell.NormalizedHeight * Config.HeightScale;

		// Classify terrain
		Cell.TerrainType = ClassifyTerrain(Cell.NormalizedHeight, Config);
		Cell.Color = GetTerrainColor(Cell.TerrainType);

		Result.Add(Cell);
	}

	return Result;
}

FLinearColor FHexTerrainGenerator::GetTerrainColor(EHexTerrainType Type)
{
	switch (Type)
	{
		case EHexTerrainType::Water: return FLinearColor(0.10f, 0.35f, 0.65f); // Deep blue
		case EHexTerrainType::Sand:  return FLinearColor(0.85f, 0.75f, 0.45f); // Sandy yellow
		case EHexTerrainType::Grass: return FLinearColor(0.20f, 0.60f, 0.15f); // Green
		case EHexTerrainType::Rock:  return FLinearColor(0.45f, 0.40f, 0.38f); // Gray-brown
		case EHexTerrainType::Snow:  return FLinearColor(0.95f, 0.95f, 0.98f); // White
		default:                     return FLinearColor::White;
	}
}

EHexTerrainType FHexTerrainGenerator::ClassifyTerrain(float NormalizedHeight, const FHexTerrainConfig& Config)
{
	if (NormalizedHeight < Config.WaterLevel)
		return EHexTerrainType::Water;
	if (NormalizedHeight < Config.SandLevel)
		return EHexTerrainType::Sand;
	if (NormalizedHeight < Config.GrassLevel)
		return EHexTerrainType::Grass;
	if (NormalizedHeight < Config.RockLevel)
		return EHexTerrainType::Rock;
	return EHexTerrainType::Snow;
}

float FHexTerrainGenerator::SampleNoise(float Q, float R, float Scale, int32 Octaves, int32 Seed, float Lacunarity, float Persistence)
{
	// Offset by seed to create different terrain layouts
	const float SeedOffset = static_cast<float>(Seed) * 100.0f;

	float Amplitude = 1.0f;
	float Frequency = Scale;
	float MaxValue = 0.0f;
	float Total = 0.0f;

	for (int32 i = 0; i < Octaves; ++i)
	{
		const float Sample = FMath::PerlinNoise2D(FVector2D(
			Q * Frequency + SeedOffset,
			R * Frequency + SeedOffset
		));
		// Perlin returns [-0.5, 0.5] in UE, remap to [0,1] per octave
		Total += (Sample + 0.5f) * Amplitude;
		MaxValue += Amplitude;

		Amplitude *= Persistence;
		Frequency *= Lacunarity;
	}

	return Total / MaxValue;
}
