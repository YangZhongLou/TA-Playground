// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsSelfTest.h"
#include "NsStun.h"
#include "NsUdpNet.h"
#include "NsCodec.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"
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
	if (!NsStunIsBindRequest(Req) || NsStunIsBindRequest(Ind))
	{
		return StunFail(TEXT("stun: bind request detect"));
	}
	TArray<uint8> Nom;
	if (!NsStunEncodeBindNominate(TxId, Nom) || Nom.Num() != NsStunHeaderBytes + 4)
	{
		return StunFail(TEXT("stun: nominate size"));
	}
	if (Nom[0] != 0 || Nom[1] != 1 || Nom[2] != 0 || Nom[3] != 4)
	{
		return StunFail(TEXT("stun: nominate type"));
	}
	if (Nom[20] != 0 || Nom[21] != 0x25 || Nom[22] != 0 || Nom[23] != 0)
	{
		return StunFail(TEXT("stun: use-candidate"));
	}
	if (!NsStunIsBindRequest(Nom) || !NsStunHasUseCandidate(Nom) || NsStunHasUseCandidate(Req)
		|| NsStunHasUseCandidate(Ind))
	{
		return StunFail(TEXT("stun: nominate detect"));
	}
	uint8 GotTx[NsStunTxIdBytes];
	if (!NsStunReadTxId(Req, GotTx) || FMemory::Memcmp(GotTx, TxId, NsStunTxIdBytes) != 0)
	{
		return StunFail(TEXT("stun: read txid"));
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

	TArray<uint8> Alloc;
	if (!NsStunEncodeAllocateRequest(TxId, Alloc) || Alloc.Num() != NsStunHeaderBytes + 8)
	{
		return StunFail(TEXT("stun: allocate size"));
	}
	if (Alloc[0] != 0 || Alloc[1] != 3)
	{
		return StunFail(TEXT("stun: allocate type"));
	}
	if (Alloc[20] != 0 || Alloc[21] != 0x19 || Alloc[24] != 17)
	{
		return StunFail(TEXT("stun: requested transport"));
	}
	if (!NsStunIsAllocateRequest(Alloc) || NsStunIsAllocateRequest(Req) || NsStunIsBindRequest(Alloc))
	{
		return StunFail(TEXT("stun: allocate detect"));
	}

	const uint32 RelayedIp = 0x0A000001u;
	const int32 RelayedPort = 23456;
	TArray<uint8> Relayed;
	if (!NsStunEncodeXorRelayedReply(TxId, RelayedIp, RelayedPort, Relayed))
	{
		return StunFail(TEXT("stun: xor relayed encode"));
	}
	uint32 GotRelayIp = 0;
	int32 GotRelayPort = 0;
	if (!NsStunDecodeRelayed(Relayed, TxId, GotRelayIp, GotRelayPort)
		|| GotRelayIp != RelayedIp || GotRelayPort != RelayedPort)
	{
		return StunFail(TEXT("stun: xor relayed decode"));
	}
	if (NsStunDecodeMapped(Relayed, TxId, GotIp, GotPort)
		|| NsStunDecodeRelayed(Reply, TxId, GotRelayIp, GotRelayPort))
	{
		return StunFail(TEXT("stun: bind/allocate cross decode"));
	}
	TArray<uint8> AllocErr;
	StunW16(AllocErr, 0x0113);
	StunW16(AllocErr, 0);
	StunW32(AllocErr, NsStunMagic);
	AllocErr.Append(TxId, NsStunTxIdBytes);
	if (NsStunDecodeRelayed(AllocErr, TxId, GotRelayIp, GotRelayPort))
	{
		return StunFail(TEXT("stun: allocate error accepted"));
	}

	const uint32 PeerIp = 0x0A000002u;
	const int32 PeerPort = 1234;
	TArray<uint8> Perm;
	if (!NsStunEncodeCreatePermissionRequest(TxId, PeerIp, PeerPort, Perm)
		|| Perm.Num() != NsStunHeaderBytes + 12)
	{
		return StunFail(TEXT("stun: permission size"));
	}
	if (Perm[0] != 0 || Perm[1] != 8)
	{
		return StunFail(TEXT("stun: permission type"));
	}
	if (!NsStunIsCreatePermissionRequest(Perm)
		|| NsStunIsCreatePermissionRequest(Alloc)
		|| NsStunIsAllocateRequest(Perm))
	{
		return StunFail(TEXT("stun: permission detect"));
	}
	uint32 GotPeerIp = 0;
	int32 GotPeerPort = 0;
	if (!NsStunDecodePeer(Perm, TxId, GotPeerIp, GotPeerPort)
		|| GotPeerIp != PeerIp || GotPeerPort != PeerPort)
	{
		return StunFail(TEXT("stun: xor peer decode"));
	}
	TArray<uint8> PermOk;
	if (!NsStunEncodeCreatePermissionSuccess(TxId, PermOk) || PermOk.Num() != NsStunHeaderBytes)
	{
		return StunFail(TEXT("stun: permission success size"));
	}
	if (!NsStunDecodePermissionSuccess(PermOk, TxId)
		|| NsStunDecodePermissionSuccess(Relayed, TxId)
		|| NsStunDecodeRelayed(PermOk, TxId, GotRelayIp, GotRelayPort)
		|| NsStunDecodePeer(Alloc, TxId, GotPeerIp, GotPeerPort))
	{
		return StunFail(TEXT("stun: permission cross decode"));
	}
	TArray<uint8> PermErr;
	StunW16(PermErr, 0x0118);
	StunW16(PermErr, 0);
	StunW32(PermErr, NsStunMagic);
	PermErr.Append(TxId, NsStunTxIdBytes);
	if (NsStunDecodePermissionSuccess(PermErr, TxId))
	{
		return StunFail(TEXT("stun: permission error accepted"));
	}

	const uint16 Ch = NsTurnChannelMin;
	TArray<uint8> ChBind;
	if (!NsStunEncodeChannelBindRequest(TxId, Ch, PeerIp, PeerPort, ChBind)
		|| ChBind.Num() != NsStunHeaderBytes + 20)
	{
		return StunFail(TEXT("stun: channel bind size"));
	}
	if (ChBind[0] != 0 || ChBind[1] != 9)
	{
		return StunFail(TEXT("stun: channel bind type"));
	}
	if (!NsStunIsChannelBindRequest(ChBind)
		|| NsStunIsChannelBindRequest(Perm)
		|| NsStunIsCreatePermissionRequest(ChBind))
	{
		return StunFail(TEXT("stun: channel bind detect"));
	}
	uint16 GotCh = 0;
	uint32 GotChIp = 0;
	int32 GotChPort = 0;
	if (!NsStunDecodeChannelBind(ChBind, TxId, GotCh, GotChIp, GotChPort)
		|| GotCh != Ch || GotChIp != PeerIp || GotChPort != PeerPort)
	{
		return StunFail(TEXT("stun: channel bind decode"));
	}
	TArray<uint8> ChOk;
	if (!NsStunEncodeChannelBindSuccess(TxId, ChOk) || ChOk.Num() != NsStunHeaderBytes)
	{
		return StunFail(TEXT("stun: channel bind success size"));
	}
	if (!NsStunDecodeChannelBindSuccess(ChOk, TxId)
		|| NsStunDecodeChannelBindSuccess(PermOk, TxId)
		|| NsStunDecodePermissionSuccess(ChOk, TxId))
	{
		return StunFail(TEXT("stun: channel bind cross decode"));
	}
	TArray<uint8> TinyPay;
	TinyPay.Add(0xAB);
	TArray<uint8> Chan;
	if (!NsEncodeChannelData(Ch, TinyPay, Chan) || Chan.Num() != 8)
	{
		return StunFail(TEXT("stun: channel data pad"));
	}
	uint16 DecCh = 0;
	TArray<uint8> DecPay;
	if (!NsDecodeChannelData(Chan, DecCh, DecPay) || DecCh != Ch
		|| DecPay.Num() != 1 || DecPay[0] != 0xAB)
	{
		return StunFail(TEXT("stun: channel data decode"));
	}
	if (NsDecodeChannelData(Req, DecCh, DecPay) || NsEncodeChannelData(1, TinyPay, Chan))
	{
		return StunFail(TEXT("stun: channel data reject"));
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

FNsSelfTestResult NsRunStunCheckSelfTest()
{
	FNsUdpNet Sv;
	FNsUdpNet C0;
	if (!Sv.Bind(ENsAddr::Sv, 0, false) || !C0.Bind(ENsAddr::C0, 0, false))
	{
		return StunFail(TEXT("stun-check: bind"));
	}
	if (!Sv.SetPeer(ENsAddr::C0, TEXT("127.0.0.1"), C0.BoundPort(ENsAddr::C0))
		|| !C0.SetPeer(ENsAddr::Sv, TEXT("127.0.0.1"), Sv.BoundPort(ENsAddr::Sv)))
	{
		return StunFail(TEXT("stun-check: set peer"));
	}
	if (!Sv.PunchPeers() || !C0.PunchPeers())
	{
		return StunFail(TEXT("stun-check: punch"));
	}

	uint8 TxSv[NsStunTxIdBytes];
	uint8 TxC0[NsStunTxIdBytes];
	NsStunFillTxId(TxSv);
	NsStunFillTxId(TxC0);
	if (!Sv.StunSendBind(ENsAddr::Sv, TEXT("127.0.0.1"), C0.BoundPort(ENsAddr::C0), TxSv)
		|| !C0.StunSendBind(ENsAddr::C0, TEXT("127.0.0.1"), Sv.BoundPort(ENsAddr::Sv), TxC0))
	{
		return StunFail(TEXT("stun-check: send bind"));
	}

	bool bSv = false;
	bool bC0 = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		FString Host;
		int32 Port = 0;
		if (Sv.StunServe(ENsAddr::Sv, TxSv, Host, Port))
		{
			bSv = true;
		}
		if (C0.StunServe(ENsAddr::C0, TxC0, Host, Port))
		{
			bC0 = true;
		}
		if (bSv && bC0)
		{
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!bSv || !bC0)
	{
		return StunFail(TEXT("stun-check: no success"));
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
		return StunFail(TEXT("stun-check: tans"));
	}

	return StunOk(TEXT("stun check"));
}

FNsSelfTestResult NsRunStunTurnSelfTest()
{
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return StunFail(TEXT("stun-turn: no sockets"));
	}

	FSocket* TurnSock = SS->CreateSocket(NAME_DGram, TEXT("NsTurnFake"), FName(FNetworkProtocolTypes::IPv4));
	if (!TurnSock)
	{
		return StunFail(TEXT("stun-turn: create"));
	}
	TurnSock->SetNonBlocking(true);
	TSharedRef<FInternetAddr> BindAddr = SS->CreateInternetAddr();
	BindAddr->SetLoopbackAddress();
	BindAddr->SetPort(0);
	if (!TurnSock->Bind(*BindAddr))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-turn: bind turn"));
	}
	TSharedRef<FInternetAddr> Bound = SS->CreateInternetAddr();
	TurnSock->GetAddress(*Bound);
	const int32 TurnPort = Bound->GetPort();
	if (TurnPort <= 0)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-turn: turn port"));
	}

	FNsUdpNet Net;
	if (!Net.Bind(ENsAddr::C0, 0, false))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-turn: bind c0"));
	}

	uint8 TxId[NsStunTxIdBytes];
	NsStunFillTxId(TxId);
	if (!Net.StunSendAllocate(ENsAddr::C0, TEXT("127.0.0.1"), TurnPort, TxId))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-turn: send allocate"));
	}

	uint8 Buf[512];
	int32 Read = 0;
	TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
	bool bGotReq = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Read = 0;
		if (TurnSock->RecvFrom(Buf, 512, Read, *From) && Read >= NsStunHeaderBytes)
		{
			bGotReq = true;
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!bGotReq)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-turn: no request"));
	}

	TArray<uint8> Req;
	Req.Append(Buf, Read);
	if (!NsStunIsAllocateRequest(Req)
		|| FMemory::Memcmp(Req.GetData() + 8, TxId, NsStunTxIdBytes) != 0)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-turn: bad request"));
	}

	const uint32 RelayedIp = 0x7F000001u;
	const int32 RelayedPort = 23456;
	TArray<uint8> Reply;
	if (!NsStunEncodeXorRelayedReply(TxId, RelayedIp, RelayedPort, Reply))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-turn: encode reply"));
	}
	int32 Sent = 0;
	if (!TurnSock->SendTo(Reply.GetData(), Reply.Num(), Sent, *From) || Sent != Reply.Num())
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-turn: send reply"));
	}

	FString Host;
	int32 Port = 0;
	bool bGotRelayed = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		if (Net.StunRecvRelayed(ENsAddr::C0, TxId, Host, Port))
		{
			bGotRelayed = true;
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	StunDestroy(TurnSock);
	if (!bGotRelayed)
	{
		return StunFail(TEXT("stun-turn: no relayed"));
	}
	if (Port != RelayedPort || Host != NsStunIpv4ToString(RelayedIp))
	{
		return StunFail(*FString::Printf(TEXT("stun-turn: relayed %s:%d want %s:%d"),
			*Host, Port, *NsStunIpv4ToString(RelayedIp), RelayedPort));
	}

	return StunOk(FString::Printf(TEXT("stun turn %s:%d"), *Host, Port));
}

