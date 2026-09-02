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
	}
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
	TSharedRef<FInternetAddr> Dest = SS->CreateInternetAddr();
	if (!MakeDest(Dst, Dest))
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
	uint8 Buf[Ns::MaxPacketBytes + 1];
	for (;;)
	{
		TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
		int32 Read = 0;
		if (!Socks[Di]->RecvFrom(Buf, Ns::MaxPacketBytes + 1, Read, *From))
		{
			break;
		}
		if (Read <= 0)
		{
			break;
		}
		if (Read > Ns::MaxPacketBytes)
		{
			continue;
		}
		TArray<uint8> Bytes;
		Bytes.Append(Buf, Read);
		FNsPacket Wired;
		if (!NsDecodePacket(Bytes, Wired))
		{
			continue;
		}
		ENsAddr Src;
		if (!FindPeer(*From, Src))
		{
			continue;
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

bool FNsUdpNet::RendezvousExchange(const TCHAR* HubHost, int32 HubPort)
{
	bool bPeer = false;
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
			if (Socks[i] && RendezvousRecvPeer(static_cast<ENsAddr>(i)))
			{
				bPeer = true;
			}
		}
		if (bPeer)
		{
			return true;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	return false;
}
