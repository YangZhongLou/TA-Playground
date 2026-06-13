// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HexDataLayerComponent.h"

UHexDataLayerComponent::UHexDataLayerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ============================================================================
// Blueprint Queries
// ============================================================================

bool UHexDataLayerComponent::GetCell(FHexCoord Coord, FHexCellRecord& OutCell) const
{
	const FHexCellRecord* Found = GridData.GetCell(Coord);
	if (Found)
	{
		OutCell = *Found;
		return true;
	}
	return false;
}

bool UHexDataLayerComponent::HasCell(FHexCoord Coord) const
{
	return GridData.HasCell(Coord);
}

TArray<FHexCellRecord> UHexDataLayerComponent::GetCellsInRadius(FHexCoord Center, int32 Radius) const
{
	TArray<const FHexCellRecord*> Ptrs = GridData.GetCellsInRadius(Center, Radius);
	TArray<FHexCellRecord> Result;
	Result.Reserve(Ptrs.Num());
	for (const FHexCellRecord* Ptr : Ptrs)
	{
		Result.Add(*Ptr);
	}
	return Result;
}

TArray<FHexCellRecord> UHexDataLayerComponent::GetNeighbors(FHexCoord Coord) const
{
	TArray<const FHexCellRecord*> Ptrs = GridData.GetNeighbors(Coord);
	TArray<FHexCellRecord> Result;
	Result.Reserve(Ptrs.Num());
	for (const FHexCellRecord* Ptr : Ptrs)
	{
		Result.Add(*Ptr);
	}
	return Result;
}

TArray<FHexCellRecord> UHexDataLayerComponent::GetRing(FHexCoord Center, int32 Radius) const
{
	TArray<const FHexCellRecord*> Ptrs = GridData.GetRing(Center, Radius);
	TArray<FHexCellRecord> Result;
	Result.Reserve(Ptrs.Num());
	for (const FHexCellRecord* Ptr : Ptrs)
	{
		Result.Add(*Ptr);
	}
	return Result;
}

TArray<FHexCellRecord> UHexDataLayerComponent::GetSpiral(FHexCoord Center, int32 MaxRadius) const
{
	TArray<const FHexCellRecord*> Ptrs = GridData.GetSpiral(Center, MaxRadius);
	TArray<FHexCellRecord> Result;
	Result.Reserve(Ptrs.Num());
	for (const FHexCellRecord* Ptr : Ptrs)
	{
		Result.Add(*Ptr);
	}
	return Result;
}

// ============================================================================
// Blueprint Mutation
// ============================================================================

void UHexDataLayerComponent::SetCell(FHexCellRecord Cell)
{
	const bool bExisted = GridData.SetCell(Cell);
	if (bExisted)
	{
		OnCellUpdated.Broadcast(Cell);
	}
	else
	{
		OnCellAdded.Broadcast(Cell);
	}
}

void UHexDataLayerComponent::RemoveCell(FHexCoord Coord)
{
	if (GridData.RemoveCell(Coord))
	{
		OnCellRemoved.Broadcast(Coord);
	}
}

void UHexDataLayerComponent::ClearAll()
{
	GridData.Reset();
	OnGridReset.Broadcast();
}

void UHexDataLayerComponent::GenerateSpiralGrid(int32 GridRadius, float InCellRadius, float CellHeight)
{
	CellRadius = InCellRadius;
	GridData.GenerateSpiralGrid(GridRadius, InCellRadius, CellHeight);
	OnGridReset.Broadcast();
}

// ============================================================================
// Serialization
// ============================================================================

void UHexDataLayerComponent::SerializeData(FArchive& Ar)
{
	GridData.Serialize(Ar);
}

TArray<uint8> UHexDataLayerComponent::SerializeToBytes() const
{
	return const_cast<FHexGridData&>(GridData).ToBytes();
}

bool UHexDataLayerComponent::DeserializeFromBytes(const TArray<uint8>& Data)
{
	if (Data.Num() == 0)
	{
		return false;
	}

	GridData.FromBytes(Data);
	OnGridReset.Broadcast();
	return true;
}
