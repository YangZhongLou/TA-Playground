// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsUdpNet.h"
#include "NsCodec.h"
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
		if (Seq.Accept(Dst, Src, Wired.Seq))
		{
			Out.Add(MoveTemp(Wired));
		}
	}
}