FNsSelfTestResult NsRunStunPermitSelfTest()
{
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return StunFail(TEXT("stun-permit: no sockets"));
	}

	FSocket* TurnSock = SS->CreateSocket(NAME_DGram, TEXT("NsTurnPermitFake"), FName(FNetworkProtocolTypes::IPv4));
	if (!TurnSock)
	{
		return StunFail(TEXT("stun-permit: create"));
	}
	TurnSock->SetNonBlocking(true);
	TSharedRef<FInternetAddr> BindAddr = SS->CreateInternetAddr();
	BindAddr->SetLoopbackAddress();
	BindAddr->SetPort(0);
	if (!TurnSock->Bind(*BindAddr))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-permit: bind turn"));
	}
	TSharedRef<FInternetAddr> Bound = SS->CreateInternetAddr();
	TurnSock->GetAddress(*Bound);
	const int32 TurnPort = Bound->GetPort();
	if (TurnPort <= 0)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-permit: turn port"));
	}

	FNsUdpNet Net;
	if (!Net.Bind(ENsAddr::C0, 0, false))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-permit: bind c0"));
	}

	uint8 TxId[NsStunTxIdBytes];
	NsStunFillTxId(TxId);
	if (!Net.StunSendPermission(ENsAddr::C0, TEXT("127.0.0.1"), TurnPort,
		TEXT("10.0.0.2"), 1234, TxId))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-permit: send"));
	}

	uint8 Buf[512];
	int32 Read = 0;
	TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
	bool bGotReq = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Read = 0;
		if (TurnSock->RecvFrom(Buf, 512, Read, *From) && Read >= NsStunHeaderBytes)
		{
			bGotReq = true;
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!bGotReq)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-permit: no request"));
	}

	TArray<uint8> Req;
	Req.Append(Buf, Read);
	uint32 PeerIp = 0;
	int32 PeerPort = 0;
	if (!NsStunIsCreatePermissionRequest(Req)
		|| !NsStunDecodePeer(Req, TxId, PeerIp, PeerPort)
		|| PeerIp != 0x0A000002u || PeerPort != 1234)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-permit: bad request"));
	}

	TArray<uint8> Reply;
	if (!NsStunEncodeCreatePermissionSuccess(TxId, Reply))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-permit: encode reply"));
	}
	int32 Sent = 0;
	if (!TurnSock->SendTo(Reply.GetData(), Reply.Num(), Sent, *From) || Sent != Reply.Num())
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-permit: send reply"));
	}

	bool bGotOk = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		if (Net.StunRecvPermission(ENsAddr::C0, TxId))
		{
			bGotOk = true;
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	StunDestroy(TurnSock);
	if (!bGotOk)
	{
		return StunFail(TEXT("stun-permit: no success"));
	}

	return StunOk(TEXT("stun permit"));
}

