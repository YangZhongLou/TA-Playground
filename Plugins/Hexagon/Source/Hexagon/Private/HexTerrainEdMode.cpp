// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HexTerrainEdMode.h"

#if WITH_EDITOR

#include "AHexTerrain.h"
#include "HexTerrainGenerator.h"
#include "HexGeometry.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "SceneView.h"
#include "CollisionQueryParams.h"

// ============================================================================
// Mode ID
// ============================================================================
const FEditorModeID FHexTerrainEdMode::EM_HexTerrainPaint("Hexagon.HexTerrainPaint");

// ============================================================================
// Construction
// ============================================================================
FHexTerrainEdMode::FHexTerrainEdMode()
{
}

FHexTerrainEdMode::~FHexTerrainEdMode()
{
}

// ============================================================================
// Enter / Exit
// ============================================================================
void FHexTerrainEdMode::Enter()
{
	FEdMode::Enter();

	// Auto-select the first AHexTerrain so the Details panel shows brush settings
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AHexTerrain> It(World); It; ++It)
		{
			if (GEditor)
			{
				GEditor->SelectNone(false, true);
				GEditor->SelectActor(*It, true, false);
			}
			break;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("HexTerrainPaint mode entered. Left-click to paint, Ctrl+Left-click to erase."));
}

void FHexTerrainEdMode::Exit()
{
	bIsPainting = false;
	bCursorOnTerrain = false;
	CachedTerrain.Reset();
	CachedHexCoord.Reset();

	FEdMode::Exit();
}

// ============================================================================
// Hit Testing
// ============================================================================
bool FHexTerrainEdMode::HitTestTerrain(
	FEditorViewportClient* ViewportClient,
	int32 X, int32 Y,
	AHexTerrain*& OutTerrain,
	FHexCoord& OutCoord)
{
	OutTerrain = nullptr;

	if (!ViewportClient) return false;

	UWorld* World = ViewportClient->GetWorld();
	if (!World) return false;

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		ViewportClient->Viewport,
		ViewportClient->GetScene(),
		ViewportClient->EngineShowFlags)
		.SetRealtimeUpdate(ViewportClient->IsRealtime()));
	FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
	if (!View) return false;

	FViewportCursorLocation Cursor(View, ViewportClient, X, Y);
	const FVector WorldOrigin = Cursor.GetOrigin();
	const FVector WorldDirection = Cursor.GetDirection();

	// Line trace against the world
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;

	if (World->LineTraceSingleByChannel(
		Hit,
		WorldOrigin,
		WorldOrigin + WorldDirection * 100000.0f,
		ECC_WorldStatic,
		Params))
	{
		// Check if the hit actor is or belongs to an AHexTerrain
		if (AActor* HitActor = Hit.GetActor())
		{
			if (AHexTerrain* Terrain = Cast<AHexTerrain>(HitActor))
			{
				OutTerrain = Terrain;
			}
			else if (UActorComponent* Comp = Hit.GetComponent())
			{
				OutTerrain = Cast<AHexTerrain>(Comp->GetOwner());
			}
		}

		if (OutTerrain)
		{
			OutCoord = FHexGeometry::WorldToHex(Hit.Location, OutTerrain->CellRadius);
			return true;
		}
	}

	return false;
}

// ============================================================================
// Brush helpers
// ============================================================================
TArray<FHexCoord> FHexTerrainEdMode::GetCellsInBrush(
	AHexTerrain* Terrain,
	const FHexCoord& Center,
	float RadiusInGridUnits)
{
	if (!Terrain) return {};

	const int32 RingRadius = FMath::RoundToInt32(RadiusInGridUnits);
	return FHexGeometry::GetSpiral(Center, RingRadius);
}

// ============================================================================
// Paint
// ============================================================================
void FHexTerrainEdMode::PaintAtCursor(
	FEditorViewportClient* ViewportClient,
	FViewport* Viewport)
{
	if (!bCursorOnTerrain || !CachedHexCoord.IsSet()) return;

	AHexTerrain* Terrain = CachedTerrain.Get();
	if (!Terrain) return;

	const TArray<FHexCoord> Cells = GetCellsInBrush(Terrain, *CachedHexCoord, Terrain->BrushRadius);

	// Ctrl = erase (reset to default type), otherwise paint
	const bool bErase = ViewportClient->Viewport->KeyState(EKeys::LeftControl) ||
	                    ViewportClient->Viewport->KeyState(EKeys::RightControl);

	const EHexTerrainType TargetType = bErase
		? Terrain->DefaultManualType
		: Terrain->BrushTerrainType;

	Terrain->PaintCells(Cells, TargetType);
}

