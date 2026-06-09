// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "EdMode.h"

#include "HexGeometry.h"
class AHexTerrain;

/**
 * Editor mode for free-form terrain shape editing.
 *
 * Usage:
 *   1. Place an AHexTerrain in the level.
 *   2. Switch to "Hex Terrain Edit" mode (Modes dropdown).
 *   3. Hover: green hex = click to add, red hex = Ctrl+Click to remove.
 *   4. Click-drag: rectangle selection, release to add/remove all cells in box.
 */
class FHexTerrainEditMode : public FEdMode
{
public:
	static const FEditorModeID EM_HexTerrainEdit;

	FHexTerrainEditMode();
	virtual ~FHexTerrainEditMode();

	virtual void Enter() override;
	virtual void Exit() override;
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 X, int32 Y) override;
	virtual bool StartTracking(FEditorViewportClient* ViewportClient, FViewport* Viewport) override;
	virtual bool EndTracking(FEditorViewportClient* ViewportClient, FViewport* Viewport) override;
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual bool InputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& Drag, FRotator& Rot, FVector& Scale) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual bool UsesToolkits() const override { return false; }

private:
	bool CursorToHex(FEditorViewportClient* ViewportClient, int32 X, int32 Y,
		AHexTerrain*& OutTerrain, FHexCoord& OutCoord);

	static TArray<FHexCoord> GetCellsInBox(const FHexCoord& A, const FHexCoord& B);

	void ExecuteCells(AHexTerrain* Terrain, const TArray<FHexCoord>& Cells, bool bRemove);

	/** Mouse is down and we are actively editing. */
	bool bIsEditing = false;

	/** Whether Ctrl was held when drag started. */
	bool bDragRemove = false;

	/** Drag-start hex coord (box corner). */
	FHexCoord DragStartCoord = FHexCoord(0, 0);

	/** Cells in current selection box (drag start → cursor). */
	TArray<FHexCoord> SelectionCells;

	/** Cached terrain under cursor. */
	TWeakObjectPtr<AHexTerrain> CachedTerrain;

	/** Cached cursor hex coord (single-cell hover). */
	TOptional<FHexCoord> CachedHexCoord;

	/** World position at cached coord. */
	FVector CachedCursorWorldPos = FVector::ZeroVector;

	/** Whether cursor projects onto a valid terrain position. */
	bool bCursorValid = false;
};

#endif // WITH_EDITOR
