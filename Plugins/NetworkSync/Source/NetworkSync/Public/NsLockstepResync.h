// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsLockstep.h"

class NETWORKSYNC_API FNsLockstepResync
{
public:
	FNsWorld LiveSnap;
	int32 LiveSnapTick = 0;
	bool bCaptured = false;
	int32 PumpCycles = 0;
	bool bGiveUp = false;

	void CaptureLive(const FNsLockstepServer& Sv);
	void SendLiveSnap(INsNet& Net, ENsAddr Dst) const;
};

NETWORKSYNC_API void NsApplyResyncSnap(FNsLockstepClient& Client, const FNsPacket& Packet);
NETWORKSYNC_API void NsPumpLockstepResyncServer(INsNet& Net, FNsLockstepServer& Sv, FNsLockstepResync& Resync, bool bWait = false);
NETWORKSYNC_API void NsPumpLockstepResyncClient(INsNet& Net, FNsLockstepClient& C, const FNsLockstepResync& Resync, bool bWait = false);
