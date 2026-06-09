// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "AHexTerrain.h"
#include "HexTerrainChunk.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "HAL/PlatformTime.h"

// ============================================================================
// Helper: map hex axial coordinate -> chunk coordinate
// ============================================================================
static FIntPoint HexToChunkCoord(const FHexCoord& Hex)
{
	const int32 CX = FMath::FloorToInt32(static_cast<float>(Hex.Q) / AHexTerrain::CHUNK_SIZE);
	const int32 CY = FMath::FloorToInt32(static_cast<float>(Hex.R) / AHexTerrain::CHUNK_SIZE);
	return FIntPoint(CX, CY);
}

// ============================================================================
// AHexTerrain
// ============================================================================

AHexTerrain::AHexTerrain(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;  // LOD check 4x per second

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("HexTerrainRoot"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial")
	);
	if (DefaultMaterial.Succeeded())
	{
		TerrainMaterial = DefaultMaterial.Object;
	}
}

#if WITH_EDITOR
void AHexTerrain::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (bAutoRegenerate)
	{
		const FName Name = PropertyChangedEvent.GetPropertyName();
		static const TSet<FName> TerrainProps = {
			GET_MEMBER_NAME_CHECKED(AHexTerrain, GridRadius),
			GET_MEMBER_NAME_CHECKED(AHexTerrain, CellRadius),
			GET_MEMBER_NAME_CHECKED(AHexTerrain, Gap),
			GET_MEMBER_NAME_CHECKED(AHexTerrain, TerrainConfig),
		};

		if (TerrainProps.Contains(Name))
		{
			RegenerateTerrain();
		}
		else if (Name == GET_MEMBER_NAME_CHECKED(AHexTerrain, TerrainMaterial))
		{
			ApplyMaterialsToAllChunks();
		}
		else if (Name == GET_MEMBER_NAME_CHECKED(AHexTerrain, LayerMaterials))
		{
			// LayerMaterials change requires full rebuild — mesh sections must be
			// split by terrain type so each section gets the correct per-layer material.
			RegenerateTerrain();
		}
		else if (Name == GET_MEMBER_NAME_CHECKED(AHexTerrain, bManualMode) ||
		         Name == GET_MEMBER_NAME_CHECKED(AHexTerrain, TextureTileSize) ||
		         Name == GET_MEMBER_NAME_CHECKED(AHexTerrain, DefaultManualType) ||
		         Name == GET_MEMBER_NAME_CHECKED(AHexTerrain, ManualCellTypes) ||
		         Name == GET_MEMBER_NAME_CHECKED(AHexTerrain, ExtraCells) ||
		         Name == GET_MEMBER_NAME_CHECKED(AHexTerrain, RemovedCells))
		{
			RegenerateTerrain();
		}
		else if (Name == GET_MEMBER_NAME_CHECKED(AHexTerrain, bDebugLODColors))
		{
			if (bDebugLODColors)
			{
				// Immediately apply LOD debug colors
				UpdateChunkLODs();
			}
			else
			{
				// Clear LOD debug colors from all chunks
				for (const auto& Pair : ChunkMap)
				{
					if (!Pair.Value) continue;
					if (bDebugChunkColors)
					{
						// Re-apply chunk debug colors without full rebuild
						const FIntPoint Coord = Pair.Value->GetChunkCoord();
						const uint32 Hash = HashCombine(GetTypeHash(Coord.X), GetTypeHash(Coord.Y));
						Pair.Value->SetDebugColor(FColor(
							static_cast<uint8>((Hash * 37 + 101) % 256),
							static_cast<uint8>((Hash * 53 + 157) % 256),
							static_cast<uint8>((Hash * 71 + 199) % 256)));
					}
					else
					{
						Pair.Value->ClearDebugColor();
					}
				}
			}
		}

		if (Name == GET_MEMBER_NAME_CHECKED(FHexTerrainConfig, bRandomizeSeed))
		{
			if (TerrainConfig.bRandomizeSeed)
			{
				TerrainConfig.NoiseSeed = FMath::RandRange(0, 99999);
				TerrainConfig.bRandomizeSeed = false;
			}
			RegenerateTerrain();
		}
	}
}

