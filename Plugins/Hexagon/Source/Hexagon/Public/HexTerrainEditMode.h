// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "EdMode.h"
#include "HexTerrainGenerator.h"
#include "HexGeometry.h"

class AHexTerrain;

/**
 * Hex terrain cell editor — state machine.
 *
 * ┌──────────STATE──────────┬───TRIGGER───┬───ACTION────────────────────┐
 * │ Idle                    │ hover cell   │ show cursor outline          │
 * │ Idle                    │ L-click cell │ select single cell           │
 * │ Idle                    │ L-drag cell  │ box-select cells             │
 * │ Idle                    │ L-click empty│ create single cell           │
 * │ Idle                    │ L-drag empty │ box-fill cells               │
 * │ ActiveSelection         │ Shift+drag   │ toggle cells in/out          │
 * │ ActiveSelection         │ Del key      │ delete selected cells        │
 * │ ActiveSelection         │ Esc          │ clear selection → idle       │
 * │ Any                     │ Esc (no sel) │ exit mode                    │
 * └─────────────────────────┴──────────────┴─────────────────────────────┘
 */
class FHexTerrainEditMode : public FEdMode
{
public:
	static const FEditorModeID EM_HexTerrainEdit;

	FHexTerrainEditMode();
	virtual ~FHexTerrainEditMode();

	// --- FEdMode ---
	virtual void Enter() override;
	virtual void Exit() override;
	virtual bool MouseMove(FEditorViewportClient* VC, FViewport* VP, int32 X, int32 Y) override;
	virtual bool StartTracking(FEditorViewportClient* VC, FViewport* VP) override;
	virtual bool EndTracking(FEditorViewportClient* VC, FViewport* VP) override;
	virtual bool CapturedMouseMove(FEditorViewportClient* VC, FViewport* VP, int32 X, int32 Y) override;
	virtual bool InputKey(FEditorViewportClient* VC, FViewport* VP, FKey Key, EInputEvent Event) override;
	virtual bool InputDelta(FEditorViewportClient* VC, FViewport* VP, FVector& D, FRotator& R, FVector& S) override;
	virtual void Render(const FSceneView* View, FViewport* VP, FPrimitiveDrawInterface* PDI) override;
	virtual bool UsesToolkits() const override { return false; }

private:
	// ── Hit testing ─────────────────────────────────────────────
	/** Project viewport coords to hex grid. Returns true, fills OutCoord + bOutOnCell. */
	bool HitTest(FEditorViewportClient* VC, int32 X, int32 Y,
		FHexCoord& OutCoord, bool& bOutOnCell);

	// ── Geometry helpers ────────────────────────────────────────
	static TArray<FHexCoord> BoxCoords(const FHexCoord& A, const FHexCoord& B);

	// ── Terrain mutations ───────────────────────────────────────
	void AddCells(const TArray<FHexCoord>& Coords);
	void DeleteCells(const TArray<FHexCoord>& Coords);

	// ── Selection ───────────────────────────────────────────────
	void SelectOnly(const TArray<FHexCoord>& Coords);
	void ToggleSelection(const TArray<FHexCoord>& Coords);
	void ClearSelection();

	// ── Visuals ─────────────────────────────────────────────────
	void DrawHexOutline(const FHexCoord& C, const FColor& Color, float W, FPrimitiveDrawInterface* PDI);

	// ── State ───────────────────────────────────────────────────
	enum class EState : uint8
	{
		Idle,             // nothing happening, cursor may be hovering
		DraggingAdd,      // mouse held down over empty space — adding cells
		DraggingSelect,   // mouse held down over cell — selecting cells
	};

	EState State = EState::Idle;

	// Cached terrain actor (looked up on Enter, refreshed on each hit-test)
	TWeakObjectPtr<AHexTerrain> Terrain;

	// Current cursor state
	bool  bCursorValid = false;
	bool  bCursorOnCell = false;
	FHexCoord CursorCoord;

	// Drag state (valid while DraggingAdd or DraggingSelect)
	FHexCoord DragAnchor;      // where the drag started
	TArray<FHexCoord> DragCells; // cells in the current drag box

	// Persistent selection (survives between drags)
	TArray<FHexCoord> Selected;
};

#endif // WITH_EDITOR
