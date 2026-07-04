// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "AHexGrid.h"
#include "ProceduralMeshComponent.h"
#include "HexPrismGenerator.h"
#include "UObject/ConstructorHelpers.h"

AHexGrid::AHexGrid(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HexGridMesh"));
	SetRootComponent(MeshComponent);
	MeshComponent->bUseAsyncCooking = false;
	MeshComponent->bUseComplexAsSimpleCollision = true;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial")
	);
	if (DefaultMaterial.Succeeded())
	{
		GridMaterial = DefaultMaterial.Object;
	}
}

#if WITH_EDITOR
void AHexGrid::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const TSet<FName> GridProperties = {
		GET_MEMBER_NAME_CHECKED(AHexGrid, CellRadius),
		GET_MEMBER_NAME_CHECKED(AHexGrid, GridRadius),
		GET_MEMBER_NAME_CHECKED(AHexGrid, CellHeight),
		GET_MEMBER_NAME_CHECKED(AHexGrid, Gap),
		GET_MEMBER_NAME_CHECKED(AHexGrid, bRainbowColors),
	};

	if (GridProperties.Contains(PropertyChangedEvent.GetPropertyName()))
	{
		RegenerateMesh();
	}
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(AHexGrid, GridMaterial))
	{
		ApplyMaterial();
	}
}
#endif

void AHexGrid::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RegenerateMesh();
}

void AHexGrid::BeginPlay()
{
	Super::BeginPlay();
	RegenerateMesh();
}

void AHexGrid::RegenerateMesh()
{
	if (!MeshComponent)
	{
		return;
	}

	// Generate cell layout
	CellData = FHexGridGenerator::GenerateSpiral(CellRadius, GridRadius, CellHeight);

	// Apply rainbow coloring
	if (bRainbowColors && CellData.Num() > 0)
	{
		static const FLinearColor Rainbow[] = {
			FLinearColor(1.0f, 0.0f, 0.0f),  // Red
			FLinearColor(1.0f, 0.5f, 0.0f),  // Orange
			FLinearColor(1.0f, 1.0f, 0.0f),  // Yellow
			FLinearColor(0.0f, 1.0f, 0.0f),  // Green
			FLinearColor(0.0f, 0.5f, 1.0f),  // Blue
			FLinearColor(0.5f, 0.0f, 1.0f),  // Purple
			FLinearColor(1.0f, 0.0f, 1.0f),  // Magenta
		};
		constexpr int32 RainbowCount = UE_ARRAY_COUNT(Rainbow);

		for (int32 i = 0; i < CellData.Num(); ++i)
		{
			const int32 ColorIndex = i % RainbowCount;
			CellData[i].Color = Rainbow[ColorIndex];
		}
	}

	BuildMeshFromCellData();
}

void AHexGrid::BuildMeshFromCellData()
{
	MeshComponent->ClearMeshSection(0);

	if (CellData.Num() == 0)
	{
		return;
	}

	// Effective radius accounting for gap
	const float EffectiveRadius = CellRadius * (1.0f - FMath::Clamp(Gap, 0.0f, 0.95f));

	FHexPrismMeshData CombinedMesh;

	for (const FHexCellData& Cell : CellData)
	{
		FHexPrismMeshData CellMesh;
		const FColor CellColor = Cell.Color.ToFColor(true);

		if (Cell.Height <= KINDA_SMALL_NUMBER)
		{
			FHexPrismGenerator::GenerateFlatHexagon(CellMesh, EffectiveRadius, 0.0f, CellColor);
		}
		else
		{
			FHexPrismGenerator::Generate(CellMesh, EffectiveRadius, Cell.Height, true, true, 1, CellColor);
		}

		// Append cell mesh into combined mesh with world position offset
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
		true  // bCreateCollision
	);

	ApplyMaterial();
}

FVector AHexGrid::GetCellWorldPosition(const FHexCoord& Coord) const
{
	return FHexGeometry::HexToWorld(Coord, CellRadius);
}

FHexCoord AHexGrid::GetCellFromWorldPosition(const FVector& Pos) const
{
	return FHexGeometry::WorldToHex(Pos, CellRadius);
}

TArray<FHexCoord> AHexGrid::GetNeighbors(const FHexCoord& Coord) const
{
	return FHexGeometry::GetNeighbors(Coord);
}

int32 AHexGrid::GetCellDistance(const FHexCoord& A, const FHexCoord& B) const
{
	return FHexGeometry::Distance(A, B);
}

void AHexGrid::ApplyMaterial()
{
	if (MeshComponent && GridMaterial)
	{
		MeshComponent->SetMaterial(0, GridMaterial);
	}
}
