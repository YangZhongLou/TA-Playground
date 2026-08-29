// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsSelfTest.h"
#include "NsFakeNet.h"
#include "NsLockstepWait.h"

static FNsSelfTestResult WaitFail(const TCHAR* Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult WaitFailStr(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult WaitOk(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = true;
	R.Detail = Msg;
	return R;
}

static void WaitInit(FNsLockstepWaitClient& C0, FNsLockstepWaitClient& C1)
{
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;
}

static void WaitPump(FNsFakeNet& Net, FNsLockstepWaitServer& Sv,
	FNsLockstepWaitClient& C0, FNsLockstepWaitClient& C1)
{
	NsPumpLockstepWaitServer(Net, Sv);
	NsPumpLockstepWaitClient(Net, C0);
	NsPumpLockstepWaitClient(Net, C1);
}

FNsSelfTestResult NsRunLockstepWaitCleanSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepWaitServer Sv;
	FNsLockstepWaitClient C0;
	FNsLockstepWaitClient C1;
	WaitInit(C0, C1);

	const int32 Steps = 40;
	for (int32 S = 0; S < Steps; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		WaitPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}

	if (Sv.Frame != Steps || C0.ExecFrame != Steps || C1.ExecFrame != Steps)
	{
		return WaitFailStr(FString::Printf(
			TEXT("lockstep-wait-clean: frame sv=%d c0=%d c1=%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return WaitFail(TEXT("lockstep-wait-clean: worlds"));
	}
	if (C0.World.Checksum() != C1.World.Checksum())
	{
		return WaitFail(TEXT("lockstep-wait-clean: checksum"));
	}
	return WaitOk(TEXT("lockstep-wait-clean"));
}

FNsSelfTestResult NsRunLockstepWaitStallSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepWaitServer Sv;
	FNsLockstepWaitClient C0;
	FNsLockstepWaitClient C1;
	WaitInit(C0, C1);

	for (int32 S = 0; S < 5; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		WaitPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}

	const int32 Held = Sv.Frame;
	const double StallFrom = Sv.FrameStartMs;
	while (Net.Now < StallFrom + NsLockstepWaitStallMs)
	{
		C0.SendInput(Net, 1);
		WaitPump(Net, Sv, C0, C1);
		if (Sv.Frame != Held)
		{
			return WaitFailStr(FString::Printf(
				TEXT("lockstep-wait-stall: stepped early now=%.0f frame=%d"),
				Net.Now, Sv.Frame));
		}
		Net.Advance(1.0);
	}

	C0.SendInput(Net, 1);
	WaitPump(Net, Sv, C0, C1);
	if (Sv.Frame != Held + 1)
	{
		return WaitFailStr(FString::Printf(
			TEXT("lockstep-wait-stall: expected %d got %d"), Held + 1, Sv.Frame));
	}
	return WaitOk(TEXT("lockstep-wait-stall"));
}

FNsSelfTestResult NsRunLockstepWaitDropSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.1f;
	Net.RttMs = 80.f;
	Net.JitterMs = 8.f;
	Net.Rng.Initialize(1);
	FNsLockstepWaitServer Sv;
	FNsLockstepWaitClient C0;
	FNsLockstepWaitClient C1;
	WaitInit(C0, C1);

	const int32 Frames = 90;
	const double End = Frames * Ns::LogicDtMs;
	while (Net.Now < End)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		WaitPump(Net, Sv, C0, C1);
		Net.Advance(1.0);
	}

	for (int32 i = 0; i < 800; ++i)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		WaitPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	for (int32 i = 0; i < 16; ++i)
	{
		WaitPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	WaitPump(Net, Sv, C0, C1);

	if (C0.ExecFrame <= 10 || C1.ExecFrame != C0.ExecFrame)
	{
		return WaitFailStr(FString::Printf(
			TEXT("lockstep-wait-drop: frames c0=%d c1=%d"), C0.ExecFrame, C1.ExecFrame));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return WaitFail(TEXT("lockstep-wait-drop: diverged"));
	}
	return WaitOk(FString::Printf(TEXT("lockstep-wait-drop frames=%d"), C0.ExecFrame));
}
