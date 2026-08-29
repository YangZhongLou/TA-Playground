// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SCheckBox;

/** Show the Hunyuan mesh decimation dialog as a centered editor window. */
void ShowHunyuanDecimationDialog();

/** Slate dialog for generating LOD0/LOD1/LOD2 from a Hunyuan3D .glb. */
class SDecimationDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDecimationDialog)
	{}
	SLATE_ARGUMENT(TSharedPtr<SWindow>, OwnerWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnBrowseInputClicked();
	FReply OnBrowseOutputClicked();
	FReply OnGenerateClicked();
	FReply OnCloseClicked();

	bool ValidateInputs(FString& OutError) const;
	void ShowNotification(const FText& Message, bool bSuccess) const;

private:
	TWeakPtr<SWindow> OwnerWindow;

	TSharedPtr<SEditableTextBox> InputPathTextBox;
	TSharedPtr<SEditableTextBox> OutputDirTextBox;
	TSharedPtr<SEditableTextBox> Lod0TextBox;
	TSharedPtr<SEditableTextBox> Lod1TextBox;
	TSharedPtr<SEditableTextBox> Lod2TextBox;
	TSharedPtr<SCheckBox> bImportCheckBox;
	TSharedPtr<SEditableTextBox> DestinationPathTextBox;
};
