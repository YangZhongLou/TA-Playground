// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsLockstepWaitResync.h"
#include "NsLockstepDoor.h"
#include "NsPump.h"

void NsApplyWaitResyncSnap(FNsLockstepWaitClient& Client, const FNsPacket& Packet)
{
	Client.World.X[0] = Packet.SnapX[0];
	Client.World.X[1] = Packet.SnapX[1];
	Client.World.Rng = Packet.SnapRng;
	Client.PrevX[0] = Client.World.X[0];
	Client.PrevX[1] = Client.World.X[1];
	Client.ExecFrame = Packet.Tick;
	Client.Buf.Reset();
}

void NsPumpLockstepWaitResyncServer(INsNet& Net, FNsLockstepWaitServer& Sv, FNsLockstepResync& Resync, bool bWait)
{
	TArray<FNsPacket> ToSv;
	NsDrain(Net, ENsAddr::Sv, ToSv, bWait);
	for (const FNsPacket& P : ToSv)
	{
		if (P.Type == ENsMsg::C2SInput)
		{
			const int32 Id = NsPlayerIdFromAddr(P.Src);
			if (Id < 0 || !Resync.Alive[Id] || P.SeqWindow.Num() == 0 || P.DxWindow.Num() == 0)
			{
				continue;
			}
			Sv.OnInput(Id, P.SeqWindow[0], P.DxWindow[0]);
		}
		else if (P.Type == ENsMsg::C2SChecksum)
		{
			const int32 Id = NsPlayerIdFromAddr(P.Src);
			if (Id < 0 || !Resync.Alive[Id])
			{
				continue;
			}
			if (Sv.bDesync && Resync.bCaptured && !Resync.bGiveUp && !Resync.bResumed
				&& P.Tick == Resync.LiveSnapTick
				&& P.Hash == Resync.LiveSnap.Checksum())
			{
				Resync.Acked[Id] = true;
			}
			else if (Resync.KickIfMismatch(Id, P.Tick, P.Hash, Sv.Checksums))
			{
				Sv.Alive[Id] = false;
				Sv.Slot.Dx[Id] = 0;
				continue;
			}
			else
			{
				Sv.OnChecksum(P.Tick, P.Hash);
			}
		}
	}

	if (Sv.bDesync && Resync.Acked[0] && Resync.Acked[1] && !Resync.bGiveUp)
	{
		Sv.bDesync = false;
		Resync.FinishResume();
		Sv.FrameStartMs = Net.Now;
	}

	if (!Sv.bDesync)
	{
		Sv.Tick(Net);
		return;
	}

	if (!Resync.bCaptured)
	{
		Resync.CaptureLive(Sv.World, Sv.Frame);
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

void NsPumpLockstepWaitResyncClient(
	INsNet& Net, FNsLockstepWaitClient& C, FNsLockstepResyncClient& View, bool bWait, FNsDoorOpen* Door)
{
	TArray<FNsPacket> ToC;
	NsDrain(Net, C.Addr, ToC, bWait);
	bool bApplied = false;
	for (const FNsPacket& P : ToC)
	{
		if (NsIsResyncLiveSnap(P) && P.Tick != View.DoneSnapTick)
		{
			NsApplyWaitResyncSnap(C, P);
			View.HaltTick = P.Tick;
			bApplied = true;
		}
	}
	for (const FNsPacket& P : ToC)
	{
		if (NsIsResyncLiveSnap(P))
		{
			continue;
		}
		if (View.HaltTick >= 0)
		{
			if (NsS2CResumesHalt(P, View.HaltTick))
			{
				View.DoneSnapTick = View.HaltTick;
				View.HaltTick = -1;
				C.OnS2C(P.Frames);
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
	if (View.HaltTick < 0)
	{
		C.Logic(Net);
	}
}