FNsSelfTestResult NsRunStunChannelSelfTest()
{
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return StunFail(TEXT("stun-channel: no sockets"));
	}

	FSocket* TurnSock = SS->CreateSocket(NAME_DGram, TEXT("NsTurnChanFake"), FName(FNetworkProtocolTypes::IPv4));
	if (!TurnSock)
	{
		return StunFail(TEXT("stun-channel: create"));
	}
	TurnSock->SetNonBlocking(true);
	TSharedRef<FInternetAddr> BindAddr = SS->CreateInternetAddr();
	BindAddr->SetLoopbackAddress();
	BindAddr->SetPort(0);
	if (!TurnSock->Bind(*BindAddr))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: bind turn"));
	}
	TSharedRef<FInternetAddr> Bound = SS->CreateInternetAddr();
	TurnSock->GetAddress(*Bound);
	const int32 TurnPort = Bound->GetPort();
	if (TurnPort <= 0)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: turn port"));
	}
	FSocket* Relay = SS->CreateSocket(NAME_DGram, TEXT("NsTurnRelay"), FName(FNetworkProtocolTypes::IPv4));
	if (!Relay)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: relay create"));
	}
	ON_SCOPE_EXIT { StunDestroy(Relay); };
	Relay->SetNonBlocking(true);
	if (!Relay->Bind(*BindAddr))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: relay bind"));
	}
	TSharedRef<FInternetAddr> RelayAddr = SS->CreateInternetAddr();
	Relay->GetAddress(*RelayAddr);

	FNsUdpNet C0;
	FNsUdpNet Sv;
	if (!C0.Bind(ENsAddr::C0, 0, false) || !Sv.Bind(ENsAddr::Sv, 0, false))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: bind peers"));
	}
	if (!Sv.SetPeer(ENsAddr::C0, TEXT("127.0.0.1"), RelayAddr->GetPort()))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: relayed peer"));
	}

	const uint16 Channel = NsTurnChannelMin;
	uint8 TxId[NsStunTxIdBytes];
	NsStunFillTxId(TxId);
	if (!C0.StunSendChannelBind(ENsAddr::C0, TEXT("127.0.0.1"), TurnPort, Channel,
		TEXT("127.0.0.1"), Sv.BoundPort(ENsAddr::Sv), TxId))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: send bind"));
	}

	uint8 Buf[Ns::MaxPacketBytes + NsChannelDataHeaderBytes + 4];
	int32 Read = 0;
	TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
	bool bGotBind = false;
	uint32 PeerIp = 0;
	int32 PeerPort = 0;
	uint16 GotCh = 0;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Read = 0;
		if (TurnSock->RecvFrom(Buf, 512, Read, *From) && Read >= NsStunHeaderBytes)
		{
			TArray<uint8> Req;
			Req.Append(Buf, Read);
			if (NsStunDecodeChannelBind(Req, TxId, GotCh, PeerIp, PeerPort) && GotCh == Channel)
			{
				bGotBind = true;
				break;
			}
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!bGotBind || PeerPort != Sv.BoundPort(ENsAddr::Sv))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: no bind"));
	}

	TArray<uint8> BindOk;
	if (!NsStunEncodeChannelBindSuccess(TxId, BindOk))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: encode success"));
	}
	int32 Sent = 0;
	if (!TurnSock->SendTo(BindOk.GetData(), BindOk.Num(), Sent, *From) || Sent != BindOk.Num())
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: send success"));
	}

	bool bGotBindOk = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		if (C0.StunRecvChannelBind(ENsAddr::C0, TxId))
		{
			bGotBindOk = true;
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!bGotBindOk)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: no bind ok"));
	}

	FNsPacket Pkt;
	Pkt.Type = ENsMsg::S2CFrame;
	Pkt.Session = 1;
	Pkt.Seq = 1;
	FNsInputs Input;
	Input.Dx[0] = 1;
	Input.Dx[1] = -1;
	for (int32 Frame = 0; Frame < Ns::MaxS2CFrameEntries; ++Frame)
	{
		Pkt.Frames.Add(Frame, Input);
	}
	TArray<uint8> Tans;
	if (!NsEncodePacket(Pkt, Tans)
		|| !C0.StunSendChannelData(ENsAddr::C0, TEXT("127.0.0.1"), TurnPort, Channel, Tans))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: send data"));
	}

	bool bFwd = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Read = 0;
		if (TurnSock->RecvFrom(Buf, sizeof(Buf), Read, *From) && Read > 0)
		{
			TArray<uint8> Chan;
			Chan.Append(Buf, Read);
			uint16 DataCh = 0;
			TArray<uint8> Payload;
			if (NsDecodeChannelData(Chan, DataCh, Payload) && DataCh == Channel && Payload == Tans)
			{
				TSharedRef<FInternetAddr> Dest = SS->CreateInternetAddr();
				bool bOk = false;
				Dest->SetIp(*NsStunIpv4ToString(PeerIp), bOk);
				Dest->SetPort(PeerPort);
				Sent = 0;
				// TURN sends only the application payload, from the allocated relay endpoint.
				if (bOk && Relay->SendTo(Payload.GetData(), Payload.Num(), Sent, *Dest) && Sent == Payload.Num())
				{
					bFwd = true;
					break;
				}
			}
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!bFwd)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: no forward"));
	}

	TArray<FNsPacket> PeerPackets;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Sv.Drain(ENsAddr::Sv, PeerPackets);
		if (!PeerPackets.IsEmpty())
		{
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (PeerPackets.Num() != 1 || PeerPackets[0].Src != ENsAddr::C0
		|| PeerPackets[0].Frames.Num() != Pkt.Frames.Num())
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: peer must receive raw TANS from relay"));
	}

	Sv.Send(ENsAddr::Sv, ENsAddr::C0, Pkt);
	bool bReturnForwarded = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Read = 0;
		if (Relay->RecvFrom(Buf, sizeof(Buf), Read, *From) && Read > 0)
		{
			TArray<uint8> PeerData;
			PeerData.Append(Buf, Read);
			FNsPacket PeerPacket;
			TArray<uint8> ChannelData;
			if (From->GetPort() != Sv.BoundPort(ENsAddr::Sv)
				|| !NsDecodePacket(PeerData, PeerPacket)
				|| !NsEncodeChannelData(Channel, PeerData, ChannelData))
			{
				StunDestroy(TurnSock);
				return StunFail(TEXT("stun-channel: raw peer return"));
			}
			TSharedRef<FInternetAddr> ClientAddr = SS->CreateInternetAddr();
			ClientAddr->SetLoopbackAddress();
			ClientAddr->SetPort(C0.BoundPort(ENsAddr::C0));
			Sent = 0;
			bReturnForwarded = TurnSock->SendTo(ChannelData.GetData(), ChannelData.Num(), Sent, *ClientAddr)
				&& Sent == ChannelData.Num();
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!bReturnForwarded)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-channel: return forward"));
	}
	bool bGotData = false;
	uint16 RecvCh = 0;
	TArray<uint8> RecvPay;
	for (int32 Try = 0; Try < 50 && !bGotData; ++Try)
	{
		bGotData = C0.StunRecvChannelData(ENsAddr::C0, RecvCh, RecvPay);
		if (!bGotData)
		{
			FPlatformProcess::Sleep(0.001f);
		}
	}
	StunDestroy(TurnSock);
	FNsPacket Got;
	if (!bGotData || RecvCh != Channel || !NsDecodePacket(RecvPay, Got)
		|| Got.Frames.Num() != Pkt.Frames.Num())
	{
		return StunFail(TEXT("stun-channel: client must receive ChannelData on return"));
	}

	return StunOk(TEXT("stun channel"));
}

