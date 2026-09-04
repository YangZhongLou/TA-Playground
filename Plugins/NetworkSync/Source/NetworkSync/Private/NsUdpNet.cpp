// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsUdpNet.h"
#include "NsCodec.h"
#include "HAL/PlatformProcess.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

namespace
{
	bool NsIsLoopbackHost(const FString& Host)
	{
		return Host.Equals(TEXT("127.0.0.1"), ESearchCase::IgnoreCase)
			|| Host.Equals(TEXT("localhost"), ESearchCase::IgnoreCase)
			|| Host.StartsWith(TEXT("127."));
	}
}

FNsUdpNet::~FNsUdpNet()
{
	Close();
}

bool FNsUdpNet::IsBound() const
{
	return Socks[0] != nullptr || Socks[1] != nullptr || Socks[2] != nullptr;
}

bool FNsUdpNet::Owns(ENsAddr Addr) const
{
	const int32 i = static_cast<int32>(Addr);
	return i >= 0 && i <= 2 && Socks[i] != nullptr;
}

int32 FNsUdpNet::BoundPort(ENsAddr Addr) const
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2)
	{
		return 0;
	}
	return LocalPorts[i];
}

int32 FNsUdpNet::PeerPort(ENsAddr Addr) const
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2)
	{
		return 0;
	}
	return PeerPorts[i];
}

void FNsUdpNet::DestroySock(int32 Index)
{
	if (Index < 0 || Index > 2 || !Socks[Index])
	{
		return;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	Socks[Index]->Close();
	if (SS)
	{
		SS->DestroySocket(Socks[Index]);
	}
	Socks[Index] = nullptr;
	LocalPorts[Index] = 0;
}

void FNsUdpNet::Close()
{
	for (int32 i = 0; i < 3; ++i)
	{
		DestroySock(i);
		PeerHosts[i].Reset();
		PeerPorts[i] = 0;
		MappedIpv4[i] = 0;
		MappedPorts[i] = 0;
		RelayedIpv4[i] = 0;
		RelayedPorts[i] = 0;
		PeerCandCount[i] = 0;
	}
	TurnIpv4 = 0;
	TurnServerPort = 0;
	bTurnRelay = false;
	ResetSession();
}

void FNsUdpNet::ResetSession()
{
	Now = 0.0;
	Seq = FNsSeqWindow();
}

bool FNsUdpNet::Bind(ENsAddr Addr, int32 Port, bool bAnyAddress)
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2)
	{
		return false;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return false;
	}
	DestroySock(i);
	FSocket* Sock = SS->CreateSocket(NAME_DGram, TEXT("NsUdp"), FName(FNetworkProtocolTypes::IPv4));
	if (!Sock)
	{
		return false;
	}
	Sock->SetNonBlocking(true);
	Sock->SetReuseAddr(true);
	TSharedRef<FInternetAddr> BindAddr = SS->CreateInternetAddr();
	if (bAnyAddress)
	{
		BindAddr->SetAnyAddress();
	}
	else
	{
		BindAddr->SetLoopbackAddress();
	}
	BindAddr->SetPort(Port);
	if (!Sock->Bind(*BindAddr))
	{
		SS->DestroySocket(Sock);
		return false;
	}
	TSharedRef<FInternetAddr> Bound = SS->CreateInternetAddr();
	Sock->GetAddress(*Bound);
	const int32 BoundPortValue = Bound->GetPort();
	if (BoundPortValue <= 0)
	{
		SS->DestroySocket(Sock);
		return false;
	}
	Socks[i] = Sock;
	LocalPorts[i] = BoundPortValue;
	if (PeerPorts[i] <= 0)
	{
		PeerHosts[i] = TEXT("127.0.0.1");
		PeerPorts[i] = BoundPortValue;
	}
	return true;
}

bool FNsUdpNet::BindLoopback(int32 BasePort)
{
	Close();
	for (int32 i = 0; i < 3; ++i)
	{
		const int32 Port = (BasePort > 0) ? (BasePort + i) : 0;
		if (!Bind(static_cast<ENsAddr>(i), Port, false))
		{
			Close();
			return false;
		}
	}
	return true;
}

bool FNsUdpNet::SetPeer(ENsAddr Addr, const TCHAR* Host, int32 Port)
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || Port <= 0 || Host == nullptr || Host[0] == 0)
	{
		return false;
	}
	PeerHosts[i] = Host;
	PeerPorts[i] = Port;
	return true;
}

bool FNsUdpNet::EnableTurnRelay(const TCHAR* Host, int32 Port)
{
	uint32 Parsed = 0;
	if (!Host || Port <= 0 || !NsStunParseIpv4(Host, Parsed))
	{
		return false;
	}
	TurnIpv4 = Parsed;
	TurnServerPort = Port;
	bTurnRelay = true;
	return true;
}

bool FNsUdpNet::UsesTurnRelay() const
{
	return bTurnRelay;
}

