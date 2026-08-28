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

enum class ENsMsg : uint8
{
	C2SInput = 1,
	S2CFrame = 2,
	S2CSnapshot = 3,
	C2SSnapAck = 4,
	P2PInput = 5,
	C2SChecksum = 6,
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
	TMap<int32, int8> RemoteDx;
};

class NETWORKSYNC_API FNsFakeNet
{
public:
	double Now = 0.0;
	float RttMs = 80.f;
	float Drop = 0.f;
	float JitterMs = 5.f;
	FRandomStream Rng;

	void Send(ENsAddr Src, ENsAddr Dst, const FNsPacket& Packet);
	void Drain(ENsAddr Dst, TArray<FNsPacket>& Out);
	void Advance(double Ms);

private:
	bool AcceptSeq(ENsAddr Dst, int32 S);

	TArray<FNsPacket> Queue;
	int32 NextSeq[3] = {1, 1, 1};
	int32 RecvMax[3] = {0, 0, 0};
	uint32 RecvBits[3] = {0, 0, 0};
};
