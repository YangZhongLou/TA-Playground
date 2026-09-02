// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsSelfTest.h"
#include "NsStun.h"
#include "NsUdpNet.h"
#include "HAL/PlatformProcess.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

namespace
{
FNsSelfTestResult StunFail(const TCHAR* Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

FNsSelfTestResult StunOk(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = true;
	R.Detail = Msg;
	return R;
}

void StunW16(TArray<uint8>& Out, uint16 Value)
{
	Out.Add(static_cast<uint8>(Value >> 8));
	Out.Add(static_cast<uint8>(Value));
}

void StunW32(TArray<uint8>& Out, uint32 Value)
{
	Out.Add(static_cast<uint8>(Value >> 24));
	Out.Add(static_cast<uint8>(Value >> 16));
	Out.Add(static_cast<uint8>(Value >> 8));
	Out.Add(static_cast<uint8>(Value));
}

bool StunParseIpv4(const FString& Host, uint32& OutIpv4)
{
	TArray<FString> Parts;
	Host.ParseIntoArray(Parts, TEXT("."), true);
	if (Parts.Num() != 4)
	{
		return false;
	}
	uint32 Acc = 0;
	for (int32 i = 0; i < 4; ++i)
	{
		const int32 Octet = FCString::Atoi(*Parts[i]);
		if (Octet < 0 || Octet > 255)
		{
			return false;
		}
		Acc = (Acc << 8) | static_cast<uint32>(Octet);
	}
	OutIpv4 = Acc;
	return true;
}

void StunDestroy(FSocket* Sock)
{
	if (!Sock)
	{
		return;
	}
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	Sock->Close();
	if (SS)
	{
		SS->DestroySocket(Sock);
	}
}
}

FNsSelfTestResult NsRunStunBindSelfTest()
{
	uint8 TxId[NsStunTxIdBytes];
	for (int32 i = 0; i < NsStunTxIdBytes; ++i)
	{
		TxId[i] = static_cast<uint8>(0xA0 + i);
	}

	TArray<uint8> Req;
	if (!NsStunEncodeBindRequest(TxId, Req) || Req.Num() != NsStunHeaderBytes)
	{
		return StunFail(TEXT("stun: bind request size"));
	}
	if (Req[0] != 0 || Req[1] != 1 || Req[2] != 0 || Req[3] != 0)
	{
		return StunFail(TEXT("stun: bind request type"));
	}
	if (Req[4] != 0x21 || Req[5] != 0x12 || Req[6] != 0xA4 || Req[7] != 0x42)
	{
		return StunFail(TEXT("stun: bind request magic"));
	}
	if (FMemory::Memcmp(Req.GetData() + 8, TxId, NsStunTxIdBytes) != 0)
	{
		return StunFail(TEXT("stun: bind request txid"));
	}

	const uint32 MappedIp = 0x7F000001u;
	const int32 MappedPort = 12345;
	TArray<uint8> Reply;
	if (!NsStunEncodeXorMappedReply(TxId, MappedIp, MappedPort, Reply))
	{
		return StunFail(TEXT("stun: xor reply encode"));
	}
	uint32 GotIp = 0;
	int32 GotPort = 0;
	if (!NsStunDecodeMapped(Reply, TxId, GotIp, GotPort) || GotIp != MappedIp || GotPort != MappedPort)
	{
		return StunFail(TEXT("stun: xor reply decode"));
	}

	TArray<uint8> Mapped;
	StunW16(Mapped, 0x0101);
	StunW16(Mapped, 12);
	StunW32(Mapped, NsStunMagic);
	Mapped.Append(TxId, NsStunTxIdBytes);
	StunW16(Mapped, 0x0001);
	StunW16(Mapped, 8);
	Mapped.Add(0);
	Mapped.Add(0x01);
	StunW16(Mapped, static_cast<uint16>(MappedPort));
	StunW32(Mapped, MappedIp);
	GotIp = 0;
	GotPort = 0;
	if (!NsStunDecodeMapped(Mapped, TxId, GotIp, GotPort) || GotIp != MappedIp || GotPort != MappedPort)
	{
		return StunFail(TEXT("stun: mapped-address decode"));
	}

	TArray<uint8> Trunc = Reply;
	Trunc.SetNum(NsStunHeaderBytes - 1);
	if (NsStunDecodeMapped(Trunc, TxId, GotIp, GotPort))
	{
		return StunFail(TEXT("stun: truncated accepted"));
	}

	TArray<uint8> BadMagic = Reply;
	BadMagic[7] ^= 1;
	if (NsStunDecodeMapped(BadMagic, TxId, GotIp, GotPort))
	{
		return StunFail(TEXT("stun: bad magic accepted"));
	}

	uint8 OtherTx[NsStunTxIdBytes];
	FMemory::Memcpy(OtherTx, TxId, NsStunTxIdBytes);
	OtherTx[0] ^= 1;
	if (NsStunDecodeMapped(Reply, OtherTx, GotIp, GotPort))
	{
		return StunFail(TEXT("stun: wrong txid accepted"));
	}

	TArray<uint8> Err;
	StunW16(Err, 0x0111);
	StunW16(Err, 0);
	StunW32(Err, NsStunMagic);
	Err.Append(TxId, NsStunTxIdBytes);
	if (NsStunDecodeMapped(Err, TxId, GotIp, GotPort))
	{
		return StunFail(TEXT("stun: error response accepted"));
	}

	if (NsStunIpv4ToString(MappedIp) != TEXT("127.0.0.1"))
	{
		return StunFail(TEXT("stun: ipv4 string"));
	}

	TArray<uint8> Ind;
	if (!NsStunEncodeBindIndication(TxId, Ind) || Ind.Num() != NsStunHeaderBytes)
	{
		return StunFail(TEXT("stun: indication size"));
	}
	if (Ind[0] != 0 || Ind[1] != 0x11)
	{
		return StunFail(TEXT("stun: indication type"));
	}
	if (NsStunDecodeMapped(Ind, TxId, GotIp, GotPort))
	{
		return StunFail(TEXT("stun: indication decoded as mapped"));
	}
	if (!NsStunIsBindIndication(Ind))
	{
		return StunFail(TEXT("stun: indication not recognized"));
	}
	TArray<uint8> BadInd = Ind;
	BadInd[7] ^= 1;
	if (NsStunIsBindIndication(BadInd))
	{
		return StunFail(TEXT("stun: bad magic indication accepted"));
	}
	if (NsStunIsBindIndication(Req))
	{
		return StunFail(TEXT("stun: request treated as indication"));
	}

	TArray<uint8> Offer;
	if (!NsRendezvousEncode(1, MappedIp, MappedPort, Offer) || Offer.Num() != NsRendezvousBytes)
	{
		return StunFail(TEXT("stun: rendezvous size"));
	}
	if (Offer[0] != 0x56 || Offer[1] != 0x52 || Offer[2] != 0x53 || Offer[3] != 0x4E)
	{
		return StunFail(TEXT("stun: rendezvous magic"));
	}
	uint8 Slot = 99;
	uint32 OfferIp = 0;
	int32 OfferPort = 0;
	if (!NsRendezvousDecode(Offer, Slot, OfferIp, OfferPort)
		|| Slot != 1 || OfferIp != MappedIp || OfferPort != MappedPort)
	{
		return StunFail(TEXT("stun: rendezvous decode"));
	}
	if (NsRendezvousDecode(Req, Slot, OfferIp, OfferPort))
	{
		return StunFail(TEXT("stun: request treated as rendezvous"));
	}
	uint32 ParsedIp = 0;
	if (!NsStunParseIpv4(TEXT("127.0.0.1"), ParsedIp) || ParsedIp != MappedIp)
	{
		return StunFail(TEXT("stun: parse ipv4"));
	}

	return StunOk(TEXT("stun bind codec"));
}

FNsSelfTestResult NsRunStunLoopbackSelfTest()
{
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return StunFail(TEXT("stun-loop: no sockets"));
	}

