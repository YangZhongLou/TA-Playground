// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsTypes.h"

enum class ENsAddr : uint8
{
	Sv = 0,
	C0 = 1,
	C1 = 2,
};

inline int32 NsPlayerIdFromAddr(ENsAddr Addr)
{
	if (Addr == ENsAddr::C0)
	{
		return 0;
	}
	if (Addr == ENsAddr::C1)
	{
		return 1;
	}
	return -1;
}

enum class ENsMsg : uint8
{
	C2SInput = 1,
	S2CFrame = 2,
	S2CSnapshot = 3,
	C2SSnapAck = 4,
	P2PInput = 5,
	C2SChecksum = 6,
	S2CJoinSnap = 7,
	S2CDoorOpen = 8,
};

struct FNsPacket
{
	ENsAddr Src = ENsAddr::Sv;
	ENsAddr Dst = ENsAddr::Sv;
	ENsMsg Type = ENsMsg::C2SInput;
	double DeliverAt = 0.0;
	int32 Seq = 0;
	int32 Ack = 0;
	uint32 AckBits = 0;
	int32 PlayerId = 0;
	int32 Tick = 0;
	int8 Dx = 0;
	TMap<int32, FNsInputs> Frames;
	TArray<int32> SeqWindow;
	TArray<int8> DxWindow;
	int32 SnapX[Ns::PlayerCount] = {0, 0};
	int32 SnapSeq[Ns::PlayerCount] = {0, 0};
	int32 BaseTick = 0;
	uint32 Hash = 0;
	uint32 SnapRng = 1;
	int32 DoorOpen = 0;
	TMap<int32, int8> RemoteDx;
	TMap<int32, int32> TurnFpt;
};

inline bool NsIsTurnFpt(int32 Value)
{
	return Value >= 2 && Value <= 6;
}
