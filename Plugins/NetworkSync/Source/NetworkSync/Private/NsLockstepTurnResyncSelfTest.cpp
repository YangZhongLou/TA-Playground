// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsSelfTest.h"
#include "NsFakeNet.h"
#include "NsLockstepTurn.h"
#include "NsLockstepTurnResync.h"

static FNsSelfTestResult TrFail(const TCHAR* Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult TrFailStr(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult TrOk(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = true;
	R.Detail = Msg;
	return R;
}

static void TrInit(FNsLockstepTurnClient& C0, FNsLockstepTurnClient& C1)
{
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;
}

static void TrWarm(FNsFakeNet& Net, FNsLockstepTurnServer& Sv,
	FNsLockstepTurnClient& C0, FNsLockstepTurnClient& C1, int32 Steps)
{
	FNsLockstepResync Warm;
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	for (int32 S = 0; S < Steps; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		NsPumpLockstepTurnResyncServer(Net, Sv, Warm);
		NsPumpLockstepTurnResyncClient(Net, C0, V0);
		NsPumpLockstepTurnResyncClient(Net, C1, V1);
		Net.Advance(Ns::LogicDtMs);
	}
}

static bool TrForceDesyncAt(FNsLockstepTurnServer& Sv, int32 Tick)
{
	const uint32* Found = Sv.Checksums.Find(Tick);
	if (!Found)
	{
		return false;
	}
	Sv.OnChecksum(Tick, *Found ^ 1u);
	return Sv.bDesync;
}

static bool TrForceDesync(FNsLockstepTurnServer& Sv)
{
	return TrForceDesyncAt(Sv, Ns::ChecksumEvery);
}

static bool TrAligned(const FNsLockstepTurnServer& Sv, const FNsLockstepResync& Repair,
	const FNsLockstepTurnClient& C0, const FNsLockstepTurnClient& C1)
{
	return Repair.bCaptured
		&& C0.World.Equals(Repair.LiveSnap)
		&& C1.World.Equals(Repair.LiveSnap)
		&& Sv.World.Equals(Repair.LiveSnap)
		&& C0.ExecFrame == Repair.LiveSnapTick
		&& C1.ExecFrame == Repair.LiveSnapTick
		&& Sv.Frame == Repair.LiveSnapTick;
}

FNsSelfTestResult NsRunLockstepTurnResyncAlignSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepTurnServer Sv;
	FNsLockstepTurnClient C0;
	FNsLockstepTurnClient C1;
	TrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	TrWarm(Net, Sv, C0, C1, 24);
	if (!TrForceDesync(Sv))
	{
		return TrFail(TEXT("lockstep-turn-resync-align: no checksum record"));
	}
	const int32 FrameAt = Sv.Frame;
	FNsLockstepResync Repair;
	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);
	if (Sv.Frame != FrameAt)
	{
		return TrFailStr(FString::Printf(
			TEXT("lockstep-turn-resync-align: ticked frame=%d was=%d"), Sv.Frame, FrameAt));
	}
	if (!TrAligned(Sv, Repair, C0, C1))
	{
		return TrFail(TEXT("lockstep-turn-resync-align: worlds not pulled to live snap"));
	}
	if (Repair.bGiveUp)
	{
		return TrFail(TEXT("lockstep-turn-resync-align: gave up"));
	}
	return TrOk(TEXT("lockstep-turn-resync-align"));
}

FNsSelfTestResult NsRunLockstepTurnResyncForceSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepTurnServer Sv;
	FNsLockstepTurnClient C0;
	FNsLockstepTurnClient C1;
	TrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	TrWarm(Net, Sv, C0, C1, 24);
	if (!TrForceDesync(Sv))
	{
		return TrFail(TEXT("lockstep-turn-resync-force: no checksum record"));
	}
	FNsLockstepResync Repair;
	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	C0.ExecFrame = Repair.LiveSnapTick + 8;
	C0.World.X[0] = 999;
	C0.World.Rng = 7;
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);
	if (C0.ExecFrame != Repair.LiveSnapTick || !C0.World.Equals(Repair.LiveSnap))
	{
		return TrFail(TEXT("lockstep-turn-resync-force: ahead client not rewound"));
	}
	return TrOk(TEXT("lockstep-turn-resync-force"));
}

