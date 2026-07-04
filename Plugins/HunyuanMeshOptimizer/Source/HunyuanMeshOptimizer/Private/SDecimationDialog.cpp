// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "SDecimationDialog.h"

#include "AssetToolsModule.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "IAssetTools.h"
#include "IDesktopPlatform.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HunyuanMeshOptimizer"

void ShowHunyuanDecimationDialog()
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Hunyuan Mesh Optimizer"))
		.SizingRule(ESizingRule::Autosized)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	Window->SetContent(SNew(SDecimationDialog).OwnerWindow(Window));

	FSlateApplication::Get().AddWindow(Window);
}

void SDecimationDialog::Construct(const FArguments& InArgs)
{
	OwnerWindow = InArgs._OwnerWindow;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(16.0f)
		[
			SNew(SVerticalBox)

			// Input GLB
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("InputLabel", "Input GLB"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(InputPathTextBox, SEditableTextBox)
					.HintText(LOCTEXT("InputHint", "Path to the Hunyuan3D-generated .glb file"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("BrowseInput", "Browse"))
					.OnClicked(this, &SDecimationDialog::OnBrowseInputClicked)
				]
			]

			// Output directory
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("OutputLabel", "Output Directory"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(OutputDirTextBox, SEditableTextBox)
					.HintText(LOCTEXT("OutputHint", "Directory where LOD files will be saved"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("BrowseOutput", "Browse"))
					.OnClicked(this, &SDecimationDialog::OnBrowseOutputClicked)
				]
			]

			// Target face counts
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("TargetFacesLabel", "Target Face Counts"))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Lod0Label", "LOD0"))]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(Lod0TextBox, SEditableTextBox)
						.Text(FText::FromString(TEXT("20000")))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Lod1Label", "LOD1"))]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(Lod1TextBox, SEditableTextBox)
						.Text(FText::FromString(TEXT("8000")))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Lod2Label", "LOD2"))]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(Lod2TextBox, SEditableTextBox)
						.Text(FText::FromString(TEXT("2500")))
					]
				]
			]

			// Import option
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(bImportCheckBox, SCheckBox)
					.IsChecked(ECheckBoxState::Checked)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("ImportLabel", "Import to Content Browser"))
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(24.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("DestinationLabel", "Destination Path"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(DestinationPathTextBox, SEditableTextBox)
					.Text(FText::FromString(TEXT("/Game/Hunyuan_LODs")))
				]
			]

			// Action buttons
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0.0f, 20.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("GenerateButton", "Generate LODs"))
					.OnClicked(this, &SDecimationDialog::OnGenerateClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CloseButton", "Close"))
					.OnClicked(this, &SDecimationDialog::OnCloseClicked)
				]
			]
		]
	];
}

FReply SDecimationDialog::OnBrowseInputClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return FReply::Handled();
	}

	const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(OwnerWindow.Pin());

	FString DefaultPath = FPaths::ProjectDir() / TEXT("hunyuan");
	if (InputPathTextBox.IsValid() && !InputPathTextBox->GetText().IsEmpty())
	{
		DefaultPath = FPaths::GetPath(InputPathTextBox->GetText().ToString());
	}

	TArray<FString> OutFiles;
	if (DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		LOCTEXT("OpenInputDialogTitle", "Select Input GLB").ToString(),
		DefaultPath,
		TEXT(""),
		TEXT("GLB files|*.glb"),
		EFileDialogFlags::None,
		OutFiles)
		&& OutFiles.Num() > 0)
	{
		InputPathTextBox->SetText(FText::FromString(OutFiles[0]));
	}

	return FReply::Handled();
}

FReply SDecimationDialog::OnBrowseOutputClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return FReply::Handled();
	}

	const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(OwnerWindow.Pin());

	FString DefaultPath = FPaths::ProjectDir() / TEXT("hunyuan");
	if (OutputDirTextBox.IsValid() && !OutputDirTextBox->GetText().IsEmpty())
	{
		DefaultPath = OutputDirTextBox->GetText().ToString();
	}

	FString OutFolder;
	if (DesktopPlatform->OpenDirectoryDialog(
		ParentWindowHandle,
		LOCTEXT("OpenOutputDialogTitle", "Select Output Directory").ToString(),
		DefaultPath,
		OutFolder))
	{
		OutputDirTextBox->SetText(FText::FromString(OutFolder));
	}

	return FReply::Handled();
}