void AHexTerrain::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	if (!bAutoRegenerate) return;

	const FName HeadName = PropertyChangedEvent.PropertyChain.GetHead()->GetValue()->GetFName();
	static const TSet<FName> TerrainProps = {
		GET_MEMBER_NAME_CHECKED(AHexTerrain, TerrainConfig),
	};

	if (TerrainProps.Contains(HeadName))
	{
		RegenerateTerrain();
	}
}
#endif

void AHexTerrain::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RegenerateTerrain();
}

void AHexTerrain::BeginPlay()
{
	Super::BeginPlay();
	RegenerateTerrain();
}

void AHexTerrain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateChunkLODs();
}

// ============================================================================
// LOD
// ============================================================================

void AHexTerrain::UpdateChunkLODs()
{
	if (ChunkMap.Num() == 0)
	{
		return;
	}

	// Get camera position
	FVector CameraLocation = FVector::ZeroVector;
	bool bGotCamera = false;

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawnOrSpectator())
			{
				CameraLocation = Pawn->GetActorLocation();
				bGotCamera = true;
			}
			else if (PC->PlayerCameraManager)
			{
				CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
				bGotCamera = true;
			}
		}

		if (!bGotCamera)
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (APlayerController* PC = It->Get())
				{
					if (PC->PlayerCameraManager)
					{
						CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
						bGotCamera = true;
						break;
					}
				}
			}
		}
	}

	if (!bGotCamera)
	{
		return;
	}

	const float EffectiveRadius = CellRadius * (1.0f - FMath::Clamp(Gap, 0.0f, 0.95f));

	// Build layer materials pointer map for chunks
	TMap<EHexTerrainType, UMaterialInterface*> LayerMatPtrs;
	for (const auto& Pair : LayerMaterials)
	{
		if (Pair.Value)
		{
			LayerMatPtrs.Add(Pair.Key, Pair.Value);
		}
	}

	auto CellLookup = [this](const FHexCoord& Coord) -> const FHexTerrainCellData* {
		return GetCell(Coord);
	};

	for (const auto& Pair : ChunkMap)
	{
		if (Pair.Value)
		{
			Pair.Value->UpdateLOD(
				CameraLocation, LODSettings,
				EffectiveRadius, Gap, CachedConfig,
				LayerMatPtrs.Num() > 0 ? &LayerMatPtrs : nullptr,
				TerrainMaterial,
				TextureTileSize, CellLookup, &LayerUVScales
			);

			// LOD debug visualization: color chunks by LOD level
			if (bDebugLODColors)
			{
				FColor LODColor;
				switch (Pair.Value->GetCurrentLOD())
				{
				case 0: LODColor = FColor(0, 200, 0);    break; // Green
				case 1: LODColor = FColor(220, 220, 0);  break; // Yellow
				case 2: LODColor = FColor(220, 0, 0);    break; // Red
				default: LODColor = FColor(100, 100, 100); break; // Gray
				}
				Pair.Value->SetDebugColor(LODColor);
			}
		}
	}
}

// ============================================================================
// Regeneration
// ============================================================================

