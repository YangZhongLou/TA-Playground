// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsSelfTest.h"
#include "NsStun.h"
#include "NsUdpNet.h"
#include "NsCodec.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "IPAddress.h"
#include "Misc/ScopeExit.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

namespace
{
FNsSelfTestResult StunRegressionResult(bool bOk, const TCHAR* Detail)
{
	FNsSelfTestResult Result;
	Result.bOk = bOk;
	Result.Detail = Detail;
	return Result;
}

FSocket* StunRegressionSocket(ISocketSubsystem& SS, int32& OutPort)
{
	FSocket* Socket = SS.CreateSocket(NAME_DGram, TEXT("NsStunRegression"), FName(FNetworkProtocolTypes::IPv4));
	if (!Socket)
	{
		return nullptr;
	}
	Socket->SetNonBlocking(true);
	TSharedRef<FInternetAddr> Addr = SS.CreateInternetAddr();
	Addr->SetLoopbackAddress();
	Addr->SetPort(0);
	if (!Socket->Bind(*Addr))
	{
		SS.DestroySocket(Socket);
		return nullptr;
	}
	Socket->GetAddress(*Addr);
	OutPort = Addr->GetPort();
	return Socket;
}

bool StunPeerTransactions(bool bChannels, bool bHost, bool bDropReply)
{
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return false;
	}
	int32 Port = 0;
	FSocket* Turn = StunRegressionSocket(*SS, Port);
	if (!Turn)
	{
		return false;
	}
	ON_SCOPE_EXIT { Turn->Close(); SS->DestroySocket(Turn); };
	FNsUdpNet Net;
	if (bHost)
	{
		if (!Net.Bind(ENsAddr::Sv) || !Net.Bind(ENsAddr::C0)
			|| !Net.SetPeer(ENsAddr::C1, TEXT("127.0.0.1"), 31002))
		{
			return false;
		}
	}
	else if (!Net.Bind(ENsAddr::C1)
		|| !Net.SetPeer(ENsAddr::Sv, TEXT("127.0.0.1"), 31000)
		|| !Net.SetPeer(ENsAddr::C0, TEXT("127.0.0.1"), 31001))
	{
		return false;
	}

	auto Server = Async(EAsyncExecution::Thread, [SS, Turn, bChannels, bDropReply]()
	{
		TArray<TArray<uint8>> Requests;
		TArray<TSharedRef<FInternetAddr>> Clients;
		for (int32 Try = 0; Try < 500 && Requests.Num() < 2; ++Try)
		{
			uint8 Buf[512];
			int32 Read = 0;
			TSharedRef<FInternetAddr> From = SS->CreateInternetAddr();
			if (Turn->RecvFrom(Buf, sizeof(Buf), Read, *From) && Read > 0)
			{
				TArray<uint8> Bytes;
				Bytes.Append(Buf, Read);
				Requests.Add(MoveTemp(Bytes));
				Clients.Add(From);
			}
			else
			{
				FPlatformProcess::Sleep(0.001f);
			}
		}
		if (Requests.Num() != 2)
		{
			return false;
		}
		uint8 TxIds[2][NsStunTxIdBytes];
		for (int32 i = 0; i < 2; ++i)
		{
			uint32 PeerIp = 0;
			int32 PeerPort = 0;
			uint16 Channel = 0;
			if (!NsStunReadTxId(Requests[i], TxIds[i])
				|| !(bChannels
					? NsStunDecodeChannelBind(Requests[i], TxIds[i], Channel, PeerIp, PeerPort)
					: NsStunDecodePeer(Requests[i], TxIds[i], PeerIp, PeerPort)))
			{
				return false;
			}
		}
		// Reply out of order, including a duplicate that must not complete another request.
		for (int32 i = 1; i >= (bDropReply ? 1 : 0); --i)
		{
			TArray<uint8> Reply;
			if (!(bChannels ? NsStunEncodeChannelBindSuccess(TxIds[i], Reply)
				: NsStunEncodeCreatePermissionSuccess(TxIds[i], Reply)))
			{
				return false;
			}
			for (int32 Copy = 0; Copy < 2; ++Copy)
			{
				int32 Sent = 0;
				if (!Turn->SendTo(Reply.GetData(), Reply.Num(), Sent, *Clients[i]) || Sent != Reply.Num())
				{
					return false;
				}
			}
		}
		return FMemory::Memcmp(TxIds[0], TxIds[1], NsStunTxIdBytes) != 0;
	});
	const bool bCompleted = bChannels ? Net.StunBindPeerChannels(TEXT("127.0.0.1"), Port)
		: Net.StunPermitPeers(TEXT("127.0.0.1"), Port);
	return Server.Get() && bCompleted == !bDropReply;
}
}

FNsSelfTestResult NsRunStunChannelPeersSelfTest()
{
	for (bool bHost : {false, true})
	{
		for (bool bDropReply : {false, true})
		{
			if (!StunPeerTransactions(true, bHost, bDropReply))
			{
				return StunRegressionResult(false, TEXT("stun-channel-peers: unique transactions and all replies required"));
			}
		}
	}
	return StunRegressionResult(true, TEXT("stun channel peers"));
}

