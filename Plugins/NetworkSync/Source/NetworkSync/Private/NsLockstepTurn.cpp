// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsLockstepTurn.h"
#include "NsPump.h"

namespace
{
void NsTurnAdvanceCursor(int32 LogicFrame, const TMap<int32, int32>& TurnLen,
	int32 LiveFpt, int32& ExecTurn, int32& ExecTurnStart)
{
	while (LogicFrame >= ExecTurnStart + NsLockstepTurnLen(TurnLen, ExecTurn, LiveFpt))
	{
		ExecTurnStart += NsLockstepTurnLen(TurnLen, ExecTurn, LiveFpt);
		++ExecTurn;
	}
}

bool NsTurnInputsForFrame(int32 ExecTurn, const TMap<int32, FNsInputs>& Cmds, FNsInputs& Out)
{
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

void NsTurnBroadcast(INsNet& Net, const TMap<int32, FNsInputs>& Cmds, int32 ClosedTurn,
	int32 LiveFpt, const TMap<int32, int32>& TurnLen)
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
	Pkt.Tick = LiveFpt;
	Pkt.BaseTick = NsLockstepTurnLen(TurnLen, ClosedTurn, LiveFpt);
	Pkt.Frames = Packed;
	for (const TPair<int32, FNsInputs>& Kv : Packed)
	{
		Pkt.TurnFpt.Add(Kv.Key, NsLockstepTurnLen(TurnLen, Kv.Key, LiveFpt));
	}
	Net.Send(ENsAddr::Sv, ENsAddr::C0, Pkt);
	Net.Send(ENsAddr::Sv, ENsAddr::C1, Pkt);
}

void NsTurnPrune(TMap<int32, FNsInputs>& Cmds, TMap<int32, int32>& TurnLen, int32 ExecTurn)
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
	Dead.Reset();
	for (const TPair<int32, int32>& Kv : TurnLen)
	{
		if (Kv.Key < KeepFrom)
		{
			Dead.Add(Kv.Key);
		}
	}
	for (int32 K : Dead)
	{
		TurnLen.Remove(K);
	}
}

bool NsTurnTryStep(int32& LogicFrame, int32& ExecTurn, int32& ExecTurnStart,
	const TMap<int32, FNsInputs>& Cmds, const TMap<int32, int32>& TurnLen,
	int32 LiveFpt, FNsWorld& World, int32* PrevX)
{
	NsTurnAdvanceCursor(LogicFrame, TurnLen, LiveFpt, ExecTurn, ExecTurnStart);
	if (ExecTurn >= NsLockstepTurnLead && !TurnLen.Contains(ExecTurn))
	{
		return false;
	}
	FNsInputs I;
	if (!NsTurnInputsForFrame(ExecTurn, Cmds, I))
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

void NsTurnAdjustFpt(int32& FramesPerTurn, double WaitedMs)
{
	int32 Needed = NsLockstepTurnFptMin;
	if (WaitedMs >= 1.0)
	{
		Needed = FMath::Clamp(
			FMath::CeilToInt(static_cast<float>(WaitedMs / static_cast<double>(Ns::LogicDtMs))),
			NsLockstepTurnFptMin, NsLockstepTurnFptMax);
	}
	if (Needed > FramesPerTurn)
	{
		FramesPerTurn = Needed;
	}
	else if (Needed < FramesPerTurn)
	{
		FramesPerTurn = FMath::Max(NsLockstepTurnFptMin, FramesPerTurn - 1);
	}
}
}

void FNsLockstepTurnServer::OnInput(int32 PlayerId, int32 Turn, int8 Dx, double NowMs)
{
	if (PlayerId < 0 || PlayerId >= Ns::PlayerCount || Turn != CollectTurn)
	{
		return;
	}
	Slot.Dx[PlayerId] = NsClampDx(Dx);
	Got[PlayerId] = true;
	ArriveMs[PlayerId] = NowMs;
}