void AHexTerrain::RegenerateTerrain()
{
	if (!Root)
	{
		return;
	}

	// 1. Generate all cell data with auto-classification
	FHexTerrainConfig Config = TerrainConfig;
	Config.CellRadius = CellRadius;
	TerrainCells = FHexTerrainGenerator::Generate(GridRadius, Config);

	// 1a. Filter out RemovedCells
	if (RemovedCells.Num() > 0)
	{
		TerrainCells.RemoveAll([this](const FHexTerrainCellData& C) {
			return RemovedCells.Contains(C.Axial);
		});
	}

	// 1b. Add ExtraCells (noise-sampled heights for seamless blending)
	if (ExtraCells.Num() > 0)
	{
		TSet<FHexCoord> Existing;
		for (const FHexTerrainCellData& C : TerrainCells) { Existing.Add(C.Axial); }

		for (const FHexCoord& Coord : ExtraCells)
		{
			if (Existing.Contains(Coord)) continue;
			Existing.Add(Coord);

			FHexTerrainCellData Cell;
			Cell.Axial = Coord;
			Cell.WorldPos = FHexGeometry::HexToWorld(Coord, Config.CellRadius);
			Cell.NormalizedHeight = FHexTerrainGenerator::SampleNoise(
				static_cast<float>(Coord.Q), static_cast<float>(Coord.R),
				Config.NoiseScale, Config.NoiseOctaves, Config.NoiseSeed,
				Config.Lacunarity, Config.Persistence);
			Cell.NormalizedHeight = FMath::Clamp(Cell.NormalizedHeight, 0.0f, 1.0f);
			Cell.Height = Config.SeaLevel + Cell.NormalizedHeight * Config.HeightScale;
			Cell.TerrainType = FHexTerrainGenerator::ClassifyTerrain(Cell.NormalizedHeight, Config);
			Cell.Color = FHexTerrainGenerator::GetTerrainColor(Cell.TerrainType);
			TerrainCells.Add(Cell);
		}
	}

	// 2. Apply manual terrain type overrides
	if (bManualMode)
	{
		for (FHexTerrainCellData& Cell : TerrainCells)
		{
			if (const EHexTerrainType* Override = ManualCellTypes.Find(Cell.Axial))
			{
				Cell.TerrainType = *Override;
				Cell.Color = FHexTerrainGenerator::GetTerrainColor(*Override);
			}
			else
			{
				Cell.TerrainType = DefaultManualType;
				Cell.Color = FHexTerrainGenerator::GetTerrainColor(DefaultManualType);
			}
		}
	}

	// 3. Cache for LOD updates
	CachedConfig = Config;
	CachedEffectiveRadius = CellRadius * (1.0f - FMath::Clamp(Gap, 0.0f, 0.95f));

	// 4. Distribute to chunks and build each
	BuildAllChunks(Config);
}