// ============================================================================
// Input handling
// ============================================================================
bool FHexTerrainEdMode::MouseMove(
	FEditorViewportClient* ViewportClient,
	FViewport* Viewport,
	int32 X, int32 Y)
{
	if (!ViewportClient) return false;

	AHexTerrain* Terrain = nullptr;
	FHexCoord Coord;

	if (HitTestTerrain(ViewportClient, X, Y, Terrain, Coord))
	{
		bCursorOnTerrain = true;
		CachedTerrain = Terrain;
		CachedHexCoord = Coord;

		if (Terrain)
		{
			CachedCursorWorldPos = FHexGeometry::HexToWorld(Coord, Terrain->CellRadius);
		}

		// If painting (mouse held down), continue painting at new position
		if (bIsPainting)
		{
			PaintAtCursor(ViewportClient, Viewport);
		}
	}
	else
	{
		bCursorOnTerrain = false;
		CachedTerrain.Reset();
		CachedHexCoord.Reset();
	}

	return bCursorOnTerrain;
}

bool FHexTerrainEdMode::StartTracking(
	FEditorViewportClient* ViewportClient,
	FViewport* Viewport)
{
	if (bCursorOnTerrain)
	{
		bIsPainting = true;
		PaintAtCursor(ViewportClient, Viewport);
	}
	return bIsPainting;
}

bool FHexTerrainEdMode::EndTracking(
	FEditorViewportClient* ViewportClient,
	FViewport* Viewport)
{
	bIsPainting = false;
	return true;
}

bool FHexTerrainEdMode::InputKey(
	FEditorViewportClient* ViewportClient,
	FViewport* Viewport,
	FKey Key,
	EInputEvent Event)
{
	// Let base class handle camera movement keys
	if (Key == EKeys::RightMouseButton ||
	    Key == EKeys::MiddleMouseButton ||
	    Key == EKeys::MouseScrollUp ||
	    Key == EKeys::MouseScrollDown)
	{
		return false;
	}

	// Esc exits the mode
	if (Key == EKeys::Escape && Event == IE_Pressed)
	{
		if (GEditor)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		}
		// Deactivate this mode — return to default
		if (GetModeManager())
		{
			GetModeManager()->ActivateDefaultMode();
		}
		return true;
	}

	return false;
}

bool FHexTerrainEdMode::InputDelta(
	FEditorViewportClient* ViewportClient,
	FViewport* Viewport,
	FVector& Drag,
	FRotator& Rot,
	FVector& Scale)
{
	// Consume input while painting so the camera doesn't move
	if (bIsPainting)
	{
		return true;
	}
	return false;
}

// ============================================================================
// Render brush preview
// ============================================================================
void FHexTerrainEdMode::Render(
	const FSceneView* View,
	FViewport* Viewport,
	FPrimitiveDrawInterface* PDI)
{
	FEdMode::Render(View, Viewport, PDI);

	if (!bCursorOnTerrain || !CachedTerrain.IsValid() || !CachedHexCoord.IsSet())
	{
		return;
	}

	AHexTerrain* Terrain = CachedTerrain.Get();
	if (!Terrain) return;

	const float CellRadius = Terrain->CellRadius;
	const float BrushR = Terrain->BrushRadius;

	const FVector Center = CachedCursorWorldPos;
	const FColor BrushColor = bIsPainting
		? FColor(255, 180, 0)    // Orange when painting
		: FColor(50, 180, 255); // Cyan when hovering

	const float DrawRadius = BrushR * CellRadius * UE_SQRT_3;

	// Draw concentric circles for the brush preview
	for (int32 Ring = 0; Ring < 3; ++Ring)
	{
		const float R = DrawRadius * (1.0f - Ring * 0.3f);
		// Manual circle drawing (PDI::DrawCircle not available in UE 5.7)
		for (int32 Seg = 0; Seg < 32; ++Seg) { const float A0 = UE_TWO_PI * Seg / 32.0f; const float A1 = UE_TWO_PI * (Seg + 1) / 32.0f; PDI->DrawLine(FVector(Center.X + FMath::Cos(A0)*R, Center.Y + FMath::Sin(A0)*R, Center.Z), FVector(Center.X + FMath::Cos(A1)*R, Center.Y + FMath::Sin(A1)*R, Center.Z), BrushColor, SDPG_Foreground, 1.5f, 0, true); }
	}

	// Draw hex cell outlines for cells in the brush (only for small brush sizes)
	if (BrushR <= 5.0f)
	{
		const TArray<FHexCoord> Cells = GetCellsInBrush(Terrain, *CachedHexCoord, BrushR);
		for (const FHexCoord& Cell : Cells)
		{
			const FVector CellPos = FHexGeometry::HexToWorld(Cell, CellRadius);
			const TArray<FVector> Corners = FHexGeometry::GetHexCornersAt(CellPos, CellRadius * 0.95f);

			for (int32 i = 0; i < Corners.Num(); ++i)
			{
				const int32 Next = (i + 1) % Corners.Num();
				PDI->DrawLine(Corners[i], Corners[Next],
					FColor::White, SDPG_Foreground, 1.0f);
			}
		}
	}
}

#endif // WITH_EDITOR
