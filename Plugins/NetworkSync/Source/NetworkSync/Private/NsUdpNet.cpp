// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsUdpNet.h"
#include "NsCodec.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

FNsUdpNet::~FNsUdpNet()
{
	Close();
}

int32 FNsUdpNet::BoundPort(ENsAddr Addr) const
{
	const int32 i = static_cast<int32>(Addr);
	if (i < 0 || i > 2)
	{
		return 0;
	}
	return Ports[i];
}

void FNsUdpNet::Close()
{
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	for (int32 i = 0; i < 3; ++i)
	{
		if (Socks[i])
		{
			Socks[i]->Close();
			if (SS)
			{
				SS->DestroySocket(Socks[i]);
			}
			Socks[i] = nullptr;
		}
		Ports[i] = 0;
	}
}

bool FNsUdpNet::BindLoopback(int32 BasePort)
{
	Close();
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return false;
	}
	for (int32 i = 0; i < 3; ++i)
	{
		FSocket* Sock = SS->CreateSocket(NAME_DGram, TEXT("NsUdp"), FName(FNetworkProtocolTypes::IPv4));
		if (!Sock)
		{
			Close();
			return false;
		}
		Sock->SetNonBlocking(true);
		Sock->SetReuseAddr(true);
		TSharedRef<FInternetAddr> Addr = SS->CreateInternetAddr();
		Addr->SetLoopbackAddress();
		Addr->SetPort((BasePort > 0) ? (BasePort + i) : 0);
		if (!Sock->Bind(*Addr))
		{
			SS->DestroySocket(Sock);
			Close();
			return false;
		}
		TSharedRef<FInternetAddr> Bound = SS->CreateInternetAddr();
		Sock->GetAddress(*Bound);
		Socks[i] = Sock;
		Ports[i] = Bound->GetPort();
		if (Ports[i] <= 0)
		{
			Close();
			return false;
		}
	}
	return true;
}

void FNsUdpNet::Send(ENsAddr Src, ENsAddr Dst, const FNsPacket& Packet)
{
	const int32 Si = static_cast<int32>(Src);
	const int32 Di = static_cast<int32>(Dst);
	if (Si < 0 || Si > 2 || Di < 0 || Di > 2 || !Socks[Si] || Ports[Di] <= 0)
	{
		return;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return;
	}
	FNsPacket Copy = Packet;
	Copy.Src = Src;
	Copy.Dst = Dst;
	Seq.Stamp(Src, Copy);
	TArray<uint8> Bytes;
	if (!NsEncodePacket(Copy, Bytes))
	{
		return;
	}
	TSharedRef<FInternetAddr> Dest = SS->CreateInternetAddr();
	Dest->SetLoopbackAddress();
	Dest->SetPort(Ports[Di]);
	int32 Sent = 0;
	Socks[Si]->SendTo(Bytes.GetData(), Bytes.Num(), Sent, *Dest);
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
	uint8 Buf[Ns::MaxPacketBytes];
	for (;;)
	{
		TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
		int32 Read = 0;
		if (!Socks[Di]->RecvFrom(Buf, Ns::MaxPacketBytes, Read, *From))
		{
			break;
		}
		if (Read <= 0)
		{
			break;
		}
		TArray<uint8> Bytes;
		Bytes.Append(Buf, Read);
		FNsPacket Wired;
		if (!NsDecodePacket(Bytes, Wired))
		{
			continue;
		}
		const int32 FromPort = From->GetPort();
		bool bKnown = false;
		for (int32 i = 0; i < 3; ++i)
		{
			if (Ports[i] == FromPort)
			{
				Wired.Src = static_cast<ENsAddr>(i);
				bKnown = true;
				break;
			}
		}
		if (!bKnown)
		{
			continue;
		}
		Wired.Dst = Dst;
		if (Seq.Accept(Dst, Wired.Seq))
		{
			Out.Add(MoveTemp(Wired));
		}
	}
}
