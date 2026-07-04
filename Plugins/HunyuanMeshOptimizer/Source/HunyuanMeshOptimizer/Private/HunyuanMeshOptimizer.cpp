// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HunyuanMeshOptimizer.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "LevelEditor.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "ToolMenuEntry.h"
#endif

#define LOCTEXT_NAMESPACE "HunyuanMeshOptimizer"

void ShowHunyuanDecimationDialog();

#if WITH_EDITOR
// ============================================================================
// Commands
// ============================================================================
class FHunyuanMeshOptimizerCommands : public TCommands<FHunyuanMeshOptimizerCommands>
{
public:
	FHunyuanMeshOptimizerCommands()
		: TCommands<FHunyuanMeshOptimizerCommands>(
			TEXT("HunyuanMeshOptimizer"),
			NSLOCTEXT("Contexts", "HunyuanMeshOptimizer", "Hunyuan Mesh Optimizer"),
			NAME_None,
			FAppStyle::GetAppStyleSetName())
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(
			OpenDecimationDialog,
			"Hunyuan LOD",
			"Open the Hunyuan Mesh Optimizer decimation dialog.",
			EUserInterfaceActionType::Button,
			FInputChord());
	}

	TSharedPtr<FUICommandInfo> OpenDecimationDialog;
};
#endif // WITH_EDITOR

// ============================================================================
// Module
// ============================================================================
void FHunyuanMeshOptimizerModule::StartupModule()
{
#if WITH_EDITOR
	FHunyuanMeshOptimizerCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FHunyuanMeshOptimizerCommands::Get().OpenDecimationDialog,
		FExecuteAction::CreateStatic(&ShowHunyuanDecimationDialog),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FHunyuanMeshOptimizerModule::RegisterMenus));
#endif
}

void FHunyuanMeshOptimizerModule::ShutdownModule()
{
#if WITH_EDITOR
	UToolMenus::UnRegisterStartupCallback(this);

	if (UToolMenus* ToolMenus = UToolMenus::TryGet())
	{
		ToolMenus->UnregisterOwner(this);
	}

	FHunyuanMeshOptimizerCommands::Unregister();
	PluginCommands.Reset();
#endif
}

#if WITH_EDITOR
void FHunyuanMeshOptimizerModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
	FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("HunyuanMeshOptimizer");

	FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(
		FHunyuanMeshOptimizerCommands::Get().OpenDecimationDialog,
		LOCTEXT("HunyuanLODButton", "Hunyuan LOD"),
		LOCTEXT("HunyuanLODButtonTooltip", "Open the Hunyuan Mesh Optimizer dialog"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Build")));
	Entry.SetCommandList(PluginCommands);
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHunyuanMeshOptimizerModule, HunyuanMeshOptimizer)
