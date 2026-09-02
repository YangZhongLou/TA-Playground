// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsLockstep.h"

struct FNsDoorOpen;

class NETWORKSYNC_API FNsLockstepResync
{
public:
	FNsWorld LiveSnap;
	int32 LiveSnapTick = 0;
	bool bCaptured = false;
	bool bResumed = false;
	bool Acked[Ns::PlayerCount] = {};
	int32 PumpCycles = 0;
	bool bGiveUp = false;

	void CaptureLive(const FNsWorld& World, int32 Frame);
	void SendLiveSnap(INsNet& Net, ENsAddr Dst) const;
	void FinishResume();
	void Resume(FNsLockstepServer& Sv, INsNet& Net);
};

struct FNsLockstepResyncClient
{
	int32 HaltTick = -1;
	int32 DoneSnapTick = -1;
};

inline bool NsIsResyncLiveSnap(const FNsPacket& Packet)
{
	return Packet.Type == ENsMsg::S2CJoinSnap && Packet.Frames.Num() == 0 && Packet.Tick > 0;
}

inline bool NsS2CResumesHalt(const FNsPacket& Packet, int32 HaltTick)
{
	if (Packet.Type != ENsMsg::S2CFrame || HaltTick < 0)
	{
		return false;
	}
	for (const TPair<int32, FNsInputs>& Kv : Packet.Frames)
	{
		if (Kv.Key >= HaltTick)
		{
			return true;
		}
	}
	return false;
}

NETWORKSYNC_API void NsApplyResyncSnap(FNsLockstepClient& Client, const FNsPacket& Packet);
NETWORKSYNC_API void NsPumpLockstepResyncServer(INsNet& Net, FNsLockstepServer& Sv, FNsLockstepResync& Resync, bool bWait = false);
NETWORKSYNC_API void NsPumpLockstepResyncClient(INsNet& Net, FNsLockstepClient& C, FNsLockstepResyncClient& View, bool bWait = false, FNsDoorOpen* Door = nullptr);
