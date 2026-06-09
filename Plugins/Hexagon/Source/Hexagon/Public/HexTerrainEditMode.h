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
 *   3. Left-click/drag to add cells.
 *   4. Shift+drag to select cells → Del key to delete selected.
 *   5. Esc: clear selection (1st) / exit mode (2nd).
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
	virtual bool CapturedMouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 X, int32 Y) override;
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual bool InputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& Drag, FRotator& Rot, FVector& Scale) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual bool UsesToolkits() const override { return false; }

private:
	bool CursorToHex(FEditorViewportClient* ViewportClient, int32 X, int32 Y,
		AHexTerrain*& OutTerrain, FHexCoord& OutCoord);
	static TArray<FHexCoord> GetCellsInBox(const FHexCoord& A, const FHexCoord& B);
	void AddCells(AHexTerrain* Terrain, const TArray<FHexCoord>& Cells);
	void DeleteSelected(AHexTerrain* Terrain);

	/** Drag mode: Add or Select. */
	enum class EDragOp : uint8 { Add, Select };
	EDragOp DragOp = EDragOp::Add;

	bool bIsEditing = false;
	FHexCoord DragStartCoord = FHexCoord(0, 0);
	TArray<FHexCoord> PreviewCells;   // cells in current drag box
	TArray<FHexCoord> SelectedCells;  // selected cells (for deletion)

	TWeakObjectPtr<AHexTerrain> CachedTerrain;
	TOptional<FHexCoord> CachedHexCoord;
	FVector CachedCursorWorldPos = FVector::ZeroVector;
	bool bCursorValid = false;
};

#endif // WITH_EDITOR
