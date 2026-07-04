// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HexDataLayer.h"
#include "HexGridGenerator.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

// ============================================================================
// FHexCellRecordFactory
// ============================================================================

FHexCellRecord FHexCellRecordFactory::FromCellData(const FHexCellData& CellData)
{
	FHexCellRecord Record;
	Record.Axial = CellData.Axial;
	Record.WorldPos = CellData.WorldPos;
	Record.Height = CellData.Height;
	Record.TerrainType = static_cast<EHexTerrainType>(CellData.TerrainType);
	Record.Color = CellData.Color;
	Record.bIsBlocked = false;
	return Record;
}

FHexCellRecord FHexCellRecordFactory::FromTerrainCellData(const FHexTerrainCellData& CellData)
{
	FHexCellRecord Record;
	Record.Axial = CellData.Axial;
	Record.WorldPos = CellData.WorldPos;
	Record.Height = CellData.Height;
	Record.TerrainType = CellData.TerrainType;
	Record.Color = CellData.Color;
	Record.bIsBlocked = false;
	return Record;
}

// ============================================================================
// FHexGridData: Mutation
// ============================================================================

bool FHexGridData::SetCell(const FHexCellRecord& Record)
{
	const bool bExisted = Cells.Contains(Record.Axial);
	Cells.Add(Record.Axial, Record);
	return bExisted;
}

bool FHexGridData::RemoveCell(const FHexCoord& Coord)
{
	return Cells.Remove(Coord) > 0;
}

void FHexGridData::AddCells(const TArray<FHexCellRecord>& Records)
{
	Cells.Reserve(Cells.Num() + Records.Num());
	for (const FHexCellRecord& Record : Records)
	{
		Cells.Add(Record.Axial, Record);
	}
}

void FHexGridData::GenerateSpiralGrid(int32 GridRadius, float InCellRadius, float CellHeight)
{
	Reset();
	CellRadius = InCellRadius;

	const TArray<FHexCoord> Coords = FHexGeometry::GetSpiral(FHexCoord(0, 0), GridRadius);
	Cells.Reserve(Coords.Num());

	for (const FHexCoord& Coord : Coords)
	{
		FHexCellRecord Record;
		Record.Axial = Coord;
		Record.WorldPos = FHexGeometry::HexToWorld(Coord, InCellRadius);
		Record.Height = CellHeight;
		Cells.Add(Coord, Record);
	}
}

// ============================================================================
// FHexGridData: Queries
// ============================================================================

const FHexCellRecord* FHexGridData::GetCell(const FHexCoord& Coord) const
{
	return Cells.Find(Coord);
}

FHexCellRecord* FHexGridData::GetCellMutable(const FHexCoord& Coord)
{
	return Cells.Find(Coord);
}

bool FHexGridData::HasCell(const FHexCoord& Coord) const
{
	return Cells.Contains(Coord);
}

// ============================================================================
// FHexGridData: Spatial Queries
// ============================================================================

TArray<const FHexCellRecord*> FHexGridData::FilterByContainment(const TArray<FHexCoord>& Coords) const
{
	TArray<const FHexCellRecord*> Result;
	Result.Reserve(Coords.Num());
	for (const FHexCoord& C : Coords)
	{
		if (const FHexCellRecord* Cell = Cells.Find(C))
		{
			Result.Add(Cell);
		}
	}
	return Result;
}

TArray<const FHexCellRecord*> FHexGridData::GetCellsInRadius(const FHexCoord& Center, int32 Radius) const
{
	return FilterByContainment(FHexGeometry::GetSpiral(Center, Radius));
}

TArray<const FHexCellRecord*> FHexGridData::GetNeighbors(const FHexCoord& Coord) const
{
	return FilterByContainment(FHexGeometry::GetNeighbors(Coord));
}

TArray<const FHexCellRecord*> FHexGridData::GetRing(const FHexCoord& Center, int32 Radius) const
{
	return FilterByContainment(FHexGeometry::GetRing(Center, Radius));
}

TArray<const FHexCellRecord*> FHexGridData::GetSpiral(const FHexCoord& Center, int32 MaxRadius) const
{
	return FilterByContainment(FHexGeometry::GetSpiral(Center, MaxRadius));
}

