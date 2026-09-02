// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsTypes.h"
#include "NsNet.h"

constexpr int32 NsLockstepTurnFrames = 3;
constexpr int32 NsLockstepTurnFptMin = 2;
constexpr int32 NsLockstepTurnFptMax = 6;
constexpr int32 NsLockstepTurnLead = 2;
constexpr int32 NsLockstepTurnStallMs = 500;
constexpr int32 NsLockstepTurnResendTurns = 16;
constexpr int32 NsLockstepTurnCatchupTurns = 128;

static_assert(NsLockstepTurnCatchupTurns + NsLockstepTurnResendTurns
	<= Ns::MaxS2CTurnFrameEntries,
	"turn catch-up and safety window must fit in one datagram");

inline int32 NsLockstepTurnLen(const TMap<int32, int32>& TurnLen, int32 Turn, int32 LiveFpt)
{
	if (const int32* Found = TurnLen.Find(Turn))
	{
		return FMath::Clamp(*Found, NsLockstepTurnFptMin, NsLockstepTurnFptMax);
	}
	return FMath::Clamp(LiveFpt, NsLockstepTurnFptMin, NsLockstepTurnFptMax);
}

inline int32 NsLockstepTurnFrameStart(const TMap<int32, int32>& TurnLen, int32 Turn, int32 LiveFpt)
{
	int32 Start = 0;
	for (int32 T = 0; T < Turn; ++T)
	{
		Start += NsLockstepTurnLen(TurnLen, T, LiveFpt);
	}
	return Start;
}

inline void NsLockstepTurnSyncCursor(int32 LogicFrame, const TMap<int32, int32>& TurnLen,
	int32 LiveFpt, int32& ExecTurn, int32& ExecTurnStart)
{
	ExecTurn = 0;
	ExecTurnStart = 0;
	while (LogicFrame >= ExecTurnStart + NsLockstepTurnLen(TurnLen, ExecTurn, LiveFpt))
	{
		ExecTurnStart += NsLockstepTurnLen(TurnLen, ExecTurn, LiveFpt);
		++ExecTurn;
	}
}

class NETWORKSYNC_API FNsLockstepTurnServer
{
public:
	int32 Frame = 0;
	int32 ExecTurn = 0;
	int32 ExecTurnStart = 0;
	int32 CollectTurn = 0;
	int32 ClientNeedTurn[Ns::PlayerCount] = {0, 0};
	bool bCatchupBlocked = false;
	int32 FramesPerTurn = NsLockstepTurnFrames;
	bool Got[Ns::PlayerCount] = {};
	double ArriveMs[Ns::PlayerCount] = {};
	FNsInputs Slot;
	double TurnStartMs = 0.0;
	FNsWorld World;
	TMap<int32, FNsInputs> Cmds;
	TMap<int32, int32> TurnLen;
	TMap<int32, uint32> Checksums;
	int32 ChecksumOk = 0;
	bool bDesync = false;

	void OnInput(int32 PlayerId, int32 Turn, int8 Dx, double NowMs);
	void OnChecksum(int32 FrameIndex, uint32 Hash);
	void Tick(INsNet& Net);
	void Resend(INsNet& Net);
};

class NETWORKSYNC_API FNsLockstepTurnClient
{
public:
	int32 PlayerId = 0;
	ENsAddr Addr = ENsAddr::C0;
	int32 ExecFrame = 0;
	int32 ExecTurn = 0;
	int32 ExecTurnStart = 0;
	int32 SendTurn = 0;
	int32 FramesPerTurn = NsLockstepTurnFrames;
	TMap<int32, FNsInputs> Cmds;
	TMap<int32, int32> TurnLen;
	FNsWorld World;
	int32 PrevX[Ns::PlayerCount] = {0, 0};

	void SendInput(INsNet& Net, int8 Dx);
	void OnS2C(const TMap<int32, FNsInputs>& Turns, int32 LiveFpt, int32 ClosedLen,
		const TMap<int32, int32>& Lens);
	void Logic(INsNet& Net);
	void CatchUpTo(int32 TargetFrame);
};

NETWORKSYNC_API void NsPumpLockstepTurnServer(INsNet& Net, FNsLockstepTurnServer& Sv, bool bWait = false);
NETWORKSYNC_API void NsPumpLockstepTurnClient(INsNet& Net, FNsLockstepTurnClient& C, bool bWait = false);
