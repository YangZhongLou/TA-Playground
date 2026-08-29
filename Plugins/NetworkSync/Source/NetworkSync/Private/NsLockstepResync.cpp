// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsLockstepResync.h"
#include "NsPump.h"

void FNsLockstepResync::CaptureLive(const FNsLockstepServer& Sv)
{
	LiveSnap = Sv.World;
	LiveSnapTick = Sv.Frame;
	bCaptured = true;
}

void FNsLockstepResync::SendLiveSnap(INsNet& Net, ENsAddr Dst) const
{
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::S2CJoinSnap;
	Pkt.Tick = LiveSnapTick;
	Pkt.SnapX[0] = LiveSnap.X[0];
	Pkt.SnapX[1] = LiveSnap.X[1];
	Pkt.SnapRng = LiveSnap.Rng;
	Net.Send(ENsAddr::Sv, Dst, Pkt);
	Net.Send(ENsAddr::Sv, Dst, Pkt);
}

void NsApplyResyncSnap(FNsLockstepClient& Client, const FNsPacket& Packet)
{
	Client.World.X[0] = Packet.SnapX[0];
	Client.World.X[1] = Packet.SnapX[1];
	Client.World.Rng = Packet.SnapRng;
	Client.PrevX[0] = Client.World.X[0];
	Client.PrevX[1] = Client.World.X[1];
	Client.ExecFrame = Packet.Tick;
	Client.Buf.Reset();
}

void NsPumpLockstepResyncServer(INsNet& Net, FNsLockstepServer& Sv, FNsLockstepResync& Resync, bool bWait)
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
	}

	if (!Sv.bDesync)
	{
		Sv.Tick(Net);
		return;
	}

	if (!Resync.bCaptured)
	{
		Resync.CaptureLive(Sv);
	}

	++Resync.PumpCycles;
	if (Resync.PumpCycles > Ns::ResyncGiveUpPumps)
	{
		Resync.bGiveUp = true;
		return;
	}

	Resync.SendLiveSnap(Net, ENsAddr::C0);
	Resync.SendLiveSnap(Net, ENsAddr::C1);
}

void NsPumpLockstepResyncClient(INsNet& Net, FNsLockstepClient& C, bool bWait)
{
	TArray<FNsPacket> ToC;
	NsDrain(Net, C.Addr, ToC, bWait);
	for (const FNsPacket& P : ToC)
	{
		if (P.Type == ENsMsg::S2CJoinSnap)
		{
			NsApplyResyncSnap(C, P);
		}
	}
}
