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
NETWORKSYNC_API FNsSelfTestResult NsRunRouteGuardSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunFakeNetDropRateSelfTest();

struct NETWORKSYNC_API FNsDropRateSample
{
	float Wanted = 0.f;
	int32 Sent = 0;
	int32 Got = 0;
	float Measured = 0.f;
};

NETWORKSYNC_API FNsDropRateSample NsMeasureFakeNetDrop(float Drop, int32 Count = 2000, uint32 Seed = 1);
NETWORKSYNC_API void NsLogFakeNetDropRate(float Drop, int32 Count);

NETWORKSYNC_API FNsSelfTestResult NsRunLockstepSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepCleanSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepHighDropSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepJoinSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepLateJoinSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepNoSkipSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepJoinFragSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepDesyncSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepStressSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunSchemeSwitchSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncCleanSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncRewindSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncNackSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncInboxHoleSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncInboxCapSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncUnackedWindowSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncLongOutageSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncClockOffsetSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncOldSnapSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncSpoofSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunStateSyncStressSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunRollbackSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunRollbackCleanSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunRollbackWaitSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunRollbackHoleSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunRollbackMidHoleSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunRollbackConflictingInputSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunRollbackStressSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunUdpLoopbackSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpLockstepSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpStateSyncSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpRollbackSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpPeersSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpSplitLockstepSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpSplitStateSyncSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpSplitRollbackSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpBurstSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunUdpSessionRestartSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunCodecStressSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunFakeNetStressSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunWorldStressSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunMtuSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunLockstepResyncAlignSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepResyncForceSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepResyncIgnoreFrameSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepResyncDropSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepResyncApplyJoinSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepResyncStaleJoinSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepResyncGiveUpSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepResyncResumeSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepResyncAgainSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepResyncCleanSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepDoorCleanSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepDoorDropOpenSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepDoorDropFrameSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepDoorIgnoreSnapSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepDoorNotInStepSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepDoorComposeSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunLockstepWaitCleanSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepWaitStallSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepWaitDropSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepWaitJoinSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunLockstepTurnCleanSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepTurnLateSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepTurnDropSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepTurnSpeedSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepTurnLenDropSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepTurnLongRunSelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunLockstepDelayCleanSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepDelayRttSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepDelayHighRttSelfTest();
NETWORKSYNC_API FNsSelfTestResult NsRunLockstepDelayRecoverySelfTest();

NETWORKSYNC_API FNsSelfTestResult NsRunAllSelfTests();
NETWORKSYNC_API void NsRunSelfTestAndLog();
