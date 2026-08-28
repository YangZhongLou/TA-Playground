// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsLockstep.h"

void FNsLockstepServer::OnInput(int32 PlayerId, int8 Dx)
{
	if (PlayerId < 0 || PlayerId >= Ns::PlayerCount)
	{
		return;
	}
	Latest.Dx[PlayerId] = NsClampDx(Dx);
}

void FNsLockstepServer::OnChecksum(int32 FrameIndex, uint32 Hash)
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

void FNsLockstepServer::Tick(INsNet& Net)
{
	while (Net.Now >= NextMs)
	{
		Hist.Add(Frame, Latest);
		World.Step(Latest.Dx, Ns::LockstepSpeed);
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
		++Frame;
		NextMs += Ns::LogicDtMs;
	}
}

void FNsLockstepServer::SendJoin(INsNet& Net, ENsAddr Dst) const
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

void FNsLockstepClient::SendInput(INsNet& Net, int8 Dx)
{
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Pkt.PlayerId = PlayerId;
	Pkt.Dx = NsClampDx(Dx);
	Net.Send(Addr, ENsAddr::Sv, Pkt);
}

void FNsLockstepClient::OnS2C(const TMap<int32, FNsInputs>& Frames)
{
	for (const TPair<int32, FNsInputs>& Kv : Frames)
	{
		if (Kv.Key >= ExecFrame)
		{
			Buf.Add(Kv.Key, Kv.Value);
		}
	}
}

void FNsLockstepClient::ApplyJoin(const FNsPacket& Packet)
{
	if (Packet.Tick > ExecFrame)
	{
		World.X[0] = Packet.SnapX[0];
		World.X[1] = Packet.SnapX[1];
		World.Rng = Packet.SnapRng;
		PrevX[0] = World.X[0];
		PrevX[1] = World.X[1];
		ExecFrame = Packet.Tick;
		Buf.Reset();
	}
	OnS2C(Packet.Frames);
}

void FNsLockstepClient::Logic(INsNet& Net)
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
