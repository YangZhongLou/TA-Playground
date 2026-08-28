// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsFakeNet.h"

class FSocket;

class NETWORKSYNC_API FNsUdpNet : public INsNet
{
public:
	FNsUdpNet() = default;
	virtual ~FNsUdpNet() override;
	FNsUdpNet(const FNsUdpNet&) = delete;
	FNsUdpNet& operator=(const FNsUdpNet&) = delete;

	bool BindLoopback(int32 BasePort = 0);
	void Close();
	bool IsBound() const { return Socks[0] != nullptr; }
	int32 BoundPort(ENsAddr Addr) const;

	virtual void Send(ENsAddr Src, ENsAddr Dst, const FNsPacket& Packet) override;
	virtual void Drain(ENsAddr Dst, TArray<FNsPacket>& Out) override;

private:
	FSocket* Socks[3] = {};
	int32 Ports[3] = {};
	FNsSeqWindow Seq;
};
