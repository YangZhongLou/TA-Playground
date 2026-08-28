// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsTypes.h"
#include "NsFakeNet.h"

class NETWORKSYNC_API FNsLockstepServer
{
public:
	FNsInputs Latest;
	TMap<int32, FNsInputs> Hist;
	TMap<int32, uint32> Checksums;
	int32 Frame = 0;
	double NextMs = 0.0;
	FNsWorld World;
	int32 ChecksumOk = 0;
	bool bDesync = false;

	void OnInput(int32 PlayerId, int8 Dx);
	void OnChecksum(int32 FrameIndex, uint32 Hash);
	void Tick(FNsFakeNet& Net);
};

class NETWORKSYNC_API FNsLockstepClient
{
public:
	int32 PlayerId = 0;
	ENsAddr Addr = ENsAddr::C0;
	int32 ExecFrame = 0;
	TMap<int32, FNsInputs> Buf;
	FNsWorld World;
	int32 PrevX[Ns::PlayerCount] = {0, 0};

	void SendInput(FNsFakeNet& Net, int8 Dx);
	void OnS2C(const TMap<int32, FNsInputs>& Frames);
	void Logic(FNsFakeNet& Net);
};