bool FNsUdpNet::MakeDest(ENsAddr Dst, TSharedRef<FInternetAddr>& Out) const
{
	const int32 Di = static_cast<int32>(Dst);
	if (Di < 0 || Di > 2 || PeerPorts[Di] <= 0)
	{
		return false;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return false;
	}
	const FString& Host = PeerHosts[Di];
	TSharedPtr<FInternetAddr> Resolved;
	if (!Host.IsEmpty())
	{
		Resolved = SS->GetAddressFromString(Host);
	}
	if (!Resolved.IsValid())
	{
		Resolved = SS->CreateInternetAddr();
		bool bOk = false;
		if (!Host.IsEmpty())
		{
			Resolved->SetIp(*Host, bOk);
		}
		if (!bOk)
		{
			return false;
		}
	}
	Resolved->SetPort(PeerPorts[Di]);
	Out = Resolved.ToSharedRef();
	return true;
}

bool FNsUdpNet::FindPeer(const FInternetAddr& From, ENsAddr& OutAddr) const
{
	const int32 FromPort = From.GetPort();
	const FString FromHost = From.ToString(false);
	int32 Matched = -1;
	for (int32 i = 0; i < 3; ++i)
	{
		if (PeerPorts[i] != FromPort)
		{
			continue;
		}
		const FString& Peer = PeerHosts[i];
		if (Peer.IsEmpty())
		{
			continue;
		}
		const bool bHostOk = FromHost.Equals(Peer, ESearchCase::IgnoreCase)
			|| (NsIsLoopbackHost(Peer) && NsIsLoopbackHost(FromHost));
		if (!bHostOk)
		{
			continue;
		}
		if (Matched >= 0)
		{
			return false;
		}
		Matched = i;
	}
	if (Matched < 0)
	{
		return false;
	}
	OutAddr = static_cast<ENsAddr>(Matched);
	return true;
}

void FNsUdpNet::Send(ENsAddr Src, ENsAddr Dst, const FNsPacket& Packet)
{
	const int32 Si = static_cast<int32>(Src);
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (Si < 0 || Si > 2 || !Socks[Si] || !SS)
	{
		return;
	}
	const int32 Di = static_cast<int32>(Dst);
	const bool bRelay = bTurnRelay && TurnIpv4 != 0 && TurnServerPort > 0
		&& Di >= 0 && Di <= 2 && !Socks[Di] && PeerPorts[Di] > 0;
	TSharedRef<FInternetAddr> Dest = SS->CreateInternetAddr();
	if (!bRelay && !MakeDest(Dst, Dest))
	{
		return;
	}
	FNsPacket Copy = Packet;
	Copy.Src = Src;
	Copy.Dst = Dst;
	TArray<FNsPacket> Parts;
	NsSplitForMtu(Copy, Parts);
	for (FNsPacket& Part : Parts)
	{
		Part.Src = Src;
		Part.Dst = Dst;
		Seq.Stamp(Src, Part);
		TArray<uint8> Bytes;
		if (!NsEncodePacket(Part, Bytes))
		{
			continue;
		}
		if (bRelay)
		{
			const uint16 Channel = static_cast<uint16>(NsTurnChannelMin + Di);
			StunSendChannelData(Src, *NsStunIpv4ToString(TurnIpv4), TurnServerPort, Channel, Bytes);
			continue;
		}
		int32 Sent = 0;
		Socks[Si]->SendTo(Bytes.GetData(), Bytes.Num(), Sent, *Dest);
	}
}

void FNsUdpNet::Drain(ENsAddr Dst, TArray<FNsPacket>& Out)
{
	Out.Reset();
	const int32 Di = static_cast<int32>(Dst);
	if (Di < 0 || Di > 2 || !Socks[Di])
	{
		return;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return;
	}
	constexpr int32 MaxChannelBytes = (Ns::MaxPacketBytes + NsChannelDataHeaderBytes + 3) & ~3;
	uint8 Buf[MaxChannelBytes + 1];
	for (;;)
	{
		TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
		int32 Read = 0;
		if (!Socks[Di]->RecvFrom(Buf, sizeof(Buf), Read, *From))
		{
			break;
		}
		if (Read <= 0)
		{
			break;
		}
		if (Read > MaxChannelBytes)
		{
			continue;
		}
		TArray<uint8> Bytes;
		Bytes.Append(Buf, Read);
		FNsPacket Wired;
		ENsAddr Src;
		uint16 Channel = 0;
		TArray<uint8> Payload;
		if (bTurnRelay && NsDecodeChannelData(Bytes, Channel, Payload))
		{
			const int32 Slot = static_cast<int32>(Channel) - NsTurnChannelMin;
			if (Slot < 0 || Slot > 2 || Slot == Di || Socks[Slot] || !NsDecodePacket(Payload, Wired))
			{
				continue;
			}
			Src = static_cast<ENsAddr>(Slot);
		}
		else
		{
			if (Read > Ns::MaxPacketBytes || !NsDecodePacket(Bytes, Wired) || !FindPeer(*From, Src))
			{
				continue;
			}
		}
		Wired.Src = Src;
		Wired.Dst = Dst;
		if (Seq.Accept(Dst, Src, Wired.Session, Wired.Seq))
		{
			Out.Add(MoveTemp(Wired));
		}
	}
}

namespace
{
bool NsUdpResolve(const TCHAR* Host, int32 Port, TSharedRef<FInternetAddr>& Out)
{
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS || Host == nullptr || Host[0] == 0 || Port <= 0)
	{
		return false;
	}
	TSharedPtr<FInternetAddr> Resolved = SS->GetAddressFromString(Host);
	if (!Resolved.IsValid())
	{
		Resolved = SS->CreateInternetAddr();
		bool bOk = false;
		Resolved->SetIp(Host, bOk);
		if (!bOk)
		{
			return false;
		}
	}
	Resolved->SetPort(Port);
	Out = Resolved.ToSharedRef();
	return true;
}

