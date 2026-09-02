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
	if (Sv.ChecksumOk <= 0)
	{
		return WaitFail(TEXT("lockstep-wait-clean: no checksum ack"));
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
	const int32 SilentX = -5 * Ns::LockstepSpeed;
	if (C0.World.X[1] != SilentX || C1.World.X[1] != SilentX || Sv.World.X[1] != SilentX)
	{
		return WaitFailStr(FString::Printf(
			TEXT("lockstep-wait-stall: fill-0 x1=%d (Latest would be %d)"),
			C0.World.X[1], SilentX - Ns::LockstepSpeed));
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

FNsSelfTestResult NsRunLockstepWaitJoinSelfTest()
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
	if (C1.ExecFrame != 5)
	{
		return WaitFailStr(FString::Printf(
			TEXT("lockstep-wait-join: warmup exec=%d"), C1.ExecFrame));
	}

	for (int32 S = 0; S < 20; ++S)
	{
		C0.SendInput(Net, 1);
		Net.Advance(static_cast<double>(NsLockstepWaitStallMs));
		NsPumpLockstepWaitServer(Net, Sv);
		NsPumpLockstepWaitClient(Net, C0);
		TArray<FNsPacket> Dropped;
		Net.Drain(ENsAddr::C1, Dropped);
	}
	if (C1.ExecFrame != 5)
	{
		return WaitFailStr(FString::Printf(
			TEXT("lockstep-wait-join: silent client moved exec=%d"), C1.ExecFrame));
	}
	if (Sv.Frame < 5 + Ns::RedundantFrames + 2)
	{
		return WaitFailStr(FString::Printf(
			TEXT("lockstep-wait-join: server did not pull ahead frame=%d"), Sv.Frame));
	}

	C0.SendInput(Net, 1);
	NsPumpLockstepWaitServer(Net, Sv);
	NsPumpLockstepWaitClient(Net, C0);
	NsPumpLockstepWaitClient(Net, C1);
	if (C1.ExecFrame != 5)
	{
		return WaitFailStr(FString::Printf(
			TEXT("lockstep-wait-join: redundant window caught up exec=%d"), C1.ExecFrame));
	}

	Sv.SendJoin(Net, C1.Addr);
	NsPumpLockstepWaitClient(Net, C1);
	if (C1.ExecFrame != Sv.Frame)
	{
		return WaitFailStr(FString::Printf(
			TEXT("lockstep-wait-join: after snap sv=%d c1=%d"), Sv.Frame, C1.ExecFrame));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return WaitFail(TEXT("lockstep-wait-join: worlds after snap"));
	}

	for (int32 S = 0; S < 8; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		WaitPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	if (C1.ExecFrame != Sv.Frame || C0.ExecFrame != Sv.Frame)
	{
		return WaitFailStr(FString::Printf(
			TEXT("lockstep-wait-join: resume sv=%d c0=%d c1=%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return WaitFail(TEXT("lockstep-wait-join: worlds after resume"));
	}
	return WaitOk(FString::Printf(TEXT("lockstep-wait-join frame=%d"), Sv.Frame));
}

FNsSelfTestResult NsRunLockstepWaitKickSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepWaitServer Sv;
	FNsLockstepWaitClient C0;
	FNsLockstepWaitClient C1;
	WaitInit(C0, C1);
	Sv.KickAfterStalls = 2;

	for (int32 S = 0; S < 5; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		WaitPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}

	const int32 SilentX = C0.World.X[1];
	int32 Held = Sv.Frame;
	double StallFrom = Sv.FrameStartMs;
	while (Net.Now < StallFrom + NsLockstepWaitStallMs)
	{
		C0.SendInput(Net, 1);
		WaitPump(Net, Sv, C0, C1);
		if (Sv.Frame != Held)
		{
			return WaitFail(TEXT("lockstep-wait-kick: first stall stepped early"));
		}
		Net.Advance(1.0);
	}
	C0.SendInput(Net, 1);
	WaitPump(Net, Sv, C0, C1);
	if (Sv.Frame != Held + 1 || !Sv.Alive[1] || C0.World.X[1] != SilentX)
	{
		return WaitFailStr(FString::Printf(
			TEXT("lockstep-wait-kick: first stall frame=%d alive=%d x1=%d"),
			Sv.Frame, Sv.Alive[1] ? 1 : 0, C0.World.X[1]));
	}

	Held = Sv.Frame;
	StallFrom = Sv.FrameStartMs;
	while (Net.Now < StallFrom + NsLockstepWaitStallMs)
	{
		C0.SendInput(Net, 1);
		WaitPump(Net, Sv, C0, C1);
		if (Sv.Frame != Held)
		{
			return WaitFail(TEXT("lockstep-wait-kick: second stall stepped early"));
		}
		Net.Advance(1.0);
	}
	C0.SendInput(Net, 1);
	WaitPump(Net, Sv, C0, C1);
	if (Sv.Frame != Held + 1 || Sv.Alive[1] || C0.World.X[1] != SilentX)
	{
		return WaitFailStr(FString::Printf(
			TEXT("lockstep-wait-kick: after two stalls frame=%d alive=%d x1=%d"),
			Sv.Frame, Sv.Alive[1] ? 1 : 0, C0.World.X[1]));
	}

	Held = Sv.Frame;
	const int32 X0 = C0.World.X[0];
	C0.SendInput(Net, 1);
	WaitPump(Net, Sv, C0, C1);
	if (Sv.Frame != Held + 1)
	{
		return WaitFail(TEXT("lockstep-wait-kick: still waiting for kicked slot"));
	}
	if (C0.World.X[0] == X0 || C0.World.X[1] != SilentX)
	{
		return WaitFailStr(FString::Printf(
			TEXT("lockstep-wait-kick: post-kick x0=%d was=%d x1=%d"),
			C0.World.X[0], X0, C0.World.X[1]));
	}

	C1.SendInput(Net, 1);
	C0.SendInput(Net, 1);
	WaitPump(Net, Sv, C0, C1);
	if (C0.World.X[1] != SilentX || C1.World.X[1] != SilentX || Sv.World.X[1] != SilentX)
	{
		return WaitFail(TEXT("lockstep-wait-kick: kicked input wrote x1"));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return WaitFail(TEXT("lockstep-wait-kick: worlds"));
	}
	return WaitOk(TEXT("lockstep-wait-kick"));
}

FNsSelfTestResult NsRunLockstepWaitKickResumeSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepWaitServer Sv;
	FNsLockstepWaitClient C0;
	FNsLockstepWaitClient C1;
	WaitInit(C0, C1);
	Sv.KickAfterStalls = 2;

	for (int32 S = 0; S < 5; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		WaitPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}

	int32 Held = Sv.Frame;
	double StallFrom = Sv.FrameStartMs;
	while (Net.Now < StallFrom + NsLockstepWaitStallMs)
	{
		C0.SendInput(Net, 1);
		WaitPump(Net, Sv, C0, C1);
		if (Sv.Frame != Held)
		{
			return WaitFail(TEXT("lockstep-wait-kick-resume: stall stepped early"));
		}
		Net.Advance(1.0);
	}
	C0.SendInput(Net, 1);
	WaitPump(Net, Sv, C0, C1);
	if (!Sv.Alive[1] || Sv.MissStreak[1] != 1)
	{
		return WaitFail(TEXT("lockstep-wait-kick-resume: expected one miss"));
	}

	for (int32 S = 0; S < 3; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		WaitPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	if (!Sv.Alive[1] || Sv.MissStreak[1] != 0)
	{
		return WaitFail(TEXT("lockstep-wait-kick-resume: streak not cleared"));
	}

	Held = Sv.Frame;
	C0.SendInput(Net, 1);
	WaitPump(Net, Sv, C0, C1);
	if (Sv.Frame != Held)
	{
		return WaitFail(TEXT("lockstep-wait-kick-resume: stepped without C1"));
	}

	StallFrom = Sv.FrameStartMs;
	while (Net.Now < StallFrom + NsLockstepWaitStallMs)
	{
		C0.SendInput(Net, 1);
		WaitPump(Net, Sv, C0, C1);
		if (Sv.Frame != Held)
		{
			return WaitFail(TEXT("lockstep-wait-kick-resume: still waiting stepped early"));
		}
		Net.Advance(1.0);
	}
	C0.SendInput(Net, 1);
	WaitPump(Net, Sv, C0, C1);
	if (Sv.Frame != Held + 1 || !Sv.Alive[1])
	{
		return WaitFailStr(FString::Printf(
			TEXT("lockstep-wait-kick-resume: after return stall frame=%d alive=%d"),
			Sv.Frame, Sv.Alive[1] ? 1 : 0));
	}
	return WaitOk(TEXT("lockstep-wait-kick-resume"));
}
