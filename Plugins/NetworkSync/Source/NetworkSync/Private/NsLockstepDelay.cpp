// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsLockstepDelay.h"
#include "NsPump.h"

namespace
{
void NsDelayBroadcast(INsNet& Net, const TMap<int32, FNsInputs>& Hist, int32 Frame)
{
	TMap<int32, FNsInputs> Packed;
	const int32 First = FMath::Max(0, Frame - Ns::RedundantFrames);
	for (int32 F = First; F <= Frame; ++F)
	{
		if (const FNsInputs* Found = Hist.Find(F))
		{
			Packed.Add(F, *Found);
		}
	}

	FNsPacket Pkt;
	Pkt.Type = ENsMsg::S2CFrame;
	Pkt.Frames = Packed;
	Net.Send(ENsAddr::Sv, ENsAddr::C0, Pkt);
	Net.Send(ENsAddr::Sv, ENsAddr::C1, Pkt);
}

void NsDelayPruneHist(TMap<int32, FNsInputs>& Hist, int32 Frame)
{
	const int32 KeepFrom = FMath::Max(0, Frame - Ns::RedundantFrames);
	TArray<int32> Dead;
	for (const TPair<int32, FNsInputs>& Kv : Hist)
	{
		if (Kv.Key < KeepFrom)
		{
			Dead.Add(Kv.Key);
		}
	}
	for (int32 K : Dead)
	{
		Hist.Remove(K);
	}
}

void NsDelaySendSnapshot(INsNet& Net, const FNsLockstepDelayServer& Sv)
{
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::S2CJoinSnap;
	Pkt.Tick = Sv.Frame + 1;
	Pkt.SnapX[0] = Sv.World.X[0];
	Pkt.SnapX[1] = Sv.World.X[1];
	Pkt.SnapRng = Sv.World.Rng;
	if (const FNsInputs* Found = Sv.Hist.Find(Sv.Frame))
	{
		Pkt.Frames.Add(Sv.Frame, *Found);
	}
	Net.Send(ENsAddr::Sv, ENsAddr::C0, Pkt);
	Net.Send(ENsAddr::Sv, ENsAddr::C1, Pkt);
}

void NsDelayFinishFrame(FNsLockstepDelayServer& Sv, INsNet& Net, const FNsInputs& Slot)
{
	Sv.Hist.Add(Sv.Frame, Slot);
	Sv.World.Step(Slot.Dx, Ns::LockstepSpeed);
	if (Sv.Frame % Ns::ChecksumEvery == 0)
	{
		Sv.Checksums.Add(Sv.Frame, Sv.World.Checksum());
	}
	NsDelayBroadcast(Net, Sv.Hist, Sv.Frame);
	if ((Sv.Frame + 1) % (Ns::RedundantFrames + 1) == 0)
	{
		NsDelaySendSnapshot(Net, Sv);
	}
	NsDelayPruneHist(Sv.Hist, Sv.Frame);
	Sv.Inbox.Remove(Sv.Frame);
	++Sv.Frame;
	Sv.FrameStartMs = Net.Now;
}
}

void FNsLockstepDelayServer::OnInput(int32 PlayerId, int32 Tick, int8 Dx)
{
	if (PlayerId < 0 || PlayerId >= Ns::PlayerCount || Tick < Frame)
	{
		return;
	}
	if (Tick < NsLockstepDelayFrames)
	{
		return;
	}
	FNsDelayInbox& Entry = Inbox.FindOrAdd(Tick);
	Entry.Slot.Dx[PlayerId] = NsClampDx(Dx);
	Entry.Got[PlayerId] = true;
}

void FNsLockstepDelayServer::OnChecksum(int32 FrameIndex, uint32 Hash)
{
	if (const uint32* Found = Checksums.Find(FrameIndex))
	{
		if (*Found == Hash)
		{
			++ChecksumOk;
		}
		else
		{
			bDesync = true;
		}
	}
}