void FNsLockstepTurnServer::Tick(INsNet& Net)
{
	NsTurnAdvanceCursor(Frame, TurnLen, FramesPerTurn, ExecTurn, ExecTurnStart);
	if (!TurnLen.Contains(CollectTurn))
	{
		TurnLen.Add(CollectTurn, FramesPerTurn);
	}

	const bool bAll = Got[0] && Got[1];
	const bool bStall = (Net.Now - TurnStartMs) >= NsLockstepTurnStallMs;
	if (CollectTurn <= ExecTurn && (bAll || bStall))
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

		double WaitedMs = Net.Now - TurnStartMs;
		if (bAll)
		{
			WaitedMs = FMath::Max(ArriveMs[0], ArriveMs[1]) - TurnStartMs;
		}
		WaitedMs = FMath::Max(0.0, WaitedMs);

		TurnLen.Add(CollectTurn, FramesPerTurn);
		if (CollectTurn >= NsLockstepTurnLead)
		{
			NsTurnAdjustFpt(FramesPerTurn, WaitedMs);
		}

		Cmds.Add(CollectTurn, Slot);
		NsTurnBroadcast(Net, Cmds, CollectTurn, FramesPerTurn, TurnLen);
		++CollectTurn;
		TurnLen.Add(CollectTurn, FramesPerTurn);
		Got[0] = false;
		Got[1] = false;
		Slot = FNsInputs();
		ArriveMs[0] = Net.Now;
		ArriveMs[1] = Net.Now;
		TurnStartMs = Net.Now;
	}

	NsTurnTryStep(Frame, ExecTurn, ExecTurnStart, Cmds, TurnLen, FramesPerTurn, World, nullptr);
	Resend(Net);
	NsTurnPrune(Cmds, TurnLen, ExecTurn);
}

void FNsLockstepTurnServer::Resend(INsNet& Net)
{
	NsTurnBroadcast(Net, Cmds, CollectTurn - 1, FramesPerTurn, TurnLen);
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

void FNsLockstepTurnClient::OnS2C(const TMap<int32, FNsInputs>& Turns, int32 LiveFpt,
	int32 ClosedLen, const TMap<int32, int32>& Lens)
{
	int32 Closed = -1;
	for (const TPair<int32, FNsInputs>& Kv : Turns)
	{
		Cmds.Add(Kv.Key, Kv.Value);
		Closed = FMath::Max(Closed, Kv.Key);
	}
	for (const TPair<int32, int32>& Kv : Lens)
	{
		if (NsIsTurnFpt(Kv.Value))
		{
			TurnLen.Add(Kv.Key, Kv.Value);
		}
	}
	if (Closed >= 0)
	{
		if (NsIsTurnFpt(ClosedLen))
		{
			TurnLen.Add(Closed, ClosedLen);
		}
		else if (!TurnLen.Contains(Closed))
		{
			TurnLen.Add(Closed, FramesPerTurn);
		}
		if (NsIsTurnFpt(LiveFpt))
		{
			FramesPerTurn = LiveFpt;
			TurnLen.Add(Closed + 1, FramesPerTurn);
		}
	}
	while (Cmds.Contains(SendTurn))
	{
		++SendTurn;
	}
}

void FNsLockstepTurnClient::Logic()
{
	NsTurnTryStep(ExecFrame, ExecTurn, ExecTurnStart,
		Cmds, TurnLen, FramesPerTurn, World, PrevX);
	NsTurnPrune(Cmds, TurnLen, ExecTurn);
}

void FNsLockstepTurnClient::CatchUpTo(int32 TargetFrame)
{
	while (ExecFrame < TargetFrame
		&& NsTurnTryStep(ExecFrame, ExecTurn, ExecTurnStart,
			Cmds, TurnLen, FramesPerTurn, World, PrevX))
	{
		NsTurnPrune(Cmds, TurnLen, ExecTurn);
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
			Sv.OnInput(Id, P.SeqWindow[0], P.DxWindow[0], Net.Now);
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
			C.OnS2C(P.Frames, P.Tick, P.BaseTick, P.TurnFpt);
		}
	}
	C.Logic();
}
