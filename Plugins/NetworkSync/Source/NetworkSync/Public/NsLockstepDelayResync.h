// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsLockstepResync.h"
#include "NsLockstepDelay.h"

struct FNsDoorOpen;

NETWORKSYNC_API void NsApplyDelayResyncSnap(FNsLockstepDelayClient& Client, const FNsPacket& Packet);
NETWORKSYNC_API void NsPumpLockstepDelayResyncServer(
	INsNet& Net, FNsLockstepDelayServer& Sv, FNsLockstepResync& Resync, bool bWait = false);
NETWORKSYNC_API void NsPumpLockstepDelayResyncClient(
	INsNet& Net, FNsLockstepDelayClient& C, FNsLockstepResyncClient& View, bool bWait = false,
	FNsDoorOpen* Door = nullptr);
