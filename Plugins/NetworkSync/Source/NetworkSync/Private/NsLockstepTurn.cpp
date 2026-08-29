// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsLockstepTurn.h"
#include "NsPump.h"

namespace
{
bool NsTurnInputsForFrame(int32 LogicFrame, const TMap<int32, FNsInputs>& Cmds, FNsInputs& Out)
{
	const int32 ExecTurn = LogicFrame / NsLockstepTurnFrames;
	const int32 SrcTurn = ExecTurn - NsLockstepTurnLead;
	if (SrcTurn < 0)
	{
		Out = FNsInputs();
		return true;
	}
	if (const FNsInputs* Found = Cmds.Find(SrcTurn))
	{
		Out = *Found;
		return true;
	}
	return false;
}

void NsTurnBroadcast(INsNet& Net, const TMap<int32, FNsInputs>& Cmds, int32 ClosedTurn)
{
	if (ClosedTurn < 0)
	{
		return;
	}
	TMap<int32, FNsInputs> Packed;
	const int32 First = FMath::Max(0, ClosedTurn - 16);
	for (int32 T = First; T <= ClosedTurn; ++T)
	{
		if (const FNsInputs* Found = Cmds.Find(T))
		{
			Packed.Add(T, *Found);
		}
	}
	if (Packed.Num() == 0)
	{
		return;
	}

	FNsPacket Pkt;
	Pkt.Type = ENsMsg::S2CFrame;
	Pkt.Frames = Packed;
	Net.Send(ENsAddr::Sv, ENsAddr::C0, Pkt);
	Net.Send(ENsAddr::Sv, ENsAddr::C1, Pkt);
}

void NsTurnPrune(TMap<int32, FNsInputs>& Cmds, int32 ExecTurn)
{
	const int32 KeepFrom = FMath::Max(0, ExecTurn - 16);
	TArray<int32> Dead;
	for (const TPair<int32, FNsInputs>& Kv : Cmds)
	{
		if (Kv.Key < KeepFrom)
		{
			Dead.Add(Kv.Key);
		}
	}
	for (int32 K : Dead)
	{
		Cmds.Remove(K);
	}
}

bool NsTurnTryStep(int32& LogicFrame, const TMap<int32, FNsInputs>& Cmds, FNsWorld& World,
	int32* PrevX)
{
	FNsInputs I;
	if (!NsTurnInputsForFrame(LogicFrame, Cmds, I))
	{
		return false;
	}
	if (PrevX)
	{
		PrevX[0] = World.X[0];
		PrevX[1] = World.X[1];
	}
	World.Step(I.Dx, Ns::LockstepSpeed);
	++LogicFrame;
	return true;
}
}

void FNsLockstepTurnServer::OnInput(int32 PlayerId, int32 Turn, int8 Dx)
{
	if (PlayerId < 0 || PlayerId >= Ns::PlayerCount || Turn != CollectTurn)
	{
		return;
	}
	Slot.Dx[PlayerId] = NsClampDx(Dx);
	Got[PlayerId] = true;
}

void FNsLockstepTurnServer::Tick(INsNet& Net)
{
	const bool bAll = Got[0] && Got[1];
	const bool bStall = (Net.Now - TurnStartMs) >= NsLockstepTurnStallMs;
	if (bAll || bStall)
	{
		if (bStall)
		{
			if (!Got[0])
			{
				Slot.Dx[0] = 0;
			}
			if (!Got[1])
			{
				Slot.Dx[1] = 0;
			}
		}
		Cmds.Add(CollectTurn, Slot);
		NsTurnBroadcast(Net, Cmds, CollectTurn);
		++CollectTurn;
		Got[0] = false;
		Got[1] = false;
		Slot = FNsInputs();
		TurnStartMs = Net.Now;
	}

	NsTurnTryStep(Frame, Cmds, World, nullptr);
	Resend(Net);
	NsTurnPrune(Cmds, Frame / NsLockstepTurnFrames);
}

void FNsLockstepTurnServer::Resend(INsNet& Net)
{
	NsTurnBroadcast(Net, Cmds, CollectTurn - 1);
}

void FNsLockstepTurnClient::SendInput(INsNet& Net, int8 Dx)
{
	const int8 Clamped = NsClampDx(Dx);
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Pkt.PlayerId = PlayerId;
	Pkt.Dx = Clamped;
	Pkt.SeqWindow.Add(SendTurn);
	Pkt.DxWindow.Add(Clamped);
	Net.Send(Addr, ENsAddr::Sv, Pkt);
}

void FNsLockstepTurnClient::OnS2C(const TMap<int32, FNsInputs>& Turns)
{
	for (const TPair<int32, FNsInputs>& Kv : Turns)
	{
		Cmds.Add(Kv.Key, Kv.Value);
	}
	while (Cmds.Contains(SendTurn))
	{
		++SendTurn;
	}
}

void FNsLockstepTurnClient::Logic()
{
	NsTurnTryStep(ExecFrame, Cmds, World, PrevX);
}

void FNsLockstepTurnClient::CatchUpTo(int32 TargetFrame)
{
	while (ExecFrame < TargetFrame && NsTurnTryStep(ExecFrame, Cmds, World, PrevX))
	{
	}
}

void NsPumpLockstepTurnServer(INsNet& Net, FNsLockstepTurnServer& Sv, bool bWait)
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

void NsPumpLockstepTurnClient(INsNet& Net, FNsLockstepTurnClient& C, bool bWait)
{
	TArray<FNsPacket> ToC;
	NsDrain(Net, C.Addr, ToC, bWait);
	for (const FNsPacket& P : ToC)
	{
		if (P.Type == ENsMsg::S2CFrame)
		{
			C.OnS2C(P.Frames);
		}
	}
	C.Logic();
}