FNsSelfTestResult NsRunStunRelaySelfTest()
{
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return StunFail(TEXT("stun-relay: no sockets"));
	}

	FSocket* TurnSock = SS->CreateSocket(NAME_DGram, TEXT("NsTurnRelayFake"), FName(FNetworkProtocolTypes::IPv4));
	if (!TurnSock)
	{
		return StunFail(TEXT("stun-relay: create"));
	}
	TurnSock->SetNonBlocking(true);
	TSharedRef<FInternetAddr> BindAddr = SS->CreateInternetAddr();
	BindAddr->SetLoopbackAddress();
	BindAddr->SetPort(0);
	if (!TurnSock->Bind(*BindAddr))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-relay: bind turn"));
	}
	TSharedRef<FInternetAddr> Bound = SS->CreateInternetAddr();
	TurnSock->GetAddress(*Bound);
	const int32 TurnPort = Bound->GetPort();
	if (TurnPort <= 0)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-relay: turn port"));
	}
	FSocket* Relay = SS->CreateSocket(NAME_DGram, TEXT("NsTurnRelayPath"), FName(FNetworkProtocolTypes::IPv4));
	if (!Relay)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-relay: relay create"));
	}
	ON_SCOPE_EXIT { StunDestroy(Relay); };
	Relay->SetNonBlocking(true);
	if (!Relay->Bind(*BindAddr))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-relay: relay bind"));
	}
	TSharedRef<FInternetAddr> RelayAddr = SS->CreateInternetAddr();
	Relay->GetAddress(*RelayAddr);

	FNsUdpNet C0;
	FNsUdpNet Sv;
	if (!C0.Bind(ENsAddr::C0, 0, false) || !Sv.Bind(ENsAddr::Sv, 0, false))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-relay: bind peers"));
	}
	if (!C0.SetPeer(ENsAddr::Sv, TEXT("127.0.0.1"), Sv.BoundPort(ENsAddr::Sv))
		|| !Sv.SetPeer(ENsAddr::C0, TEXT("127.0.0.1"), RelayAddr->GetPort()))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-relay: set peer"));
	}

	const uint16 Channel = static_cast<uint16>(NsTurnChannelMin + static_cast<int32>(ENsAddr::Sv));
	uint8 TxId[NsStunTxIdBytes];
	NsStunFillTxId(TxId);
	if (!C0.StunSendChannelBind(ENsAddr::C0, TEXT("127.0.0.1"), TurnPort, Channel,
		TEXT("127.0.0.1"), Sv.BoundPort(ENsAddr::Sv), TxId))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-relay: send bind"));
	}

	uint8 Buf[Ns::MaxPacketBytes + NsChannelDataHeaderBytes + 4];
	int32 Read = 0;
	TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
	bool bGotBind = false;
	uint32 PeerIp = 0;
	int32 PeerPort = 0;
	uint16 GotCh = 0;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Read = 0;
		if (TurnSock->RecvFrom(Buf, 512, Read, *From) && Read >= NsStunHeaderBytes)
		{
			TArray<uint8> Req;
			Req.Append(Buf, Read);
			if (NsStunDecodeChannelBind(Req, TxId, GotCh, PeerIp, PeerPort) && GotCh == Channel)
			{
				bGotBind = true;
				break;
			}
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!bGotBind || PeerPort != Sv.BoundPort(ENsAddr::Sv))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-relay: no bind"));
	}

	TArray<uint8> BindOk;
	if (!NsStunEncodeChannelBindSuccess(TxId, BindOk))
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-relay: encode success"));
	}
	int32 Sent = 0;
	if (!TurnSock->SendTo(BindOk.GetData(), BindOk.Num(), Sent, *From) || Sent != BindOk.Num())
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-relay: send success"));
	}

	bool bGotBindOk = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		if (C0.StunRecvChannelBind(ENsAddr::C0, TxId))
		{
			bGotBindOk = true;
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!bGotBindOk || !C0.EnableTurnRelay(TEXT("127.0.0.1"), TurnPort) || !C0.UsesTurnRelay())
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-relay: enable"));
	}

	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Pkt.PlayerId = 0;
	Pkt.Dx = 1;
	C0.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);

	bool bFwd = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Read = 0;
		if (TurnSock->RecvFrom(Buf, sizeof(Buf), Read, *From) && Read > 0)
		{
			TArray<uint8> Chan;
			Chan.Append(Buf, Read);
			uint16 DataCh = 0;
			TArray<uint8> Payload;
			if (NsDecodeChannelData(Chan, DataCh, Payload) && DataCh == Channel)
			{
				TSharedRef<FInternetAddr> Dest = SS->CreateInternetAddr();
				bool bOk = false;
				Dest->SetIp(*NsStunIpv4ToString(PeerIp), bOk);
				Dest->SetPort(PeerPort);
				Sent = 0;
				if (bOk && Relay->SendTo(Payload.GetData(), Payload.Num(), Sent, *Dest) && Sent == Payload.Num())
				{
					bFwd = true;
					break;
				}
			}
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!bFwd)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-relay: no forward"));
	}

	TArray<FNsPacket> PeerPackets;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Sv.Drain(ENsAddr::Sv, PeerPackets);
		if (!PeerPackets.IsEmpty())
		{
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (PeerPackets.Num() != 1 || PeerPackets[0].Src != ENsAddr::C0 || PeerPackets[0].Dx != 1)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-relay: peer must Drain raw TANS from relay"));
	}

	FNsPacket Reply;
	Reply.Type = ENsMsg::S2CSnapshot;
	Reply.Tick = 3;
	Sv.Send(ENsAddr::Sv, ENsAddr::C0, Reply);
	bool bReturnForwarded = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Read = 0;
		if (Relay->RecvFrom(Buf, sizeof(Buf), Read, *From) && Read > 0)
		{
			TArray<uint8> PeerData;
			PeerData.Append(Buf, Read);
			FNsPacket PeerPacket;
			TArray<uint8> ChannelData;
			if (From->GetPort() != Sv.BoundPort(ENsAddr::Sv)
				|| !NsDecodePacket(PeerData, PeerPacket)
				|| !NsEncodeChannelData(Channel, PeerData, ChannelData))
			{
				StunDestroy(TurnSock);
				return StunFail(TEXT("stun-relay: raw peer return"));
			}
			TSharedRef<FInternetAddr> ClientAddr = SS->CreateInternetAddr();
			ClientAddr->SetLoopbackAddress();
			ClientAddr->SetPort(C0.BoundPort(ENsAddr::C0));
			Sent = 0;
			bReturnForwarded = TurnSock->SendTo(ChannelData.GetData(), ChannelData.Num(), Sent, *ClientAddr)
				&& Sent == ChannelData.Num();
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!bReturnForwarded)
	{
		StunDestroy(TurnSock);
		return StunFail(TEXT("stun-relay: return forward"));
	}

	TArray<FNsPacket> ClientPackets;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		C0.Drain(ENsAddr::C0, ClientPackets);
		if (!ClientPackets.IsEmpty())
		{
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	StunDestroy(TurnSock);
	if (ClientPackets.Num() != 1 || ClientPackets[0].Src != ENsAddr::Sv || ClientPackets[0].Tick != 3)
	{
		return StunFail(TEXT("stun-relay: Drain must unwrap ChannelData"));
	}

	return StunOk(TEXT("stun relay"));
}