void FNsLockstepDelayServer::Tick(INsNet& Net)
{
	if (Frame < NsLockstepDelayFrames)
	{
		NsDelayFinishFrame(*this, Net, FNsInputs());
		return;
	}

	FNsDelayInbox* Entry = Inbox.Find(Frame);
	const bool bAll = Entry && Entry->Got[0] && Entry->Got[1];
	const bool bStall = (Net.Now - FrameStartMs) >= NsLockstepDelayStallMs;
	if (!bAll && !bStall)
	{
		++WaitTicks;
		return;
	}

	FNsInputs Slot = Entry ? Entry->Slot : FNsInputs();
	if (bStall)
	{
		const bool bMiss0 = !Entry || !Entry->Got[0];
		const bool bMiss1 = !Entry || !Entry->Got[1];
		if (bMiss0)
		{
			Slot.Dx[0] = 0;
		}
		if (bMiss1)
		{
			Slot.Dx[1] = 0;
		}
		if (bMiss0 || bMiss1)
		{
			++StallFills;
		}
	}

	NsDelayFinishFrame(*this, Net, Slot);
}

void FNsLockstepDelayClient::SendInput(INsNet& Net, int8 Dx)
{
	const int8 Clamped = NsClampDx(Dx);
	const int32 Seq = (KnownFrame < 0)
		? NsLockstepDelayFrames
		: KnownFrame + NsLockstepDelayFrames;
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Pkt.PlayerId = PlayerId;
	Pkt.Dx = Clamped;
	Pkt.SeqWindow.Add(Seq);
	Pkt.DxWindow.Add(Clamped);
	Net.Send(Addr, ENsAddr::Sv, Pkt);
}

void FNsLockstepDelayClient::OnS2C(const TMap<int32, FNsInputs>& Frames)
{
	for (const TPair<int32, FNsInputs>& Kv : Frames)
	{
		if (Kv.Key >= ExecFrame)
		{
			Buf.Add(Kv.Key, Kv.Value);
		}
		if (Kv.Key > KnownFrame)
		{
			KnownFrame = Kv.Key;
		}
	}
}

void FNsLockstepDelayClient::ApplyJoin(const FNsPacket& Packet)
{
	if (Packet.Tick <= ExecFrame)
	{
		return;
	}
	World.X[0] = Packet.SnapX[0];
	World.X[1] = Packet.SnapX[1];
	World.Rng = Packet.SnapRng;
	PrevX[0] = World.X[0];
	PrevX[1] = World.X[1];
	ExecFrame = Packet.Tick;
	KnownFrame = FMath::Max(KnownFrame, ExecFrame - 1);
	TArray<int32> Dead;
	for (const TPair<int32, FNsInputs>& Kv : Buf)
	{
		if (Kv.Key < ExecFrame)
		{
			Dead.Add(Kv.Key);
		}
	}
	for (int32 Frame : Dead)
	{
		Buf.Remove(Frame);
	}
}

void FNsLockstepDelayClient::Logic(INsNet& Net)
{
	while (const FNsInputs* Found = Buf.Find(ExecFrame))
	{
		PrevX[0] = World.X[0];
		PrevX[1] = World.X[1];
		World.Step(Found->Dx, Ns::LockstepSpeed);
		if (ExecFrame % Ns::ChecksumEvery == 0)
		{
			FNsPacket Pkt;
			Pkt.Type = ENsMsg::C2SChecksum;
			Pkt.PlayerId = PlayerId;
			Pkt.Tick = ExecFrame;
			Pkt.Hash = World.Checksum();
			Net.Send(Addr, ENsAddr::Sv, Pkt);
		}
		Buf.Remove(ExecFrame);
		++ExecFrame;
	}
}

void NsPumpLockstepDelayServer(INsNet& Net, FNsLockstepDelayServer& Sv, bool bWait)
{
	TArray<FNsPacket> ToSv;
	NsDrain(Net, ENsAddr::Sv, ToSv, bWait);
	for (const FNsPacket& P : ToSv)
	{
		if (P.Type == ENsMsg::C2SInput)
		{
			const int32 Id = NsPlayerIdFromAddr(P.Src);
			if (Id < 0 || P.SeqWindow.Num() == 0 || P.DxWindow.Num() == 0)
			{
				continue;
			}
			Sv.OnInput(Id, P.SeqWindow[0], P.DxWindow[0]);
		}
	}
	Sv.Tick(Net);
}

void NsPumpLockstepDelayClient(INsNet& Net, FNsLockstepDelayClient& C, bool bWait)
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
	}
	C.Logic(Net);
}