TArray<const FHexCellRecord*> FHexGridData::GetLine(const FHexCoord& Start, const FHexCoord& End) const
{
	return FilterByContainment(FHexGeometry::GetLine(Start, End));
}

TArray<const FHexCellRecord*> FHexGridData::FindCells(TFunctionRef<bool(const FHexCellRecord&)> Predicate) const
{
	TArray<const FHexCellRecord*> Result;
	for (const auto& Pair : Cells)
	{
		if (Predicate(Pair.Value))
		{
			Result.Add(&Pair.Value);
		}
	}
	return Result;
}

// ============================================================================
// FHexGridData: Iteration
// ============================================================================

void FHexGridData::ForEachCell(TFunctionRef<void(const FHexCellRecord&)> Func) const
{
	for (const auto& Pair : Cells)
	{
		Func(Pair.Value);
	}
}

void FHexGridData::ForEachInRadius(const FHexCoord& Center, int32 Radius,
                                   TFunctionRef<void(const FHexCellRecord&)> Func) const
{
	const TArray<FHexCoord> Coords = FHexGeometry::GetSpiral(Center, Radius);
	for (const FHexCoord& C : Coords)
	{
		if (const FHexCellRecord* Cell = Cells.Find(C))
		{
			Func(*Cell);
		}
	}
}

// ============================================================================
// FHexGridData: Serialization
// ============================================================================

void FHexGridData::Serialize(FArchive& Ar)
{
	// Version tag for forward compatibility
	static constexpr uint32 CurrentVersion = 1;
	uint32 Version = CurrentVersion;
	Ar << Version;

	Ar << CellRadius;

	int32 Count = Cells.Num();
	Ar << Count;

	if (Ar.IsLoading())
	{
		Cells.Reset();
		Cells.Reserve(Count);

		for (int32 i = 0; i < Count; ++i)
		{
			FHexCellRecord Record;
			Ar << Record.Axial.Q;
			Ar << Record.Axial.R;
			Ar << Record.WorldPos;
			Ar << Record.Height;
			uint8 TerrainTypeRaw = 0;
			Ar << TerrainTypeRaw;
			Record.TerrainType = static_cast<EHexTerrainType>(TerrainTypeRaw);
			uint8 BlockedRaw = 0;
			Ar << BlockedRaw;
			Record.bIsBlocked = BlockedRaw != 0;
			Ar << Record.Color;

			int32 MetadataCount = 0;
			Ar << MetadataCount;
			for (int32 m = 0; m < MetadataCount; ++m)
			{
				FName Key;
				float Value = 0.0f;
				Ar << Key;
				Ar << Value;
				Record.Metadata.Add(Key, Value);
			}

			Cells.Add(Record.Axial, Record);
		}
	}
	else
	{
		for (const auto& Pair : Cells)
		{
			// Copy to non-const for serialization — FArchive operator<<
			// requires non-const references for several types (float, FVector, FLinearColor).
			FHexCellRecord Record = Pair.Value;
			Ar << Record.Axial.Q;
			Ar << Record.Axial.R;
			Ar << Record.WorldPos;
			Ar << Record.Height;
			uint8 TerrainTypeRaw = static_cast<uint8>(Record.TerrainType);
			Ar << TerrainTypeRaw;
			uint8 BlockedRaw = Record.bIsBlocked ? 1 : 0;
			Ar << BlockedRaw;
			Ar << Record.Color;

			int32 MetadataCount = Record.Metadata.Num();
			Ar << MetadataCount;
			for (const auto& Meta : Record.Metadata)
			{
				FName Key = Meta.Key;
				float Value = Meta.Value;
				Ar << Key;
				Ar << Value;
			}
		}
	}
}

TArray<uint8> FHexGridData::ToBytes() const
{
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	const_cast<FHexGridData*>(this)->Serialize(Writer);
	return Bytes;
}

void FHexGridData::FromBytes(const TArray<uint8>& Bytes)
{
	FMemoryReader Reader(Bytes);
	Serialize(Reader);
}
