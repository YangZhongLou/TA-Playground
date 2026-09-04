// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

class FNetworkSyncModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	IConsoleCommand* SelfTestCmd = nullptr;
	IConsoleCommand* DropRateCmd = nullptr;
	IConsoleCommand* SpawnCmd = nullptr;
	IConsoleCommand* HubCmd = nullptr;
	IConsoleCommand* HubStopCmd = nullptr;
};
