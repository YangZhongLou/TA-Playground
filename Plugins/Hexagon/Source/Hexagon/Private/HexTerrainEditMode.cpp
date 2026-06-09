// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HexTerrainEditMode.h"

#if WITH_EDITOR

#include "AHexTerrain.h"
#include "HexTerrainGenerator.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "EngineUtils.h"
#include "SceneView.h"

const FEditorModeID FHexTerrainEditMode::EM_HexTerrainEdit("Hexagon.HexTerrainEdit");

// ============================================================================
// Construction
// ============================================================================
FHexTerrainEditMode::FHexTerrainEditMode() = default;
FHexTerrainEditMode::~FHexTerrainEditMode() = default;

// ============================================================================
// Enter / Exit
// ============================================================================
void FHexTerrainEditMode::Enter()
{
	FEdMode::Enter();

	// Find and select the terrain actor
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AHexTerrain> It(World); It; ++It)
		{
			Terrain = *It;
			if (GEditor)
			{
				GEditor->SelectNone(false, true);
				GEditor->SelectActor(*It, true, false);
			}
			break;
		}
	}

	ClearSelection();
	State = EState::Idle;

	UE_LOG(LogTemp, Log, TEXT("HexTerrainEdit: Click empty=Add | Click cell=Select | Shift=Multi | Del=Delete | Esc=Exit"));
}

void FHexTerrainEditMode::Exit()
{
	State = EState::Idle;
	bCursorValid = false;
	ClearSelection();
	Terrain.Reset();
	FEdMode::Exit();
}

// ============================================================================
// Hit-testing
// ============================================================================
bool FHexTerrainEditMode::HitTest(
	FEditorViewportClient* VC, int32 X, int32 Y,
	FHexCoord& OutCoord, bool& bOutOnCell)
{
	bOutOnCell = false;

	AHexTerrain* T = Terrain.Get();
	if (!T || !VC) return false;

	// Build scene view for deprojection
	FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(
		VC->Viewport, VC->GetScene(), VC->EngineShowFlags)
		.SetRealtimeUpdate(VC->IsRealtime()));
	FSceneView* View = VC->CalcSceneView(&Family);
	if (!View) return false;

	// Deproject cursor to world ray
	FViewportCursorLocation Cursor(View, VC, X, Y);
	const FVector Origin = Cursor.GetOrigin();
	const FVector Dir = Cursor.GetDirection();
	if (FMath::IsNearlyZero(Dir.Z)) return false;

	// Intersect with terrain XY plane
	const float TParam = (T->GetActorLocation().Z - Origin.Z) / Dir.Z;
	if (TParam < 0.0f) return false;

	const FVector Hit = Origin + Dir * TParam;
	OutCoord = FHexGeometry::WorldToHex(Hit, T->CellRadius);
	bOutOnCell = T->HasCell(OutCoord);
	return true;
}

// ============================================================================
// Geometry
// ============================================================================
TArray<FHexCoord> FHexTerrainEditMode::BoxCoords(const FHexCoord& A, const FHexCoord& B)
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
// Selection
// ============================================================================
void FHexTerrainEditMode::SelectOnly(const TArray<FHexCoord>& Coords)
{
	Selected.Empty();
	for (const FHexCoord& C : Coords)
	{
		AHexTerrain* T = Terrain.Get();
		if (T && T->HasCell(C))
			Selected.AddUnique(C);
	}
	UE_LOG(LogTemp, Log, TEXT("HexTerrainEdit: %d cells selected"), Selected.Num());
}

void FHexTerrainEditMode::ToggleSelection(const TArray<FHexCoord>& Coords)
{
	AHexTerrain* T = Terrain.Get();
	if (!T) return;

	for (const FHexCoord& C : Coords)
	{
		if (!T->HasCell(C)) continue;
		if (Selected.Contains(C))
			Selected.Remove(C);
		else
			Selected.AddUnique(C);
	}
	UE_LOG(LogTemp, Log, TEXT("HexTerrainEdit: %d cells selected"), Selected.Num());
}

void FHexTerrainEditMode::ClearSelection()
{
	Selected.Empty();
}

// ============================================================================
// Terrain mutations
// ============================================================================
void FHexTerrainEditMode::AddCells(const TArray<FHexCoord>& Coords)
{
	AHexTerrain* T = Terrain.Get();
	if (!T || Coords.Num() == 0) return;

	// Mark cells for creation and set their terrain type
	T->bManualMode = true;
	for (const FHexCoord& C : Coords)
	{
		if (T->RemovedCells.Contains(C))
			T->RemovedCells.Remove(C);
		if (!T->HasCell(C))
			T->ExtraCells.Add(C);
		T->ManualCellTypes.Add(C, T->BrushTerrainType);
	}

	// Full regeneration handles both ExtraCells and type overrides
	T->RegenerateTerrain();
	UE_LOG(LogTemp, Log, TEXT("HexTerrainEdit: added %d cells"), Coords.Num());
}

void FHexTerrainEditMode::DeleteCells(const TArray<FHexCoord>& Coords)
{
	AHexTerrain* T = Terrain.Get();
	if (!T || Coords.Num() == 0) return;

	for (const FHexCoord& C : Coords)
	{
		if (T->ExtraCells.Contains(C))
			T->ExtraCells.Remove(C);
		else if (T->HasCell(C))
			T->RemovedCells.Add(C);
	}
	T->RegenerateTerrain();
	UE_LOG(LogTemp, Log, TEXT("HexTerrainEdit: deleted %d cells"), Coords.Num());
}

