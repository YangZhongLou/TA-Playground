// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HexTerrainEditMode.h"

#if WITH_EDITOR

#include "AHexTerrain.h"
#include "HexGeometry.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "EngineUtils.h"
#include "SceneView.h"

// ============================================================================
// Mode ID
// ============================================================================
const FEditorModeID FHexTerrainEditMode::EM_HexTerrainEdit("Hexagon.HexTerrainEdit");

FHexTerrainEditMode::FHexTerrainEditMode() {}
FHexTerrainEditMode::~FHexTerrainEditMode() {}

// ============================================================================
// Enter / Exit
// ============================================================================
void FHexTerrainEditMode::Enter()
{
	FEdMode::Enter();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AHexTerrain> It(World); It; ++It)
		{
			if (GEditor) { GEditor->SelectNone(false, true); GEditor->SelectActor(*It, true, false); }
			break;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("HexTerrainEdit mode entered. Drag to box-select, Ctrl+Drag to remove. Esc to exit."));
}

void FHexTerrainEditMode::Exit()
{
	bIsEditing = false;
	bCursorValid = false;
	SelectionCells.Empty();
	CachedTerrain.Reset();
	CachedHexCoord.Reset();
	FEdMode::Exit();
}

// ============================================================================
// Box-compute helper: all hex coords within axis-aligned bounding box
// ============================================================================
TArray<FHexCoord> FHexTerrainEditMode::GetCellsInBox(const FHexCoord& A, const FHexCoord& B)
{
	TArray<FHexCoord> Out;
	const int32 Q0 = FMath::Min(A.Q, B.Q), Q1 = FMath::Max(A.Q, B.Q);
	const int32 R0 = FMath::Min(A.R, B.R), R1 = FMath::Max(A.R, B.R);
	for (int32 Q = Q0; Q <= Q1; ++Q)
		for (int32 R = R0; R <= R1; ++R)
			Out.Add(FHexCoord(Q, R));
	return Out;
}

// ============================================================================
// Cursor → hex projection (ray-plane intersection)
// ============================================================================
bool FHexTerrainEditMode::CursorToHex(
	FEditorViewportClient* ViewportClient, int32 X, int32 Y,
	AHexTerrain*& OutTerrain, FHexCoord& OutCoord)
{
	OutTerrain = nullptr;
	if (!ViewportClient) return false;

	UWorld* World = ViewportClient->GetWorld();
	if (!World) return false;

	for (TActorIterator<AHexTerrain> It(World); It; ++It) { OutTerrain = *It; break; }
	if (!OutTerrain) return false;

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		ViewportClient->Viewport, ViewportClient->GetScene(), ViewportClient->EngineShowFlags)
		.SetRealtimeUpdate(ViewportClient->IsRealtime()));
	FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
	if (!View) return false;

	FViewportCursorLocation Cursor(View, ViewportClient, X, Y);
	const FVector O = Cursor.GetOrigin(), D = Cursor.GetDirection();
	if (FMath::IsNearlyZero(D.Z)) return false;
	const float T = (OutTerrain->GetActorLocation().Z - O.Z) / D.Z;
	if (T < 0.0f) return false;

	OutCoord = FHexGeometry::WorldToHex(O + D * T, OutTerrain->CellRadius);
	return true;
}

// ============================================================================
// Batch add / remove for a list of cells
// ============================================================================
void FHexTerrainEditMode::ExecuteCells(AHexTerrain* Terrain, const TArray<FHexCoord>& Cells, bool bRemoveOnly)
{
	if (!Terrain || Cells.Num() == 0) return;

	int32 Added = 0, Removed = 0;
	for (const FHexCoord& Coord : Cells)
	{
		const bool bExists = Terrain->HasCell(Coord);
		if (bRemoveOnly)
		{
			// Ctrl+drag: only remove existing cells
			if (!bExists) continue;
			if (Terrain->ExtraCells.Contains(Coord))
				{ Terrain->ExtraCells.Remove(Coord); ++Removed; }
			else
				{ Terrain->RemovedCells.Add(Coord); ++Removed; }
		}
		else
		{
			// Plain drag: toggle — add empty cells, remove existing cells
			if (bExists)
			{
				if (Terrain->ExtraCells.Contains(Coord))
					{ Terrain->ExtraCells.Remove(Coord); ++Removed; }
				else
					{ Terrain->RemovedCells.Add(Coord); ++Removed; }
			}
			else
			{
				if (Terrain->RemovedCells.Contains(Coord))
					{ Terrain->RemovedCells.Remove(Coord); ++Added; }
				else
					{ Terrain->ExtraCells.Add(Coord); ++Added; }
			}
		}
	}
	Terrain->RegenerateTerrain();
	UE_LOG(LogTemp, Log, TEXT("HexTerrainEdit: %d added, %d removed (%d cells in box)"),
		Added, Removed, Cells.Num());
}

