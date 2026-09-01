// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsTypes.h"
#include "NsNet.h"

constexpr int32 NsLockstepDelayFrames = 3;
constexpr int32 NsLockstepDelayStallMs = 500;

struct FNsDelayInbox
{
	FNsInputs Slot;
	bool Got[Ns::PlayerCount] = {};
};

class NETWORKSYNC_API FNsLockstepDelayServer
{
public:
	int32 Frame = 0;
	double FrameStartMs = 0.0;
	FNsWorld World;
	TMap<int32, FNsInputs> Hist;
	TMap<int32, FNsDelayInbox> Inbox;
	int32 StallFills = 0;
	int32 WaitTicks = 0;

	void OnInput(int32 PlayerId, int32 Tick, int8 Dx);
	void Tick(INsNet& Net);
};

class NETWORKSYNC_API FNsLockstepDelayClient
{
public:
	int32 PlayerId = 0;
	ENsAddr Addr = ENsAddr::C0;
	int32 ExecFrame = 0;
	int32 KnownFrame = -1;
	TMap<int32, FNsInputs> Buf;
	FNsWorld World;
	int32 PrevX[Ns::PlayerCount] = {0, 0};

	void SendInput(INsNet& Net, int8 Dx);
	void OnS2C(const TMap<int32, FNsInputs>& Frames);
	void ApplyJoin(const FNsPacket& Packet);
	void Logic();
};

NETWORKSYNC_API void NsPumpLockstepDelayServer(INsNet& Net, FNsLockstepDelayServer& Sv, bool bWait = false);
NETWORKSYNC_API void NsPumpLockstepDelayClient(INsNet& Net, FNsLockstepDelayClient& C, bool bWait = false);
