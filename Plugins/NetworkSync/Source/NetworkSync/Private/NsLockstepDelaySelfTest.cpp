// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsSelfTest.h"
#include "NsFakeNet.h"
#include "NsLockstepDelay.h"

static FNsSelfTestResult DelayFail(const TCHAR* Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult DelayFailStr(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult DelayOk(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = true;
	R.Detail = Msg;
	return R;
}

static void DelayInit(FNsLockstepDelayClient& C0, FNsLockstepDelayClient& C1)
{
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;
}

static void DelayPump(FNsFakeNet& Net, FNsLockstepDelayServer& Sv,
	FNsLockstepDelayClient& C0, FNsLockstepDelayClient& C1)
{
	NsPumpLockstepDelayServer(Net, Sv);
	NsPumpLockstepDelayClient(Net, C0);
	NsPumpLockstepDelayClient(Net, C1);
}

static void DelayCatchUp(FNsFakeNet& Net, FNsLockstepDelayClient& C0, FNsLockstepDelayClient& C1)
{
	Net.Advance(Net.RttMs + Net.JitterMs + 1.0);
	NsPumpLockstepDelayClient(Net, C0);
	NsPumpLockstepDelayClient(Net, C1);
}

FNsSelfTestResult NsRunLockstepDelayCleanSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepDelayServer Sv;
	FNsLockstepDelayClient C0;
	FNsLockstepDelayClient C1;
	DelayInit(C0, C1);

	bool bSawPreApply = false;
	bool bSawApply = false;
	const int32 Steps = 40;
	for (int32 S = 0; S < Steps; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		DelayPump(Net, Sv, C0, C1);
		if (C0.ExecFrame == NsLockstepDelayFrames)
		{
			if (C0.World.X[0] != 0 || C0.World.X[1] != 0)
			{
				return DelayFail(TEXT("lockstep-delay-clean: x moved before d"));
			}
			bSawPreApply = true;
		}
		if (C0.ExecFrame == NsLockstepDelayFrames + 1)
		{
			if (C0.World.X[0] != Ns::LockstepSpeed || C0.World.X[1] != -Ns::LockstepSpeed)
			{
				return DelayFailStr(FString::Printf(
					TEXT("lockstep-delay-clean: apply x0=%d x1=%d"),
					C0.World.X[0], C0.World.X[1]));
			}
			bSawApply = true;
		}
		Net.Advance(Ns::LogicDtMs);
	}

	if (!bSawPreApply || !bSawApply)
	{
		return DelayFailStr(FString::Printf(
			TEXT("lockstep-delay-clean: missed d check pre=%d apply=%d exec=%d"),
			bSawPreApply ? 1 : 0, bSawApply ? 1 : 0, C0.ExecFrame));
	}
	if (Sv.Frame != Steps || C0.ExecFrame != Steps || C1.ExecFrame != Steps)
	{
		return DelayFailStr(FString::Printf(
			TEXT("lockstep-delay-clean: frame sv=%d c0=%d c1=%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return DelayFail(TEXT("lockstep-delay-clean: worlds"));
	}
	if (Sv.StallFills != 0)
	{
		return DelayFail(TEXT("lockstep-delay-clean: stall on rtt0"));
	}
	return DelayOk(TEXT("lockstep-delay-clean"));
}

FNsSelfTestResult NsRunLockstepDelayRttSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 80.f;
	Net.JitterMs = 0.f;
	FNsLockstepDelayServer Sv;
	FNsLockstepDelayClient C0;
	FNsLockstepDelayClient C1;
	DelayInit(C0, C1);

	const int32 Steps = 40;
	for (int32 S = 0; S < Steps; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		DelayPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	for (int32 i = 0; i < 32; ++i)
	{
		DelayPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	DelayPump(Net, Sv, C0, C1);
	DelayCatchUp(Net, C0, C1);

	if (C0.ExecFrame < 20 || C1.ExecFrame != C0.ExecFrame)
	{
		return DelayFailStr(FString::Printf(
			TEXT("lockstep-delay-rtt: frames sv=%d c0=%d c1=%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return DelayFail(TEXT("lockstep-delay-rtt: worlds"));
	}
	return DelayOk(FString::Printf(TEXT("lockstep-delay-rtt frames=%d wait=%d"),
		C0.ExecFrame, Sv.WaitTicks));
}

FNsSelfTestResult NsRunLockstepDelayHighRttSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 400.f;
	Net.JitterMs = 0.f;
	FNsLockstepDelayServer Sv;
	FNsLockstepDelayClient C0;
	FNsLockstepDelayClient C1;
	DelayInit(C0, C1);

	const double End = 40 * Ns::LogicDtMs;
	while (Net.Now < End)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		DelayPump(Net, Sv, C0, C1);
		Net.Advance(1.0);
	}
	for (int32 i = 0; i < 800; ++i)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		DelayPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	for (int32 i = 0; i < 32; ++i)
	{
		DelayPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	DelayPump(Net, Sv, C0, C1);
	DelayCatchUp(Net, C0, C1);

	if (Sv.WaitTicks <= 0)
	{
		return DelayFail(TEXT("lockstep-delay-high-rtt: expected wait ticks"));
	}
	if (C0.ExecFrame != C1.ExecFrame)
	{
		return DelayFailStr(FString::Printf(
			TEXT("lockstep-delay-high-rtt: frames sv=%d c0=%d c1=%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return DelayFail(TEXT("lockstep-delay-high-rtt: worlds"));
	}
	return DelayOk(FString::Printf(
		TEXT("lockstep-delay-high-rtt wait=%d stall=%d frames=%d"),
		Sv.WaitTicks, Sv.StallFills, C0.ExecFrame));
}

FNsSelfTestResult NsRunLockstepDelayFromRttSelfTest()
{
	if (NsLockstepDelayFromRtt(0.0) != NsLockstepDelayFramesMin)
	{
		return DelayFail(TEXT("lockstep-delay-from-rtt: rtt0"));
	}
	if (NsLockstepDelayFromRtt(80.0) != 3)
	{
		return DelayFail(TEXT("lockstep-delay-from-rtt: rtt80"));
	}
	if (NsLockstepDelayFromRtt(400.0) != 8)
	{
		return DelayFail(TEXT("lockstep-delay-from-rtt: rtt400"));
	}
	if (NsLockstepDelayFromRtt(100000.0) != NsLockstepDelayFramesMax)
	{
		return DelayFail(TEXT("lockstep-delay-from-rtt: clamp max"));
	}
	return DelayOk(TEXT("lockstep-delay-from-rtt"));
}

FNsSelfTestResult NsRunLockstepDelayAdaptSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 400.f;
	Net.JitterMs = 0.f;
	FNsLockstepDelayServer Sv;
	FNsLockstepDelayClient C0;
	FNsLockstepDelayClient C1;
	DelayInit(C0, C1);
	const int32 D = NsLockstepDelayFromRtt(Net.RttMs);
	NsLockstepDelayApplyFrames(Sv, C0, C1, D);
	if (D != 8 || Sv.DelayFrames != 8 || C0.DelayFrames != C1.DelayFrames)
	{
		return DelayFailStr(FString::Printf(TEXT("lockstep-delay-adapt: d=%d"), D));
	}

	bool bSawPreApply = false;
	bool bSawApply = false;
	const int32 Steps = 40;
	for (int32 S = 0; S < Steps; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		DelayPump(Net, Sv, C0, C1);
		if (C0.ExecFrame == D)
		{
			if (C0.World.X[0] != 0 || C0.World.X[1] != 0)
			{
				return DelayFail(TEXT("lockstep-delay-adapt: x moved before d"));
			}
			bSawPreApply = true;
		}
		if (C0.ExecFrame == D + 1)
		{
			if (C0.World.X[0] != Ns::LockstepSpeed || C0.World.X[1] != -Ns::LockstepSpeed)
			{
				return DelayFailStr(FString::Printf(
					TEXT("lockstep-delay-adapt: apply x0=%d x1=%d"),
					C0.World.X[0], C0.World.X[1]));
			}
			bSawApply = true;
		}
		Net.Advance(Ns::LogicDtMs);
	}
	if (Sv.StallFills != 0)
	{
		return DelayFailStr(FString::Printf(
			TEXT("lockstep-delay-adapt: stall fills=%d wait=%d"), Sv.StallFills, Sv.WaitTicks));
	}
	for (int32 i = 0; i < 32; ++i)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		DelayPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	DelayPump(Net, Sv, C0, C1);
	DelayCatchUp(Net, C0, C1);

	if (!bSawPreApply || !bSawApply)
	{
		return DelayFailStr(FString::Printf(
			TEXT("lockstep-delay-adapt: missed d check pre=%d apply=%d exec=%d"),
			bSawPreApply ? 1 : 0, bSawApply ? 1 : 0, C0.ExecFrame));
	}
	if (Sv.StallFills != 0)
	{
		return DelayFailStr(FString::Printf(
			TEXT("lockstep-delay-adapt: stall fills=%d wait=%d"), Sv.StallFills, Sv.WaitTicks));
	}
	if (C0.ExecFrame < 20 || C1.ExecFrame != C0.ExecFrame)
	{
		return DelayFailStr(FString::Printf(
			TEXT("lockstep-delay-adapt: frames sv=%d c0=%d c1=%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return DelayFail(TEXT("lockstep-delay-adapt: worlds"));
	}
	return DelayOk(FString::Printf(TEXT("lockstep-delay-adapt d=%d frames=%d wait=%d"),
		D, C0.ExecFrame, Sv.WaitTicks));
}

FNsSelfTestResult NsRunLockstepDelayRecoverySelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	Net.bDropType = true;
	Net.DropType = ENsMsg::S2CFrame;
	FNsLockstepDelayServer Sv;
	FNsLockstepDelayClient C0;
	FNsLockstepDelayClient C1;
	DelayInit(C0, C1);

	for (int32 S = 0; S < 20; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		DelayPump(Net, Sv, C0, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	if (C0.ExecFrame == 0 || C1.ExecFrame == 0)
	{
		return DelayFail(TEXT("lockstep-delay-recovery: no snapshot catch-up"));
	}
	if (!C0.World.Equals(C1.World))
	{
		return DelayFail(TEXT("lockstep-delay-recovery: clients diverged"));
	}
	return DelayOk(FString::Printf(TEXT("lockstep-delay-recovery frame=%d"), C0.ExecFrame));
}