bool NsUdpSendTo(FSocket* Sock, const TCHAR* Host, int32 Port, const TArray<uint8>& Bytes)
{
	if (!Sock || Bytes.Num() <= 0)
	{
		return false;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return false;
	}
	TSharedRef<FInternetAddr> Dest = SS->CreateInternetAddr();
	if (!NsUdpResolve(Host, Port, Dest))
	{
		return false;
	}
	int32 Sent = 0;
	return Sock->SendTo(Bytes.GetData(), Bytes.Num(), Sent, *Dest) && Sent == Bytes.Num();
}

bool NsIcePickAddress(const TArray<FNsIceCandidate>& Cands, uint32& OutIpv4, int32& OutPort)
{
	OutIpv4 = 0;
	OutPort = 0;
	const FNsIceCandidate* Pick = nullptr;
	for (const FNsIceCandidate& Cand : Cands)
	{
		if (Cand.Port <= 0)
		{
			continue;
		}
		if (Cand.Type == ENsIceType::Host)
		{
			Pick = &Cand;
			break;
		}
		if (!Pick)
		{
			Pick = &Cand;
		}
	}
	if (!Pick)
	{
		return false;
	}
	OutIpv4 = Pick->Ipv4;
	OutPort = Pick->Port;
	return true;
}

struct FNsStunPendingRequest
{
	int32 SocketIndex = 0;
	uint8 TxId[NsStunTxIdBytes] = {};
	bool bComplete = false;
};

bool NsUdpAwaitStunReplies(FSocket* const* Socks, TArray<FNsStunPendingRequest>& Requests,
	bool (*DecodeSuccess)(const TArray<uint8>&, const uint8*))
{
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS || Requests.IsEmpty())
	{
		return false;
	}
	int32 Remaining = Requests.Num();
	for (int32 Try = 0; Try < 50; ++Try)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			if (!Socks[i])
			{
				continue;
			}
			uint8 Buf[512];
			TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
			int32 Read = 0;
			if (!Socks[i]->RecvFrom(Buf, sizeof(Buf), Read, *From) || Read <= 0)
			{
				continue;
			}
			TArray<uint8> Bytes;
			Bytes.Append(Buf, Read);
			// Match the received transaction against every pending peer on this socket.
			for (FNsStunPendingRequest& Request : Requests)
			{
				if (Request.SocketIndex == i && !Request.bComplete && DecodeSuccess(Bytes, Request.TxId))
				{
					Request.bComplete = true;
					if (--Remaining == 0)
					{
						return true;
					}
					break;
				}
			}
		}
		FPlatformProcess::Sleep(0.001f);
	}
	return false;
}
}

bool FNsUdpNet::StunSendBind(ENsAddr Addr, const TCHAR* Host, int32 Port, uint8 TxId[NsStunTxIdBytes])
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i] || !TxId)
	{
		return false;
	}
	TArray<uint8> Bytes;
	if (!NsStunEncodeBindRequest(TxId, Bytes))
	{
		return false;
	}
	return NsUdpSendTo(Socks[i], Host, Port, Bytes);
}

bool FNsUdpNet::StunSendNominate(ENsAddr Addr, const TCHAR* Host, int32 Port, uint8 TxId[NsStunTxIdBytes])
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i] || !TxId)
	{
		return false;
	}
	TArray<uint8> Bytes;
	if (!NsStunEncodeBindNominate(TxId, Bytes))
	{
		return false;
	}
	return NsUdpSendTo(Socks[i], Host, Port, Bytes);
}

bool FNsUdpNet::StunSendAllocate(ENsAddr Addr, const TCHAR* Host, int32 Port, uint8 TxId[NsStunTxIdBytes])
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i] || !TxId)
	{
		return false;
	}
	TArray<uint8> Bytes;
	if (!NsStunEncodeAllocateRequest(TxId, Bytes))
	{
		return false;
	}
	return NsUdpSendTo(Socks[i], Host, Port, Bytes);
}

bool FNsUdpNet::StunSendPermission(ENsAddr Addr, const TCHAR* TurnHost, int32 TurnPort,
	const TCHAR* PeerHost, int32 PeerPort, uint8 TxId[NsStunTxIdBytes])
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i] || !TxId || !PeerHost)
	{
		return false;
	}
	uint32 PeerIpv4 = 0;
	if (!NsStunParseIpv4(PeerHost, PeerIpv4))
	{
		return false;
	}
	TArray<uint8> Bytes;
	if (!NsStunEncodeCreatePermissionRequest(TxId, PeerIpv4, PeerPort, Bytes))
	{
		return false;
	}
	return NsUdpSendTo(Socks[i], TurnHost, TurnPort, Bytes);
}

bool FNsUdpNet::StunSendIndication(ENsAddr Addr, const TCHAR* Host, int32 Port, uint8 TxId[NsStunTxIdBytes])
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i] || !TxId)
	{
		return false;
	}
	TArray<uint8> Bytes;
	if (!NsStunEncodeBindIndication(TxId, Bytes))
	{
		return false;
	}
	return NsUdpSendTo(Socks[i], Host, Port, Bytes);
}

