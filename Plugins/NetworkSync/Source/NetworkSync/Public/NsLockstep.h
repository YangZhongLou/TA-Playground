// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsTypes.h"
#include "NsNet.h"

// Optimistic lockstep only (ENsLockstepKind::Optimistic).
// Wait-all / comm-turn / delay-based get their own types. Do not branch Tick on Kind.
class NETWORKSYNC_API FNsLockstepServer
{
public:
	FNsInputs Latest;
	TMap<int32, FNsInputs> Hist;
	TMap<int32, uint32> Checksums;
	int32 Frame = 0;
	int32 SnapFrame = -1;
	double NextMs = 0.0;
	FNsWorld World;
	FNsWorld SnapWorld;
	int32 ChecksumOk = 0;
	bool bDesync = false;

	void OnInput(int32 PlayerId, int8 Dx);
	void OnChecksum(int32 FrameIndex, uint32 Hash);
	void OnNack(INsNet& Net, ENsAddr Dst, const TArray<int32>& Frames);
	void Tick(INsNet& Net);
	void SendJoin(INsNet& Net, ENsAddr Dst) const;
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

	void SendInput(INsNet& Net, int8 Dx);
	void OnS2C(const TMap<int32, FNsInputs>& Frames);
	void ApplyJoin(const FNsPacket& Packet);
	void Logic(INsNet& Net);
};
