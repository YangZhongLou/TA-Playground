// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsSelfTest.h"
#include "NsFakeNet.h"
#include "NsLockstepTurn.h"
#include "NsPump.h"

static FNsSelfTestResult TurnFail(const TCHAR* Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult TurnFailStr(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult TurnOk(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = true;
	R.Detail = Msg;
	return R;
}

static void TurnInit(FNsLockstepTurnClient& C0, FNsLockstepTurnClient& C1)
{
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;
}

static void TurnPump(FNsFakeNet& Net, FNsLockstepTurnServer& Sv,
	FNsLockstepTurnClient& C0, FNsLockstepTurnClient& C1)
{
	NsPumpLockstepTurnServer(Net, Sv);
	NsPumpLockstepTurnClient(Net, C0);
	NsPumpLockstepTurnClient(Net, C1);
}

static void TurnReceive(FNsFakeNet& Net, FNsLockstepTurnClient& C)
{
	TArray<FNsPacket> ToC;
	NsDrain(Net, C.Addr, ToC, false);
	for (const FNsPacket& P : ToC)
	{
		if (P.Type == ENsMsg::S2CFrame)
		{
			C.OnS2C(P.Frames);
		}
	}
}

static void TurnCatchUp(FNsFakeNet& Net, FNsLockstepTurnServer& Sv,
	FNsLockstepTurnClient& C0, FNsLockstepTurnClient& C1)
{
	for (int32 i = 0; i < 8; ++i)
	{
		Sv.Resend(Net);
		Net.Advance(Net.RttMs + Net.JitterMs + 1.0);
		TurnReceive(Net, C0);
		C0.CatchUpTo(Sv.Frame);
		TurnReceive(Net, C1);
		C1.CatchUpTo(Sv.Frame);
	}
}

FNsSelfTestResult NsRunLockstepTurnCleanSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepTurnServer Sv;
	FNsLockstepTurnClient C0;
	FNsLockstepTurnClient C1;
	TurnInit(C0, C1);

	bool bSawPreApply = false;
	bool bSawApply = false;
	const int32 ApplyFrame = (0 + NsLockstepTurnLead) * NsLockstepTurnFrames;
	const int32 Steps = 40;
	for (int32 S = 0; S < Steps; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		TurnPump(Net, Sv, C0, C1);
		if (C0.ExecFrame == ApplyFrame)
		{
			if (C0.World.X[0] != 0 || C0.World.X[1] != 0)
			{
				return TurnFail(TEXT("lockstep-turn-clean: x moved before lead"));
			}
			bSawPreApply = true;
		}
		if (C0.ExecFrame == ApplyFrame + 1)
		{
			if (C0.World.X[0] != Ns::LockstepSpeed || C0.World.X[1] != -Ns::LockstepSpeed)
			{
				return TurnFailStr(FString::Printf(
					TEXT("lockstep-turn-clean: apply x0=%d x1=%d"),
					C0.World.X[0], C0.World.X[1]));
			}
			bSawApply = true;
		}
		Net.Advance(Ns::LogicDtMs);
	}

	if (!bSawPreApply || !bSawApply)
	{
		return TurnFailStr(FString::Printf(
			TEXT("lockstep-turn-clean: missed lead check pre=%d apply=%d exec=%d"),
			bSawPreApply ? 1 : 0, bSawApply ? 1 : 0, C0.ExecFrame));
	}
	if (Sv.Frame != Steps || C0.ExecFrame != Steps || C1.ExecFrame != Steps)
	{
		return TurnFailStr(FString::Printf(
			TEXT("lockstep-turn-clean: frame sv=%d c0=%d c1=%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return TurnFail(TEXT("lockstep-turn-clean: worlds"));
	}
	return TurnOk(TEXT("lockstep-turn-clean"));
}

FNsSelfTestResult NsRunLockstepTurnLateSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepTurnServer Sv;
	FNsLockstepTurnClient C0;
	FNsLockstepTurnClient C1;
	TurnInit(C0, C1);

	const int32 Boundary = (0 + NsLockstepTurnLead) * NsLockstepTurnFrames;
	for (int32 S = 0; S < Boundary + 1; ++S)
	{
		C0.SendInput(Net, 1);
		TurnPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}

	if (C0.ExecFrame != Boundary || C1.ExecFrame != Boundary || Sv.Frame != Boundary)
	{
		return TurnFailStr(FString::Printf(
			TEXT("lockstep-turn-late: expected exec=%d got sv=%d c0=%d c1=%d"),
			Boundary, Sv.Frame, C0.ExecFrame, C1.ExecFrame));
	}
	if (C0.World.X[0] != 0 || C0.World.X[1] != 0 || Sv.CollectTurn != 0)
	{
		return TurnFailStr(FString::Printf(
			TEXT("lockstep-turn-late: x0=%d collect=%d now=%.0f"),
			C0.World.X[0], Sv.CollectTurn, Net.Now));
	}
	return TurnOk(TEXT("lockstep-turn-late"));
}

FNsSelfTestResult NsRunLockstepTurnDropSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.1f;
	Net.RttMs = 80.f;
	Net.JitterMs = 8.f;
	Net.Rng.Initialize(1);
	FNsLockstepTurnServer Sv;
	FNsLockstepTurnClient C0;
	FNsLockstepTurnClient C1;
	TurnInit(C0, C1);

	const int32 Frames = 90;
	const double End = Frames * Ns::LogicDtMs;
	while (Net.Now < End)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		TurnPump(Net, Sv, C0, C1);
		Net.Advance(1.0);
	}

	for (int32 i = 0; i < 800; ++i)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		TurnPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	for (int32 i = 0; i < 32; ++i)
	{
		TurnPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	TurnPump(Net, Sv, C0, C1);
	TurnCatchUp(Net, Sv, C0, C1);

	if (C0.ExecFrame <= 10 || C1.ExecFrame != C0.ExecFrame)
	{
		return TurnFailStr(FString::Printf(
			TEXT("lockstep-turn-drop: frames sv=%d c0=%d c1=%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return TurnFailStr(FString::Printf(
			TEXT("lockstep-turn-drop: diverged svF=%d c0F=%d c1F=%d x sv=%d/%d c0=%d/%d c1=%d/%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame,
			Sv.World.X[0], Sv.World.X[1],
			C0.World.X[0], C0.World.X[1],
			C1.World.X[0], C1.World.X[1]));
	}
	return TurnOk(FString::Printf(TEXT("lockstep-turn-drop frames=%d"), C0.ExecFrame));
}