bool FNsUdpNet::StunRecvMapped(ENsAddr Addr, const uint8 TxId[NsStunTxIdBytes], FString& OutHost, int32& OutPort)
{
	OutHost.Reset();
	OutPort = 0;
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i] || !TxId)
	{
		return false;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return false;
	}
	uint8 Buf[512];
	TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
	int32 Read = 0;
	if (!Socks[i]->RecvFrom(Buf, 512, Read, *From) || Read <= 0)
	{
		return false;
	}
	TArray<uint8> Bytes;
	Bytes.Append(Buf, Read);
	uint32 Ipv4 = 0;
	int32 MappedPort = 0;
	if (!NsStunDecodeMapped(Bytes, TxId, Ipv4, MappedPort))
	{
		return false;
	}
	OutHost = NsStunIpv4ToString(Ipv4);
	OutPort = MappedPort;
	MappedIpv4[i] = Ipv4;
	MappedPorts[i] = OutPort;
	return true;
}

bool FNsUdpNet::StunRecvRelayed(ENsAddr Addr, const uint8 TxId[NsStunTxIdBytes], FString& OutHost, int32& OutPort)
{
	OutHost.Reset();
	OutPort = 0;
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i] || !TxId)
	{
		return false;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return false;
	}
	uint8 Buf[512];
	TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
	int32 Read = 0;
	if (!Socks[i]->RecvFrom(Buf, 512, Read, *From) || Read <= 0)
	{
		return false;
	}
	TArray<uint8> Bytes;
	Bytes.Append(Buf, Read);
	uint32 Ipv4 = 0;
	int32 RelayedPort = 0;
	if (!NsStunDecodeRelayed(Bytes, TxId, Ipv4, RelayedPort))
	{
		return false;
	}
	OutHost = NsStunIpv4ToString(Ipv4);
	OutPort = RelayedPort;
	RelayedIpv4[i] = Ipv4;
	RelayedPorts[i] = OutPort;
	return true;
}

bool FNsUdpNet::StunRecvPermission(ENsAddr Addr, const uint8 TxId[NsStunTxIdBytes])
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i] || !TxId)
	{
		return false;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return false;
	}
	uint8 Buf[512];
	TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
	int32 Read = 0;
	if (!Socks[i]->RecvFrom(Buf, 512, Read, *From) || Read <= 0)
	{
		return false;
	}
	TArray<uint8> Bytes;
	Bytes.Append(Buf, Read);
	return NsStunDecodePermissionSuccess(Bytes, TxId);
}

bool FNsUdpNet::StunSendChannelBind(ENsAddr Addr, const TCHAR* TurnHost, int32 TurnPort, uint16 Channel,
	const TCHAR* PeerHost, int32 PeerPort, uint8 TxId[NsStunTxIdBytes])
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i] || !TxId || !PeerHost)
	{
		return false;
	}
	uint32 PeerIpv4 = 0;
	if (!NsStunParseIpv4(PeerHost, PeerIpv4))
	{
		return false;
	}
	TArray<uint8> Bytes;
	if (!NsStunEncodeChannelBindRequest(TxId, Channel, PeerIpv4, PeerPort, Bytes))
	{
		return false;
	}
	return NsUdpSendTo(Socks[i], TurnHost, TurnPort, Bytes);
}

bool FNsUdpNet::StunRecvChannelBind(ENsAddr Addr, const uint8 TxId[NsStunTxIdBytes])
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i] || !TxId)
	{
		return false;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return false;
	}
	uint8 Buf[512];
	TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
	int32 Read = 0;
	if (!Socks[i]->RecvFrom(Buf, 512, Read, *From) || Read <= 0)
	{
		return false;
	}
	TArray<uint8> Bytes;
	Bytes.Append(Buf, Read);
	return NsStunDecodeChannelBindSuccess(Bytes, TxId);
}

bool FNsUdpNet::StunSendChannelData(ENsAddr Addr, const TCHAR* TurnHost, int32 TurnPort, uint16 Channel,
	const TArray<uint8>& Payload)
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i] || Payload.Num() > Ns::MaxPacketBytes)
	{
		return false;
	}
	TArray<uint8> Bytes;
	if (!NsEncodeChannelData(Channel, Payload, Bytes))
	{
		return false;
	}
	return NsUdpSendTo(Socks[i], TurnHost, TurnPort, Bytes);
}

bool FNsUdpNet::StunRecvChannelData(ENsAddr Addr, uint16& OutChannel, TArray<uint8>& OutPayload)
{
	OutChannel = 0;
	OutPayload.Reset();
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i])
	{
		return false;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return false;
	}
	constexpr int32 MaxChannelBytes = (Ns::MaxPacketBytes + NsChannelDataHeaderBytes + 3) & ~3;
	uint8 Buf[MaxChannelBytes + 1];
	TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
	int32 Read = 0;
	if (!Socks[i]->RecvFrom(Buf, sizeof(Buf), Read, *From) || Read <= 0 || Read > MaxChannelBytes)
	{
		return false;
	}
	TArray<uint8> Bytes;
	Bytes.Append(Buf, Read);
	return NsDecodeChannelData(Bytes, OutChannel, OutPayload);
}