// ============================================================================
// Mouse input — state machine
// ============================================================================
bool FHexTerrainEditMode::MouseMove(
	FEditorViewportClient* VC, FViewport* VP, int32 X, int32 Y)
{
	// Update cursor tracking regardless of state
	if (HitTest(VC, X, Y, CursorCoord, bCursorOnCell))
	{
		bCursorValid = true;
	}
	else
	{
		bCursorValid = false;
		bCursorOnCell = false;
	}
	return bCursorValid;
}

bool FHexTerrainEditMode::StartTracking(
	FEditorViewportClient* VC, FViewport* VP)
{
	// Only handle left mouse button
	if (!VP->KeyState(EKeys::LeftMouseButton)) return false;
	if (!bCursorValid) return false;

	// Transition to drag state — decided by what's under cursor
	if (bCursorOnCell)
	{
		State = EState::DraggingSelect;
	}
	else
	{
		State = EState::DraggingAdd;
	}

	DragAnchor = CursorCoord;
	DragCells = { CursorCoord };
	return true;
}

bool FHexTerrainEditMode::CapturedMouseMove(
	FEditorViewportClient* VC, FViewport* VP, int32 X, int32 Y)
{
	// Only process during drag
	if (State != EState::DraggingAdd && State != EState::DraggingSelect)
		return false;

	FHexCoord Coord;
	bool bDummy;
	if (HitTest(VC, X, Y, Coord, bDummy))
	{
		DragCells = BoxCoords(DragAnchor, Coord);
	}
	return true;
}

bool FHexTerrainEditMode::EndTracking(
	FEditorViewportClient* VC, FViewport* VP)
{
	if (DragCells.Num() == 0) return true;

	const bool bShift = VP->KeyState(EKeys::LeftShift) ||
	                    VP->KeyState(EKeys::RightShift);

	switch (State)
	{
	case EState::DraggingAdd:
		// Creating new cells — never affected by Shift
		AddCells(DragCells);
		break;

	case EState::DraggingSelect:
		if (bShift)
			ToggleSelection(DragCells);
		else
			SelectOnly(DragCells);
		break;

	default:
		break;
	}

	State = EState::Idle;
	DragCells.Empty();
	return true;
}

// ============================================================================
// Keyboard input
// ============================================================================
bool FHexTerrainEditMode::InputKey(
	FEditorViewportClient* VC, FViewport* VP,
	FKey Key, EInputEvent Event)
{
	// Pass through camera controls
	if (Key == EKeys::RightMouseButton || Key == EKeys::MiddleMouseButton ||
	    Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown)
		return false;

	// Delete selected cells
	if (Key == EKeys::Delete && Event == IE_Pressed && Selected.Num() > 0)
	{
		DeleteCells(Selected);
		ClearSelection();
		return true;
	}

	// ESC: clear selection first, then exit mode
	if (Key == EKeys::Escape && Event == IE_Pressed)
	{
		if (Selected.Num() > 0)
		{
			ClearSelection();
			UE_LOG(LogTemp, Log, TEXT("HexTerrainEdit: selection cleared"));
		}
		else
		{
			if (GetModeManager()) GetModeManager()->ActivateDefaultMode();
		}
		return true;
	}

	return false;
}

bool FHexTerrainEditMode::InputDelta(
	FEditorViewportClient* VC, FViewport* VP,
	FVector& Drag, FRotator& Rot, FVector& Scale)
{
	// Block camera movement while dragging
	return (State == EState::DraggingAdd || State == EState::DraggingSelect);
}

// ============================================================================
// Render — visual feedback
// ============================================================================
void FHexTerrainEditMode::DrawHexOutline(
	const FHexCoord& C, const FColor& Color, float Thickness,
	FPrimitiveDrawInterface* PDI)
{
	AHexTerrain* T = Terrain.Get();
	if (!T) return;

	const FVector Center = FHexGeometry::HexToWorld(C, T->CellRadius);
	const TArray<FVector> Corners = FHexGeometry::GetHexCornersAt(Center, T->CellRadius * 0.95f);
	for (int32 i = 0; i < Corners.Num(); ++i)
		PDI->DrawLine(Corners[i], Corners[(i+1) % Corners.Num()], Color, SDPG_Foreground, Thickness);
}

void FHexTerrainEditMode::Render(
	const FSceneView* View, FViewport* VP, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, VP, PDI);
	if (!bCursorValid) return;

	// 1. Selected cells — thick yellow
	for (const FHexCoord& C : Selected)
		DrawHexOutline(C, FColor(255, 220, 40), 3.0f, PDI);

	// 2. Drag preview — green (add) or blue (select)
	if (State == EState::DraggingAdd || State == EState::DraggingSelect)
	{
		const FColor PreviewColor = (State == EState::DraggingAdd)
			? FColor(80, 255, 80)   // green = adding
			: FColor(80, 200, 255); // blue = selecting

		for (const FHexCoord& C : DragCells)
		{
			if (!Selected.Contains(C))
				DrawHexOutline(C, PreviewColor, 2.0f, PDI);
		}
	}

	// 3. Cursor highlight — only when idle
	if (State == EState::Idle && !Selected.Contains(CursorCoord))
	{
		const FColor CursorColor = bCursorOnCell
			? FColor(255, 255, 100)  // yellow = on existing cell
			: FColor(100, 255, 100); // green = empty space
		DrawHexOutline(CursorCoord, CursorColor, 1.5f, PDI);
	}
}

#endif // WITH_EDITOR
