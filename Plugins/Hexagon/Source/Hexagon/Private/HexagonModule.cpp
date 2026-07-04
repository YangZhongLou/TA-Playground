// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HexagonModule.h"
#include "HexagonCommands.h"

#if WITH_EDITOR
#include "HexTerrainEdMode.h"
#include "HexTerrainEditMode.h"
#include "EditorModeRegistry.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "ToolMenuEntry.h"
#include "Framework/Commands/UIAction.h"
#include "AHexTerrain.h"  // for terrain command lambdas
#endif

// ============================================================================
// Editor mode registration
// ============================================================================
#if WITH_EDITOR
static void RegisterHexEditorModes()
{
	FEditorModeRegistry::Get().RegisterMode<FHexTerrainEdMode>(
		FHexTerrainEdMode::EM_HexTerrainPaint,
		NSLOCTEXT("Hexagon", "HexTerrainPaintMode", "Hex Terrain Paint"),
		FSlateIcon(), true);

	FEditorModeRegistry::Get().RegisterMode<FHexTerrainEditMode>(
		FHexTerrainEditMode::EM_HexTerrainEdit,
		NSLOCTEXT("Hexagon", "HexTerrainEditMode", "Hex Terrain Edit"),
		FSlateIcon(), true);

	UE_LOG(LogTemp, Log, TEXT("Hexagon: registered editor modes"));
}

static void UnregisterHexEditorModes()
{
	FEditorModeRegistry::Get().UnregisterMode(FHexTerrainEdMode::EM_HexTerrainPaint);
	FEditorModeRegistry::Get().UnregisterMode(FHexTerrainEditMode::EM_HexTerrainEdit);
}

// ============================================================================
// Editor menu registration
// ============================================================================
static bool GHexMenuRegistered = false;

static void RegisterHexEditorMenus()
{
	if (GHexMenuRegistered) return;
	GHexMenuRegistered = true;

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = WindowMenu->FindOrAddSection("WindowLayout");

		Section.AddSubMenu("HexagonMenu",
			NSLOCTEXT("Hexagon", "HexagonMenu", "Hexagon"),
			NSLOCTEXT("Hexagon", "HexagonMenuTooltip", "Hexagon terrain tools"),
			FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
			{
				FToolMenuSection& Presets = SubMenu->AddSection("HexagonPresets",
					NSLOCTEXT("Hexagon", "PresetsSection", "Terrain Presets"));

				// Use console-command lambdas via the existing registered commands
				auto ExecCmd = [](const TCHAR* Cmd) {
					GEditor->Exec(GEditor->GetEditorWorldContext().World(), Cmd);
				};

				Presets.AddMenuEntry("HexCreateTestScene",
					NSLOCTEXT("Hexagon", "CreateTestScene", "Create Test Scene"),
					NSLOCTEXT("Hexagon", "CreateTestSceneTip", "Full test scene with lights and debug colors"),
					FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([ExecCmd]() { ExecCmd(TEXT("hex.CreateTestScene")); })));

				Presets.AddMenuEntry("HexCreateGrassTerrain",
					NSLOCTEXT("Hexagon", "CreateGrassTerrain", "Create Grass Terrain"),
					NSLOCTEXT("Hexagon", "CreateGrassTerrainTip", "Grass-only rolling hills"),
					FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([ExecCmd]() { ExecCmd(TEXT("hex.CreateGrassTerrain")); })));

				Presets.AddMenuEntry("HexCreateGrassSandTerrain",
					NSLOCTEXT("Hexagon", "CreateGrassSandTerrain", "Create Grass+Sand Terrain"),
					NSLOCTEXT("Hexagon", "CreateGrassSandTerrainTip", "Mixed grass, sand, and water terrain"),
					FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([ExecCmd]() { ExecCmd(TEXT("hex.CreateGrassSandTerrain")); })));

				Presets.AddMenuEntry("HexCreateFullTerrain",
					NSLOCTEXT("Hexagon", "CreateFullTerrain", "Create Full Terrain (5x)"),
					NSLOCTEXT("Hexagon", "CreateFullTerrainTip", "Large terrain with all 5 types"),
					FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([ExecCmd]() { ExecCmd(TEXT("hex.CreateFullTerrain")); })));

				FToolMenuSection& Utils = SubMenu->AddSection("HexagonUtilities",
					NSLOCTEXT("Hexagon", "UtilitiesSection", "Utilities"));

				Utils.AddMenuEntry("HexDestroyAll",
					NSLOCTEXT("Hexagon", "DestroyAll", "Destroy All"),
					NSLOCTEXT("Hexagon", "DestroyAllTip", "Destroy all hex actors"),
					FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([ExecCmd]() { ExecCmd(TEXT("hex.DestroyAll")); })));

				Utils.AddMenuEntry("HexPrintStats",
					NSLOCTEXT("Hexagon", "PrintStats", "Print Stats"),
					NSLOCTEXT("Hexagon", "PrintStatsTip", "Print terrain statistics to Output Log"),
					FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([ExecCmd]() { ExecCmd(TEXT("hex.Stats")); })));
			}));
	}));

	UE_LOG(LogTemp, Log, TEXT("Hexagon: registered editor menus"));
}
#endif // WITH_EDITOR

// ============================================================================
// Module
// ============================================================================
void FHexagonModule::StartupModule()
{
#if WITH_EDITOR
	FCoreDelegates::OnPostEngineInit.AddStatic(&RegisterHexCommands);
	FCoreDelegates::OnPostEngineInit.AddStatic(&RegisterHexEditorModes);
	RegisterHexEditorMenus();
#endif
}

void FHexagonModule::ShutdownModule()
{
#if WITH_EDITOR
	UnregisterHexCommands();
	UnregisterHexEditorModes();
#endif
}

IMPLEMENT_MODULE(FHexagonModule, Hexagon)