FNsSelfTestResult NsRunLockstepTurnResyncIgnoreFrameSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepTurnServer Sv;
	FNsLockstepTurnClient C0;
	FNsLockstepTurnClient C1;
	TrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	TrWarm(Net, Sv, C0, C1, 24);
	if (!TrForceDesync(Sv))
	{
		return TrFail(TEXT("lockstep-turn-resync-ignore: no checksum record"));
	}
	FNsLockstepResync Repair;
	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);
	const FNsWorld Before = C0.World;
	const int32 Exec = C0.ExecFrame;
	FNsInputs In;
	In.Dx[0] = 1;
	In.Dx[1] = 1;
	TMap<int32, FNsInputs> Extra;
	Extra.Add(C0.ExecTurn, In);
	TMap<int32, int32> Lens;
	C0.OnS2C(Extra, C0.FramesPerTurn, C0.FramesPerTurn, Lens);
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	if (!C0.World.Equals(Before) || C0.ExecFrame != Exec)
	{
		return TrFail(TEXT("lockstep-turn-resync-ignore: S2CFrame still stepped"));
	}
	return TrOk(TEXT("lockstep-turn-resync-ignore-frame"));
}

FNsSelfTestResult NsRunLockstepTurnResyncResumeSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepTurnServer Sv;
	FNsLockstepTurnClient C0;
	FNsLockstepTurnClient C1;
	TrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	TrWarm(Net, Sv, C0, C1, 24);
	if (!TrForceDesync(Sv))
	{
		return TrFail(TEXT("lockstep-turn-resync-resume: no checksum record"));
	}
	const int32 FrameAt = Sv.Frame;
	FNsLockstepResync Repair;
	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);
	if (!TrAligned(Sv, Repair, C0, C1))
	{
		return TrFail(TEXT("lockstep-turn-resync-resume: not aligned"));
	}
	if (Repair.bResumed || !Sv.bDesync)
	{
		return TrFail(TEXT("lockstep-turn-resync-resume: resumed before acks"));
	}

	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	if (!Repair.bResumed || Sv.bDesync)
	{
		return TrFail(TEXT("lockstep-turn-resync-resume: did not resume after acks"));
	}
	if (Sv.Frame != FrameAt)
	{
		return TrFailStr(FString::Printf(
			TEXT("lockstep-turn-resync-resume: ticked on ack pump frame=%d was=%d"),
			Sv.Frame, FrameAt));
	}

	C0.SendInput(Net, 1);
	C1.SendInput(Net, -1);
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);
	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);
	if (Sv.Frame <= FrameAt || C0.ExecFrame != Sv.Frame || C1.ExecFrame != Sv.Frame)
	{
		return TrFailStr(FString::Printf(
			TEXT("lockstep-turn-resync-resume: did not tick sv=%d c0=%d c1=%d was=%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame, FrameAt));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return TrFail(TEXT("lockstep-turn-resync-resume: worlds"));
	}
	return TrOk(FString::Printf(TEXT("lockstep-turn-resync-resume frame=%d"), Sv.Frame));
}

FNsSelfTestResult NsRunLockstepTurnResyncAgainSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepTurnServer Sv;
	FNsLockstepTurnClient C0;
	FNsLockstepTurnClient C1;
	TrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	TrWarm(Net, Sv, C0, C1, 24);
	if (!TrForceDesync(Sv))
	{
		return TrFail(TEXT("lockstep-turn-resync-again: no checksum record"));
	}
	const int32 FirstHalt = Sv.Frame;
	FNsLockstepResync Repair;
	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);
	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	if (!Repair.bResumed || Repair.bCaptured || Sv.bDesync)
	{
		return TrFail(TEXT("lockstep-turn-resync-again: first resume"));
	}

	C0.SendInput(Net, 1);
	C1.SendInput(Net, -1);
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);
	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);

	const int32 SecondTick = Ns::ChecksumEvery * 2;
	for (int32 S = 0; S < 40 && !Sv.Checksums.Contains(SecondTick); ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		NsPumpLockstepTurnResyncClient(Net, C0, V0);
		NsPumpLockstepTurnResyncClient(Net, C1, V1);
		NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
		NsPumpLockstepTurnResyncClient(Net, C0, V0);
		NsPumpLockstepTurnResyncClient(Net, C1, V1);
	}
	if (!TrForceDesyncAt(Sv, SecondTick))
	{
		return TrFailStr(FString::Printf(
			TEXT("lockstep-turn-resync-again: no checksum %d frame=%d"), SecondTick, Sv.Frame));
	}
	const int32 SecondHalt = Sv.Frame;
	if (SecondHalt == FirstHalt)
	{
		return TrFail(TEXT("lockstep-turn-resync-again: second halt at same frame"));
	}
	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	if (Repair.bResumed || !Repair.bCaptured || Repair.LiveSnapTick != SecondHalt)
	{
		return TrFailStr(FString::Printf(
			TEXT("lockstep-turn-resync-again: no recapture resumed=%d captured=%d snap=%d halt=%d"),
			Repair.bResumed ? 1 : 0, Repair.bCaptured ? 1 : 0, Repair.LiveSnapTick, SecondHalt));
	}
	if (Sv.Frame != SecondHalt)
	{
		return TrFailStr(FString::Printf(
			TEXT("lockstep-turn-resync-again: ticked on second halt frame=%d was=%d"),
			Sv.Frame, SecondHalt));
	}

	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);
	if (!TrAligned(Sv, Repair, C0, C1))
	{
		return TrFail(TEXT("lockstep-turn-resync-again: second align"));
	}
	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	if (!Repair.bResumed || Repair.bCaptured || Sv.bDesync)
	{
		return TrFail(TEXT("lockstep-turn-resync-again: second resume"));
	}

	C0.SendInput(Net, 1);
	C1.SendInput(Net, -1);
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);
	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);
	if (Sv.Frame <= SecondHalt || C0.ExecFrame != Sv.Frame || C1.ExecFrame != Sv.Frame)
	{
		return TrFailStr(FString::Printf(
			TEXT("lockstep-turn-resync-again: did not tick sv=%d c0=%d c1=%d halt=%d"),
			Sv.Frame, C0.ExecFrame, C1.ExecFrame, SecondHalt));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return TrFail(TEXT("lockstep-turn-resync-again: worlds"));
	}
	return TrOk(FString::Printf(TEXT("lockstep-turn-resync-again snap=%d then %d"), FirstHalt, SecondHalt));
}

