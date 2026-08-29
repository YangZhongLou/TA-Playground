// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsSelfTest.h"
#include "NsFakeNet.h"
#include "NsLockstep.h"
#include "NsLockstepResync.h"
#include "NsLockstepDoor.h"
#include "NsPump.h"

static FNsSelfTestResult HyFail(const TCHAR* Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult HyFailStr(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult HyOk(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = true;
	R.Detail = Msg;
	return R;
}

static void HyInitLs(FNsLockstepClient& C0, FNsLockstepClient& C1)
{
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;
}

static void HyWarmLockstep(FNsFakeNet& Net, FNsLockstepServer& Sv, FNsLockstepClient& C0, FNsLockstepClient& C1, int32 Steps)
{
	for (int32 S = 0; S < Steps; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C0);
		NsPumpLockstepClient(Net, C1);
		Net.Advance(Ns::LogicDtMs);
	}
}

static bool HyForceDesync(FNsLockstepServer& Sv)
{
	const uint32* Found = Sv.Checksums.Find(Ns::ChecksumEvery);
	if (!Found)
	{
		return false;
	}
	Sv.OnChecksum(Ns::ChecksumEvery, *Found ^ 1u);
	return Sv.bDesync;
}

static bool HyAligned(const FNsLockstepServer& Sv, const FNsLockstepResync& Repair,
	const FNsLockstepClient& C0, const FNsLockstepClient& C1)
{
	return Repair.bCaptured
		&& C0.World.Equals(Repair.LiveSnap)
		&& C1.World.Equals(Repair.LiveSnap)
		&& Sv.World.Equals(Repair.LiveSnap)
		&& C0.ExecFrame == Repair.LiveSnapTick
		&& C1.ExecFrame == Repair.LiveSnapTick
		&& Sv.Frame == Repair.LiveSnapTick;
}

FNsSelfTestResult NsRunLockstepResyncAlignSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepServer Sv;
	FNsLockstepClient C0;
	FNsLockstepClient C1;
	HyInitLs(C0, C1);
	HyWarmLockstep(Net, Sv, C0, C1, 24);
	if (!HyForceDesync(Sv))
	{
		return HyFail(TEXT("hybrid-repair-align: no checksum record"));
	}
	const int32 FrameAt = Sv.Frame;
	FNsLockstepResync Repair;
	NsPumpLockstepResyncServer(Net, Sv, Repair);
	NsPumpLockstepResyncClient(Net, C0);
	NsPumpLockstepResyncClient(Net, C1);
	if (Sv.Frame != FrameAt)
	{
		return HyFailStr(FString::Printf(TEXT("hybrid-repair-align: ticked frame=%d was=%d"), Sv.Frame, FrameAt));
	}
	if (!HyAligned(Sv, Repair, C0, C1))
	{
		return HyFail(TEXT("hybrid-repair-align: worlds not pulled to live snap"));
	}
	if (Repair.bGiveUp)
	{
		return HyFail(TEXT("hybrid-repair-align: gave up"));
	}
	return HyOk(TEXT("hybrid-repair-align"));
}

FNsSelfTestResult NsRunLockstepResyncForceSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepServer Sv;
	FNsLockstepClient C0;
	FNsLockstepClient C1;
	HyInitLs(C0, C1);
	HyWarmLockstep(Net, Sv, C0, C1, 24);
	if (!HyForceDesync(Sv))
	{
		return HyFail(TEXT("hybrid-repair-force: no checksum record"));
	}
	FNsLockstepResync Repair;
	NsPumpLockstepResyncServer(Net, Sv, Repair);
	C0.ExecFrame = Repair.LiveSnapTick + 8;
	C0.World.X[0] = 999;
	C0.World.Rng = 7;
	NsPumpLockstepResyncClient(Net, C0);
	NsPumpLockstepResyncClient(Net, C1);
	if (C0.ExecFrame != Repair.LiveSnapTick || !C0.World.Equals(Repair.LiveSnap))
	{
		return HyFail(TEXT("hybrid-repair-force: ahead client not rewound"));
	}
	return HyOk(TEXT("hybrid-repair-force"));
}

FNsSelfTestResult NsRunLockstepResyncIgnoreFrameSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepServer Sv;
	FNsLockstepClient C0;
	FNsLockstepClient C1;
	HyInitLs(C0, C1);
	HyWarmLockstep(Net, Sv, C0, C1, 24);
	if (!HyForceDesync(Sv))
	{
		return HyFail(TEXT("hybrid-repair-ignore: no checksum record"));
	}
	FNsLockstepResync Repair;
	NsPumpLockstepResyncServer(Net, Sv, Repair);
	NsPumpLockstepResyncClient(Net, C0);
	NsPumpLockstepResyncClient(Net, C1);
	const FNsWorld Before = C0.World;
	const int32 Exec = C0.ExecFrame;
	FNsInputs In;
	In.Dx[0] = 1;
	In.Dx[1] = 1;
	TMap<int32, FNsInputs> Extra;
	Extra.Add(Exec, In);
	C0.OnS2C(Extra);
	NsPumpLockstepResyncClient(Net, C0);
	if (!C0.World.Equals(Before) || C0.ExecFrame != Exec)
	{
		return HyFail(TEXT("hybrid-repair-ignore: S2CFrame still stepped"));
	}
	return HyOk(TEXT("hybrid-repair-ignore-frame"));
}

