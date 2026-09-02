// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsLockstepResync.h"
#include "NsLockstepTurn.h"

struct FNsDoorOpen;

inline bool NsS2CResumesTurnHalt(const FNsPacket& Packet)
{
	return Packet.Type == ENsMsg::S2CFrame && Packet.Frames.Num() == 0;
}

NETWORKSYNC_API void NsApplyTurnResyncSnap(FNsLockstepTurnClient& Client, const FNsPacket& Packet);
NETWORKSYNC_API void NsPumpLockstepTurnResyncServer(
	INsNet& Net, FNsLockstepTurnServer& Sv, FNsLockstepResync& Resync, bool bWait = false);
NETWORKSYNC_API void NsPumpLockstepTurnResyncClient(
	INsNet& Net, FNsLockstepTurnClient& C, FNsLockstepResyncClient& View, bool bWait = false,
	FNsDoorOpen* Door = nullptr);
