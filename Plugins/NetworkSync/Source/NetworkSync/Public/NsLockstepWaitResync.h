// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsLockstepResync.h"
#include "NsLockstepWait.h"

struct FNsDoorOpen;

NETWORKSYNC_API void NsApplyWaitResyncSnap(FNsLockstepWaitClient& Client, const FNsPacket& Packet);
NETWORKSYNC_API void NsPumpLockstepWaitResyncServer(
	INsNet& Net, FNsLockstepWaitServer& Sv, FNsLockstepResync& Resync, bool bWait = false);
NETWORKSYNC_API void NsPumpLockstepWaitResyncClient(
	INsNet& Net, FNsLockstepWaitClient& C, FNsLockstepResyncClient& View, bool bWait = false,
	FNsDoorOpen* Door = nullptr);
