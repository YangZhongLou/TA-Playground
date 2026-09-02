// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsTypes.h"
#include "NsNet.h"

constexpr int32 NsLockstepWaitStallMs = 500;

class NETWORKSYNC_API FNsLockstepWaitServer
{
public:
	int32 Frame = 0;
	bool Got[Ns::PlayerCount] = {};
	bool Alive[Ns::PlayerCount] = {true, true};
	int32 MissStreak[Ns::PlayerCount] = {};
	int32 KickAfterStalls = 0;
	FNsInputs Slot;
	double FrameStartMs = 0.0;
	FNsWorld World;
	TMap<int32, FNsInputs> Hist;
	TMap<int32, uint32> Checksums;
	int32 SnapFrame = -1;
	FNsWorld SnapWorld;
	int32 ChecksumOk = 0;
	bool bDesync = false;

	void OnInput(int32 PlayerId, int32 Tick, int8 Dx);
	void OnChecksum(int32 FrameIndex, uint32 Hash);
	void Tick(INsNet& Net);
	void SendJoin(INsNet& Net, ENsAddr Dst) const;
};

class NETWORKSYNC_API FNsLockstepWaitClient
{
public:
	int32 PlayerId = 0;
	ENsAddr Addr = ENsAddr::C0;
	int32 ExecFrame = 0;
	TMap<int32, FNsInputs> Buf;
	FNsWorld World;
	int32 PrevX[Ns::PlayerCount] = {0, 0};

	void SendInput(INsNet& Net, int8 Dx);
	void OnS2C(const TMap<int32, FNsInputs>& Frames);
	void ApplyJoin(const FNsPacket& Packet);
	void Logic(INsNet& Net);
};

NETWORKSYNC_API void NsPumpLockstepWaitServer(INsNet& Net, FNsLockstepWaitServer& Sv, bool bWait = false);
NETWORKSYNC_API void NsPumpLockstepWaitClient(INsNet& Net, FNsLockstepWaitClient& C, bool bWait = false);
