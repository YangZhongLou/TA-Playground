// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsLockstepWait.h"
#include "NsPump.h"

void FNsLockstepWaitServer::OnInput(int32 PlayerId, int32 Tick, int8 Dx)
{
	if (PlayerId < 0 || PlayerId >= Ns::PlayerCount || Tick != Frame || !Alive[PlayerId])
	{
		return;
	}
	Slot.Dx[PlayerId] = NsClampDx(Dx);
	Got[PlayerId] = true;
}

void FNsLockstepWaitServer::OnChecksum(int32 FrameIndex, uint32 Hash)
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

void FNsLockstepWaitServer::OnNack(INsNet& Net, ENsAddr Dst, const TArray<int32>& Frames)
{
	if (Dst != ENsAddr::C0 && Dst != ENsAddr::C1)
	{
		return;
	}
	TMap<int32, FNsInputs> Packed;
	bool bMiss = false;
	for (int32 F : Frames)
	{
		if (F < 0)
		{
			continue;
		}
		if (const FNsInputs* Found = Hist.Find(F))
		{
			Packed.Add(F, *Found);
		}
		else
		{
			bMiss = true;
			break;
		}
	}
	if (bMiss)
	{
		SendJoin(Net, Dst);
		return;
	}
	if (Packed.Num() == 0)
	{
		return;
	}
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::S2CFrame;
	Pkt.Frames = Packed;
	Net.Send(ENsAddr::Sv, Dst, Pkt);
}

void FNsLockstepWaitServer::Tick(INsNet& Net)
{
	bool bWaiting = false;
	bool bAll = true;
	for (int32 Id = 0; Id < Ns::PlayerCount; ++Id)
	{
		if (!Alive[Id])
		{
			continue;
		}
		bWaiting = true;
		if (!Got[Id])
		{
			bAll = false;
		}
	}
	if (!bWaiting)
	{
		return;
	}
	const bool bStall = (Net.Now - FrameStartMs) >= NsLockstepWaitStallMs;
	if (!bAll && !bStall)
	{
		return;
	}
	for (int32 Id = 0; Id < Ns::PlayerCount; ++Id)
	{
		if (!Alive[Id])
		{
			continue;
		}
		if (Got[Id])
		{
			MissStreak[Id] = 0;
			continue;
		}
		Slot.Dx[Id] = 0;
		++MissStreak[Id];
		if (KickAfterStalls > 0 && MissStreak[Id] >= KickAfterStalls)
		{
			Alive[Id] = false;
		}
	}

	Hist.Add(Frame, Slot);
	World.Step(Slot.Dx, Ns::LockstepSpeed);
	if (Frame % Ns::ChecksumEvery == 0)
	{
		Checksums.Add(Frame, World.Checksum());
	}
	if (Frame > 0 && (Frame % Ns::JoinSnapEvery) == 0)
	{
		SnapFrame = Frame;
		SnapWorld = World;
		const int32 KeepFrom = FMath::Max(0, SnapFrame - Ns::RedundantFrames);
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
			Checksums.Remove(K);
		}
	}

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
	if (Frame > 0 && (Frame % (Ns::RedundantFrames + 1)) == 0)
	{
		SendJoin(Net, ENsAddr::C0);
		SendJoin(Net, ENsAddr::C1);
	}

	Got[0] = false;
	Got[1] = false;
	Slot = FNsInputs();
	++Frame;
	FrameStartMs = Net.Now;
}

void FNsLockstepWaitServer::SendJoin(INsNet& Net, ENsAddr Dst) const
{
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::S2CJoinSnap;
	if (SnapFrame >= 0)
	{
		Pkt.Tick = SnapFrame + 1;
		Pkt.SnapX[0] = SnapWorld.X[0];
		Pkt.SnapX[1] = SnapWorld.X[1];
		Pkt.SnapRng = SnapWorld.Rng;
	}
	else
	{
		Pkt.Tick = 0;
		Pkt.SnapRng = 1;
	}
	for (const TPair<int32, FNsInputs>& Kv : Hist)
	{
		if (Kv.Key >= Pkt.Tick)
		{
			Pkt.Frames.Add(Kv.Key, Kv.Value);
		}
	}
	Net.Send(ENsAddr::Sv, Dst, Pkt);
	Net.Send(ENsAddr::Sv, Dst, Pkt);
}

void FNsLockstepWaitClient::SendInput(INsNet& Net, int8 Dx)
{
	const int8 Clamped = NsClampDx(Dx);
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Pkt.PlayerId = PlayerId;
	Pkt.Dx = Clamped;
	Pkt.SeqWindow.Add(ExecFrame);
	Pkt.DxWindow.Add(Clamped);
	Net.Send(Addr, ENsAddr::Sv, Pkt);
}

void FNsLockstepWaitClient::OnS2C(const TMap<int32, FNsInputs>& Frames)
{
	for (const TPair<int32, FNsInputs>& Kv : Frames)
	{
		if (Kv.Key >= ExecFrame)
		{
			Buf.Add(Kv.Key, Kv.Value);
		}
	}
}

void FNsLockstepWaitClient::ApplyJoin(const FNsPacket& Packet)
{
	if (Packet.Tick > ExecFrame)
	{
		World.X[0] = Packet.SnapX[0];
		World.X[1] = Packet.SnapX[1];
		World.Rng = Packet.SnapRng;
		PrevX[0] = World.X[0];
		PrevX[1] = World.X[1];
		ExecFrame = Packet.Tick;
		TArray<int32> Dead;
		for (const TPair<int32, FNsInputs>& Kv : Buf)
		{
			if (Kv.Key < ExecFrame)
			{
				Dead.Add(Kv.Key);
			}
		}
		for (int32 K : Dead)
		{
			Buf.Remove(K);
		}
	}
	OnS2C(Packet.Frames);
}

void FNsLockstepWaitClient::Logic(INsNet& Net)
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

	int32 Future = ExecFrame;
	bool bHole = false;
	for (const TPair<int32, FNsInputs>& Kv : Buf)
	{
		if (Kv.Key > ExecFrame)
		{
			bHole = true;
			Future = FMath::Max(Future, Kv.Key);
		}
	}
	if (!bHole)
	{
		return;
	}

	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SFrameNack;
	Pkt.PlayerId = PlayerId;
	for (int32 F = ExecFrame; F < Future && Pkt.SeqWindow.Num() < Ns::LockstepNackMax; ++F)
	{
		if (Buf.Contains(F))
		{
			break;
		}
		Pkt.SeqWindow.Add(F);
	}
	if (Pkt.SeqWindow.Num() > 0)
	{
		Net.Send(Addr, ENsAddr::Sv, Pkt);
	}
}

void NsPumpLockstepWaitServer(INsNet& Net, FNsLockstepWaitServer& Sv, bool bWait)
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
		else if (P.Type == ENsMsg::C2SChecksum)
		{
			Sv.OnChecksum(P.Tick, P.Hash);
		}
		else if (P.Type == ENsMsg::C2SFrameNack && !Sv.bDesync)
		{
			Sv.OnNack(Net, P.Src, P.SeqWindow);
		}
	}
	Sv.Tick(Net);
}

void NsPumpLockstepWaitClient(INsNet& Net, FNsLockstepWaitClient& C, bool bWait)
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
