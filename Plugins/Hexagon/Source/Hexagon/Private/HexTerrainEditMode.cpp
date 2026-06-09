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
		for (TActorIterator<AHexTerrain> It(World); It; ++It)
			{ if (GEditor) { GEditor->SelectNone(false, true); GEditor->SelectActor(*It, true, false); } break; }
	UE_LOG(LogTemp, Log, TEXT("HexTerrainEdit: Left=Select  Shift=Multi-select  Del=Delete  Esc=Exit"));
}

void FHexTerrainEditMode::Exit()
{
	bIsEditing = false; bCursorValid = false;
	PreviewCells.Empty(); SelectedCells.Empty();
	CachedTerrain.Reset(); CachedHexCoord.Reset();
	FEdMode::Exit();
}

// ============================================================================
// Box helper
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
// Cursor to hex
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
// Delete selected
// ============================================================================
void FHexTerrainEditMode::DeleteSelected(AHexTerrain* Terrain)
{
	if (!Terrain || SelectedCells.Num() == 0) return;
	for (const FHexCoord& Coord : SelectedCells)
	{
		if (Terrain->ExtraCells.Contains(Coord))
			Terrain->ExtraCells.Remove(Coord);
		else if (Terrain->HasCell(Coord))
			Terrain->RemovedCells.Add(Coord);
	}
	Terrain->RegenerateTerrain();
	UE_LOG(LogTemp, Log, TEXT("HexTerrainEdit: deleted %d cells"), SelectedCells.Num());
	SelectedCells.Empty();
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
	}
	else
	{
		bCursorValid = false;
		CachedTerrain.Reset(); CachedHexCoord.Reset();
	}
	return bCursorValid;
}

bool FHexTerrainEditMode::StartTracking(
	FEditorViewportClient* ViewportClient, FViewport* Viewport)
{
	if (!Viewport->KeyState(EKeys::LeftMouseButton)) return false;
	if (!bCursorValid || !CachedHexCoord.IsSet()) return false;

	bIsEditing = true;
	DragStartCoord = *CachedHexCoord;
	DragOp = EDragOp::Select;
	PreviewCells = { DragStartCoord };
	return true;
}

bool FHexTerrainEditMode::CapturedMouseMove(
	FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 X, int32 Y)
{
	if (!bIsEditing || !ViewportClient) return false;
	AHexTerrain* Terrain = nullptr;
	FHexCoord Coord;
	if (CursorToHex(ViewportClient, X, Y, Terrain, Coord))
	{
		PreviewCells = GetCellsInBox(DragStartCoord, Coord);
		CachedHexCoord = Coord;
		CachedCursorWorldPos = FHexGeometry::HexToWorld(Coord, Terrain->CellRadius);
	}
	return true;
}

bool FHexTerrainEditMode::EndTracking(
	FEditorViewportClient* ViewportClient, FViewport* Viewport)
{
	bIsEditing = false;
	if (PreviewCells.Num() == 0) return true;

	AHexTerrain* Terrain = CachedTerrain.Get();
	if (!Terrain) return true;

	const bool bShift = Viewport->KeyState(EKeys::LeftShift) ||
	                    Viewport->KeyState(EKeys::RightShift);

	if (bShift)
	{
		// Shift+drag: toggle/add to multi-selection
		for (const FHexCoord& Coord : PreviewCells)
		{
			if (Terrain->HasCell(Coord))
			{
				if (SelectedCells.Contains(Coord))
					SelectedCells.Remove(Coord);
				else
					SelectedCells.AddUnique(Coord);
			}
		}
	}
	else
	{
		// No shift: replace selection
		SelectedCells.Empty();
		for (const FHexCoord& Coord : PreviewCells)
		{
			if (Terrain->HasCell(Coord))
				SelectedCells.AddUnique(Coord);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("HexTerrainEdit: %d cells selected (Del=delete, Shift=toggle)"), SelectedCells.Num());
	PreviewCells = { *CachedHexCoord };
	return true;
}

bool FHexTerrainEditMode::InputKey(
	FEditorViewportClient* ViewportClient, FViewport* Viewport,
	FKey Key, EInputEvent Event)
{
	if (Key == EKeys::RightMouseButton || Key == EKeys::MiddleMouseButton ||
	    Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown)
		return false;

	if (Key == EKeys::Delete && Event == IE_Pressed && SelectedCells.Num() > 0)
	{
		DeleteSelected(CachedTerrain.Get());
		return true;
	}

	if (Key == EKeys::Escape && Event == IE_Pressed)
	{
		if (SelectedCells.Num() > 0)
		{
			SelectedCells.Empty();
			UE_LOG(LogTemp, Log, TEXT("HexTerrainEdit: selection cleared"));
			return true;
		}
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
// Render
// ============================================================================
void FHexTerrainEditMode::Render(
	const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);
	if (!bCursorValid) return;

	AHexTerrain* Terrain = CachedTerrain.Get();
	if (!Terrain) return;

	const float CellR = Terrain->CellRadius * 0.95f;

	auto DrawHex = [&](const FHexCoord& Coord, const FColor& Color, float Thickness) {
		const FVector Center = FHexGeometry::HexToWorld(Coord, Terrain->CellRadius);
		const TArray<FVector> C = FHexGeometry::GetHexCornersAt(Center, CellR);
		for (int32 i = 0; i < C.Num(); ++i)
			PDI->DrawLine(C[i], C[(i+1)%C.Num()], Color, SDPG_Foreground, Thickness);
	};

	// Selected cells — yellow thick outline
	for (const FHexCoord& Coord : SelectedCells)
		DrawHex(Coord, FColor(255, 220, 40), 3.0f);

	// Drag preview cells — light blue
	const FColor PreviewColor(80, 200, 255);
	for (const FHexCoord& Coord : PreviewCells)
		if (!SelectedCells.Contains(Coord))
			DrawHex(Coord, PreviewColor, 2.0f);
}

#endif // WITH_EDITOR
