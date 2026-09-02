// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsSelfTest.h"
#include "NsFakeNet.h"
#include "NsLockstepDelay.h"
#include "NsLockstepDelayResync.h"

static FNsSelfTestResult DrFail(const TCHAR* Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult DrFailStr(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult DrOk(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = true;
	R.Detail = Msg;
	return R;
}

static void DrInit(FNsLockstepDelayClient& C0, FNsLockstepDelayClient& C1)
{
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;
}

static void DrWarm(FNsFakeNet& Net, FNsLockstepDelayServer& Sv,
	FNsLockstepDelayClient& C0, FNsLockstepDelayClient& C1, int32 Steps)
{
	FNsLockstepResync Warm;
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	for (int32 S = 0; S < Steps; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		NsPumpLockstepDelayResyncServer(Net, Sv, Warm);
		NsPumpLockstepDelayResyncClient(Net, C0, V0);
		NsPumpLockstepDelayResyncClient(Net, C1, V1);
		Net.Advance(Ns::LogicDtMs);
	}
}

static bool DrForceDesyncAt(FNsLockstepDelayServer& Sv, int32 Tick)
{
	const uint32* Found = Sv.Checksums.Find(Tick);
	if (!Found)
	{
		return false;
	}
	Sv.OnChecksum(Tick, *Found ^ 1u);
	return Sv.bDesync;
}

static bool DrForceDesync(FNsLockstepDelayServer& Sv)
{
	return DrForceDesyncAt(Sv, Ns::ChecksumEvery);
}

static bool DrAligned(const FNsLockstepDelayServer& Sv, const FNsLockstepResync& Repair,
	const FNsLockstepDelayClient& C0, const FNsLockstepDelayClient& C1)
{
	return Repair.bCaptured
		&& C0.World.Equals(Repair.LiveSnap)
		&& C1.World.Equals(Repair.LiveSnap)
		&& Sv.World.Equals(Repair.LiveSnap)
		&& C0.ExecFrame == Repair.LiveSnapTick
		&& C1.ExecFrame == Repair.LiveSnapTick
		&& Sv.Frame == Repair.LiveSnapTick;
}

FNsSelfTestResult NsRunLockstepDelayResyncAlignSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepDelayServer Sv;
	FNsLockstepDelayClient C0;
	FNsLockstepDelayClient C1;
	DrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	DrWarm(Net, Sv, C0, C1, 24);
	if (!DrForceDesync(Sv))
	{
		return DrFail(TEXT("lockstep-delay-resync-align: no checksum record"));
	}
	const int32 FrameAt = Sv.Frame;
	FNsLockstepResync Repair;
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);
	if (Sv.Frame != FrameAt)
	{
		return DrFailStr(FString::Printf(
			TEXT("lockstep-delay-resync-align: ticked frame=%d was=%d"), Sv.Frame, FrameAt));
	}
	if (!DrAligned(Sv, Repair, C0, C1))
	{
		return DrFail(TEXT("lockstep-delay-resync-align: worlds not pulled to live snap"));
	}
	if (Repair.bGiveUp)
	{
		return DrFail(TEXT("lockstep-delay-resync-align: gave up"));
	}
	return DrOk(TEXT("lockstep-delay-resync-align"));
}

FNsSelfTestResult NsRunLockstepDelayResyncForceSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepDelayServer Sv;
	FNsLockstepDelayClient C0;
	FNsLockstepDelayClient C1;
	DrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	DrWarm(Net, Sv, C0, C1, 24);
	if (!DrForceDesync(Sv))
	{
		return DrFail(TEXT("lockstep-delay-resync-force: no checksum record"));
	}
	FNsLockstepResync Repair;
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	C0.ExecFrame = Repair.LiveSnapTick + 8;
	C0.World.X[0] = 999;
	C0.World.Rng = 7;
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);
	if (C0.ExecFrame != Repair.LiveSnapTick || !C0.World.Equals(Repair.LiveSnap))
	{
		return DrFail(TEXT("lockstep-delay-resync-force: ahead client not rewound"));
	}
	return DrOk(TEXT("lockstep-delay-resync-force"));
}

