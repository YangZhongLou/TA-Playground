// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct NETWORKSYNC_API FNsSelfTestResult
{
	bool bOk = false;
	FString Detail;
};

NETWORKSYNC_API FNsSelfTestResult NsRunLockstepSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepJoinSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunRollbackSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpLoopbackSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpLockstepSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunAllSelfTests();
NETWORKSYNC_API void NsRunSelfTestAndLog();
