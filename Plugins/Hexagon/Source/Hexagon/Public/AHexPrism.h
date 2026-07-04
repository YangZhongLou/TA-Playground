// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HexPrismGenerator.h"
#include "AHexPrism.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

/**
 * Runtime-generated hexagonal prism actor.
 * Parameters are editable in-editor and at runtime; mesh regenerates automatically.
 */
UCLASS(ClassGroup = (Hexagon), meta = (BlueprintSpawnableComponent))
class HEXAGON_API AHexPrism : public AActor
{
	GENERATED_BODY()

public:
	AHexPrism(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Regenerate the mesh from current parameters. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon")
	void RegenerateMesh();

	/** Set radius and regenerate. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon")
	void SetRadius(float NewRadius);

	/** Set height and regenerate. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon")
	void SetHeight(float NewHeight);

	/** Set material and apply. */
	UFUNCTION(BlueprintCallable, Category = "Hexagon")
	void SetHexMaterial(UMaterialInterface* NewMaterial);

public:
	/** Circumradius — distance from center to any vertex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Shape", meta = (ClampMin = "0.1", ClampMax = "10000.0"))
	float Radius = 100.0f;

	/** Total height of the prism. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Shape", meta = (ClampMin = "0.0", ClampMax = "10000.0"))
	float Height = 200.0f;

	/** Whether to generate the top face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Shape")
	bool bCapTop = true;

	/** Whether to generate the bottom face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Shape")
	bool bCapBottom = true;

	/** Number of vertical subdivisions per side face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Shape", meta = (ClampMin = "1", ClampMax = "32"))
	int32 HeightSegments = 1;

	/** Optional material override. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Material")
	TObjectPtr<UMaterialInterface> HexMaterial;

	/** Vertex color tint applied to all vertices. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Material")
	FColor VertexColor = FColor::White;

	/** If true, mesh auto-regenerates when parameters change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hexagon|Debug")
	bool bAutoRegenerate = true;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hexagon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProceduralMeshComponent> MeshComponent;

	void ApplyMaterial();
};