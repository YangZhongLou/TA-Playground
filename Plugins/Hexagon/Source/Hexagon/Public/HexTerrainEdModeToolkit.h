// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"

class FHexTerrainEdMode;
class AHexTerrain;
class IDetailsView;

/**
 * Standalone toolkit for Hex Terrain Paint mode.
 * Shows Brush and Material properties in a dedicated panel.
 */
class FHexTerrainEdModeToolkit : public FModeToolkit
{
public:
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost) override;

	virtual FName GetToolkitFName() const override { return FName("HexTerrainPaintToolkit"); }
	virtual FText GetBaseToolkitName() const override { return NSLOCTEXT("Hexagon", "HexTerrainPaintToolkit", "Hex Terrain Paint"); }
	virtual FEdMode* GetEditorMode() const override;
	virtual TSharedPtr<SWidget> GetInlineContent() const override;

private:
	TSharedPtr<IDetailsView> DetailsPanel;
	mutable TWeakObjectPtr<AHexTerrain> LastTerrain;

	/** Build the details panel showing brush settings. */
	TSharedRef<SWidget> BuildPanel();
};

#endif // WITH_EDITOR