	FSocket* StunSock = SS->CreateSocket(NAME_DGram, TEXT("NsStunFake"), FName(FNetworkProtocolTypes::IPv4));
	if (!StunSock)
	{
		return StunFail(TEXT("stun-loop: create"));
	}
	StunSock->SetNonBlocking(true);
	TSharedRef<FInternetAddr> BindAddr = SS->CreateInternetAddr();
	BindAddr->SetLoopbackAddress();
	BindAddr->SetPort(0);
	if (!StunSock->Bind(*BindAddr))
	{
		StunDestroy(StunSock);
		return StunFail(TEXT("stun-loop: bind stun"));
	}
	TSharedRef<FInternetAddr> Bound = SS->CreateInternetAddr();
	StunSock->GetAddress(*Bound);
	const int32 StunPort = Bound->GetPort();
	if (StunPort <= 0)
	{
		StunDestroy(StunSock);
		return StunFail(TEXT("stun-loop: stun port"));
	}

	FNsUdpNet Net;
	if (!Net.Bind(ENsAddr::C0, 0, false))
	{
		StunDestroy(StunSock);
		return StunFail(TEXT("stun-loop: bind c0"));
	}

	uint8 TxId[NsStunTxIdBytes];
	NsStunFillTxId(TxId);
	if (!Net.StunSendBind(ENsAddr::C0, TEXT("127.0.0.1"), StunPort, TxId))
	{
		StunDestroy(StunSock);
		return StunFail(TEXT("stun-loop: send bind"));
	}

