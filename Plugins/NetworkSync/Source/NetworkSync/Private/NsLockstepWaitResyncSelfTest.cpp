// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsSelfTest.h"
#include "NsFakeNet.h"
#include "NsLockstepWait.h"
#include "NsLockstepWaitResync.h"

static FNsSelfTestResult WrFail(const TCHAR* Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult WrFailStr(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult WrOk(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = true;
	R.Detail = Msg;
	return R;
}

static void WrInit(FNsLockstepWaitClient& C0, FNsLockstepWaitClient& C1)
{
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;
}

static void WrWarm(FNsFakeNet& Net, FNsLockstepWaitServer& Sv,
	FNsLockstepWaitClient& C0, FNsLockstepWaitClient& C1, int32 Steps)
{
	FNsLockstepResync Warm;
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	for (int32 S = 0; S < Steps; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		NsPumpLockstepWaitResyncServer(Net, Sv, Warm);
		NsPumpLockstepWaitResyncClient(Net, C0, V0);
		NsPumpLockstepWaitResyncClient(Net, C1, V1);
		Net.Advance(Ns::LogicDtMs);
	}
}

static bool WrForceDesyncAt(FNsLockstepWaitServer& Sv, int32 Tick)
{
	const uint32* Found = Sv.Checksums.Find(Tick);
	if (!Found)
	{
		return false;
	}
	Sv.OnChecksum(Tick, *Found ^ 1u);
	return Sv.bDesync;
}

static bool WrForceDesync(FNsLockstepWaitServer& Sv)
{
	return WrForceDesyncAt(Sv, Ns::ChecksumEvery);
}

static bool WrAligned(const FNsLockstepWaitServer& Sv, const FNsLockstepResync& Repair,
	const FNsLockstepWaitClient& C0, const FNsLockstepWaitClient& C1)
{
	return Repair.bCaptured
		&& C0.World.Equals(Repair.LiveSnap)
		&& C1.World.Equals(Repair.LiveSnap)
		&& Sv.World.Equals(Repair.LiveSnap)
		&& C0.ExecFrame == Repair.LiveSnapTick
		&& C1.ExecFrame == Repair.LiveSnapTick
		&& Sv.Frame == Repair.LiveSnapTick;
}

FNsSelfTestResult NsRunLockstepWaitResyncAlignSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepWaitServer Sv;
	FNsLockstepWaitClient C0;
	FNsLockstepWaitClient C1;
	WrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	WrWarm(Net, Sv, C0, C1, 24);
	if (!WrForceDesync(Sv))
	{
		return WrFail(TEXT("lockstep-wait-resync-align: no checksum record"));
	}
	const int32 FrameAt = Sv.Frame;
	FNsLockstepResync Repair;
	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);
	if (Sv.Frame != FrameAt)
	{
		return WrFailStr(FString::Printf(
			TEXT("lockstep-wait-resync-align: ticked frame=%d was=%d"), Sv.Frame, FrameAt));
	}
	if (!WrAligned(Sv, Repair, C0, C1))
	{
		return WrFail(TEXT("lockstep-wait-resync-align: worlds not pulled to live snap"));
	}
	if (Repair.bGiveUp)
	{
		return WrFail(TEXT("lockstep-wait-resync-align: gave up"));
	}
	return WrOk(TEXT("lockstep-wait-resync-align"));
}

FNsSelfTestResult NsRunLockstepWaitResyncForceSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepWaitServer Sv;
	FNsLockstepWaitClient C0;
	FNsLockstepWaitClient C1;
	WrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	WrWarm(Net, Sv, C0, C1, 24);
	if (!WrForceDesync(Sv))
	{
		return WrFail(TEXT("lockstep-wait-resync-force: no checksum record"));
	}
	FNsLockstepResync Repair;
	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	C0.ExecFrame = Repair.LiveSnapTick + 8;
	C0.World.X[0] = 999;
	C0.World.Rng = 7;
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);
	if (C0.ExecFrame != Repair.LiveSnapTick || !C0.World.Equals(Repair.LiveSnap))
	{
		return WrFail(TEXT("lockstep-wait-resync-force: ahead client not rewound"));
	}
	return WrOk(TEXT("lockstep-wait-resync-force"));
}