FNsSelfTestResult NsRunStunPermitPeersSelfTest()
{
	for (bool bHost : {false, true})
	{
		for (bool bDropReply : {false, true})
		{
			if (!StunPeerTransactions(false, bHost, bDropReply))
			{
				return StunRegressionResult(false, TEXT("stun-permit-peers: unique transactions and all replies required"));
			}
		}
	}
	return StunRegressionResult(true, TEXT("stun permit peers"));
}

FNsSelfTestResult NsRunStunRendezvousOrderSelfTest()
{
	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SS)
	{
		return StunRegressionResult(false, TEXT("stun-rendezvous-order: sockets"));
	}
	int32 HubPort = 0;
	FSocket* Hub = StunRegressionSocket(*SS, HubPort);
	if (!Hub)
	{
		return StunRegressionResult(false, TEXT("stun-rendezvous-order: hub"));
	}
	ON_SCOPE_EXIT { Hub->Close(); SS->DestroySocket(Hub); };
	const auto Exchange = [SS, Hub, HubPort](bool bHost, const TArray<ENsAddr>& Required,
		const TArray<ENsAddr>& Offers, bool bExpected)
	{
		FNsUdpNet Net;
		const ENsAddr Local = bHost ? ENsAddr::Sv : ENsAddr::C1;
		if (!Net.Bind(Local) || (bHost && !Net.Bind(ENsAddr::C0)))
		{
			return false;
		}
		TSharedRef<FInternetAddr> Dest = SS->CreateInternetAddr();
		Dest->SetLoopbackAddress();
		Dest->SetPort(Net.BoundPort(Local));
		for (ENsAddr Peer : Offers)
		{
			const uint8 Slot = static_cast<uint8>(Peer);
			TArray<uint8> Reply;
			int32 Sent = 0;
			if (!NsRendezvousEncode(Slot, 0x7f000001u, 31000 + Slot, Reply)
				|| !Hub->SendTo(Reply.GetData(), Reply.Num(), Sent, *Dest) || Sent != Reply.Num())
			{
				return false;
			}
		}
		if (Net.RendezvousExchange(TEXT("127.0.0.1"), HubPort, Required) != bExpected)
		{
			return false;
		}
		for (ENsAddr Peer : Offers)
		{
			if (Net.PeerPort(Peer) != 31000 + static_cast<int32>(Peer))
			{
				return false;
			}
		}
		return true;
	};
	// A client needs both host endpoints even when C0 arrives before Sv.
	const bool bOk = Exchange(false, {ENsAddr::Sv, ENsAddr::C0}, {ENsAddr::C0, ENsAddr::Sv}, true)
		&& Exchange(false, {ENsAddr::Sv, ENsAddr::C0}, {ENsAddr::Sv, ENsAddr::C0}, true)
		&& Exchange(false, {ENsAddr::Sv, ENsAddr::C0}, {ENsAddr::C0}, false)
		&& Exchange(false, {ENsAddr::C0}, {ENsAddr::C0}, true)
		&& Exchange(true, {ENsAddr::C1}, {ENsAddr::C1}, true);
	return StunRegressionResult(bOk, TEXT("stun rendezvous required peers, reply order, missing peer and rollback"));
}

FNsSelfTestResult NsRunStunChannelMtuSelfTest()
{
	FNsUdpNet Sender;
	FNsUdpNet Receiver;
	if (!Sender.Bind(ENsAddr::C0) || !Receiver.Bind(ENsAddr::C1))
	{
		return StunRegressionResult(false, TEXT("stun-channel-mtu: bind"));
	}
	for (int32 Size : {508, 509, Ns::MaxPacketBytes})
	{
		TArray<uint8> Payload;
		Payload.Init(0xa5, Size);
		if (!Sender.StunSendChannelData(ENsAddr::C0, TEXT("127.0.0.1"),
			Receiver.BoundPort(ENsAddr::C1), NsTurnChannelMin, Payload))
		{
			return StunRegressionResult(false, TEXT("stun-channel-mtu: send"));
		}
		uint16 Channel = 0;
		TArray<uint8> Received;
		bool bReceived = false;
		for (int32 Try = 0; Try < 50 && !bReceived; ++Try)
		{
			bReceived = Receiver.StunRecvChannelData(ENsAddr::C1, Channel, Received);
			if (!bReceived)
			{
				FPlatformProcess::Sleep(0.001f);
			}
		}
		if (!bReceived || Channel != NsTurnChannelMin || Received != Payload)
		{
			return StunRegressionResult(false, TEXT("stun-channel-mtu: payload truncated"));
		}
	}
	TArray<uint8> Oversize;
	Oversize.Init(0, Ns::MaxPacketBytes + 1);
	return StunRegressionResult(!Sender.StunSendChannelData(ENsAddr::C0, TEXT("127.0.0.1"),
		Receiver.BoundPort(ENsAddr::C1), NsTurnChannelMin, Oversize), TEXT("stun channel MTU and oversize rejection"));
}
