// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsTypes.h"
#include "NsNet.h"

constexpr int32 NsLockstepDelayFrames = 3;
constexpr int32 NsLockstepDelayFramesMin = 1;
constexpr int32 NsLockstepDelayFramesMax = 8;
constexpr int32 NsLockstepDelayStallMs = 500;

inline int32 NsLockstepDelayFromRtt(double RttMs)
{
	const int32 Rtt = FMath::Max(0, FMath::CeilToInt(static_cast<float>(RttMs)));
	const int32 D = (Rtt + Ns::LogicDtMs - 1) / Ns::LogicDtMs + 1;
	return FMath::Clamp(D, NsLockstepDelayFramesMin, NsLockstepDelayFramesMax);
}

struct FNsDelayInbox
{
	FNsInputs Slot;
	bool Got[Ns::PlayerCount] = {};
};

class NETWORKSYNC_API FNsLockstepDelayServer
{
public:
	int32 Frame = 0;
	int32 DelayFrames = NsLockstepDelayFrames;
	double FrameStartMs = 0.0;
	FNsWorld World;
	TMap<int32, FNsInputs> Hist;
	TMap<int32, FNsDelayInbox> Inbox;
	TMap<int32, uint32> Checksums;
	int32 StallFills = 0;
	int32 WaitTicks = 0;
	int32 ChecksumOk = 0;
	bool bDesync = false;

	void OnInput(int32 PlayerId, int32 Tick, int8 Dx);
	void OnChecksum(int32 FrameIndex, uint32 Hash);
	void OnNack(INsNet& Net, ENsAddr Dst, const TArray<int32>& Frames);
	void Tick(INsNet& Net);
	void SendJoin(INsNet& Net, ENsAddr Dst) const;
};

class NETWORKSYNC_API FNsLockstepDelayClient
{
public:
	int32 PlayerId = 0;
	ENsAddr Addr = ENsAddr::C0;
	int32 ExecFrame = 0;
	int32 KnownFrame = -1;
	int32 DelayFrames = NsLockstepDelayFrames;
	TMap<int32, FNsInputs> Buf;
	FNsWorld World;
	int32 PrevX[Ns::PlayerCount] = {0, 0};

	void SendInput(INsNet& Net, int8 Dx);
	void OnS2C(const TMap<int32, FNsInputs>& Frames);
	void ApplyJoin(const FNsPacket& Packet);
	void Logic(INsNet& Net);
};

inline void NsLockstepDelayApplyFrames(FNsLockstepDelayServer& Sv,
	FNsLockstepDelayClient& C0, FNsLockstepDelayClient& C1, int32 Frames)
{
	const int32 D = FMath::Clamp(Frames, NsLockstepDelayFramesMin, NsLockstepDelayFramesMax);
	Sv.DelayFrames = D;
	C0.DelayFrames = D;
	C1.DelayFrames = D;
}

NETWORKSYNC_API void NsPumpLockstepDelayServer(INsNet& Net, FNsLockstepDelayServer& Sv, bool bWait = false);
NETWORKSYNC_API void NsPumpLockstepDelayClient(INsNet& Net, FNsLockstepDelayClient& C, bool bWait = false);
