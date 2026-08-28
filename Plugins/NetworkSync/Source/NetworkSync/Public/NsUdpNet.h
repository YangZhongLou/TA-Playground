// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsNet.h"

class FSocket;
class FInternetAddr;

class NETWORKSYNC_API FNsUdpNet : public INsNet
{
public:
	FNsUdpNet() = default;
	virtual ~FNsUdpNet() override;
	FNsUdpNet(const FNsUdpNet&) = delete;
	FNsUdpNet& operator=(const FNsUdpNet&) = delete;

	bool Bind(ENsAddr Addr, int32 Port = 0, bool bAnyAddress = false);
	bool BindLoopback(int32 BasePort = 0);
	bool SetPeer(ENsAddr Addr, const TCHAR* Host, int32 Port);
	void Close();
	bool IsBound() const;
	bool Owns(ENsAddr Addr) const;
	int32 BoundPort(ENsAddr Addr) const;
	int32 PeerPort(ENsAddr Addr) const;

	virtual void Send(ENsAddr Src, ENsAddr Dst, const FNsPacket& Packet) override;
	virtual void Drain(ENsAddr Dst, TArray<FNsPacket>& Out) override;

private:
	bool MakeDest(ENsAddr Dst, TSharedRef<FInternetAddr>& Out) const;
	bool FindPeer(const FInternetAddr& From, ENsAddr& OutAddr) const;
	void DestroySock(int32 Index);

	FSocket* Socks[3] = {};
	int32 LocalPorts[3] = {};
	FString PeerHosts[3];
	int32 PeerPorts[3] = {};
	FNsSeqWindow Seq;
};