FNsSelfTestResult NsRunLockstepTurnResyncWireSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepTurnServer Sv;
	FNsLockstepTurnClient C0;
	FNsLockstepTurnClient C1;
	TrInit(C0, C1);
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	TrWarm(Net, Sv, C0, C1, 24);
	if (!TrForceDesync(Sv))
	{
		return TrFail(TEXT("lockstep-turn-resync-wire: no checksum record"));
	}
	FNsLockstepResync Repair;
	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);
	if (V0.HaltTick != Repair.LiveSnapTick || V1.HaltTick != Repair.LiveSnapTick)
	{
		return TrFailStr(FString::Printf(
			TEXT("lockstep-turn-resync-wire: halt from packet v0=%d v1=%d snap=%d"),
			V0.HaltTick, V1.HaltTick, Repair.LiveSnapTick));
	}
	if (!TrAligned(Sv, Repair, C0, C1))
	{
		return TrFail(TEXT("lockstep-turn-resync-wire: not aligned"));
	}
	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	C0.SendInput(Net, 1);
	C1.SendInput(Net, -1);
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);
	NsPumpLockstepTurnResyncServer(Net, Sv, Repair);
	NsPumpLockstepTurnResyncClient(Net, C0, V0);
	NsPumpLockstepTurnResyncClient(Net, C1, V1);
	if (V0.HaltTick >= 0 || V1.HaltTick >= 0)
	{
		return TrFail(TEXT("lockstep-turn-resync-wire: still halted after resume token"));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return TrFail(TEXT("lockstep-turn-resync-wire: worlds"));
	}
	return TrOk(FString::Printf(TEXT("lockstep-turn-resync-wire snap=%d"), Repair.LiveSnapTick));
}
