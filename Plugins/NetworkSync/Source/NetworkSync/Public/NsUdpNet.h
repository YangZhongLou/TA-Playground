// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsNet.h"
#include "NsStun.h"

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

	bool StunSendBind(ENsAddr Addr, const TCHAR* Host, int32 Port, uint8 TxId[NsStunTxIdBytes]);
	bool StunSendAllocate(ENsAddr Addr, const TCHAR* Host, int32 Port, uint8 TxId[NsStunTxIdBytes]);
	bool StunSendIndication(ENsAddr Addr, const TCHAR* Host, int32 Port, uint8 TxId[NsStunTxIdBytes]);
	bool StunRecvMapped(ENsAddr Addr, const uint8 TxId[NsStunTxIdBytes], FString& OutHost, int32& OutPort);
	bool StunRecvRelayed(ENsAddr Addr, const uint8 TxId[NsStunTxIdBytes], FString& OutHost, int32& OutPort);
	bool StunRecvIndication(ENsAddr Addr);
	bool StunServe(ENsAddr Addr, const uint8* ExpectTxId, FString& OutHost, int32& OutPort);
	bool PunchPeers();
	bool StunCheckPeers();
	bool RendezvousSendOffer(ENsAddr From, const TCHAR* HubHost, int32 HubPort);
	bool RendezvousRecvPeer(ENsAddr From);
	bool RendezvousExchange(const TCHAR* HubHost, int32 HubPort);

	virtual void Send(ENsAddr Src, ENsAddr Dst, const FNsPacket& Packet) override;
	virtual void Drain(ENsAddr Dst, TArray<FNsPacket>& Out) override;
	virtual void ResetSession() override;

private:
	bool MakeDest(ENsAddr Dst, TSharedRef<FInternetAddr>& Out) const;
	bool FindPeer(const FInternetAddr& From, ENsAddr& OutAddr) const;
	void DestroySock(int32 Index);

	FSocket* Socks[3] = {};
	int32 LocalPorts[3] = {};
	FString PeerHosts[3];
	int32 PeerPorts[3] = {};
	uint32 MappedIpv4[3] = {};
	int32 MappedPorts[3] = {};
	FNsSeqWindow Seq;
};
