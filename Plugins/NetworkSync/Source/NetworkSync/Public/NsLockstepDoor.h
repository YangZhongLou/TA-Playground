// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsLockstep.h"

struct NETWORKSYNC_API FNsDoorOpen
{
	int32 Open = 0;
};

NETWORKSYNC_API void NsBroadcastDoorOpen(INsNet& Net, int32 Open);
NETWORKSYNC_API void NsApplyDoorOpen(FNsDoorOpen& Door, const FNsPacket& Packet);

class NETWORKSYNC_API FNsLockstepDoorServer
{
public:
	FNsLockstepServer Ls;
	FNsDoorOpen Door;

	void SetOpen(INsNet& Net, int32 Open);
	void BroadcastDoorOpen(INsNet& Net) const;
};

class NETWORKSYNC_API FNsLockstepDoorClient
{
public:
	FNsLockstepClient Ls;
	FNsDoorOpen Door;
};

NETWORKSYNC_API void NsPumpLockstepDoorServer(INsNet& Net, FNsLockstepDoorServer& Sv, bool bWait = false);
NETWORKSYNC_API void NsPumpLockstepDoorClient(INsNet& Net, FNsLockstepDoorClient& C, bool bWait = false);