// ============================================================================
// Input
// ============================================================================
bool FHexTerrainEditMode::MouseMove(
	FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 X, int32 Y)
{
	if (!ViewportClient) return false;

	AHexTerrain* Terrain = nullptr;
	FHexCoord Coord;

	if (CursorToHex(ViewportClient, X, Y, Terrain, Coord))
	{
		bCursorValid = true;
		CachedTerrain = Terrain;
		CachedHexCoord = Coord;
		CachedCursorWorldPos = FHexGeometry::HexToWorld(Coord, Terrain->CellRadius);

		if (bIsEditing)
		{
			// Update selection box (don't apply until mouse up)
			SelectionCells = GetCellsInBox(DragStartCoord, Coord);
		}
		else
		{
			SelectionCells = { Coord };
		}
	}
	else
	{
		bCursorValid = false;
		CachedTerrain.Reset();
		CachedHexCoord.Reset();
		if (bIsEditing) SelectionCells = GetCellsInBox(DragStartCoord, CachedHexCoord.Get(FHexCoord(0,0)));
	}

	return bCursorValid;
}

bool FHexTerrainEditMode::StartTracking(
	FEditorViewportClient* ViewportClient, FViewport* Viewport)
{
	if (!Viewport->KeyState(EKeys::LeftMouseButton)) return false;
	if (!bCursorValid || !CachedHexCoord.IsSet()) return false;

	bIsEditing = true;
	bDragRemove = Viewport->KeyState(EKeys::LeftControl) ||
	              Viewport->KeyState(EKeys::RightControl);
	DragStartCoord = *CachedHexCoord;
	SelectionCells = { DragStartCoord };

	return true;
}

bool FHexTerrainEditMode::EndTracking(
	FEditorViewportClient* ViewportClient, FViewport* Viewport)
{
	bIsEditing = false;
	if (SelectionCells.Num() > 0)
	{
		AHexTerrain* Terrain = CachedTerrain.Get();
		ExecuteCells(Terrain, SelectionCells, bDragRemove);
		SelectionCells.Empty();
	}
	return true;
}

bool FHexTerrainEditMode::InputKey(
	FEditorViewportClient* ViewportClient, FViewport* Viewport,
	FKey Key, EInputEvent Event)
{
	if (Key == EKeys::RightMouseButton || Key == EKeys::MiddleMouseButton ||
	    Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown)
		return false;

	if (Key == EKeys::Escape && Event == IE_Pressed)
	{
		if (GetModeManager()) GetModeManager()->ActivateDefaultMode();
		return true;
	}
	return false;
}

bool FHexTerrainEditMode::InputDelta(
	FEditorViewportClient* ViewportClient, FViewport* Viewport,
	FVector& Drag, FRotator& Rot, FVector& Scale)
{
	return bIsEditing;
}

// ============================================================================
// Render: single-cell hover + box-selection preview
// ============================================================================
void FHexTerrainEditMode::Render(
	const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);
	if (!bCursorValid) return;

	AHexTerrain* Terrain = CachedTerrain.Get();
	if (!Terrain) return;

	const float CellR = Terrain->CellRadius * 0.95f;

	// Draw all cells in the selection box
	for (const FHexCoord& Coord : SelectionCells)
	{
		const FVector Center = FHexGeometry::HexToWorld(Coord, Terrain->CellRadius);
		const TArray<FVector> Corners = FHexGeometry::GetHexCornersAt(Center, CellR);

		const bool bExists = Terrain->HasCell(Coord);
		const FColor Color = bExists
			? FColor(255, 100, 100, 140)   // red-ish — exists, Ctrl+drag will remove
			: FColor(100, 255, 100, 140);  // green — will be added

		for (int32 i = 0; i < Corners.Num(); ++i)
		{
			const int32 Next = (i + 1) % Corners.Num();
			PDI->DrawLine(Corners[i], Corners[Next], Color, SDPG_Foreground,
				bIsEditing ? 2.0f : 1.5f);
		}
	}
}

#endif // WITH_EDITOR