	uint8 Buf[512];
	int32 Read = 0;
	TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
	bool bGotReq = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Read = 0;
		if (StunSock->RecvFrom(Buf, 512, Read, *From) && Read >= NsStunHeaderBytes)
		{
			bGotReq = true;
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!bGotReq)
	{
		StunDestroy(StunSock);
		return StunFail(TEXT("stun-loop: no request"));
	}

	TArray<uint8> Req;
	Req.Append(Buf, Read);
	if (FMemory::Memcmp(Req.GetData() + 8, TxId, NsStunTxIdBytes) != 0)
	{
		StunDestroy(StunSock);
		return StunFail(TEXT("stun-loop: txid mismatch"));
	}

	uint32 FromIp = 0;
	if (!StunParseIpv4(From->ToString(false), FromIp) || From->GetPort() <= 0)
	{
		StunDestroy(StunSock);
		return StunFail(TEXT("stun-loop: from addr"));
	}
	TArray<uint8> Reply;
	if (!NsStunEncodeXorMappedReply(TxId, FromIp, From->GetPort(), Reply))
	{
		StunDestroy(StunSock);
		return StunFail(TEXT("stun-loop: encode reply"));
	}
	int32 Sent = 0;
	if (!StunSock->SendTo(Reply.GetData(), Reply.Num(), Sent, *From) || Sent != Reply.Num())
	{
		StunDestroy(StunSock);
		return StunFail(TEXT("stun-loop: send reply"));
	}

