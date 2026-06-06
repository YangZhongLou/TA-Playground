// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "AHexTerrain.h"
#include "ProceduralMeshComponent.h"
#include "HexPrismGenerator.h"
#include "UObject/ConstructorHelpers.h"

AHexTerrain::AHexTerrain(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HexTerrainMesh"));
	SetRootComponent(MeshComponent);
	MeshComponent->bUseAsyncCooking = false;
	MeshComponent->bUseComplexAsSimpleCollision = true;

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
			ApplyMaterial();
		}

		// Handle the "Randomize Seed" toggle — when checked, randomize and reset
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

	// When any nested struct property changes, regenerate.
	// PostEditChangeChainProperty reliably catches deep property changes
	// that PostEditChangeProperty might miss with nested structs.
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

void AHexTerrain::RegenerateTerrain()
{
	if (!MeshComponent)
	{
		return;
	}

	// Update config radius to match actor
	FHexTerrainConfig Config = TerrainConfig;
	Config.CellRadius = CellRadius;

	TerrainCells = FHexTerrainGenerator::Generate(GridRadius, Config);
	BuildTerrainMesh();
}

void AHexTerrain::BuildTerrainMesh()
{
	MeshComponent->ClearMeshSection(0);

	if (TerrainCells.Num() == 0)
	{
		return;
	}

	const float EffectiveRadius = CellRadius * (1.0f - FMath::Clamp(Gap, 0.0f, 0.95f));

	FHexPrismMeshData CombinedMesh;

	for (const FHexTerrainCellData& Cell : TerrainCells)
	{
		FHexPrismMeshData CellMesh;
		const FColor CellColor = Cell.Color.ToFColor(true);

		// Water cells: flat slab extending below sea level
		// Land cells: hex prism with height based on terrain elevation
		if (Cell.TerrainType == EHexTerrainType::Water)
		{
			const float WaterTop = Cell.Height;
			const float WaterBottom = Cell.Height - TerrainConfig.WaterThickness;
			const float WaterHeight = WaterTop - WaterBottom;

			FHexPrismGenerator::Generate(
				CellMesh, EffectiveRadius, WaterHeight,
				true, true, 1, CellColor
			);

			// Offset so bottom is at WaterBottom
			const float ZOffset = WaterBottom + WaterHeight * 0.5f;
			for (FVector& V : CellMesh.Vertices)
			{
				V.Z += ZOffset;
			}
		}
		else
		{
			// Land cell: prism from 0 up to height
			FHexPrismGenerator::Generate(
				CellMesh, EffectiveRadius, Cell.Height,
				true, true, 1, CellColor
			);

			// Shift so bottom sits at 0 and top at Height
			const float ZOffset = Cell.Height * 0.5f;
			for (FVector& V : CellMesh.Vertices)
			{
				V.Z += ZOffset;
			}
		}

		// Append into combined mesh
		const int32 BaseVertex = CombinedMesh.Vertices.Num();
		for (const FVector& V : CellMesh.Vertices)
		{
			CombinedMesh.Vertices.Add(V + Cell.WorldPos);
		}
		CombinedMesh.UVs.Append(CellMesh.UVs);
		CombinedMesh.Normals.Append(CellMesh.Normals);
		CombinedMesh.VertexColors.Append(CellMesh.VertexColors);
		CombinedMesh.Tangents.Append(CellMesh.Tangents);

		for (int32 Tri : CellMesh.Triangles)
		{
			CombinedMesh.Triangles.Add(Tri + BaseVertex);
		}
	}

	MeshComponent->CreateMeshSection(
		0,
		CombinedMesh.Vertices,
		CombinedMesh.Triangles,
		CombinedMesh.Normals,
		CombinedMesh.UVs,
		CombinedMesh.VertexColors,
		CombinedMesh.Tangents,
		true
	);

	ApplyMaterial();
}

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

EHexTerrainType AHexTerrain::GetTerrainTypeAtPosition(const FVector& Pos) const
{
	const FHexCoord Coord = FHexGeometry::WorldToHex(Pos, CellRadius);
	if (const FHexTerrainCellData* Cell = GetCell(Coord))
	{
		return Cell->TerrainType;
	}
	return EHexTerrainType::Water;
}

void AHexTerrain::ApplyMaterial()
{
	if (MeshComponent && TerrainMaterial)
	{
		MeshComponent->SetMaterial(0, TerrainMaterial);
	}
}
