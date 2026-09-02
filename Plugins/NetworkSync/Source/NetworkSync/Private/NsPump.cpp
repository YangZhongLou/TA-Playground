// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsPump.h"
#include "NsLockstepDoor.h"
#include "HAL/PlatformProcess.h"

namespace
{
bool NsRouteAllowed(const FNsPacket& Packet)
{
	const bool bFromClient = Packet.Src == ENsAddr::C0 || Packet.Src == ENsAddr::C1;
	const bool bToClient = Packet.Dst == ENsAddr::C0 || Packet.Dst == ENsAddr::C1;
	switch (Packet.Type)
	{
	case ENsMsg::C2SInput:
	case ENsMsg::C2SSnapAck:
	case ENsMsg::C2SChecksum:
	case ENsMsg::C2SFrameNack:
		return bFromClient && Packet.Dst == ENsAddr::Sv;
	case ENsMsg::S2CFrame:
	case ENsMsg::S2CSnapshot:
	case ENsMsg::S2CJoinSnap:
	case ENsMsg::S2CDoorOpen:
		return Packet.Src == ENsAddr::Sv && bToClient;
	case ENsMsg::P2PInput:
		return (Packet.Src == ENsAddr::C0 && Packet.Dst == ENsAddr::C1)
			|| (Packet.Src == ENsAddr::C1 && Packet.Dst == ENsAddr::C0);
	default:
		return false;
	}
}

void NsAppendAllowed(ENsAddr Dst, const TArray<FNsPacket>& Batch, TArray<FNsPacket>& Out)
{
	for (const FNsPacket& Packet : Batch)
	{
		if (Packet.Dst == Dst && NsRouteAllowed(Packet))
		{
			Out.Add(Packet);
		}
	}
}
}

void NsDrain(INsNet& Net, ENsAddr Dst, TArray<FNsPacket>& Out, bool bWait)
{
	Out.Reset();
	const int32 Tries = bWait ? 30 : 1;
	for (int32 i = 0; i < Tries; ++i)
	{
		TArray<FNsPacket> Batch;
		Net.Drain(Dst, Batch);
		NsAppendAllowed(Dst, Batch, Out);
		if (Out.Num() > 0)
		{
			for (int32 j = 0; j < 8; ++j)
			{
				Net.Drain(Dst, Batch);
				if (Batch.Num() == 0)
				{
					break;
				}
				NsAppendAllowed(Dst, Batch, Out);
			}
			return;
		}
		if (bWait)
		{
			FPlatformProcess::Sleep(0.001f);
		}
	}
}

void NsPumpLockstepServer(INsNet& Net, FNsLockstepServer& Sv, bool bWait)
{
	TArray<FNsPacket> ToSv;
	NsDrain(Net, ENsAddr::Sv, ToSv, bWait);
	for (const FNsPacket& P : ToSv)
	{
		if (P.Type == ENsMsg::C2SInput)
		{
			const int32 Id = NsPlayerIdFromAddr(P.Src);
			if (Id >= 0)
			{
				Sv.OnInput(Id, P.Dx);
			}
		}
		else if (P.Type == ENsMsg::C2SChecksum)
		{
			Sv.OnChecksum(P.Tick, P.Hash);
		}
		else if (P.Type == ENsMsg::C2SFrameNack)
		{
			Sv.OnNack(Net, P.Src, P.SeqWindow);
		}
	}
	Sv.Tick(Net);
}

void NsPumpLockstepClient(INsNet& Net, FNsLockstepClient& C, bool bWait, FNsDoorOpen* Door)
{
	TArray<FNsPacket> ToC;
	NsDrain(Net, C.Addr, ToC, bWait);
	for (const FNsPacket& P : ToC)
	{
		if (P.Type == ENsMsg::S2CJoinSnap)
		{
			C.ApplyJoin(P);
		}
		else if (P.Type == ENsMsg::S2CFrame)
		{
			C.OnS2C(P.Frames);
		}
		else if (Door)
		{
			NsApplyDoorOpen(*Door, P);
		}
	}
	C.Logic(Net);
}

void NsPumpStateServer(INsNet& Net, FNsStateSyncServer& Sv, bool bWait)
{
	TArray<FNsPacket> ToSv;
	NsDrain(Net, ENsAddr::Sv, ToSv, bWait);
	for (const FNsPacket& P : ToSv)
	{
		if (P.Type == ENsMsg::C2SInput)
		{
			const int32 Id = NsPlayerIdFromAddr(P.Src);
			if (Id >= 0)
			{
				for (int32 i = 0; i < P.SeqWindow.Num(); ++i)
				{
					Sv.OnInput(Id, P.SeqWindow[i], P.DxWindow[i]);
				}
			}
		}
		else if (P.Type == ENsMsg::C2SSnapAck)
		{
			const int32 Id = NsPlayerIdFromAddr(P.Src);
			if (Id >= 0)
			{
				Sv.OnAck(Id, P.Tick);
			}
		}
	}
	Sv.Sim(Net);
}

void NsPumpStateClient(INsNet& Net, FNsStateSyncClient& C, bool bWait)
{
	TArray<FNsPacket> ToC;
	NsDrain(Net, C.Addr, ToC, bWait);
	for (const FNsPacket& P : ToC)
	{
		if (P.Type == ENsMsg::S2CSnapshot)
		{
			C.OnSnap(Net, P);
		}
	}
	C.UpdateRemoteDraw(Net.Now);
}

void NsPumpRollbackPeer(INsNet& Net, FNsRollbackPeer& Peer, bool bWait)
{
	TArray<FNsPacket> ToPeer;
	NsDrain(Net, Peer.Addr, ToPeer, bWait);
	for (const FNsPacket& P : ToPeer)
	{
		if (P.Type == ENsMsg::P2PInput)
		{
			Peer.OnRemote(P.RemoteDx);
		}
	}
}