FNsSelfTestResult NsRunStunIceSelfTest()
{
	TArray<FNsIceCandidate> Cands;
	FNsIceCandidate HostCand;
	HostCand.Type = ENsIceType::Host;
	HostCand.Ipv4 = 0x7F000001u;
	HostCand.Port = 27000;
	FNsIceCandidate SrflxCand;
	SrflxCand.Type = ENsIceType::Srflx;
	SrflxCand.Ipv4 = 0x0A000001u;
	SrflxCand.Port = 40000;
	FNsIceCandidate RelayCand;
	RelayCand.Type = ENsIceType::Relay;
	RelayCand.Ipv4 = 0x0A000002u;
	RelayCand.Port = 50000;
	Cands.Add(HostCand);
	Cands.Add(SrflxCand);
	Cands.Add(RelayCand);

	TArray<uint8> Wire;
	if (!NsIceEncode(1, Cands, Wire)
		|| Wire.Num() != NsIceHeaderBytes + NsIceCandBytes * 3
		|| Wire[0] != 0x43 || Wire[1] != 0x49 || Wire[2] != 0x53 || Wire[3] != 0x4E)
	{
		return StunFail(TEXT("stun-ice: encode"));
	}
	uint8 Slot = 99;
	TArray<FNsIceCandidate> Got;
	if (!NsIceDecode(Wire, Slot, Got) || Slot != 1 || Got.Num() != 3
		|| Got[0].Type != ENsIceType::Host || Got[0].Port != 27000 || Got[0].Ipv4 != HostCand.Ipv4
		|| Got[1].Type != ENsIceType::Srflx || Got[1].Port != 40000 || Got[1].Ipv4 != SrflxCand.Ipv4
		|| Got[2].Type != ENsIceType::Relay || Got[2].Port != 50000 || Got[2].Ipv4 != RelayCand.Ipv4)
	{
		return StunFail(TEXT("stun-ice: decode"));
	}

	TArray<FNsIceCandidate> Empty;
	TArray<uint8> Bad;
	FNsIceCandidate BadType = HostCand;
	BadType.Type = static_cast<ENsIceType>(3);
	TArray<FNsIceCandidate> BadCands;
	BadCands.Add(BadType);
	FNsIceCandidate ZeroPort = HostCand;
	ZeroPort.Port = 0;
	TArray<FNsIceCandidate> ZeroCands;
	ZeroCands.Add(ZeroPort);
	if (NsIceEncode(3, Cands, Bad) || NsIceEncode(0, Empty, Bad) || NsIceEncode(0, BadCands, Bad)
		|| NsIceEncode(0, ZeroCands, Bad))
	{
		return StunFail(TEXT("stun-ice: bad encode accepted"));
	}
	TArray<uint8> Trunc = Wire;
	Trunc.SetNum(Wire.Num() - 1);
	TArray<uint8> Extra = Wire;
	Extra.Add(0);
	TArray<uint8> Offer;
	uint8 OfferSlot = 0;
	uint32 OfferIp = 0;
	int32 OfferPort = 0;
	if (!NsRendezvousEncode(1, HostCand.Ipv4, HostCand.Port, Offer)
		|| NsIceDecode(Trunc, Slot, Got) || NsIceDecode(Extra, Slot, Got)
		|| NsIceDecode(Offer, Slot, Got) || NsRendezvousDecode(Wire, OfferSlot, OfferIp, OfferPort))
	{
		return StunFail(TEXT("stun-ice: cross decode"));
	}

	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return StunFail(TEXT("stun-ice: no sockets"));
	}
	FNsUdpNet Net;
	if (!Net.Bind(ENsAddr::C0, 0, false) || !Net.GatherIceCandidates(ENsAddr::C0, Got)
		|| Got.Num() != 1 || Got[0].Type != ENsIceType::Host
		|| Got[0].Port != Net.BoundPort(ENsAddr::C0) || Got[0].Ipv4 != 0x7F000001u)
	{
		return StunFail(TEXT("stun-ice: host candidate"));
	}

	auto ServeXor = [SS](FNsUdpNet& IceNet, bool bRelayed, uint32 Ipv4, int32 Port, const TCHAR* Fail) -> FNsSelfTestResult
	{
		FSocket* Fake = SS->CreateSocket(NAME_DGram, TEXT("NsIceFake"), FName(FNetworkProtocolTypes::IPv4));
		if (!Fake)
		{
			return StunFail(Fail);
		}
		Fake->SetNonBlocking(true);
		TSharedRef<FInternetAddr> BindAddr = SS->CreateInternetAddr();
		BindAddr->SetLoopbackAddress();
		BindAddr->SetPort(0);
		if (!Fake->Bind(*BindAddr))
		{
			StunDestroy(Fake);
			return StunFail(Fail);
		}
		TSharedRef<FInternetAddr> Bound = SS->CreateInternetAddr();
		Fake->GetAddress(*Bound);
		const int32 FakePort = Bound->GetPort();
		uint8 TxId[NsStunTxIdBytes];
		NsStunFillTxId(TxId);
		const bool bSent = bRelayed
			? IceNet.StunSendAllocate(ENsAddr::C0, TEXT("127.0.0.1"), FakePort, TxId)
			: IceNet.StunSendBind(ENsAddr::C0, TEXT("127.0.0.1"), FakePort, TxId);
		if (FakePort <= 0 || !bSent)
		{
			StunDestroy(Fake);
			return StunFail(Fail);
		}
		uint8 Buf[512];
		int32 Read = 0;
		TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
		bool bGotReq = false;
		for (int32 Try = 0; Try < 50 && !bGotReq; ++Try)
		{
			Read = 0;
			bGotReq = Fake->RecvFrom(Buf, 512, Read, *From) && Read >= NsStunHeaderBytes;
			if (!bGotReq)
			{
				FPlatformProcess::Sleep(0.001f);
			}
		}
		TArray<uint8> Reply;
		const bool bEncoded = bRelayed
			? NsStunEncodeXorRelayedReply(TxId, Ipv4, Port, Reply)
			: NsStunEncodeXorMappedReply(TxId, Ipv4, Port, Reply);
		int32 Sent = 0;
		if (!bGotReq || !bEncoded || !Fake->SendTo(Reply.GetData(), Reply.Num(), Sent, *From)
			|| Sent != Reply.Num())
		{
			StunDestroy(Fake);
			return StunFail(Fail);
		}
		FString Host;
		int32 GotPort = 0;
		bool bGot = false;
		for (int32 Try = 0; Try < 50 && !bGot; ++Try)
		{
			bGot = bRelayed
				? IceNet.StunRecvRelayed(ENsAddr::C0, TxId, Host, GotPort)
				: IceNet.StunRecvMapped(ENsAddr::C0, TxId, Host, GotPort);
			if (!bGot)
			{
				FPlatformProcess::Sleep(0.001f);
			}
		}
		StunDestroy(Fake);
		if (!bGot || GotPort != Port)
		{
			return StunFail(Fail);
		}
		return StunOk(TEXT("ok"));
	};

	FNsSelfTestResult Mapped = ServeXor(Net, false, SrflxCand.Ipv4, SrflxCand.Port, TEXT("stun-ice: mapped"));
	if (!Mapped.bOk)
	{
		return Mapped;
	}
	if (!Net.GatherIceCandidates(ENsAddr::C0, Got) || Got.Num() != 2
		|| Got[0].Type != ENsIceType::Host || Got[1].Type != ENsIceType::Srflx
		|| Got[1].Ipv4 != SrflxCand.Ipv4 || Got[1].Port != SrflxCand.Port)
	{
		return StunFail(TEXT("stun-ice: srflx candidate"));
	}

	FNsSelfTestResult Relayed = ServeXor(Net, true, RelayCand.Ipv4, RelayCand.Port, TEXT("stun-ice: relayed"));
	if (!Relayed.bOk)
	{
		return Relayed;
	}
	if (!Net.GatherIceCandidates(ENsAddr::C0, Got) || Got.Num() != 3
		|| Got[0].Type != ENsIceType::Host || Got[1].Type != ENsIceType::Srflx
		|| Got[2].Type != ENsIceType::Relay || Got[2].Ipv4 != RelayCand.Ipv4 || Got[2].Port != RelayCand.Port)
	{
		return StunFail(TEXT("stun-ice: relay candidate"));
	}
	if (!NsIceEncode(static_cast<uint8>(ENsAddr::C0), Got, Wire) || !NsIceDecode(Wire, Slot, Cands)
		|| Slot != static_cast<uint8>(ENsAddr::C0) || Cands.Num() != 3)
	{
		return StunFail(TEXT("stun-ice: gathered wire"));
	}

	return StunOk(TEXT("stun ice"));
}

FNsSelfTestResult NsRunStunIceExchangeSelfTest()
{
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return StunFail(TEXT("stun-ice-xchg: no sockets"));
	}

	FNsUdpNet Owned;
	if (!Owned.Bind(ENsAddr::C1, 0, false)
		|| Owned.IceExchange(TEXT("127.0.0.1"), 9, TArray<ENsAddr>{ENsAddr::C1})
		|| Owned.IceExchange(TEXT("127.0.0.1"), 9, TArray<ENsAddr>{}))
	{
		return StunFail(TEXT("stun-ice-xchg: required peers"));
	}

	FSocket* Hub = SS->CreateSocket(NAME_DGram, TEXT("NsIceHub"), FName(FNetworkProtocolTypes::IPv4));
	if (!Hub)
	{
		return StunFail(TEXT("stun-ice-xchg: hub create"));
	}
	Hub->SetNonBlocking(true);
	TSharedRef<FInternetAddr> BindAddr = SS->CreateInternetAddr();
	BindAddr->SetLoopbackAddress();
	BindAddr->SetPort(0);
	if (!Hub->Bind(*BindAddr))
	{
		StunDestroy(Hub);
		return StunFail(TEXT("stun-ice-xchg: hub bind"));
	}
	TSharedRef<FInternetAddr> Bound = SS->CreateInternetAddr();
	Hub->GetAddress(*Bound);
	const int32 HubPort = Bound->GetPort();
	if (HubPort <= 0)
	{
		StunDestroy(Hub);
		return StunFail(TEXT("stun-ice-xchg: hub port"));
	}

	FNsUdpNet Sv;
	FNsUdpNet C0;
	if (!Sv.Bind(ENsAddr::Sv, 0, false) || !C0.Bind(ENsAddr::C0, 0, false))
	{
		StunDestroy(Hub);
		return StunFail(TEXT("stun-ice-xchg: bind"));
	}

	TArray<uint8> Nsrv;
	if (!NsRendezvousEncode(static_cast<uint8>(ENsAddr::Sv), 0x7F000001u, Sv.BoundPort(ENsAddr::Sv), Nsrv))
	{
		StunDestroy(Hub);
		return StunFail(TEXT("stun-ice-xchg: nsrv"));
	}
	TSharedRef<FInternetAddr> C0Addr = SS->CreateInternetAddr();
	C0Addr->SetLoopbackAddress();
	C0Addr->SetPort(C0.BoundPort(ENsAddr::C0));
	int32 Sent = 0;
	if (!Hub->SendTo(Nsrv.GetData(), Nsrv.Num(), Sent, *C0Addr) || Sent != Nsrv.Num()
		|| C0.IceRecvPeer(ENsAddr::C0) || C0.PeerPort(ENsAddr::Sv) > 0)
	{
		StunDestroy(Hub);
		return StunFail(TEXT("stun-ice-xchg: nsrv rejected"));
	}

	TArray<FNsIceCandidate> HubCands[3];
	bool HubHave[3] = {};
	bool bSvPeer = false;
	bool bC0Peer = false;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Sv.IceSendOffer(ENsAddr::Sv, TEXT("127.0.0.1"), HubPort);
		C0.IceSendOffer(ENsAddr::C0, TEXT("127.0.0.1"), HubPort);
		uint8 Buf[64];
		TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
		int32 Read = 0;
		while (Hub->RecvFrom(Buf, 64, Read, *From) && Read > 0)
		{
			TArray<uint8> Bytes;
			Bytes.Append(Buf, Read);
			uint8 Slot = 0;
			TArray<FNsIceCandidate> Cands;
			if (!NsIceDecode(Bytes, Slot, Cands) || Slot > 2)
			{
				continue;
			}
			HubCands[Slot] = MoveTemp(Cands);
			HubHave[Slot] = true;
			for (int32 Other = 0; Other < 3; ++Other)
			{
				if (Other == static_cast<int32>(Slot) || !HubHave[Other])
				{
					continue;
				}
				TArray<uint8> Reply;
				if (!NsIceEncode(static_cast<uint8>(Other), HubCands[Other], Reply))
				{
					continue;
				}
				Hub->SendTo(Reply.GetData(), Reply.Num(), Sent, *From);
			}
		}
		bSvPeer = bSvPeer || Sv.IceRecvPeer(ENsAddr::Sv);
		bC0Peer = bC0Peer || C0.IceRecvPeer(ENsAddr::C0);
		if (bSvPeer && bC0Peer)
		{
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	StunDestroy(Hub);
	if (!bSvPeer || !bC0Peer)
	{
		return StunFail(TEXT("stun-ice-xchg: no peer"));
	}
	if (Sv.PeerPort(ENsAddr::C0) != C0.BoundPort(ENsAddr::C0)
		|| C0.PeerPort(ENsAddr::Sv) != Sv.BoundPort(ENsAddr::Sv))
	{
		return StunFail(TEXT("stun-ice-xchg: host candidate"));
	}

	if (!Sv.PunchPeers() || !C0.PunchPeers())
	{
		return StunFail(TEXT("stun-ice-xchg: punch"));
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
		return StunFail(TEXT("stun-ice-xchg: tans"));
	}

	return StunOk(TEXT("stun ice exchange"));
}