FNsSelfTestResult NsRunLockstepWaitResyncIgnoreFrameSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepWaitServer Sv;
	FNsLockstepWaitClient C0;
	FNsLockstepWaitClient C1;
	WrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	WrWarm(Net, Sv, C0, C1, 24);
	if (!WrForceDesync(Sv))
	{
		return WrFail(TEXT("lockstep-wait-resync-ignore: no checksum record"));
	}
	FNsLockstepResync Repair;
	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);
	const FNsWorld Before = C0.World;
	const int32 Exec = C0.ExecFrame;
	FNsInputs In;
	In.Dx[0] = 1;
	In.Dx[1] = 1;
	TMap<int32, FNsInputs> Extra;
	Extra.Add(Exec, In);
	C0.OnS2C(Extra);
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	if (!C0.World.Equals(Before) || C0.ExecFrame != Exec)
	{
		return WrFail(TEXT("lockstep-wait-resync-ignore: S2CFrame still stepped"));
	}
	return WrOk(TEXT("lockstep-wait-resync-ignore-frame"));
}

FNsSelfTestResult NsRunLockstepWaitResyncResumeSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepWaitServer Sv;
	FNsLockstepWaitClient C0;
	FNsLockstepWaitClient C1;
	WrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	WrWarm(Net, Sv, C0, C1, 24);
	if (!WrForceDesync(Sv))
	{
		return WrFail(TEXT("lockstep-wait-resync-resume: no checksum record"));
	}
	const int32 FrameAt = Sv.Frame;
	FNsLockstepResync Repair;
	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);
	if (!WrAligned(Sv, Repair, C0, C1))
	{
		return WrFail(TEXT("lockstep-wait-resync-resume: not aligned"));
	}
	if (Repair.bResumed || !Sv.bDesync)
	{
		return WrFail(TEXT("lockstep-wait-resync-resume: resumed before acks"));
	}

	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	if (!Repair.bResumed || Sv.bDesync)
	{
		return WrFail(TEXT("lockstep-wait-resync-resume: did not resume after acks"));
	}
	if (Sv.Frame != FrameAt)
	{
		return WrFailStr(FString::Printf(
			TEXT("lockstep-wait-resync-resume: ticked on ack pump frame=%d was=%d"),
			Sv.Frame, FrameAt));
	}

	C0.SendInput(Net, 1);
	C1.SendInput(Net, -1);
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);
	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);
	if (Sv.Frame <= FrameAt || C0.ExecFrame != Sv.Frame || C1.ExecFrame != Sv.Frame)
	{
		return WrFailStr(FString::Printf(
			TEXT("lockstep-wait-resync-resume: did not tick sv=%d c0=%d c1=%d was=%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame, FrameAt));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return WrFail(TEXT("lockstep-wait-resync-resume: worlds"));
	}
	return WrOk(FString::Printf(TEXT("lockstep-wait-resync-resume frame=%d"), Sv.Frame));
}

FNsSelfTestResult NsRunLockstepWaitResyncAgainSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepWaitServer Sv;
	FNsLockstepWaitClient C0;
	FNsLockstepWaitClient C1;
	WrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	WrWarm(Net, Sv, C0, C1, 24);
	if (!WrForceDesync(Sv))
	{
		return WrFail(TEXT("lockstep-wait-resync-again: no checksum record"));
	}
	const int32 FirstHalt = Sv.Frame;
	FNsLockstepResync Repair;
	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);
	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	if (!Repair.bResumed || Repair.bCaptured || Sv.bDesync)
	{
		return WrFail(TEXT("lockstep-wait-resync-again: first resume"));
	}

	C0.SendInput(Net, 1);
	C1.SendInput(Net, -1);
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);
	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);

	const int32 SecondTick = Ns::ChecksumEvery * 2;
	for (int32 S = 0; S < 40 && !Sv.Checksums.Contains(SecondTick); ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		NsPumpLockstepWaitResyncClient(Net, C0, V0);
		NsPumpLockstepWaitResyncClient(Net, C1, V1);
		NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
		NsPumpLockstepWaitResyncClient(Net, C0, V0);
		NsPumpLockstepWaitResyncClient(Net, C1, V1);
	}
	if (!WrForceDesyncAt(Sv, SecondTick))
	{
		return WrFailStr(FString::Printf(
			TEXT("lockstep-wait-resync-again: no checksum %d frame=%d"), SecondTick, Sv.Frame));
	}
	const int32 SecondHalt = Sv.Frame;
	if (SecondHalt == FirstHalt)
	{
		return WrFail(TEXT("lockstep-wait-resync-again: second halt at same frame"));
	}
	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	if (Repair.bResumed || !Repair.bCaptured || Repair.LiveSnapTick != SecondHalt)
	{
		return WrFailStr(FString::Printf(
			TEXT("lockstep-wait-resync-again: no recapture resumed=%d captured=%d snap=%d halt=%d"),
			Repair.bResumed ? 1 : 0, Repair.bCaptured ? 1 : 0, Repair.LiveSnapTick, SecondHalt));
	}
	if (Sv.Frame != SecondHalt)
	{
		return WrFailStr(FString::Printf(
			TEXT("lockstep-wait-resync-again: ticked on second halt frame=%d was=%d"),
			Sv.Frame, SecondHalt));
	}

	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);
	if (!WrAligned(Sv, Repair, C0, C1))
	{
		return WrFail(TEXT("lockstep-wait-resync-again: second align"));
	}
	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	if (!Repair.bResumed || Repair.bCaptured || Sv.bDesync)
	{
		return WrFail(TEXT("lockstep-wait-resync-again: second resume"));
	}

	C0.SendInput(Net, 1);
	C1.SendInput(Net, -1);
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);
	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);
	if (Sv.Frame <= SecondHalt || C0.ExecFrame != Sv.Frame || C1.ExecFrame != Sv.Frame)
	{
		return WrFailStr(FString::Printf(
			TEXT("lockstep-wait-resync-again: did not tick sv=%d c0=%d c1=%d halt=%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame, SecondHalt));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return WrFail(TEXT("lockstep-wait-resync-again: worlds"));
	}
	return WrOk(FString::Printf(TEXT("lockstep-wait-resync-again snap=%d then %d"), FirstHalt, SecondHalt));
}

FNsSelfTestResult NsRunLockstepWaitResyncWireSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepWaitServer Sv;
	FNsLockstepWaitClient C0;
	FNsLockstepWaitClient C1;
	WrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	WrWarm(Net, Sv, C0, C1, 24);
	if (!WrForceDesync(Sv))
	{
		return WrFail(TEXT("lockstep-wait-resync-wire: no checksum record"));
	}
	FNsLockstepResync Repair;
	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);
	if (V0.HaltTick != Repair.LiveSnapTick || V1.HaltTick != Repair.LiveSnapTick)
	{
		return WrFailStr(FString::Printf(
			TEXT("lockstep-wait-resync-wire: halt from packet v0=%d v1=%d snap=%d"),
			V0.HaltTick, V1.HaltTick, Repair.LiveSnapTick));
	}
	if (!WrAligned(Sv, Repair, C0, C1))
	{
		return WrFail(TEXT("lockstep-wait-resync-wire: not aligned"));
	}
	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	C0.SendInput(Net, 1);
	C1.SendInput(Net, -1);
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);
	NsPumpLockstepWaitResyncServer(Net, Sv, Repair);
	NsPumpLockstepWaitResyncClient(Net, C0, V0);
	NsPumpLockstepWaitResyncClient(Net, C1, V1);
	if (V0.HaltTick >= 0 || V1.HaltTick >= 0)
	{
		return WrFail(TEXT("lockstep-wait-resync-wire: still halted after S2CFrame"));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return WrFail(TEXT("lockstep-wait-resync-wire: worlds"));
	}
	return WrOk(FString::Printf(TEXT("lockstep-wait-resync-wire snap=%d"), Repair.LiveSnapTick));
}
