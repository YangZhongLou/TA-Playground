// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsLockstepResync.h"
#include "NsLockstepDoor.h"
#include "NsPump.h"

void FNsLockstepResync::CaptureLive(const FNsLockstepServer& Sv)
{
	LiveSnap = Sv.World;
	LiveSnapTick = Sv.Frame;
	bCaptured = true;
	bResumed = false;
	Acked[0] = false;
	Acked[1] = false;
	PumpCycles = 0;
	bGiveUp = false;
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

void FNsLockstepResync::Resume(FNsLockstepServer& Sv, INsNet& Net)
{
	Sv.bDesync = false;
	bResumed = true;
	bCaptured = false;
	Acked[0] = false;
	Acked[1] = false;
	PumpCycles = 0;
	Sv.NextMs = Net.Now + Ns::LogicDtMs;
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
			const int32 Id = NsPlayerIdFromAddr(P.Src);
			if (Sv.bDesync && Resync.bCaptured && !Resync.bGiveUp && !Resync.bResumed
				&& Id >= 0 && P.Tick == Resync.LiveSnapTick
				&& P.Hash == Resync.LiveSnap.Checksum())
			{
				Resync.Acked[Id] = true;
			}
			else
			{
				Sv.OnChecksum(P.Tick, P.Hash);
			}
		}
	}

	if (Sv.bDesync && Resync.Acked[0] && Resync.Acked[1] && !Resync.bGiveUp)
	{
		Resync.Resume(Sv, Net);
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

void NsPumpLockstepResyncClient(INsNet& Net, FNsLockstepClient& C, const FNsLockstepResync& Resync, bool bWait, FNsDoorOpen* Door)
{
	TArray<FNsPacket> ToC;
	NsDrain(Net, C.Addr, ToC, bWait);
	const bool bHalt = Resync.bCaptured && !Resync.bResumed;
	bool bApplied = false;
	for (const FNsPacket& P : ToC)
	{
		if (bHalt)
		{
			if (P.Type == ENsMsg::S2CJoinSnap
				&& P.Tick == Resync.LiveSnapTick
				&& P.Frames.Num() == 0)
			{
				NsApplyResyncSnap(C, P);
				bApplied = true;
			}
			else if (Door)
			{
				NsApplyDoorOpen(*Door, P);
			}
			continue;
		}
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
	if (bApplied)
	{
		FNsPacket Pkt;
		Pkt.Type = ENsMsg::C2SChecksum;
		Pkt.PlayerId = C.PlayerId;
		Pkt.Tick = C.ExecFrame;
		Pkt.Hash = C.World.Checksum();
		Net.Send(C.Addr, ENsAddr::Sv, Pkt);
	}
	if (!bHalt)
	{
		C.Logic(Net);
	}
}