	FString Host;
	int32 Port = 0;
	bool bGotMapped = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		if (Net.StunRecvMapped(ENsAddr::C0, TxId, Host, Port))
		{
			bGotMapped = true;
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	StunDestroy(StunSock);
	if (!bGotMapped)
	{
		return StunFail(TEXT("stun-loop: no mapped"));
	}

	const int32 BoundPort = Net.BoundPort(ENsAddr::C0);
	if (Port != BoundPort || Host != NsStunIpv4ToString(FromIp))
	{
		return StunFail(*FString::Printf(TEXT("stun-loop: mapped %s:%d want %s:%d"),
			*Host, Port, *NsStunIpv4ToString(FromIp), BoundPort));
	}

	return StunOk(FString::Printf(TEXT("stun loopback %s:%d"), *Host, Port));
}

FNsSelfTestResult NsRunStunPunchSelfTest()
{
	FNsUdpNet Sv;
	FNsUdpNet C0;
	if (!Sv.Bind(ENsAddr::Sv, 0, false) || !C0.Bind(ENsAddr::C0, 0, false))
	{
		return StunFail(TEXT("stun-punch: bind"));
	}
	if (!Sv.SetPeer(ENsAddr::C0, TEXT("127.0.0.1"), C0.BoundPort(ENsAddr::C0))
		|| !C0.SetPeer(ENsAddr::Sv, TEXT("127.0.0.1"), Sv.BoundPort(ENsAddr::Sv)))
	{
		return StunFail(TEXT("stun-punch: set peer"));
	}

	uint8 TxId[NsStunTxIdBytes];
	NsStunFillTxId(TxId);
	if (!C0.StunSendIndication(ENsAddr::C0, TEXT("127.0.0.1"), Sv.BoundPort(ENsAddr::Sv), TxId))
	{
		return StunFail(TEXT("stun-punch: send indication"));
	}

	bool bGotInd = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		if (Sv.StunRecvIndication(ENsAddr::Sv))
		{
			bGotInd = true;
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!bGotInd)
	{
		return StunFail(TEXT("stun-punch: no indication"));
	}

	if (!Sv.PunchPeers() || !C0.PunchPeers())
	{
		return StunFail(TEXT("stun-punch: punch peers"));
	}
	bGotInd = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		if (C0.StunRecvIndication(ENsAddr::C0) || Sv.StunRecvIndication(ENsAddr::Sv))
		{
			bGotInd = true;
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!bGotInd)
	{
		return StunFail(TEXT("stun-punch: punch not received"));
	}

	TArray<FNsPacket> Flush;
	Sv.Drain(ENsAddr::Sv, Flush);
	C0.Drain(ENsAddr::C0, Flush);

	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Pkt.PlayerId = 0;
	Pkt.Dx = 1;
	C0.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
	TArray<FNsPacket> Got;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Sv.Drain(ENsAddr::Sv, Got);
		if (Got.Num() > 0)
		{
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (Got.Num() != 1 || Got[0].Dx != 1 || Got[0].Src != ENsAddr::C0)
	{
		return StunFail(TEXT("stun-punch: tans after punch"));
	}

	return StunOk(TEXT("stun punch"));
}

FNsSelfTestResult NsRunStunRendezvousSelfTest()
{
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return StunFail(TEXT("stun-rendezvous: no sockets"));
	}
	FSocket* Hub = SS->CreateSocket(NAME_DGram, TEXT("NsRendezvousHub"), FName(FNetworkProtocolTypes::IPv4));
	if (!Hub)
	{
		return StunFail(TEXT("stun-rendezvous: hub create"));
	}
	Hub->SetNonBlocking(true);
	TSharedRef<FInternetAddr> BindAddr = SS->CreateInternetAddr();
	BindAddr->SetLoopbackAddress();
	BindAddr->SetPort(0);
	if (!Hub->Bind(*BindAddr))
	{
		StunDestroy(Hub);
		return StunFail(TEXT("stun-rendezvous: hub bind"));
	}
	TSharedRef<FInternetAddr> Bound = SS->CreateInternetAddr();
	Hub->GetAddress(*Bound);
	const int32 HubPort = Bound->GetPort();
	if (HubPort <= 0)
	{
		StunDestroy(Hub);
		return StunFail(TEXT("stun-rendezvous: hub port"));
	}

	FNsUdpNet Sv;
	FNsUdpNet C0;
	if (!Sv.Bind(ENsAddr::Sv, 0, false) || !C0.Bind(ENsAddr::C0, 0, false))
	{
		StunDestroy(Hub);
		return StunFail(TEXT("stun-rendezvous: bind"));
	}

	uint32 HubIp[3] = {};
	int32 HubPorts[3] = {};
	bool HubHave[3] = {};
	bool bSvPeer = false;
	bool bC0Peer = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Sv.RendezvousSendOffer(ENsAddr::Sv, TEXT("127.0.0.1"), HubPort);
		C0.RendezvousSendOffer(ENsAddr::C0, TEXT("127.0.0.1"), HubPort);
		uint8 Buf[64];
		TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
		int32 Read = 0;
		while (Hub->RecvFrom(Buf, 64, Read, *From) && Read > 0)
		{
			TArray<uint8> Bytes;
			Bytes.Append(Buf, Read);
			uint8 Slot = 0;
			uint32 Ip = 0;
			int32 Port = 0;
			if (!NsRendezvousDecode(Bytes, Slot, Ip, Port) || Slot > 2)
			{
				continue;
			}
			HubIp[Slot] = Ip;
			HubPorts[Slot] = Port;
			HubHave[Slot] = true;
			for (int32 Other = 0; Other < 3; ++Other)
			{
				if (Other == static_cast<int32>(Slot) || !HubHave[Other])
				{
					continue;
				}
				TArray<uint8> Reply;
				if (!NsRendezvousEncode(static_cast<uint8>(Other), HubIp[Other], HubPorts[Other], Reply))
				{
					continue;
				}
				int32 Sent = 0;
				Hub->SendTo(Reply.GetData(), Reply.Num(), Sent, *From);
			}
		}
		bSvPeer = bSvPeer || Sv.RendezvousRecvPeer(ENsAddr::Sv);
		bC0Peer = bC0Peer || C0.RendezvousRecvPeer(ENsAddr::C0);
		if (bSvPeer && bC0Peer)
		{
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	StunDestroy(Hub);
	if (!bSvPeer || !bC0Peer)
	{
		return StunFail(TEXT("stun-rendezvous: no peer"));
	}
	if (Sv.PeerPort(ENsAddr::C0) != C0.BoundPort(ENsAddr::C0)
		|| C0.PeerPort(ENsAddr::Sv) != Sv.BoundPort(ENsAddr::Sv))
	{
		return StunFail(TEXT("stun-rendezvous: peer port"));
	}

	if (!Sv.PunchPeers() || !C0.PunchPeers())
	{
		return StunFail(TEXT("stun-rendezvous: punch"));
	}
	TArray<FNsPacket> Flush;
	Sv.Drain(ENsAddr::Sv, Flush);
	C0.Drain(ENsAddr::C0, Flush);

	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Pkt.PlayerId = 0;
	Pkt.Dx = 1;
	C0.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
	TArray<FNsPacket> Got;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Sv.Drain(ENsAddr::Sv, Got);
		if (Got.Num() > 0)
		{
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (Got.Num() != 1 || Got[0].Dx != 1 || Got[0].Src != ENsAddr::C0)
	{
		return StunFail(TEXT("stun-rendezvous: tans"));
	}

	return StunOk(TEXT("stun rendezvous"));
}