void AHexTerrain::BuildAllChunks(const FHexTerrainConfig& Config)
{
	const float EffectiveRadius = CellRadius * (1.0f - FMath::Clamp(Gap, 0.0f, 0.95f));

	// Build non-null layer material pointers for chunk API
	TMap<EHexTerrainType, UMaterialInterface*> LayerMatPtrs;
	for (const auto& Pair : LayerMaterials)
	{
		if (Pair.Value)
		{
			LayerMatPtrs.Add(Pair.Key, Pair.Value);
		}
	}

	// Group cells by chunk coordinate
	TMap<FIntPoint, TArray<FHexTerrainCellData>> Distribution;
	for (const FHexTerrainCellData& Cell : TerrainCells)
	{
		const FIntPoint ChunkCoord = HexToChunkCoord(Cell.Axial);
		Distribution.FindOrAdd(ChunkCoord).Add(Cell);
	}

	TSet<FIntPoint> ActiveChunkCoords;
	Distribution.GetKeys(ActiveChunkCoords);

	// Cell lookup for vertex color blending (corner-based, neighbor-aware)
	auto CellLookup = [this](const FHexCoord& Coord) -> const FHexTerrainCellData* {
		return GetCell(Coord);
	};

	// Create or update chunks
	for (const auto& Pair : Distribution)
	{
		const FIntPoint& Coord = Pair.Key;
		const TArray<FHexTerrainCellData>& ChunkCells = Pair.Value;

		UHexTerrainChunk* Chunk = nullptr;

		if (TObjectPtr<UHexTerrainChunk>* Existing = ChunkMap.Find(Coord))
		{
			Chunk = *Existing;
		}
		else
		{
			Chunk = NewObject<UHexTerrainChunk>(this, NAME_None, RF_Transactional);
			Chunk->SetChunkCoord(Coord);
			Chunk->RegisterComponent();
			Chunk->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
			ChunkMap.Add(Coord, Chunk);
		}

		// Debug chunk coloring: deterministic color per chunk coordinate
		if (bDebugChunkColors)
		{
			const uint32 Hash = HashCombine(GetTypeHash(Coord.X), GetTypeHash(Coord.Y));
			const FColor ChunkDebugColor(
				static_cast<uint8>((Hash * 37 + 101) % 256),
				static_cast<uint8>((Hash * 53 + 157) % 256),
				static_cast<uint8>((Hash * 71 + 199) % 256)
			);
			Chunk->SetDebugColor(ChunkDebugColor);
		}
		else
		{
			Chunk->ClearDebugColor();
		}

		// Set cells — chunk will create per-layer or single-section mesh
		const bool bHasLayerMats = LayerMatPtrs.Num() > 0;
		const double BuildStart = bLogPerformance ? FPlatformTime::Seconds() : 0.0;
		Chunk->SetCells(ChunkCells, EffectiveRadius, Gap, Config,
			bHasLayerMats ? &LayerMatPtrs : nullptr,
			bHasLayerMats ? TerrainMaterial.Get() : nullptr,
			TextureTileSize, CellLookup, &LayerUVScales);
		if (bLogPerformance)
		{
			const double ElapsedMs = (FPlatformTime::Seconds() - BuildStart) * 1000.0;
			if (ElapsedMs > PerformanceWarningThresholdMs)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("Chunk (%d,%d) rebuild: %.1f ms (%d cells) [Regenerate]"),
					Coord.X, Coord.Y, ElapsedMs, ChunkCells.Num());
			}
		}

		// Apply materials (always called so section materials stay up-to-date)
		if (bHasLayerMats)
		{
			Chunk->ApplyLayerMaterials(LayerMatPtrs, TerrainMaterial);
		}
		else
		{
			Chunk->ApplySingleMaterial(TerrainMaterial);
		}
	}

	PruneUnusedChunks(ActiveChunkCoords);
}

void AHexTerrain::PruneUnusedChunks(const TSet<FIntPoint>& ActiveChunkCoords)
{
	TArray<FIntPoint> ToRemove;
	for (const auto& Pair : ChunkMap)
	{
		if (!ActiveChunkCoords.Contains(Pair.Key))
		{
			ToRemove.Add(Pair.Key);
		}
	}

	for (const FIntPoint& Coord : ToRemove)
	{
		if (UHexTerrainChunk* Chunk = ChunkMap.FindAndRemoveChecked(Coord))
		{
			Chunk->DestroyComponent();
		}
	}
}

// ============================================================================
// Incremental paint
// ============================================================================

void AHexTerrain::PaintCells(const TArray<FHexCoord>& Coords, EHexTerrainType Type)
{
	if (Coords.Num() == 0) return;

	// Enable manual mode and record overrides
	bManualMode = true;
	for (const FHexCoord& C : Coords)
	{
		ManualCellTypes.Add(C, Type);
	}

	// Update TerrainCells in-place (avoids full regeneration)
	const FLinearColor TypeColor = FHexTerrainGenerator::GetTerrainColor(Type);
	for (FHexTerrainCellData& Cell : TerrainCells)
	{
		if (const EHexTerrainType* Override = ManualCellTypes.Find(Cell.Axial))
		{
			Cell.TerrainType = *Override;
			Cell.Color = FHexTerrainGenerator::GetTerrainColor(*Override);
		}
	}

	// Collect affected chunks
	TSet<FIntPoint> DirtyChunks;
	for (const FHexCoord& C : Coords)
	{
		DirtyChunks.Add(HexToChunkCoord(C));
	}

	RebuildDirtyChunks(DirtyChunks);
}

