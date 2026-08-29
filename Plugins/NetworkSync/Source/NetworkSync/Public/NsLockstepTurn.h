// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsTypes.h"
#include "NsNet.h"

constexpr int32 NsLockstepTurnFrames = 3;
constexpr int32 NsLockstepTurnLead = 2;
constexpr int32 NsLockstepTurnStallMs = 500;

class NETWORKSYNC_API FNsLockstepTurnServer
{
public:
	int32 Frame = 0;
	int32 CollectTurn = 0;
	bool Got[Ns::PlayerCount] = {};
	FNsInputs Slot;
	double TurnStartMs = 0.0;
	FNsWorld World;
	TMap<int32, FNsInputs> Cmds;

	void OnInput(int32 PlayerId, int32 Turn, int8 Dx);
	void Tick(INsNet& Net);
	void Resend(INsNet& Net);
};

class NETWORKSYNC_API FNsLockstepTurnClient
{
public:
	int32 PlayerId = 0;
	ENsAddr Addr = ENsAddr::C0;
	int32 ExecFrame = 0;
	int32 SendTurn = 0;
	TMap<int32, FNsInputs> Cmds;
	FNsWorld World;
	int32 PrevX[Ns::PlayerCount] = {0, 0};

	void SendInput(INsNet& Net, int8 Dx);
	void OnS2C(const TMap<int32, FNsInputs>& Turns);
	void Logic();
	void CatchUpTo(int32 TargetFrame);
};

NETWORKSYNC_API void NsPumpLockstepTurnServer(INsNet& Net, FNsLockstepTurnServer& Sv, bool bWait = false);
NETWORKSYNC_API void NsPumpLockstepTurnClient(INsNet& Net, FNsLockstepTurnClient& C, bool bWait = false);
