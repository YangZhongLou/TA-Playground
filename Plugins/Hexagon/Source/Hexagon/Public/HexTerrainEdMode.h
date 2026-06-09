// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "EdMode.h"

#include "HexGeometry.h"
class AHexTerrain;
class FHexTerrainEdModeToolkit;

/**
 * Editor mode for painting terrain types onto a HexTerrain.
 *
 * Usage:
 *   1. Select an AHexTerrain actor in the level.
 *   2. Switch to "Hex Terrain Paint" mode (available in the Modes dropdown).
 *   3. Left-click / drag to paint; Ctrl+Left-click to erase.
 *   4. Brush settings are on the AHexTerrain Details panel (Hexagon|Brush).
 */
class FHexTerrainEdMode : public FEdMode
{
public:
	static const FEditorModeID EM_HexTerrainPaint;

	FHexTerrainEdMode();
	virtual ~FHexTerrainEdMode();

	// ---- FEdMode overrides ----
	virtual void Enter() override;
	virtual void Exit() override;
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 X, int32 Y) override;
	virtual bool StartTracking(FEditorViewportClient* ViewportClient, FViewport* Viewport) override;
	virtual bool EndTracking(FEditorViewportClient* ViewportClient, FViewport* Viewport) override;
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual bool InputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& Drag, FRotator& Rot, FVector& Scale) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual bool UsesToolkits() const override { return true; }
	virtual bool CapturedMouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 X, int32 Y) override;

	/** Get the toolkit widget. */
	TSharedPtr<FHexTerrainEdModeToolkit> GetToolkit() const;

private:
	/** Line-trace to find the hex cell under the cursor. */
	bool HitTestTerrain(FEditorViewportClient* ViewportClient, int32 X, int32 Y, AHexTerrain*& OutTerrain, FHexCoord& OutCoord);

	/** Get all hex coords within brush radius of a center cell. */
	TArray<FHexCoord> GetCellsInBrush(AHexTerrain* Terrain, const FHexCoord& Center, float RadiusInGridUnits);

	/** Apply paint at current cursor position. */
	void PaintAtCursor(FEditorViewportClient* ViewportClient, FViewport* Viewport);

	/** Whether we are actively painting (mouse down / dragging). */
	bool bIsPainting = false;

	/** Cached world-space position of the cursor (updated on mouse move). */
	FVector CachedCursorWorldPos = FVector::ZeroVector;

	/** Whether the cursor is over a valid terrain cell. */
	bool bCursorOnTerrain = false;

	/** Cached terrain actor under cursor. */
	TWeakObjectPtr<AHexTerrain> CachedTerrain;

	/** Cached hex coord under cursor. */
	TOptional<FHexCoord> CachedHexCoord;
};

#endif // WITH_EDITOR