void AHexTerrain::RebuildDirtyChunks(const TSet<FIntPoint>& DirtyChunkCoords)
{
	const float EffectiveRadius = CellRadius * (1.0f - FMath::Clamp(Gap, 0.0f, 0.95f));

	TMap<EHexTerrainType, UMaterialInterface*> LayerMatPtrs;
	for (const auto& Pair : LayerMaterials)
	{
		if (Pair.Value)
		{
			LayerMatPtrs.Add(Pair.Key, Pair.Value);
		}
	}

	auto CellLookup = [this](const FHexCoord& Coord) -> const FHexTerrainCellData* {
		return GetCell(Coord);
	};

	// Re-group TerrainCells by chunk coord for the dirty chunks only
	TMap<FIntPoint, TArray<FHexTerrainCellData>> DirtyDistribution;
	for (const FHexTerrainCellData& Cell : TerrainCells)
	{
		const FIntPoint ChunkCoord = HexToChunkCoord(Cell.Axial);
		if (DirtyChunkCoords.Contains(ChunkCoord))
		{
			DirtyDistribution.FindOrAdd(ChunkCoord).Add(Cell);
		}
	}

	for (const auto& Pair : DirtyDistribution)
	{
		const FIntPoint& Coord = Pair.Key;
		const TArray<FHexTerrainCellData>& ChunkCells = Pair.Value;

		TObjectPtr<UHexTerrainChunk>* Found = ChunkMap.Find(Coord);
		if (!Found || !*Found) continue;

		UHexTerrainChunk* Chunk = *Found;
		const bool bHasLayerMats = LayerMatPtrs.Num() > 0;

		const double BuildStart = bLogPerformance ? FPlatformTime::Seconds() : 0.0;
		Chunk->SetCells(ChunkCells, EffectiveRadius, Gap, CachedConfig,
			bHasLayerMats ? &LayerMatPtrs : nullptr,
			bHasLayerMats ? TerrainMaterial.Get() : nullptr,
			TextureTileSize, CellLookup, &LayerUVScales);
		if (bLogPerformance)
		{
			const double ElapsedMs = (FPlatformTime::Seconds() - BuildStart) * 1000.0;
			if (ElapsedMs > PerformanceWarningThresholdMs)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("Chunk (%d,%d) rebuild: %.1f ms (%d cells) [Paint]"),
					Coord.X, Coord.Y, ElapsedMs, ChunkCells.Num());
			}
		}

		if (bHasLayerMats)
		{
			Chunk->ApplyLayerMaterials(LayerMatPtrs, TerrainMaterial);
		}
		else
		{
			Chunk->ApplySingleMaterial(TerrainMaterial);
		}
	}
}

// ============================================================================
// Queries
// ============================================================================

const FHexTerrainCellData* AHexTerrain::GetCell(const FHexCoord& Coord) const
{
	for (const FHexTerrainCellData& Cell : TerrainCells)
	{
		if (Cell.Axial == Coord)
		{
			return &Cell;
		}
	}
	return nullptr;
}

bool AHexTerrain::HasCell(const FHexCoord& Coord) const
{
	return GetCell(Coord) != nullptr;
}

EHexTerrainType AHexTerrain::GetTerrainTypeAtPosition(const FVector& Pos) const
{
	const FHexCoord Coord = FHexGeometry::WorldToHex(Pos, CellRadius);
	if (const FHexTerrainCellData* Cell = GetCell(Coord))
	{
		return Cell->TerrainType;
	}
	return EHexTerrainType::Water;
}

// ============================================================================
// Material
// ============================================================================

