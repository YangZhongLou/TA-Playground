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

void FNsLockstepServer::Tick(FNsFakeNet& Net)
{
	while (Net.Now >= NextMs)
	{
		Hist.Add(Frame, Latest);
		World.Step(Latest.Dx, Ns::LockstepSpeed);
		if (Frame % Ns::ChecksumEvery == 0)
		{
			Checksums.Add(Frame, World.Checksum());
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

void FNsLockstepClient::SendInput(FNsFakeNet& Net, int8 Dx)
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
		Buf.Add(Kv.Key, Kv.Value);
	}
}

void FNsLockstepClient::Logic(FNsFakeNet& Net)
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