bool FNsUdpNet::StunRecvIndication(ENsAddr Addr)
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i])
	{
		return false;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return false;
	}
	uint8 Buf[512];
	TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
	int32 Read = 0;
	if (!Socks[i]->RecvFrom(Buf, 512, Read, *From) || Read <= 0)
	{
		return false;
	}
	TArray<uint8> Bytes;
	Bytes.Append(Buf, Read);
	return NsStunIsBindIndication(Bytes);
}

bool FNsUdpNet::StunServe(ENsAddr Addr, const uint8* ExpectTxId, FString& OutHost, int32& OutPort)
{
	OutHost.Reset();
	OutPort = 0;
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i])
	{
		return false;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return false;
	}
	uint8 Buf[512];
	TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
	int32 Read = 0;
	if (!Socks[i]->RecvFrom(Buf, 512, Read, *From) || Read <= 0)
	{
		return false;
	}
	TArray<uint8> Bytes;
	Bytes.Append(Buf, Read);
	if (NsStunIsBindRequest(Bytes))
	{
		uint8 ReqTx[NsStunTxIdBytes];
		uint32 FromIp = 0;
		const int32 FromPort = From->GetPort();
		if (!NsStunReadTxId(Bytes, ReqTx) || FromPort <= 0
			|| !NsStunParseIpv4(From->ToString(false), FromIp))
		{
			return false;
		}
		TArray<uint8> Reply;
		if (!NsStunEncodeXorMappedReply(ReqTx, FromIp, FromPort, Reply))
		{
			return false;
		}
		int32 Sent = 0;
		Socks[i]->SendTo(Reply.GetData(), Reply.Num(), Sent, *From);
		return false;
	}
	if (!ExpectTxId)
	{
		return false;
	}
	uint32 Ipv4 = 0;
	int32 MappedPort = 0;
	if (!NsStunDecodeMapped(Bytes, ExpectTxId, Ipv4, MappedPort))
	{
		return false;
	}
	OutHost = NsStunIpv4ToString(Ipv4);
	OutPort = MappedPort;
	return true;
}

bool FNsUdpNet::PunchPeers()
{
	bool bSent = false;
	for (int32 From = 0; From < 3; ++From)
	{
		if (!Socks[From])
		{
			continue;
		}
		for (int32 To = 0; To < 3; ++To)
		{
			if (Socks[To] || PeerPorts[To] <= 0 || PeerHosts[To].IsEmpty())
			{
				continue;
			}
			uint8 TxId[NsStunTxIdBytes];
			NsStunFillTxId(TxId);
			for (int32 n = 0; n < 3; ++n)
			{
				if (StunSendIndication(static_cast<ENsAddr>(From), *PeerHosts[To], PeerPorts[To], TxId))
				{
					bSent = true;
				}
			}
		}
	}
	return bSent;
}