FNsSelfTestResult NsRunStunIcePairsSelfTest()
{
	TArray<FNsIceCandidate> Local;
	TArray<FNsIceCandidate> Remote;
	FNsIceCandidate Host;
	Host.Type = ENsIceType::Host;
	Host.Ipv4 = 0x7F000001u;
	Host.Port = 27000;
	FNsIceCandidate Srflx;
	Srflx.Type = ENsIceType::Srflx;
	Srflx.Ipv4 = 0x0A000001u;
	Srflx.Port = 40000;
	FNsIceCandidate PeerHost = Host;
	PeerHost.Port = 1;
	FNsIceCandidate PeerSrflx;
	PeerSrflx.Type = ENsIceType::Srflx;
	PeerSrflx.Ipv4 = 0x7F000001u;
	PeerSrflx.Port = 27001;
	FNsIceCandidate Relay;
	Relay.Type = ENsIceType::Relay;
	Relay.Ipv4 = 0x0A000002u;
	Relay.Port = 50000;
	Local.Add(Host);
	Local.Add(Srflx);
	Remote.Add(PeerHost);
	Remote.Add(PeerSrflx);
	Remote.Add(Relay);
	TArray<FNsIcePair> Pairs;
	if (!NsIceFormPairs(Local, Remote, Pairs) || Pairs.Num() != 4
		|| Pairs[0].Local.Type != ENsIceType::Host || Pairs[0].Remote.Type != ENsIceType::Host
		|| Pairs[0].Remote.Port != 1)
	{
		return StunFail(TEXT("stun-ice-pairs: host-host first"));
	}
	for (const FNsIcePair& Pair : Pairs)
	{
		if (Pair.Local.Type == ENsIceType::Relay || Pair.Remote.Type == ENsIceType::Relay)
		{
			return StunFail(TEXT("stun-ice-pairs: relay excluded"));
		}
	}
	TArray<FNsIceCandidate> Empty;
	TArray<FNsIcePair> None;
	if (NsIceFormPairs(Local, Empty, None) || NsIceFormPairs(Empty, Remote, None))
	{
		return StunFail(TEXT("stun-ice-pairs: empty"));
	}

	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return StunFail(TEXT("stun-ice-pairs: no sockets"));
	}
	FNsUdpNet Sv;
	FNsUdpNet C0;
	FNsUdpNet Dead;
	if (!Sv.Bind(ENsAddr::Sv, 0, false) || !C0.Bind(ENsAddr::C0, 0, false)
		|| !Dead.Bind(ENsAddr::C1, 0, false)
		|| !Sv.SetPeer(ENsAddr::C0, TEXT("127.0.0.1"), C0.BoundPort(ENsAddr::C0)))
	{
		return StunFail(TEXT("stun-ice-pairs: bind"));
	}

	TArray<FNsIceCandidate> Offer;
	FNsIceCandidate Bogus;
	Bogus.Type = ENsIceType::Host;
	Bogus.Ipv4 = 0x7F000001u;
	Bogus.Port = Dead.BoundPort(ENsAddr::C1);
	FNsIceCandidate Real;
	Real.Type = ENsIceType::Srflx;
	Real.Ipv4 = 0x7F000001u;
	Real.Port = Sv.BoundPort(ENsAddr::Sv);
	Offer.Add(Bogus);
	Offer.Add(Real);
	TArray<uint8> Wire;
	if (!NsIceEncode(static_cast<uint8>(ENsAddr::Sv), Offer, Wire))
	{
		return StunFail(TEXT("stun-ice-pairs: encode"));
	}
	FSocket* Inj = SS->CreateSocket(NAME_DGram, TEXT("NsIcePairInj"), FName(FNetworkProtocolTypes::IPv4));
	if (!Inj)
	{
		return StunFail(TEXT("stun-ice-pairs: inject"));
	}
	ON_SCOPE_EXIT { StunDestroy(Inj); };
	Inj->SetNonBlocking(true);
	TSharedRef<FInternetAddr> BindAddr = SS->CreateInternetAddr();
	BindAddr->SetLoopbackAddress();
	BindAddr->SetPort(0);
	if (!Inj->Bind(*BindAddr))
	{
		return StunFail(TEXT("stun-ice-pairs: inj bind"));
	}
	TSharedRef<FInternetAddr> Dest = SS->CreateInternetAddr();
	Dest->SetLoopbackAddress();
	Dest->SetPort(C0.BoundPort(ENsAddr::C0));
	int32 Sent = 0;
	if (!Inj->SendTo(Wire.GetData(), Wire.Num(), Sent, *Dest) || Sent != Wire.Num()
		|| !C0.IceRecvPeer(ENsAddr::C0) || C0.PeerPort(ENsAddr::Sv) != Bogus.Port)
	{
		return StunFail(TEXT("stun-ice-pairs: bogus host"));
	}

	auto Checker = Async(EAsyncExecution::Thread, [&C0]()
	{
		return C0.IceCheckPairs();
	});
	for (int32 Try = 0; Try < 80; ++Try)
	{
		FString MappedHost;
		int32 MappedPort = 0;
		Sv.StunServe(ENsAddr::Sv, nullptr, MappedHost, MappedPort);
		if (Checker.IsReady())
		{
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (!Checker.Get() || C0.PeerPort(ENsAddr::Sv) != Sv.BoundPort(ENsAddr::Sv))
	{
		return StunFail(TEXT("stun-ice-pairs: nominate srflx"));
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
		return StunFail(TEXT("stun-ice-pairs: tans"));
	}

	return StunOk(TEXT("stun ice pairs"));
}

FNsSelfTestResult NsRunStunIceNominateSelfTest()
{
	FNsUdpNet Empty;
	if (!Empty.Bind(ENsAddr::C0, 0, false) || Empty.IceWaitNominate())
	{
		return StunFail(TEXT("stun-ice-nom: empty"));
	}

	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return StunFail(TEXT("stun-ice-nom: no sockets"));
	}
	FNsUdpNet Sv;
	FNsUdpNet C0;
	FNsUdpNet DeadSv;
	FNsUdpNet DeadC0;
	if (!Sv.Bind(ENsAddr::Sv, 0, false) || !C0.Bind(ENsAddr::C0, 0, false)
		|| !DeadSv.Bind(ENsAddr::C1, 0, false) || !DeadC0.Bind(ENsAddr::C1, 0, false))
	{
		return StunFail(TEXT("stun-ice-nom: bind"));
	}

	FSocket* Inj = SS->CreateSocket(NAME_DGram, TEXT("NsIceNomInj"), FName(FNetworkProtocolTypes::IPv4));
	if (!Inj)
	{
		return StunFail(TEXT("stun-ice-nom: inject"));
	}
	ON_SCOPE_EXIT { StunDestroy(Inj); };
	Inj->SetNonBlocking(true);
	TSharedRef<FInternetAddr> BindAddr = SS->CreateInternetAddr();
	BindAddr->SetLoopbackAddress();
	BindAddr->SetPort(0);
	if (!Inj->Bind(*BindAddr))
	{
		return StunFail(TEXT("stun-ice-nom: inj bind"));
	}
	TSharedRef<FInternetAddr> Dest = SS->CreateInternetAddr();
	Dest->SetLoopbackAddress();
	auto Inject = [&](FNsUdpNet& Net, ENsAddr Recv, ENsAddr OfferSlot,
		const TArray<FNsIceCandidate>& Cands, int32 ExpectPort) -> bool
	{
		TArray<uint8> Wire;
		if (!NsIceEncode(static_cast<uint8>(OfferSlot), Cands, Wire))
		{
			return false;
		}
		Dest->SetPort(Net.BoundPort(Recv));
		int32 Sent = 0;
		return Inj->SendTo(Wire.GetData(), Wire.Num(), Sent, *Dest) && Sent == Wire.Num()
			&& Net.IceRecvPeer(Recv) && Net.PeerPort(OfferSlot) == ExpectPort;
	};

	TArray<FNsIceCandidate> ForC0;
	FNsIceCandidate BogusSv;
	BogusSv.Type = ENsIceType::Host;
	BogusSv.Ipv4 = 0x7F000001u;
	BogusSv.Port = DeadSv.BoundPort(ENsAddr::C1);
	FNsIceCandidate RealSv;
	RealSv.Type = ENsIceType::Srflx;
	RealSv.Ipv4 = 0x7F000001u;
	RealSv.Port = Sv.BoundPort(ENsAddr::Sv);
	ForC0.Add(BogusSv);
	ForC0.Add(RealSv);
	TArray<FNsIceCandidate> ForSv;
	FNsIceCandidate BogusC0;
	BogusC0.Type = ENsIceType::Host;
	BogusC0.Ipv4 = 0x7F000001u;
	BogusC0.Port = DeadC0.BoundPort(ENsAddr::C1);
	FNsIceCandidate RealC0;
	RealC0.Type = ENsIceType::Srflx;
	RealC0.Ipv4 = 0x7F000001u;
	RealC0.Port = C0.BoundPort(ENsAddr::C0);
	ForSv.Add(BogusC0);
	ForSv.Add(RealC0);
	if (!Inject(C0, ENsAddr::C0, ENsAddr::Sv, ForC0, BogusSv.Port)
		|| !Inject(Sv, ENsAddr::Sv, ENsAddr::C0, ForSv, BogusC0.Port))
	{
		return StunFail(TEXT("stun-ice-nom: bogus host"));
	}

	auto Checker = Async(EAsyncExecution::Thread, [&C0]()
	{
		return C0.IceCheckPairs();
	});
	const bool bNom = Sv.IceWaitNominate();
	if (!Checker.Get() || !bNom
		|| C0.PeerPort(ENsAddr::Sv) != Sv.BoundPort(ENsAddr::Sv)
		|| Sv.PeerPort(ENsAddr::C0) != C0.BoundPort(ENsAddr::C0))
	{
		return StunFail(TEXT("stun-ice-nom: use-candidate"));
	}

	TArray<FNsPacket> Flush;
	Sv.Drain(ENsAddr::Sv, Flush);
	C0.Drain(ENsAddr::C0, Flush);
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Pkt.PlayerId = 0;
	Pkt.Dx = 1;
	C0.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
	TArray<FNsPacket> GotSv;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Sv.Drain(ENsAddr::Sv, GotSv);
		if (GotSv.Num() > 0)
		{
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	Pkt.Dx = -1;
	Sv.Send(ENsAddr::Sv, ENsAddr::C0, Pkt);
	TArray<FNsPacket> GotC0;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		C0.Drain(ENsAddr::C0, GotC0);
		if (GotC0.Num() > 0)
		{
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (GotSv.Num() != 1 || GotSv[0].Dx != 1 || GotSv[0].Src != ENsAddr::C0
		|| GotC0.Num() != 1 || GotC0[0].Dx != -1 || GotC0[0].Src != ENsAddr::Sv)
	{
		return StunFail(TEXT("stun-ice-nom: tans"));
	}

	return StunOk(TEXT("stun ice nominate"));
}

FNsSelfTestResult NsRunStunHubSelfTest()
{
	FNsRendezvousHub Empty;
	if (Empty.Serve() || Empty.BoundPort() != 0)
	{
		return StunFail(TEXT("stun-hub: unbound"));
	}

	FNsRendezvousHub Hub;
	if (!Hub.Bind(0) || Hub.BoundPort() <= 0)
	{
		return StunFail(TEXT("stun-hub: bind"));
	}

	auto PumpHub = [](FNsRendezvousHub& Hub)
	{
		for (int32 Try = 0; Try < 80; ++Try)
		{
			Hub.Serve();
			FPlatformProcess::Sleep(0.001f);
		}
	};

	FNsUdpNet Sv;
	FNsUdpNet C0;
	if (!Sv.Bind(ENsAddr::Sv, 0, false) || !C0.Bind(ENsAddr::C0, 0, false))
	{
		return StunFail(TEXT("stun-hub: ice bind"));
	}
	const int32 IcePort = Hub.BoundPort();
	auto IcePump = Async(EAsyncExecution::Thread, [&Hub, &PumpHub]()
	{
		PumpHub(Hub);
	});
	auto IceSv = Async(EAsyncExecution::Thread, [&Sv, IcePort]()
	{
		return Sv.IceExchange(TEXT("127.0.0.1"), IcePort, TArray<ENsAddr>{ENsAddr::C0});
	});
	auto IceC0 = Async(EAsyncExecution::Thread, [&C0, IcePort]()
	{
		return C0.IceExchange(TEXT("127.0.0.1"), IcePort, TArray<ENsAddr>{ENsAddr::Sv});
	});
	const bool bIce = IceSv.Get() && IceC0.Get();
	IcePump.Get();
	if (!bIce
		|| Sv.PeerPort(ENsAddr::C0) != C0.BoundPort(ENsAddr::C0)
		|| C0.PeerPort(ENsAddr::Sv) != Sv.BoundPort(ENsAddr::Sv))
	{
		return StunFail(TEXT("stun-hub: ice exchange"));
	}
	if (!Sv.PunchPeers() || !C0.PunchPeers())
	{
		return StunFail(TEXT("stun-hub: ice punch"));
	}
	TArray<FNsPacket> Flush;
	Sv.Drain(ENsAddr::Sv, Flush);
	C0.Drain(ENsAddr::C0, Flush);
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Pkt.PlayerId = 0;
	Pkt.Dx = 1;
	C0.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
	TArray<FNsPacket> GotIce;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Sv.Drain(ENsAddr::Sv, GotIce);
		if (GotIce.Num() > 0)
		{
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (GotIce.Num() != 1 || GotIce[0].Dx != 1 || GotIce[0].Src != ENsAddr::C0)
	{
		return StunFail(TEXT("stun-hub: ice tans"));
	}

	Sv.Close();
	C0.Close();
	if (!Hub.Bind(0) || Hub.BoundPort() <= 0
		|| !Sv.Bind(ENsAddr::Sv, 0, false) || !C0.Bind(ENsAddr::C0, 0, false))
	{
		return StunFail(TEXT("stun-hub: nsrv bind"));
	}
	const int32 NsrvPort = Hub.BoundPort();
	auto NsrvPump = Async(EAsyncExecution::Thread, [&Hub, &PumpHub]()
	{
		PumpHub(Hub);
	});
	auto NsrvSv = Async(EAsyncExecution::Thread, [&Sv, NsrvPort]()
	{
		return Sv.RendezvousExchange(TEXT("127.0.0.1"), NsrvPort, TArray<ENsAddr>{ENsAddr::C0});
	});
	auto NsrvC0 = Async(EAsyncExecution::Thread, [&C0, NsrvPort]()
	{
		return C0.RendezvousExchange(TEXT("127.0.0.1"), NsrvPort, TArray<ENsAddr>{ENsAddr::Sv});
	});
	const bool bNsrv = NsrvSv.Get() && NsrvC0.Get();
	NsrvPump.Get();
	if (!bNsrv
		|| Sv.PeerPort(ENsAddr::C0) != C0.BoundPort(ENsAddr::C0)
		|| C0.PeerPort(ENsAddr::Sv) != Sv.BoundPort(ENsAddr::Sv))
	{
		return StunFail(TEXT("stun-hub: nsrv exchange"));
	}
	if (!Sv.PunchPeers() || !C0.PunchPeers())
	{
		return StunFail(TEXT("stun-hub: nsrv punch"));
	}
	Flush.Reset();
	Sv.Drain(ENsAddr::Sv, Flush);
	C0.Drain(ENsAddr::C0, Flush);
	Pkt.Dx = -1;
	C0.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
	TArray<FNsPacket> GotNsrv;
	for (int32 Try = 0; Try < 50; ++Try)
	{
		Sv.Drain(ENsAddr::Sv, GotNsrv);
		if (GotNsrv.Num() > 0)
		{
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (GotNsrv.Num() != 1 || GotNsrv[0].Dx != -1 || GotNsrv[0].Src != ENsAddr::C0)
	{
		return StunFail(TEXT("stun-hub: nsrv tans"));
	}

	return StunOk(TEXT("stun hub"));
}

FNsSelfTestResult NsRunStunHubProcessSelfTest()
{
	NsStopRendezvousHub();
	if (NsRendezvousHubBoundPort() != 0)
	{
		return StunFail(TEXT("stun-hub-proc: idle"));
	}
	ON_SCOPE_EXIT { NsStopRendezvousHub(); };
	if (!NsStartRendezvousHub(0) || NsRendezvousHubBoundPort() <= 0)
	{
		return StunFail(TEXT("stun-hub-proc: start"));
	}
	const int32 HubPort = NsRendezvousHubBoundPort();
	FNsUdpNet Sv;
	FNsUdpNet C0;
	if (!Sv.Bind(ENsAddr::Sv, 0, false) || !C0.Bind(ENsAddr::C0, 0, false))
	{
		return StunFail(TEXT("stun-hub-proc: bind"));
	}
	auto IceSv = Async(EAsyncExecution::Thread, [&Sv, HubPort]()
	{
		return Sv.IceExchange(TEXT("127.0.0.1"), HubPort, TArray<ENsAddr>{ENsAddr::C0});
	});
	auto IceC0 = Async(EAsyncExecution::Thread, [&C0, HubPort]()
	{
		return C0.IceExchange(TEXT("127.0.0.1"), HubPort, TArray<ENsAddr>{ENsAddr::Sv});
	});
	if (!IceSv.Get() || !IceC0.Get()
		|| Sv.PeerPort(ENsAddr::C0) != C0.BoundPort(ENsAddr::C0)
		|| C0.PeerPort(ENsAddr::Sv) != Sv.BoundPort(ENsAddr::Sv))
	{
		return StunFail(TEXT("stun-hub-proc: ice exchange"));
	}
	if (!Sv.PunchPeers() || !C0.PunchPeers())
	{
		return StunFail(TEXT("stun-hub-proc: punch"));
	}
	TArray<FNsPacket> Flush;
	Sv.Drain(ENsAddr::Sv, Flush);
	C0.Drain(ENsAddr::C0, Flush);
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Pkt.PlayerId = 0;
	Pkt.Dx = 2;
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
	if (Got.Num() != 1 || Got[0].Dx != 2 || Got[0].Src != ENsAddr::C0)
	{
		return StunFail(TEXT("stun-hub-proc: tans"));
	}
	NsStopRendezvousHub();
	if (NsRendezvousHubBoundPort() != 0)
	{
		return StunFail(TEXT("stun-hub-proc: stop"));
	}
	return StunOk(TEXT("stun hub process"));
}

FNsSelfTestResult NsRunStunIceSdpSelfTest()
{
	TArray<FNsIceCandidate> Cands;
	FNsIceCandidate HostCand;
	HostCand.Type = ENsIceType::Host;
	HostCand.Ipv4 = 0x7F000001u;
	HostCand.Port = 27000;
	FNsIceCandidate SrflxCand;
	SrflxCand.Type = ENsIceType::Srflx;
	SrflxCand.Ipv4 = 0x0A000001u;
	SrflxCand.Port = 40000;
	FNsIceCandidate RelayCand;
	RelayCand.Type = ENsIceType::Relay;
	RelayCand.Ipv4 = 0x0A000002u;
	RelayCand.Port = 50000;
	Cands.Add(HostCand);
	Cands.Add(SrflxCand);
	Cands.Add(RelayCand);

	FString Sdp;
	if (!NsIceSdpEncode(1, Cands, Sdp)
		|| !Sdp.StartsWith(TEXT("v=0\r\n"))
		|| !Sdp.Contains(TEXT("o=- 1 0 IN IP4 0.0.0.0"))
		|| !Sdp.Contains(TEXT("typ host"))
		|| !Sdp.Contains(TEXT("typ srflx"))
		|| !Sdp.Contains(TEXT("typ relay"))
		|| !Sdp.Contains(TEXT("127.0.0.1 27000"))
		|| !Sdp.Contains(TEXT("10.0.0.1 40000"))
		|| !Sdp.Contains(TEXT("10.0.0.2 50000")))
	{
		return StunFail(TEXT("stun-ice-sdp: encode"));
	}

	uint8 Slot = 99;
	TArray<FNsIceCandidate> Got;
	if (!NsIceSdpDecode(Sdp, Slot, Got) || Slot != 1 || Got.Num() != 3
		|| Got[0].Type != ENsIceType::Host || Got[0].Port != 27000 || Got[0].Ipv4 != HostCand.Ipv4
		|| Got[1].Type != ENsIceType::Srflx || Got[1].Port != 40000 || Got[1].Ipv4 != SrflxCand.Ipv4
		|| Got[2].Type != ENsIceType::Relay || Got[2].Port != 50000 || Got[2].Ipv4 != RelayCand.Ipv4)
	{
		return StunFail(TEXT("stun-ice-sdp: decode"));
	}

	const FString Lf = TEXT(
		"v=0\n"
		"o=- 0 0 IN IP4 0.0.0.0\n"
		"s=-\n"
		"t=0 0\n"
		"a=candidate:1 1 UDP 1 127.0.0.1 27000 typ host\n");
	if (!NsIceSdpDecode(Lf, Slot, Got) || Slot != 0 || Got.Num() != 1
		|| Got[0].Type != ENsIceType::Host || Got[0].Port != 27000 || Got[0].Ipv4 != HostCand.Ipv4)
	{
		return StunFail(TEXT("stun-ice-sdp: lf"));
	}

	TArray<FNsIceCandidate> Empty;
	FString Bad;
	FNsIceCandidate BadType = HostCand;
	BadType.Type = static_cast<ENsIceType>(3);
	TArray<FNsIceCandidate> BadCands;
	BadCands.Add(BadType);
	FNsIceCandidate ZeroPort = HostCand;
	ZeroPort.Port = 0;
	TArray<FNsIceCandidate> ZeroCands;
	ZeroCands.Add(ZeroPort);
	TArray<FNsIceCandidate> Four = Cands;
	Four.Add(HostCand);
	if (NsIceSdpEncode(3, Cands, Bad) || NsIceSdpEncode(0, Empty, Bad) || NsIceSdpEncode(0, BadCands, Bad)
		|| NsIceSdpEncode(0, ZeroCands, Bad) || NsIceSdpEncode(0, Four, Bad))
	{
		return StunFail(TEXT("stun-ice-sdp: bad encode accepted"));
	}

	TArray<uint8> Wire;
	if (!NsIceEncode(1, Cands, Wire) || NsIceSdpDecode(TEXT(""), Slot, Got)
		|| NsIceSdpDecode(TEXT("v=0"), Slot, Got)
		|| NsIceSdpDecode(TEXT("v=0\no=- 3 0 IN IP4 0.0.0.0\na=candidate:1 1 UDP 1 127.0.0.1 27000 typ host"), Slot, Got)
		|| NsIceSdpDecode(TEXT("v=0\no=- 1 0 IN IP4 0.0.0.0\na=candidate:1 1 TCP 1 127.0.0.1 27000 typ host"), Slot, Got)
		|| NsIceSdpDecode(TEXT("v=0\no=- 1 0 IN IP4 0.0.0.0\na=candidate:1 1 UDP 1 127.0.0.1 27000 typ prflx"), Slot, Got)
		|| NsIceSdpDecode(
			TEXT("v=0\no=- 1 0 IN IP4 0.0.0.0\n")
			TEXT("a=candidate:1 1 UDP 1 127.0.0.1 27000 typ host\n")
			TEXT("a=candidate:2 1 UDP 1 10.0.0.1 40000 typ srflx\n")
			TEXT("a=candidate:3 1 UDP 1 10.0.0.2 50000 typ relay\n")
			TEXT("a=candidate:4 1 UDP 1 10.0.0.3 50001 typ host"),
			Slot, Got)
		|| NsIceDecode(Wire, Slot, Got) == false)
	{
		return StunFail(TEXT("stun-ice-sdp: bad decode accepted"));
	}

	return StunOk(TEXT("stun ice sdp"));
}
