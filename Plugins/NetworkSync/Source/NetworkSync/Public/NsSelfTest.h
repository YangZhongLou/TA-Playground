// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct NETWORKSYNC_API FNsSelfTestResult
{
	bool bOk = false;
	FString Detail;
};

NETWORKSYNC_API FNsSelfTestResult NsRunWorldContractSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunCodecContractSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunFakeNetContractSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunSeqWindowSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunLockstepSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepCleanSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepHighDropSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepJoinSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepDesyncSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepStressSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncCleanSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncRewindSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncStressSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunRollbackSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunRollbackCleanSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunRollbackWaitSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunRollbackStressSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunUdpLoopbackSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpLockstepSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpPeersSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpSplitLockstepSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpBurstSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunCodecStressSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunFakeNetStressSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunWorldStressSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunMtuSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunAllSelfTests();
NETWORKSYNC_API void NsRunSelfTestAndLog();
