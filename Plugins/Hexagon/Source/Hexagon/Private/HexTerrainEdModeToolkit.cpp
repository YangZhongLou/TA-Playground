// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "HexTerrainEdModeToolkit.h"

#if WITH_EDITOR

#include "HexTerrainEdMode.h"
#include "AHexTerrain.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"

void FHexTerrainEdModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost)
{
	FModeToolkit::Init(InitToolkitHost);
}

FEdMode* FHexTerrainEdModeToolkit::GetEditorMode() const
{
	return GLevelEditorModeTools().GetActiveMode(FHexTerrainEdMode::EM_HexTerrainPaint);
}

static AHexTerrain* FindTerrain()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<AHexTerrain> It(World); It; ++It) return *It;
	return nullptr;
}

TSharedPtr<SWidget> FHexTerrainEdModeToolkit::GetInlineContent() const
{
	return const_cast<FHexTerrainEdModeToolkit*>(this)->BuildPanel();
}

TSharedRef<SWidget> FHexTerrainEdModeToolkit::BuildPanel()
{
	// Type name list
	static TArray<TSharedPtr<FString>> TypeNames = {
		MakeShareable(new FString(TEXT("Water"))),
		MakeShareable(new FString(TEXT("Sand"))),
		MakeShareable(new FString(TEXT("Grass"))),
		MakeShareable(new FString(TEXT("Rock"))),
		MakeShareable(new FString(TEXT("Snow"))),
	};

	AHexTerrain* Terrain = FindTerrain();

	// Current selection (lambda-read from terrain each frame)
	auto GetPaintIdx = [Terrain]() -> TSharedPtr<FString> {
		if (!Terrain) return TypeNames[2]; // default Grass
		int32 I = FMath::Clamp((int32)Terrain->BrushTerrainType, 0, 4);
		return TypeNames[I];
	};
	auto GetEraseIdx = [Terrain]() -> TSharedPtr<FString> {
		if (!Terrain) return TypeNames[2];
		int32 I = FMath::Clamp((int32)Terrain->DefaultManualType, 0, 4);
		return TypeNames[I];
	};

	return SNew(SBox)
		.Padding(6)
		.MinDesiredWidth(200)
		[
			SNew(SVerticalBox)

			// Title
			+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
			[
				SNew(STextBlock)
					.Text(NSLOCTEXT("Hexagon", "PanelTitle", "Hex Terrain Paint"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			// Paint Type
			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(STextBlock).Text(NSLOCTEXT("Hexagon", "PaintTypeLbl", "Paint Type"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&TypeNames)
					.InitiallySelectedItem(GetPaintIdx())
					.OnSelectionChanged_Lambda([Terrain](TSharedPtr<FString> Sel, ESelectInfo::Type) {
						if (!Terrain || !Sel.IsValid()) return;
						static TMap<FString, EHexTerrainType> M = {
							{TEXT("Water"),EHexTerrainType::Water},{TEXT("Sand"),EHexTerrainType::Sand},
							{TEXT("Grass"),EHexTerrainType::Grass},{TEXT("Rock"),EHexTerrainType::Rock},
							{TEXT("Snow"),EHexTerrainType::Snow}};
						if (auto* F = M.Find(*Sel)) Terrain->BrushTerrainType = *F;
					})
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
						return SNew(STextBlock).Text(FText::FromString(*Item));
					})
					.Content()
					[
						SNew(STextBlock).Text_Lambda([Terrain]() {
							static TArray<FString> N = {TEXT("Water"),TEXT("Sand"),TEXT("Grass"),TEXT("Rock"),TEXT("Snow")};
							int32 I = Terrain ? FMath::Clamp((int32)Terrain->BrushTerrainType,0,4) : 2;
							return FText::FromString(N[I]);
						})
					]
			]

			// Erase Type
			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(STextBlock).Text(NSLOCTEXT("Hexagon", "EraseTypeLbl", "Erase Type (Ctrl+Click)"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&TypeNames)
					.InitiallySelectedItem(GetEraseIdx())
					.OnSelectionChanged_Lambda([Terrain](TSharedPtr<FString> Sel, ESelectInfo::Type) {
						if (!Terrain || !Sel.IsValid()) return;
						static TMap<FString, EHexTerrainType> M = {
							{TEXT("Water"),EHexTerrainType::Water},{TEXT("Sand"),EHexTerrainType::Sand},
							{TEXT("Grass"),EHexTerrainType::Grass},{TEXT("Rock"),EHexTerrainType::Rock},
							{TEXT("Snow"),EHexTerrainType::Snow}};
						if (auto* F = M.Find(*Sel)) Terrain->DefaultManualType = *F;
					})
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
						return SNew(STextBlock).Text(FText::FromString(*Item));
					})
					.Content()
					[
						SNew(STextBlock).Text_Lambda([Terrain]() {
							static TArray<FString> N = {TEXT("Water"),TEXT("Sand"),TEXT("Grass"),TEXT("Rock"),TEXT("Snow")};
							int32 I = Terrain ? FMath::Clamp((int32)Terrain->DefaultManualType,0,4) : 2;
							return FText::FromString(N[I]);
						})
					]
			]

			// Brush Radius
			+ SVerticalBox::Slot().AutoHeight().Padding(0,2)
			[
				SNew(STextBlock).Text(NSLOCTEXT("Hexagon", "RadiusLbl", "Brush Radius"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
			[
				SNew(SSpinBox<float>)
					.MinValue(0.0f).MaxValue(20.0f)
					.Value_Lambda([Terrain]() { return Terrain ? Terrain->BrushRadius : 1.0f; })
					.OnValueChanged_Lambda([Terrain](float V) { if (Terrain) Terrain->BrushRadius = V; })
			]

			// Help text
			+ SVerticalBox::Slot().AutoHeight().Padding(0,4)
			[
				SNew(STextBlock)
					.Text(NSLOCTEXT("Hexagon", "Help", "Click = Paint\nCtrl+Click = Erase\nESC = Exit"))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f)))
			]
		];
}

#endif // WITH_EDITOR