FNsSelfTestResult NsRunLockstepResyncDropSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	Net.Rng.Initialize(1);
	FNsLockstepServer Sv;
	FNsLockstepClient C0;
	FNsLockstepClient C1;
	HyInitLs(C0, C1);
	HyWarmLockstep(Net, Sv, C0, C1, 24);
	if (!HyForceDesync(Sv))
	{
		return HyFail(TEXT("hybrid-repair-drop: no checksum record"));
	}
	Net.Drop = 0.1f;
	FNsLockstepResync Repair;
	for (int32 i = 0; i < Ns::ResyncGiveUpPumps; ++i)
	{
		NsPumpLockstepResyncServer(Net, Sv, Repair);
		NsPumpLockstepResyncClient(Net, C0);
		NsPumpLockstepResyncClient(Net, C1);
		Net.Advance(1.0);
		if (HyAligned(Sv, Repair, C0, C1))
		{
			return HyOk(TEXT("hybrid-repair-drop"));
		}
	}
	return HyFail(TEXT("hybrid-repair-drop: did not align"));
}

FNsSelfTestResult NsRunLockstepResyncApplyJoinSelfTest()
{
	FNsLockstepClient C;
	C.ExecFrame = 10;
	C.World.X[0] = 5;
	C.World.Rng = 3;
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::S2CJoinSnap;
	Pkt.Tick = 10;
	Pkt.SnapX[0] = 99;
	Pkt.SnapRng = 8;
	C.ApplyJoin(Pkt);
	if (C.World.X[0] != 5 || C.ExecFrame != 10)
	{
		return HyFail(TEXT("hybrid-repair-applyjoin: equal tick jumped"));
	}
	Pkt.Tick = 11;
	C.ApplyJoin(Pkt);
	if (C.World.X[0] != 99 || C.ExecFrame != 11)
	{
		return HyFail(TEXT("hybrid-repair-applyjoin: future tick did not jump"));
	}
	return HyOk(TEXT("hybrid-repair-applyjoin-guard"));
}

static void HyInitOverlay(FNsLockstepDoorServer& Sv, FNsLockstepDoorClient& C0, FNsLockstepDoorClient& C1)
{
	C0.Ls.PlayerId = 0;
	C0.Ls.Addr = ENsAddr::C0;
	C1.Ls.PlayerId = 1;
	C1.Ls.Addr = ENsAddr::C1;
}

static void HyWarmOverlay(FNsFakeNet& Net, FNsLockstepDoorServer& Sv, FNsLockstepDoorClient& C0, FNsLockstepDoorClient& C1, int32 Steps)
{
	for (int32 S = 0; S < Steps; ++S)
	{
		C0.Ls.SendInput(Net, 1);
		C1.Ls.SendInput(Net, -1);
		NsPumpLockstepDoorServer(Net, Sv);
		NsPumpLockstepDoorClient(Net, C0);
		NsPumpLockstepDoorClient(Net, C1);
		Net.Advance(Ns::LogicDtMs);
	}
}

FNsSelfTestResult NsRunLockstepDoorCleanSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepDoorServer Sv;
	FNsLockstepDoorClient C0;
	FNsLockstepDoorClient C1;
	HyInitOverlay(Sv, C0, C1);
	Sv.SetOpen(Net, 1);
	HyWarmOverlay(Net, Sv, C0, C1, 40);
	if (!C0.Ls.World.Equals(C1.Ls.World) || C0.Ls.ExecFrame < 20)
	{
		return HyFail(TEXT("hybrid-overlay-clean: pawns"));
	}
	if (C0.Door.Open != 1 || C1.Door.Open != 1 || Sv.Door.Open != 1)
	{
		return HyFail(TEXT("hybrid-overlay-clean: gate"));
	}
	return HyOk(TEXT("hybrid-overlay-clean"));
}

