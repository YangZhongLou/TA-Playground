// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "EdMode.h"
#include "HexTerrainGenerator.h"
#include "HexGeometry.h"

class AHexTerrain;

/**
 * Editor mode for hex terrain cell editing.
 *
 * Left-click/drag  = select cells (yellow outline)
 * Shift+drag       = toggle/add to multi-selection
 * Delete key       = remove selected cells
 * Esc              = clear selection (1st) / exit mode (2nd)
 *
 * To add cells: switch to "Hex Terrain Paint" mode, or use hex.PaintCell console command.
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
	void DeleteSelected(AHexTerrain* Terrain);

	enum class EDragOp : uint8 { Add, Select };
	EDragOp DragOp = EDragOp::Select;

	bool bIsEditing = false;
	FHexCoord DragStartCoord = FHexCoord(0, 0);
	TArray<FHexCoord> PreviewCells;
	TArray<FHexCoord> SelectedCells;

	TWeakObjectPtr<AHexTerrain> CachedTerrain;
	TOptional<FHexCoord> CachedHexCoord;
	FVector CachedCursorWorldPos = FVector::ZeroVector;
	bool bCursorValid = false;
};

#endif // WITH_EDITOR
