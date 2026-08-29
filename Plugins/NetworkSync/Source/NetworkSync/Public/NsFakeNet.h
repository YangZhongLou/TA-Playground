// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsNet.h"

class NETWORKSYNC_API FNsFakeNet : public INsNet
{
public:
	float RttMs = 80.f;
	float Drop = 0.f;
	float JitterMs = 5.f;
	FRandomStream Rng;
	bool bDropType = false;
	ENsMsg DropType = ENsMsg::C2SInput;

	virtual void Send(ENsAddr Src, ENsAddr Dst, const FNsPacket& Packet) override;
	virtual void Drain(ENsAddr Dst, TArray<FNsPacket>& Out) override;
	virtual void ResetSession() override;

private:
	TArray<FNsPacket> Queue;
	FNsSeqWindow Seq;
};