bool FNsUdpNet::StunCheckPeers()
{
	uint8 TxIds[3][NsStunTxIdBytes] = {};
	bool bExpect[3] = {};
	bool bSent = false;
	for (int32 From = 0; From < 3; ++From)
	{
		if (!Socks[From])
		{
			continue;
		}
		NsStunFillTxId(TxIds[From]);
		for (int32 To = 0; To < 3; ++To)
		{
			if (Socks[To] || PeerPorts[To] <= 0 || PeerHosts[To].IsEmpty())
			{
				continue;
			}
			for (int32 n = 0; n < 3; ++n)
			{
				if (StunSendBind(static_cast<ENsAddr>(From), *PeerHosts[To], PeerPorts[To], TxIds[From]))
				{
					bSent = true;
					bExpect[From] = true;
				}
			}
		}
	}
	if (!bSent)
	{
		return false;
	}
	for (int32 Try = 0; Try < 50; ++Try)
	{
		bool bHit = false;
		for (int32 i = 0; i < 3; ++i)
		{
			if (!Socks[i])
			{
				continue;
			}
			FString Host;
			int32 Port = 0;
			if (StunServe(static_cast<ENsAddr>(i), bExpect[i] ? TxIds[i] : nullptr, Host, Port))
			{
				bHit = true;
			}
		}
		if (bHit)
		{
			return true;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	return false;
}

bool FNsUdpNet::StunPermitPeers(const TCHAR* TurnHost, int32 TurnPort)
{
	if (!TurnHost || TurnPort <= 0)
	{
		return false;
	}
	TArray<FNsStunPendingRequest> Requests;
	for (int32 From = 0; From < 3; ++From)
	{
		if (!Socks[From])
		{
			continue;
		}
		for (int32 To = 0; To < 3; ++To)
		{
			if (Socks[To] || PeerPorts[To] <= 0 || PeerHosts[To].IsEmpty())
			{
				continue;
			}
			FNsStunPendingRequest Request;
			Request.SocketIndex = From;
			NsStunFillTxId(Request.TxId);
			if (!StunSendPermission(static_cast<ENsAddr>(From), TurnHost, TurnPort,
				*PeerHosts[To], PeerPorts[To], Request.TxId))
			{
				return false;
			}
			Requests.Add(Request);
		}
	}
	return NsUdpAwaitStunReplies(Socks, Requests, NsStunDecodePermissionSuccess);
}

bool FNsUdpNet::StunBindPeerChannels(const TCHAR* TurnHost, int32 TurnPort)
{
	if (!TurnHost || TurnPort <= 0)
	{
		return false;
	}
	TArray<FNsStunPendingRequest> Requests;
	for (int32 From = 0; From < 3; ++From)
	{
		if (!Socks[From])
		{
			continue;
		}
		for (int32 To = 0; To < 3; ++To)
		{
			if (Socks[To] || PeerPorts[To] <= 0 || PeerHosts[To].IsEmpty())
			{
				continue;
			}
			const uint16 Channel = static_cast<uint16>(NsTurnChannelMin + To);
			FNsStunPendingRequest Request;
			Request.SocketIndex = From;
			NsStunFillTxId(Request.TxId);
			if (!StunSendChannelBind(static_cast<ENsAddr>(From), TurnHost, TurnPort, Channel,
				*PeerHosts[To], PeerPorts[To], Request.TxId))
			{
				return false;
			}
			Requests.Add(Request);
		}
	}
	return NsUdpAwaitStunReplies(Socks, Requests, NsStunDecodeChannelBindSuccess);
}

bool FNsUdpNet::GatherIceCandidates(ENsAddr Addr, TArray<FNsIceCandidate>& Out) const
{
	Out.Reset();
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2 || !Socks[i] || LocalPorts[i] <= 0)
	{
		return false;
	}
	uint32 HostIp = 0;
	if (!NsStunParseIpv4(TEXT("127.0.0.1"), HostIp))
	{
		return false;
	}
	auto AddUnique = [&Out](ENsIceType Type, uint32 Ipv4, int32 Port)
	{
		if (Port <= 0 || Out.Num() >= NsIceMaxCandidates)
		{
			return;
		}
		for (const FNsIceCandidate& Cand : Out)
		{
			if (Cand.Ipv4 == Ipv4 && Cand.Port == Port)
			{
				return;
			}
		}
		FNsIceCandidate Cand;
		Cand.Type = Type;
		Cand.Ipv4 = Ipv4;
		Cand.Port = Port;
		Out.Add(Cand);
	};
	AddUnique(ENsIceType::Host, HostIp, LocalPorts[i]);
	if (MappedPorts[i] > 0)
	{
		AddUnique(ENsIceType::Srflx, MappedIpv4[i], MappedPorts[i]);
	}
	if (RelayedPorts[i] > 0)
	{
		AddUnique(ENsIceType::Relay, RelayedIpv4[i], RelayedPorts[i]);
	}
	return Out.Num() > 0;
}

bool FNsUdpNet::IceSendOffer(ENsAddr From, const TCHAR* HubHost, int32 HubPort)
{
	TArray<FNsIceCandidate> Cands;
	if (!GatherIceCandidates(From, Cands))
	{
		return false;
	}
	TArray<uint8> Bytes;
	if (!NsIceEncode(static_cast<uint8>(From), Cands, Bytes))
	{
		return false;
	}
	const int32 i = static_cast<int32>(From);
	return NsUdpSendTo(Socks[i], HubHost, HubPort, Bytes);
}

bool FNsUdpNet::IceRecvPeer(ENsAddr From)
{
	const int32 i = static_cast<int32>(From);
	if (i < 0 || i > 2 || !Socks[i])
	{
		return false;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return false;
	}
	uint8 Buf[64];
	TSharedRef<FInternetAddr> FromAddr = SS->CreateInternetAddr();
	int32 Read = 0;
	if (!Socks[i]->RecvFrom(Buf, 64, Read, *FromAddr) || Read <= 0)
	{
		return false;
	}
	TArray<uint8> Bytes;
	Bytes.Append(Buf, Read);
	uint8 Slot = 0;
	TArray<FNsIceCandidate> Cands;
	uint32 Ipv4 = 0;
	int32 Port = 0;
	if (!NsIceDecode(Bytes, Slot, Cands) || Slot > 2 || Socks[Slot]
		|| !NsIcePickAddress(Cands, Ipv4, Port))
	{
		return false;
	}
	const int32 n = FMath::Min(Cands.Num(), NsIceMaxCandidates);
	PeerCandCount[Slot] = n;
	for (int32 c = 0; c < n; ++c)
	{
		PeerCands[Slot][c] = Cands[c];
	}
	return SetPeer(static_cast<ENsAddr>(Slot), *NsStunIpv4ToString(Ipv4), Port);
}

bool FNsUdpNet::IceExchange(const TCHAR* HubHost, int32 HubPort, const TArray<ENsAddr>& RequiredPeers)
{
	if (RequiredPeers.IsEmpty())
	{
		return false;
	}
	for (ENsAddr Peer : RequiredPeers)
	{
		const int32 Slot = static_cast<int32>(Peer);
		if (Slot < 0 || Slot > 2 || Socks[Slot])
		{
			return false;
		}
	}
	for (int32 Try = 0; Try < 50; ++Try)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			if (Socks[i])
			{
				IceSendOffer(static_cast<ENsAddr>(i), HubHost, HubPort);
			}
		}
		for (int32 i = 0; i < 3; ++i)
		{
			if (Socks[i])
			{
				IceRecvPeer(static_cast<ENsAddr>(i));
			}
		}
		bool bComplete = true;
		for (ENsAddr Peer : RequiredPeers)
		{
			bComplete &= PeerPort(Peer) > 0;
		}
		if (bComplete)
		{
			return true;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	return false;
}

bool FNsUdpNet::IceCheckPairs()
{
	int32 FromSlot = -1;
	for (int32 i = 0; i < 3; ++i)
	{
		if (Socks[i])
		{
			FromSlot = i;
			break;
		}
	}
	if (FromSlot < 0)
	{
		return false;
	}
	TArray<FNsIceCandidate> Local;
	if (!GatherIceCandidates(static_cast<ENsAddr>(FromSlot), Local))
	{
		return false;
	}
	TArray<FNsIcePair> Pairs[3];
	bool bNeed[3] = {};
	bool bWon[3] = {};
	int32 PairIdx[3] = {};
	for (int32 To = 0; To < 3; ++To)
	{
		if (Socks[To] || PeerCandCount[To] <= 0)
		{
			continue;
		}
		TArray<FNsIceCandidate> Remote;
		for (int32 c = 0; c < PeerCandCount[To]; ++c)
		{
			Remote.Add(PeerCands[To][c]);
		}
		if (NsIceFormPairs(Local, Remote, Pairs[To]))
		{
			bNeed[To] = true;
		}
	}
	bool bAny = false;
	for (int32 To = 0; To < 3; ++To)
	{
		bAny |= bNeed[To];
	}
	if (!bAny)
	{
		return false;
	}
	uint8 TxId[NsStunTxIdBytes];
	NsStunFillTxId(TxId);
	const ENsAddr From = static_cast<ENsAddr>(FromSlot);
	for (int32 Try = 0; Try < 50; ++Try)
	{
		for (int32 To = 0; To < 3; ++To)
		{
			if (!bNeed[To] || bWon[To] || PairIdx[To] >= Pairs[To].Num())
			{
				continue;
			}
			const FNsIceCandidate& Remote = Pairs[To][PairIdx[To]].Remote;
			StunSendNominate(From, *NsStunIpv4ToString(Remote.Ipv4), Remote.Port, TxId);
		}
		for (int32 i = 0; i < 3; ++i)
		{
			if (!Socks[i])
			{
				continue;
			}
			ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
			if (!SS)
			{
				continue;
			}
			uint8 Buf[512];
			TSharedRef<FInternetAddr> ReplyFrom = SS->CreateInternetAddr();
			int32 Read = 0;
			if (!Socks[i]->RecvFrom(Buf, 512, Read, *ReplyFrom) || Read <= 0)
			{
				continue;
			}
			TArray<uint8> Bytes;
			Bytes.Append(Buf, Read);
			if (NsStunIsBindRequest(Bytes))
			{
				uint8 ReqTx[NsStunTxIdBytes];
				uint32 FromIp = 0;
				const int32 FromPort = ReplyFrom->GetPort();
				if (NsStunReadTxId(Bytes, ReqTx) && FromPort > 0
					&& NsStunParseIpv4(ReplyFrom->ToString(false), FromIp))
				{
					TArray<uint8> Reply;
					if (NsStunEncodeXorMappedReply(ReqTx, FromIp, FromPort, Reply))
					{
						int32 Sent = 0;
						Socks[i]->SendTo(Reply.GetData(), Reply.Num(), Sent, *ReplyFrom);
					}
				}
				continue;
			}
			if (i != FromSlot)
			{
				continue;
			}
			uint32 MappedIp = 0;
			int32 MappedPort = 0;
			if (!NsStunDecodeMapped(Bytes, TxId, MappedIp, MappedPort))
			{
				continue;
			}
			uint32 ReplyIp = 0;
			const int32 ReplyPort = ReplyFrom->GetPort();
			if (ReplyPort <= 0 || !NsStunParseIpv4(ReplyFrom->ToString(false), ReplyIp))
			{
				continue;
			}
			for (int32 To = 0; To < 3; ++To)
			{
				if (!bNeed[To] || bWon[To])
				{
					continue;
				}
				for (int32 c = 0; c < PeerCandCount[To]; ++c)
				{
					const FNsIceCandidate& Cand = PeerCands[To][c];
					const bool bHostOk = Cand.Ipv4 == ReplyIp
						|| (NsIsLoopbackHost(NsStunIpv4ToString(Cand.Ipv4))
							&& NsIsLoopbackHost(ReplyFrom->ToString(false)));
					if (Cand.Port == ReplyPort && bHostOk
						&& SetPeer(static_cast<ENsAddr>(To), *NsStunIpv4ToString(ReplyIp), ReplyPort))
					{
						bWon[To] = true;
						break;
					}
				}
			}
		}
		bool bDone = true;
		for (int32 To = 0; To < 3; ++To)
		{
			if (bNeed[To] && !bWon[To])
			{
				bDone = false;
				if ((Try + 1) % 15 == 0)
				{
					++PairIdx[To];
				}
			}
		}
		if (bDone)
		{
			return true;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	for (int32 To = 0; To < 3; ++To)
	{
		if (bNeed[To] && !bWon[To])
		{
			return false;
		}
	}
	return true;
}

bool FNsUdpNet::IceWaitNominate()
{
	bool bNeed[3] = {};
	bool bWon[3] = {};
	bool bAny = false;
	for (int32 To = 0; To < 3; ++To)
	{
		if (Socks[To] || PeerCandCount[To] <= 0)
		{
			continue;
		}
		bNeed[To] = true;
		bAny = true;
	}
	if (!bAny)
	{
		return false;
	}
	for (int32 Try = 0; Try < 50; ++Try)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			if (!Socks[i])
			{
				continue;
			}
			ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
			if (!SS)
			{
				continue;
			}
			uint8 Buf[512];
			TSharedRef<FInternetAddr> ReplyFrom = SS->CreateInternetAddr();
			int32 Read = 0;
			if (!Socks[i]->RecvFrom(Buf, 512, Read, *ReplyFrom) || Read <= 0)
			{
				continue;
			}
			TArray<uint8> Bytes;
			Bytes.Append(Buf, Read);
			if (!NsStunIsBindRequest(Bytes))
			{
				continue;
			}
			uint8 ReqTx[NsStunTxIdBytes];
			uint32 FromIp = 0;
			const int32 FromPort = ReplyFrom->GetPort();
			if (!NsStunReadTxId(Bytes, ReqTx) || FromPort <= 0
				|| !NsStunParseIpv4(ReplyFrom->ToString(false), FromIp))
			{
				continue;
			}
			TArray<uint8> Reply;
			if (NsStunEncodeXorMappedReply(ReqTx, FromIp, FromPort, Reply))
			{
				int32 Sent = 0;
				Socks[i]->SendTo(Reply.GetData(), Reply.Num(), Sent, *ReplyFrom);
			}
			if (!NsStunHasUseCandidate(Bytes))
			{
				continue;
			}
			for (int32 To = 0; To < 3; ++To)
			{
				if (!bNeed[To] || bWon[To])
				{
					continue;
				}
				for (int32 c = 0; c < PeerCandCount[To]; ++c)
				{
					const FNsIceCandidate& Cand = PeerCands[To][c];
					const bool bHostOk = Cand.Ipv4 == FromIp
						|| (NsIsLoopbackHost(NsStunIpv4ToString(Cand.Ipv4))
							&& NsIsLoopbackHost(ReplyFrom->ToString(false)));
					if (Cand.Port == FromPort && bHostOk
						&& SetPeer(static_cast<ENsAddr>(To), *NsStunIpv4ToString(FromIp), FromPort))
					{
						bWon[To] = true;
						break;
					}
				}
			}
		}
		bool bDone = true;
		for (int32 To = 0; To < 3; ++To)
		{
			if (bNeed[To] && !bWon[To])
			{
				bDone = false;
			}
		}
		if (bDone)
		{
			return true;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	for (int32 To = 0; To < 3; ++To)
	{
		if (bNeed[To] && !bWon[To])
		{
			return false;
		}
	}
	return true;
}

bool FNsUdpNet::RendezvousSendOffer(ENsAddr From, const TCHAR* HubHost, int32 HubPort)
{
	const int32 i = static_cast<int32>(From);
	if (i < 0 || i > 2 || !Socks[i])
	{
		return false;
	}
	const int32 AdvPort = (MappedPorts[i] > 0) ? MappedPorts[i] : LocalPorts[i];
	uint32 Ipv4 = MappedIpv4[i];
	if (MappedPorts[i] <= 0 && !NsStunParseIpv4(TEXT("127.0.0.1"), Ipv4))
	{
		return false;
	}
	if (AdvPort <= 0)
	{
		return false;
	}
	TArray<uint8> Bytes;
	if (!NsRendezvousEncode(static_cast<uint8>(i), Ipv4, AdvPort, Bytes))
	{
		return false;
	}
	return NsUdpSendTo(Socks[i], HubHost, HubPort, Bytes);
}

bool FNsUdpNet::RendezvousRecvPeer(ENsAddr From)
{
	const int32 i = static_cast<int32>(From);
	if (i < 0 || i > 2 || !Socks[i])
	{
		return false;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return false;
	}
	uint8 Buf[64];
	TSharedRef<FInternetAddr> FromAddr = SS->CreateInternetAddr();
	int32 Read = 0;
	if (!Socks[i]->RecvFrom(Buf, 64, Read, *FromAddr) || Read <= 0)
	{
		return false;
	}
	TArray<uint8> Bytes;
	Bytes.Append(Buf, Read);
	uint8 Slot = 0;
	uint32 Ipv4 = 0;
	int32 Port = 0;
	if (!NsRendezvousDecode(Bytes, Slot, Ipv4, Port) || Slot > 2 || Socks[Slot])
	{
		return false;
	}
	return SetPeer(static_cast<ENsAddr>(Slot), *NsStunIpv4ToString(Ipv4), Port);
}

bool FNsUdpNet::RendezvousExchange(const TCHAR* HubHost, int32 HubPort, const TArray<ENsAddr>& RequiredPeers)
{
	if (RequiredPeers.IsEmpty())
	{
		return false;
	}
	for (ENsAddr Peer : RequiredPeers)
	{
		const int32 Slot = static_cast<int32>(Peer);
		if (Slot < 0 || Slot > 2 || Socks[Slot])
		{
			return false;
		}
	}
	for (int32 Try = 0; Try < 50; ++Try)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			if (Socks[i])
			{
				RendezvousSendOffer(static_cast<ENsAddr>(i), HubHost, HubPort);
			}
		}
		for (int32 i = 0; i < 3; ++i)
		{
			if (Socks[i])
			{
				RendezvousRecvPeer(static_cast<ENsAddr>(i));
			}
		}
		bool bComplete = true;
		for (ENsAddr Peer : RequiredPeers)
		{
			bComplete &= PeerPort(Peer) > 0;
		}
		if (bComplete)
		{
			return true;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	return false;
}