FReply SDecimationDialog::OnGenerateClicked()
{
	FString ErrorMessage;
	if (!ValidateInputs(ErrorMessage))
	{
		ShowNotification(FText::FromString(ErrorMessage), false);
		return FReply::Handled();
	}

	const FString InputPath = InputPathTextBox->GetText().ToString();
	const FString OutputDir = OutputDirTextBox->GetText().ToString();

	// 如果输出目录不存在则自动创建
	IFileManager::Get().MakeDirectory(*OutputDir, true);

	const int32 Lod0 = FCString::Atoi(*Lod0TextBox->GetText().ToString());
	const int32 Lod1 = FCString::Atoi(*Lod1TextBox->GetText().ToString());
	const int32 Lod2 = FCString::Atoi(*Lod2TextBox->GetText().ToString());

	const FString PythonExe = FPaths::ProjectDir() / TEXT("hunyuan/venv/Scripts/python.exe");
	const FString ScriptPath = FPaths::ProjectDir() / TEXT("hunyuan/decimate_glb.py");

	if (!FPaths::FileExists(PythonExe))
	{
		ShowNotification(FText::Format(
			LOCTEXT("PythonNotFound", "Python executable not found: {0}"),
			FText::FromString(PythonExe)), false);
		return FReply::Handled();
	}

	if (!FPaths::FileExists(ScriptPath))
	{
		ShowNotification(FText::Format(
			LOCTEXT("ScriptNotFound", "Decimation script not found: {0}"),
			FText::FromString(ScriptPath)), false);
		return FReply::Handled();
	}

	const FString Args = FString::Printf(
		TEXT("\"%s\" \"%s\" --output-dir \"%s\" --lod0 %d --lod1 %d --lod2 %d"),
		*ScriptPath,
		*InputPath,
		*OutputDir,
		Lod0,
		Lod1,
		Lod2);

	int32 ReturnCode = 0;
	FString StdOut;
	FString StdErr;

	const bool bLaunched = FPlatformProcess::ExecProcess(
		*PythonExe,
		*Args,
		&ReturnCode,
		&StdOut,
		&StdErr,
		*FPaths::ProjectDir());

	if (!bLaunched || ReturnCode != 0)
	{
		FString Detail = StdErr.IsEmpty() ? StdOut : StdErr;
		ShowNotification(FText::Format(
			LOCTEXT("GenerationFailed", "LOD generation failed (code {0}): {1}"),
			FText::AsNumber(ReturnCode),
			FText::FromString(Detail)), false);
		return FReply::Handled();
	}

	if (bImportCheckBox.IsValid() && bImportCheckBox->IsChecked())
	{
		const FString BaseName = FPaths::GetBaseFilename(InputPath);
		const FString DestinationPath = DestinationPathTextBox->GetText().ToString();

		TArray<FString> Files;
		Files.Add(FPaths::Combine(OutputDir, BaseName + TEXT("_LOD0.glb")));
		Files.Add(FPaths::Combine(OutputDir, BaseName + TEXT("_LOD1.glb")));
		Files.Add(FPaths::Combine(OutputDir, BaseName + TEXT("_LOD2.glb")));

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		const TArray<UObject*> ImportedAssets = AssetToolsModule.Get().ImportAssets(Files, DestinationPath);

		if (ImportedAssets.Num() == 0)
		{
			ShowNotification(LOCTEXT("ImportFailed", "LODs generated but import to Content Browser failed."), false);
		}
		else
		{
			ShowNotification(FText::Format(
				LOCTEXT("ImportSuccess", "Successfully generated and imported {0} LOD assets to {1}."),
				FText::AsNumber(ImportedAssets.Num()),
				FText::FromString(DestinationPath)), true);
		}
	}
	else
	{
		ShowNotification(LOCTEXT("GenerationSuccess", "LODs generated successfully."), true);
	}

	return FReply::Handled();
}

FReply SDecimationDialog::OnCloseClicked()
{
	TSharedPtr<SWindow> Window = OwnerWindow.Pin();
	if (Window.IsValid())
	{
		Window->RequestDestroyWindow();
	}
	return FReply::Handled();
}

bool SDecimationDialog::ValidateInputs(FString& OutError) const
{
	if (!InputPathTextBox.IsValid() || InputPathTextBox->GetText().IsEmpty())
	{
		OutError = TEXT("Please specify an input GLB file.");
		return false;
	}

	const FString InputPath = InputPathTextBox->GetText().ToString();
	if (!FPaths::FileExists(InputPath))
	{
		OutError = FString::Printf(TEXT("Input file does not exist: %s"), *InputPath);
		return false;
	}

	if (!OutputDirTextBox.IsValid() || OutputDirTextBox->GetText().IsEmpty())
	{
		OutError = TEXT("Please specify an output directory.");
		return false;
	}

	const FString OutputDir = OutputDirTextBox->GetText().ToString();

	auto IsPositiveInteger = [](const TSharedPtr<SEditableTextBox>& Box) -> bool {
		if (!Box.IsValid()) return false;
		const FString Text = Box->GetText().ToString().TrimStartAndEnd();
		if (Text.IsEmpty()) return false;
		for (TCHAR C : Text)
		{
			if (!FChar::IsDigit(C)) return false;
		}
		return FCString::Atoi(*Text) > 0;
	};

	if (!IsPositiveInteger(Lod0TextBox) || !IsPositiveInteger(Lod1TextBox) || !IsPositiveInteger(Lod2TextBox))
	{
		OutError = TEXT("LOD target face counts must be positive integers.");
		return false;
	}

	if (bImportCheckBox.IsValid() && bImportCheckBox->IsChecked())
	{
		if (!DestinationPathTextBox.IsValid() || DestinationPathTextBox->GetText().IsEmpty())
		{
			OutError = TEXT("Please specify a destination package path for import.");
			return false;
		}
	}

	return true;
}

void SDecimationDialog::ShowNotification(const FText& Message, bool bSuccess) const
{
	FNotificationInfo Info(Message);
	Info.bFireAndForget = true;
	Info.bUseThrobber = false;
	Info.ExpireDuration = bSuccess ? 5.0f : 8.0f;
	Info.Image = bSuccess
		? FCoreStyle::Get().GetBrush(TEXT("NotificationList.SuccessImage"))
		: FCoreStyle::Get().GetBrush(TEXT("NotificationList.FailImage"));

	FSlateNotificationManager::Get().AddNotification(Info);
}

#undef LOCTEXT_NAMESPACE