FNsSelfTestResult NsRunLockstepDelayResyncIgnoreFrameSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepDelayServer Sv;
	FNsLockstepDelayClient C0;
	FNsLockstepDelayClient C1;
	DrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	DrWarm(Net, Sv, C0, C1, 24);
	if (!DrForceDesync(Sv))
	{
		return DrFail(TEXT("lockstep-delay-resync-ignore: no checksum record"));
	}
	FNsLockstepResync Repair;
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);
	const FNsWorld Before = C0.World;
	const int32 Exec = C0.ExecFrame;
	FNsInputs In;
	In.Dx[0] = 1;
	In.Dx[1] = 1;
	TMap<int32, FNsInputs> Extra;
	Extra.Add(Exec, In);
	C0.OnS2C(Extra);
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	if (!C0.World.Equals(Before) || C0.ExecFrame != Exec)
	{
		return DrFail(TEXT("lockstep-delay-resync-ignore: S2CFrame still stepped"));
	}
	return DrOk(TEXT("lockstep-delay-resync-ignore-frame"));
}

FNsSelfTestResult NsRunLockstepDelayResyncResumeSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepDelayServer Sv;
	FNsLockstepDelayClient C0;
	FNsLockstepDelayClient C1;
	DrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	DrWarm(Net, Sv, C0, C1, 24);
	if (!DrForceDesync(Sv))
	{
		return DrFail(TEXT("lockstep-delay-resync-resume: no checksum record"));
	}
	const int32 FrameAt = Sv.Frame;
	FNsLockstepResync Repair;
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);
	if (!DrAligned(Sv, Repair, C0, C1))
	{
		return DrFail(TEXT("lockstep-delay-resync-resume: not aligned"));
	}
	if (Repair.bResumed || !Sv.bDesync)
	{
		return DrFail(TEXT("lockstep-delay-resync-resume: resumed before acks"));
	}

	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	if (!Repair.bResumed || Sv.bDesync)
	{
		return DrFail(TEXT("lockstep-delay-resync-resume: did not resume after acks"));
	}
	if (Sv.Frame != FrameAt)
	{
		return DrFailStr(FString::Printf(
			TEXT("lockstep-delay-resync-resume: ticked on ack pump frame=%d was=%d"),
			Sv.Frame, FrameAt));
	}

	C0.SendInput(Net, 1);
	C1.SendInput(Net, -1);
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);
	if (Sv.Frame <= FrameAt || C0.ExecFrame != Sv.Frame || C1.ExecFrame != Sv.Frame)
	{
		return DrFailStr(FString::Printf(
			TEXT("lockstep-delay-resync-resume: did not tick sv=%d c0=%d c1=%d was=%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame, FrameAt));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return DrFail(TEXT("lockstep-delay-resync-resume: worlds"));
	}
	return DrOk(FString::Printf(TEXT("lockstep-delay-resync-resume frame=%d"), Sv.Frame));
}

FNsSelfTestResult NsRunLockstepDelayResyncAgainSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepDelayServer Sv;
	FNsLockstepDelayClient C0;
	FNsLockstepDelayClient C1;
	DrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	DrWarm(Net, Sv, C0, C1, 24);
	if (!DrForceDesync(Sv))
	{
		return DrFail(TEXT("lockstep-delay-resync-again: no checksum record"));
	}
	const int32 FirstHalt = Sv.Frame;
	FNsLockstepResync Repair;
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	if (!Repair.bResumed || Repair.bCaptured || Sv.bDesync)
	{
		return DrFail(TEXT("lockstep-delay-resync-again: first resume"));
	}

	C0.SendInput(Net, 1);
	C1.SendInput(Net, -1);
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);

	const int32 SecondTick = Ns::ChecksumEvery * 2;
	for (int32 S = 0; S < 40 && !Sv.Checksums.Contains(SecondTick); ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		NsPumpLockstepDelayResyncClient(Net, C0, V0);
		NsPumpLockstepDelayResyncClient(Net, C1, V1);
		NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
		NsPumpLockstepDelayResyncClient(Net, C0, V0);
		NsPumpLockstepDelayResyncClient(Net, C1, V1);
	}
	if (!DrForceDesyncAt(Sv, SecondTick))
	{
		return DrFailStr(FString::Printf(
			TEXT("lockstep-delay-resync-again: no checksum %d frame=%d"), SecondTick, Sv.Frame));
	}
	const int32 SecondHalt = Sv.Frame;
	if (SecondHalt == FirstHalt)
	{
		return DrFail(TEXT("lockstep-delay-resync-again: second halt at same frame"));
	}
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	if (Repair.bResumed || !Repair.bCaptured || Repair.LiveSnapTick != SecondHalt)
	{
		return DrFailStr(FString::Printf(
			TEXT("lockstep-delay-resync-again: no recapture resumed=%d captured=%d snap=%d halt=%d"),
			Repair.bResumed ? 1 : 0, Repair.bCaptured ? 1 : 0, Repair.LiveSnapTick, SecondHalt));
	}
	if (Sv.Frame != SecondHalt)
	{
		return DrFailStr(FString::Printf(
			TEXT("lockstep-delay-resync-again: ticked on second halt frame=%d was=%d"),
			Sv.Frame, SecondHalt));
	}

	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);
	if (!DrAligned(Sv, Repair, C0, C1))
	{
		return DrFail(TEXT("lockstep-delay-resync-again: second align"));
	}
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	if (!Repair.bResumed || Repair.bCaptured || Sv.bDesync)
	{
		return DrFail(TEXT("lockstep-delay-resync-again: second resume"));
	}

	C0.SendInput(Net, 1);
	C1.SendInput(Net, -1);
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);
	if (Sv.Frame <= SecondHalt || C0.ExecFrame != Sv.Frame || C1.ExecFrame != Sv.Frame)
	{
		return DrFailStr(FString::Printf(
			TEXT("lockstep-delay-resync-again: did not tick sv=%d c0=%d c1=%d halt=%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame, SecondHalt));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return DrFail(TEXT("lockstep-delay-resync-again: worlds"));
	}
	return DrOk(FString::Printf(TEXT("lockstep-delay-resync-again snap=%d then %d"), FirstHalt, SecondHalt));
}

FNsSelfTestResult NsRunLockstepDelayResyncWireSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepDelayServer Sv;
	FNsLockstepDelayClient C0;
	FNsLockstepDelayClient C1;
	DrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	DrWarm(Net, Sv, C0, C1, 24);
	if (!DrForceDesync(Sv))
	{
		return DrFail(TEXT("lockstep-delay-resync-wire: no checksum record"));
	}
	FNsLockstepResync Repair;
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);
	if (V0.HaltTick != Repair.LiveSnapTick || V1.HaltTick != Repair.LiveSnapTick)
	{
		return DrFailStr(FString::Printf(
			TEXT("lockstep-delay-resync-wire: halt from packet v0=%d v1=%d snap=%d"),
			V0.HaltTick, V1.HaltTick, Repair.LiveSnapTick));
	}
	if (!DrAligned(Sv, Repair, C0, C1))
	{
		return DrFail(TEXT("lockstep-delay-resync-wire: not aligned"));
	}
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	C0.SendInput(Net, 1);
	C1.SendInput(Net, -1);
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	NsPumpLockstepDelayResyncClient(Net, C0, V0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1);
	if (V0.HaltTick >= 0 || V1.HaltTick >= 0)
	{
		return DrFail(TEXT("lockstep-delay-resync-wire: still halted after S2CFrame"));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return DrFail(TEXT("lockstep-delay-resync-wire: worlds"));
	}
	return DrOk(FString::Printf(TEXT("lockstep-delay-resync-wire snap=%d"), Repair.LiveSnapTick));
}