FNsSelfTestResult NsRunLockstepDoorDropOpenSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	Net.bDropType = true;
	Net.DropType = ENsMsg::S2CDoorOpen;
	FNsLockstepDoorServer Sv;
	FNsLockstepDoorClient C0;
	FNsLockstepDoorClient C1;
	HyInitOverlay(Sv, C0, C1);
	Sv.SetOpen(Net, 1);
	HyWarmOverlay(Net, Sv, C0, C1, 40);
	if (!C0.Ls.World.Equals(C1.Ls.World) || C0.Ls.ExecFrame < 20)
	{
		return HyFail(TEXT("hybrid-overlay-dropgate: pawns diverged"));
	}
	if (C0.Door.Open != 0 || C1.Door.Open != 0)
	{
		return HyFail(TEXT("hybrid-overlay-dropgate: gate should stay closed"));
	}
	if (Sv.Door.Open != 1)
	{
		return HyFail(TEXT("hybrid-overlay-dropgate: server gate"));
	}
	return HyOk(TEXT("hybrid-overlay-drop-gate"));
}

FNsSelfTestResult NsRunLockstepDoorDropFrameSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.1f;
	Net.RttMs = 80.f;
	Net.JitterMs = 8.f;
	Net.Rng.Initialize(1);
	FNsLockstepDoorServer Sv;
	FNsLockstepDoorClient C0;
	FNsLockstepDoorClient C1;
	HyInitOverlay(Sv, C0, C1);
	Sv.SetOpen(Net, 1);
	const int32 Frames = 90;
	const double End = Frames * Ns::LogicDtMs;
	while (Net.Now < End)
	{
		C0.Ls.SendInput(Net, 1);
		C1.Ls.SendInput(Net, -1);
		NsPumpLockstepDoorServer(Net, Sv);
		NsPumpLockstepDoorClient(Net, C0);
		NsPumpLockstepDoorClient(Net, C1);
		Net.Advance(1.0);
	}
	for (int32 i = 0; i < 200; ++i)
	{
		NsPumpLockstepDoorServer(Net, Sv);
		NsPumpLockstepDoorClient(Net, C0);
		NsPumpLockstepDoorClient(Net, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	if (C0.Ls.ExecFrame <= 40 || !C0.Ls.World.Equals(C1.Ls.World))
	{
		return HyFail(TEXT("hybrid-overlay-dropframe: pawns"));
	}
	if (C0.Door.Open != 1 || C1.Door.Open != 1)
	{
		return HyFail(TEXT("hybrid-overlay-dropframe: gate"));
	}
	return HyOk(TEXT("hybrid-overlay-drop-frame"));
}

FNsSelfTestResult NsRunLockstepDoorIgnoreSnapSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepDoorServer Sv;
	FNsLockstepDoorClient C0;
	FNsLockstepDoorClient C1;
	HyInitOverlay(Sv, C0, C1);
	HyWarmOverlay(Net, Sv, C0, C1, 12);
	const int32 X0 = C0.Ls.World.X[0];
	FNsPacket Snap;
	Snap.Type = ENsMsg::S2CSnapshot;
	Snap.Tick = 99;
	Snap.SnapX[0] = 999;
	Snap.SnapX[1] = -999;
	Net.Send(ENsAddr::Sv, ENsAddr::C0, Snap);
	NsPumpLockstepDoorClient(Net, C0);
	if (C0.Ls.World.X[0] != X0)
	{
		return HyFail(TEXT("hybrid-overlay-ignoresnap: snapshot wrote X"));
	}
	return HyOk(TEXT("hybrid-overlay-ignore-snap"));
}

FNsSelfTestResult NsRunLockstepDoorNotInStepSelfTest()
{
	FNsFakeNet A;
	FNsFakeNet B;
	A.Drop = 0.f;
	A.RttMs = 0.f;
	A.JitterMs = 0.f;
	B.Drop = 0.f;
	B.RttMs = 0.f;
	B.JitterMs = 0.f;
	FNsLockstepDoorServer Ov;
	FNsLockstepDoorClient Oc0;
	FNsLockstepDoorClient Oc1;
	HyInitOverlay(Ov, Oc0, Oc1);
	Ov.SetOpen(A, 1);
	FNsLockstepServer Ls;
	FNsLockstepClient Lc0;
	FNsLockstepClient Lc1;
	HyInitLs(Lc0, Lc1);
	for (int32 S = 0; S < 30; ++S)
	{
		Oc0.Ls.SendInput(A, 1);
		Oc1.Ls.SendInput(A, -1);
		NsPumpLockstepDoorServer(A, Ov);
		NsPumpLockstepDoorClient(A, Oc0);
		NsPumpLockstepDoorClient(A, Oc1);
		A.Advance(Ns::LogicDtMs);

		Lc0.SendInput(B, 1);
		Lc1.SendInput(B, -1);
		NsPumpLockstepServer(B, Ls);
		NsPumpLockstepClient(B, Lc0);
		NsPumpLockstepClient(B, Lc1);
		B.Advance(Ns::LogicDtMs);
	}
	if (!Oc0.Ls.World.Equals(Lc0.World) || Oc0.Door.Open != 1)
	{
		return HyFail(TEXT("hybrid-overlay-gate-not-in-f: X moved with gate"));
	}
	return HyOk(TEXT("hybrid-overlay-gate-not-in-f"));
}