void AHexTerrain::ApplyMaterialsToAllChunks()
{
	TMap<EHexTerrainType, UMaterialInterface*> LayerMatPtrs;
	for (const auto& Pair : LayerMaterials)
	{
		if (Pair.Value)
		{
			LayerMatPtrs.Add(Pair.Key, Pair.Value);
		}
	}

	for (const auto& Pair : ChunkMap)
	{
		if (Pair.Value)
		{
			if (LayerMatPtrs.Num() > 0)
			{
				Pair.Value->ApplyLayerMaterials(LayerMatPtrs, TerrainMaterial);
			}
			else
			{
				Pair.Value->ApplySingleMaterial(TerrainMaterial);
			}
		}
	}
}

void AHexTerrain::SetLayerMaterial(EHexTerrainType Type, UMaterialInterface* Material)
{
	if (Material)
	{
		LayerMaterials.Add(Type, Material);
	}
	else
	{
		LayerMaterials.Remove(Type);
	}
	ApplyMaterialsToAllChunks();
}

void AHexTerrain::PrintStats() const
{
	UE_LOG(LogTemp, Log, TEXT("========== HexTerrain Stats =========="));
	UE_LOG(LogTemp, Log, TEXT("  Actor:          %s"), *GetName());
	UE_LOG(LogTemp, Log, TEXT("  GridRadius:     %d"), GridRadius);
	UE_LOG(LogTemp, Log, TEXT("  CellRadius:     %.1f"), CellRadius);
	UE_LOG(LogTemp, Log, TEXT("  Total Cells:    %d"), TerrainCells.Num());
	UE_LOG(LogTemp, Log, TEXT("  Chunks:         %d  (CHUNK_SIZE=%d)"), ChunkMap.Num(), CHUNK_SIZE);
	UE_LOG(LogTemp, Log, TEXT("  Gap:            %.2f"), Gap);

	// Per-chunk details
	TMap<int32, int32> LODCounts;
	int32 TotalChunkCells = 0;
	int32 EmptyChunks = 0;
	int32 SingleSectionChunks = 0;
	int32 MultiSectionChunks = 0;

	for (const auto& Pair : ChunkMap)
	{
		const UHexTerrainChunk* Chunk = Pair.Value;
		if (!Chunk) { ++EmptyChunks; continue; }

		const int32 CellCount = Chunk->GetCellCount();
		TotalChunkCells += CellCount;
		LODCounts.FindOrAdd(Chunk->GetCurrentLOD())++;

		const int32 NumSections = Chunk->GetNumSections();
		if (NumSections <= 1) ++SingleSectionChunks;
		else ++MultiSectionChunks;
	}

	UE_LOG(LogTemp, Log, TEXT("  Cells in chunks:%d"), TotalChunkCells);
	UE_LOG(LogTemp, Log, TEXT("  Empty chunks:   %d"), EmptyChunks);
	UE_LOG(LogTemp, Log, TEXT("  Sections:       %d multi-layer, %d single-layer"),
		MultiSectionChunks, SingleSectionChunks);

	// LOD distribution
	FString LODStr;
	for (const auto& LodPair : LODCounts)
	{
		LODStr += FString::Printf(TEXT("LOD%d=%d "), LodPair.Key, LodPair.Value);
	}
	UE_LOG(LogTemp, Log, TEXT("  LOD dist:       %s"), *LODStr);

	// Terrain type distribution
	TMap<EHexTerrainType, int32> TypeCounts;
	for (const FHexTerrainCellData& Cell : TerrainCells)
	{
		TypeCounts.FindOrAdd(Cell.TerrainType)++;
	}

	FString TypeStr;
	for (uint8 i = 0; i < static_cast<uint8>(EHexTerrainType::Count); ++i)
	{
		const EHexTerrainType Type = static_cast<EHexTerrainType>(i);
		if (int32* Count = TypeCounts.Find(Type))
		{
			TypeStr += FString::Printf(TEXT("%s=%d "),
				*UEnum::GetValueAsString(Type), *Count);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("  Terrain types:  %s"), *TypeStr);
	UE_LOG(LogTemp, Log, TEXT("========================================"));
}
