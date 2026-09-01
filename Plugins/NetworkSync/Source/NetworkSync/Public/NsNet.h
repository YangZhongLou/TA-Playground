// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsPacket.h"

struct NETWORKSYNC_API FNsSeqWindow
{
	FNsSeqWindow();

	int32 NextSeq[3] = {1, 1, 1};
	uint32 SendSession[3] = {};
	uint32 RecvSession[3][3] = {};
	int32 RecvMax[3][3] = {};
	uint32 RecvBits[3][3] = {};
	TArray<uint32> RetiredSessions[3][3];

	void Stamp(ENsAddr Src, FNsPacket& Packet);
	bool Accept(ENsAddr Dst, ENsAddr Src, uint32 Session, int32 Seq);
	bool Accept(ENsAddr Dst, ENsAddr Src, int32 Seq);
};

class NETWORKSYNC_API INsNet
{
public:
	virtual ~INsNet() = default;
	double Now = 0.0;
	virtual void Send(ENsAddr Src, ENsAddr Dst, const FNsPacket& Packet) = 0;
	virtual void Drain(ENsAddr Dst, TArray<FNsPacket>& Out) = 0;
	virtual void Advance(double Ms) { Now += Ms; }
	virtual void ResetSession() { Now = 0.0; }
};
