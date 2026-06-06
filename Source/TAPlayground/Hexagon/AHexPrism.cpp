// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "AHexPrism.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AHexPrism::AHexPrism(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HexPrismMesh"));
	SetRootComponent(MeshComponent);
	MeshComponent->bUseAsyncCooking = false;
	MeshComponent->bUseComplexAsSimpleCollision = true;

	// Default material — engine basic shape material
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial")
	);
	if (DefaultMaterial.Succeeded())
	{
		HexMaterial = DefaultMaterial.Object;
	}
}

#if WITH_EDITOR
void AHexPrism::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (bAutoRegenerate)
	{
		const FName PropertyName = PropertyChangedEvent.GetPropertyName();
		static const TSet<FName> MeshProperties = {
			GET_MEMBER_NAME_CHECKED(AHexPrism, Radius),
			GET_MEMBER_NAME_CHECKED(AHexPrism, Height),
			GET_MEMBER_NAME_CHECKED(AHexPrism, bCapTop),
			GET_MEMBER_NAME_CHECKED(AHexPrism, bCapBottom),
			GET_MEMBER_NAME_CHECKED(AHexPrism, HeightSegments),
			GET_MEMBER_NAME_CHECKED(AHexPrism, VertexColor),
		};

		if (MeshProperties.Contains(PropertyName))
		{
			RegenerateMesh();
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(AHexPrism, HexMaterial))
		{
			ApplyMaterial();
		}
	}
}
#endif

void AHexPrism::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RegenerateMesh();
}

void AHexPrism::BeginPlay()
{
	Super::BeginPlay();
	RegenerateMesh();
}

void AHexPrism::RegenerateMesh()
{
	if (!MeshComponent)
	{
		return;
	}

	FHexPrismMeshData MeshData;
	FHexPrismGenerator::Generate(
		MeshData,
		Radius,
		Height,
		bCapTop,
		bCapBottom,
		HeightSegments,
		VertexColor
	);

	MeshComponent->ClearMeshSection(0);

	if (MeshData.Vertices.Num() == 0)
	{
		return;
	}

	MeshComponent->CreateMeshSection(
		0,
		MeshData.Vertices,
		MeshData.Triangles,
		MeshData.Normals,
		MeshData.UVs,
		MeshData.VertexColors,
		MeshData.Tangents,
		true  // bCreateCollision
	);

	ApplyMaterial();
}

void AHexPrism::SetRadius(float NewRadius)
{
	Radius = FMath::Max(0.1f, NewRadius);
	RegenerateMesh();
}

void AHexPrism::SetHeight(float NewHeight)
{
	Height = FMath::Max(0.0f, NewHeight);
	RegenerateMesh();
}

void AHexPrism::SetHexMaterial(UMaterialInterface* NewMaterial)
{
	HexMaterial = NewMaterial;
	ApplyMaterial();
}

void AHexPrism::ApplyMaterial()
{
	if (MeshComponent && HexMaterial)
	{
		MeshComponent->SetMaterial(0, HexMaterial);
	}
}