// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsSelfTest.h"
#include "NsTypes.h"
#include "NsFakeNet.h"
#include "NsUdpNet.h"
#include "NsLockstep.h"
#include "NsStateSync.h"
#include "NsRollback.h"
#include "NsCodec.h"
#include "NsPump.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Containers/Set.h"

DEFINE_LOG_CATEGORY_STATIC(LogNetworkSync, Log, All);

static FNsSelfTestResult Fail(const TCHAR* Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult FailStr(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult OkStr(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = true;
	R.Detail = Msg;
	return R;
}

static bool RoundTripPacket(const FNsPacket& Src, FNsPacket& Out)
{
	TArray<uint8> Bytes;
	if (!NsEncodePacket(Src, Bytes))
	{
		return false;
	}
	return NsDecodePacket(Bytes, Out);
}

FNsSelfTestResult NsRunLockstepSelfTest()
{
	FNsFakeNet Net;
	Net.RttMs = 80.f;
	Net.Drop = 0.1f;
	Net.JitterMs = 8.f;
	Net.Rng.Initialize(1);

	FNsLockstepServer Sv;
	FNsLockstepClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	FNsLockstepClient C1;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;

	const int8 Script[][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, 1}};
	const int32 Frames = 90;
	const double End = Frames * Ns::LogicDtMs;
	while (Net.Now < End)
	{
		const int32 Step = static_cast<int32>(Net.Now / Ns::LogicDtMs);
		const int8* Pair = Script[Step % 4];
		C0.SendInput(Net, Pair[0]);
		C1.SendInput(Net, Pair[1]);
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C0);
		NsPumpLockstepClient(Net, C1);
		Net.Advance(1.0);
	}

	for (int32 i = 0; i < 200; ++i)
	{
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C0);
		NsPumpLockstepClient(Net, C1);
		Net.Advance(Ns::LogicDtMs);
	}

	if (C0.ExecFrame <= 40)
	{
		return Fail(TEXT("lockstep: too few frames"));
	}
	if (!C0.World.Equals(C1.World))
	{
		return Fail(TEXT("lockstep: worlds diverged"));
	}
	if (Sv.bDesync)
	{
		return Fail(TEXT("lockstep: checksum desync"));
	}
	if (Sv.ChecksumOk <= 0)
	{
		return Fail(TEXT("lockstep: no checksum ack"));
	}
	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = FString::Printf(TEXT("lockstep frames=%d checksums=%d x=(%d,%d)"),
		C0.ExecFrame, Sv.ChecksumOk, C0.World.X[0], C0.World.X[1]);
	return Ok;
}

FNsSelfTestResult NsRunLockstepJoinSelfTest()
{
	FNsFakeNet Net;
	Net.RttMs = 80.f;
	Net.Drop = 0.f;
	Net.JitterMs = 4.f;
	Net.Rng.Initialize(1);

	FNsLockstepServer Sv;
	FNsLockstepClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	FNsLockstepClient C1;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;

	const int8 Script[][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, 1}};
	const int32 Frames = 90;
	const double End = Frames * Ns::LogicDtMs;
	while (Net.Now < End)
	{
		const int32 Step = static_cast<int32>(Net.Now / Ns::LogicDtMs);
		const int8* Pair = Script[Step % 4];
		C0.SendInput(Net, Pair[0]);
		C1.SendInput(Net, Pair[1]);
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C0);
		NsPumpLockstepClient(Net, C1);
		Net.Advance(1.0);
	}

	if (Sv.SnapFrame < 0)
	{
		return Fail(TEXT("lockstep-join: no snapshot"));
	}

	C1.World.Reset();
	C1.ExecFrame = 0;
	C1.Buf.Reset();
	C1.PrevX[0] = 0;
	C1.PrevX[1] = 0;
	Sv.SendJoin(Net, C1.Addr);

	for (int32 i = 0; i < 200; ++i)
	{
		const int8* Pair = Script[i % 4];
		C0.SendInput(Net, Pair[0]);
		C1.SendInput(Net, Pair[1]);
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C0);
		NsPumpLockstepClient(Net, C1);
		Net.Advance(Ns::LogicDtMs);
	}

	if (C1.ExecFrame <= 40)
	{
		return Fail(TEXT("lockstep-join: late client stuck"));
	}
	if (C1.ExecFrame != C0.ExecFrame)
	{
		return Fail(TEXT("lockstep-join: exec frame mismatch"));
	}
	if (!C0.World.Equals(C1.World))
	{
		return Fail(TEXT("lockstep-join: worlds diverged after join"));
	}
	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = FString::Printf(TEXT("lockstep-join snap=%d exec=%d x=(%d,%d)"),
		Sv.SnapFrame, C1.ExecFrame, C1.World.X[0], C1.World.X[1]);
	return Ok;
}

FNsSelfTestResult NsRunLockstepLateJoinSelfTest()
{
	FNsFakeNet Net;
	Net.RttMs = 0.f;
	Net.Drop = 0.f;
	Net.JitterMs = 0.f;
	Net.Rng.Initialize(1);

	FNsLockstepServer Sv;
	FNsLockstepClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	FNsLockstepClient C1;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;

	for (int32 i = 0; i < 24; ++i)
	{
		C0.SendInput(Net, 1);
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C0);
		TArray<FNsPacket> Dropped;
		Net.Drain(ENsAddr::C1, Dropped);
		Net.Advance(Ns::LogicDtMs);
	}
	if (C1.ExecFrame != 0)
	{
		return Fail(TEXT("lockstep-late: silent client moved"));
	}

	FNsPacket Spoof;
	Spoof.Type = ENsMsg::C2SInput;
	Spoof.PlayerId = 0;
	Spoof.Dx = -1;
	Net.Send(ENsAddr::C1, ENsAddr::Sv, Spoof);
	Net.Advance(1.0);
	const int8 Before0 = Sv.Latest.Dx[0];
	NsPumpLockstepServer(Net, Sv);
	if (Sv.Latest.Dx[0] != Before0 || Sv.Latest.Dx[1] != -1)
	{
		return Fail(TEXT("lockstep-late: PlayerId spoof applied"));
	}

	for (int32 i = 0; i < 40; ++i)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, 0);
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C0);
		NsPumpLockstepClient(Net, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	if (C1.ExecFrame <= 8)
	{
		return FailStr(FString::Printf(TEXT("lockstep-late: still stuck exec=%d"), C1.ExecFrame));
	}
	if (!C0.World.Equals(C1.World))
	{
		return Fail(TEXT("lockstep-late: worlds diverged"));
	}
	return OkStr(FString::Printf(TEXT("lockstep-late exec=%d"), C1.ExecFrame));
}

FNsSelfTestResult NsRunLockstepNoSkipSelfTest()
{
	FNsFakeNet Net;
	FNsLockstepClient C;
	C.Addr = ENsAddr::C0;
	FNsInputs A;
	A.Dx[0] = 1;
	FNsInputs B;
	B.Dx[0] = -1;
	TMap<int32, FNsInputs> Frames;
	Frames.Add(0, A);
	Frames.Add(2, B);
	C.OnS2C(Frames);
	C.Logic(Net);
	if (C.ExecFrame != 1)
	{
		return FailStr(FString::Printf(TEXT("lockstep-noskip: jumped to %d"), C.ExecFrame));
	}
	if (!C.Buf.Contains(2))
	{
		return Fail(TEXT("lockstep-noskip: lost future frame"));
	}
	TMap<int32, FNsInputs> Mid;
	Mid.Add(1, A);
	C.OnS2C(Mid);
	C.Logic(Net);
	if (C.ExecFrame != 3)
	{
		return FailStr(FString::Printf(TEXT("lockstep-noskip: after fill exec=%d"), C.ExecFrame));
	}
	return OkStr(TEXT("lockstep-noskip hole then catch"));
}

FNsSelfTestResult NsRunLockstepNackSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;

	FNsLockstepServer Sv;
	FNsLockstepClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	FNsLockstepClient C1;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;

	auto StepBoth = [&]()
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C0);
		NsPumpLockstepClient(Net, C1);
		Net.Advance(Ns::LogicDtMs);
	};

	for (int32 i = 0; i < 8; ++i)
	{
		StepBoth();
	}

	for (int32 i = 0; i < 6; ++i)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C1);
		TArray<FNsPacket> Dropped;
		Net.Drain(ENsAddr::C0, Dropped);
		Net.Advance(Ns::LogicDtMs);
	}

	const int32 Hole = C0.ExecFrame;
	C0.SendInput(Net, 1);
	C1.SendInput(Net, -1);
	NsPumpLockstepServer(Net, Sv);
	NsPumpLockstepClient(Net, C0);
	NsPumpLockstepClient(Net, C1);
	if (C0.ExecFrame != Hole || C0.Buf.Contains(Hole) || C0.ExecFrame >= C1.ExecFrame)
	{
		return FailStr(FString::Printf(TEXT("lockstep-nack: no hole exec=%d peer=%d"), C0.ExecFrame, C1.ExecFrame));
	}

	NsPumpLockstepServer(Net, Sv);
	TArray<FNsPacket> Reply;
	NsDrain(Net, ENsAddr::C0, Reply);
	bool bFrame = false;
	for (const FNsPacket& P : Reply)
	{
		if (P.Type == ENsMsg::S2CJoinSnap)
		{
			return Fail(TEXT("lockstep-nack: used Join"));
		}
		if (P.Type == ENsMsg::S2CFrame && P.Frames.Contains(Hole))
		{
			bFrame = true;
			C0.OnS2C(P.Frames);
		}
	}
	if (!bFrame)
	{
		return Fail(TEXT("lockstep-nack: no Hist replay"));
	}
	C0.Logic(Net);
	if (C0.ExecFrame != C1.ExecFrame)
	{
		return FailStr(FString::Printf(TEXT("lockstep-nack: stuck exec=%d peer=%d"), C0.ExecFrame, C1.ExecFrame));
	}
	if (!C0.World.Equals(C1.World))
	{
		return Fail(TEXT("lockstep-nack: worlds diverged"));
	}
	return OkStr(FString::Printf(TEXT("lockstep-nack hole=%d exec=%d"), Hole, C0.ExecFrame));
}

FNsSelfTestResult NsRunLockstepNackJoinSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;

	FNsLockstepServer Sv;
	FNsLockstepClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	FNsLockstepClient C1;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;

	for (int32 i = 0; i < 5; ++i)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C0);
		NsPumpLockstepClient(Net, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	const int32 Stale = C0.ExecFrame;

	for (int32 i = 0; i < Ns::JoinSnapEvery + 10; ++i)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C1);
		TArray<FNsPacket> Dropped;
		Net.Drain(ENsAddr::C0, Dropped);
		Net.Advance(Ns::LogicDtMs);
	}

	if (Sv.Hist.Contains(Stale) || Sv.SnapFrame < 0)
	{
		return Fail(TEXT("lockstep-nack-join: Hist still has stale"));
	}

	FNsPacket Nack;
	Nack.Type = ENsMsg::C2SFrameNack;
	Nack.PlayerId = 0;
	Nack.SeqWindow.Add(Stale);
	Net.Send(ENsAddr::C0, ENsAddr::Sv, Nack);
	NsPumpLockstepServer(Net, Sv);
	NsPumpLockstepClient(Net, C0);
	NsPumpLockstepClient(Net, C1);
	if (C0.ExecFrame <= Stale + 8)
	{
		return FailStr(FString::Printf(TEXT("lockstep-nack-join: still stuck exec=%d"), C0.ExecFrame));
	}
	if (!C0.World.Equals(C1.World) || C0.ExecFrame != C1.ExecFrame)
	{
		return FailStr(FString::Printf(TEXT("lockstep-nack-join: worlds exec=%d peer=%d"), C0.ExecFrame, C1.ExecFrame));
	}
	return OkStr(FString::Printf(TEXT("lockstep-nack-join stale=%d exec=%d"), Stale, C0.ExecFrame));
}

FNsSelfTestResult NsRunLockstepJoinFragSelfTest()
{
	FNsFakeNet Net;
	FNsLockstepClient C;
	C.Addr = ENsAddr::C0;
	C.ExecFrame = 70;
	FNsInputs Keep;
	Keep.Dx[0] = 1;
	C.Buf.Add(80, Keep);

	FNsInputs In76;
	In76.Dx[0] = 1;
	In76.Dx[1] = -1;
	FNsPacket Part0;
	Part0.Type = ENsMsg::S2CJoinSnap;
	Part0.Tick = 76;
	Part0.SnapX[0] = 24;
	Part0.SnapX[1] = -8;
	Part0.SnapRng = 42;
	Part0.Frames.Add(76, In76);
	C.ApplyJoin(Part0);
	if (C.ExecFrame != 76)
	{
		return FailStr(FString::Printf(TEXT("lockstep-joinfrag: exec=%d"), C.ExecFrame));
	}
	if (C.World.X[0] != 24 || C.World.Rng != 42)
	{
		return Fail(TEXT("lockstep-joinfrag: snap not applied"));
	}
	if (!C.Buf.Contains(80))
	{
		return Fail(TEXT("lockstep-joinfrag: wiped future buf"));
	}

	FNsInputs In77;
	In77.Dx[1] = 1;
	FNsPacket Part1;
	Part1.Type = ENsMsg::S2CJoinSnap;
	Part1.Tick = 76;
	Part1.SnapX[0] = 0;
	Part1.SnapX[1] = 0;
	Part1.SnapRng = 1;
	Part1.Frames.Add(77, In77);
	C.ApplyJoin(Part1);
	if (C.World.X[0] != 24 || C.World.Rng != 42)
	{
		return Fail(TEXT("lockstep-joinfrag: second fragment rewound snap"));
	}
	C.Logic(Net);
	if (C.ExecFrame != 78)
	{
		return FailStr(FString::Printf(TEXT("lockstep-joinfrag: after parts exec=%d"), C.ExecFrame));
	}
	if (!C.Buf.Contains(80))
	{
		return Fail(TEXT("lockstep-joinfrag: lost frame 80"));
	}
	return OkStr(TEXT("lockstep-joinfrag merge buf"));
}

FNsSelfTestResult NsRunStateSyncSelfTest()
{
	FNsFakeNet Net;
	Net.RttMs = 80.f;
	Net.Drop = 0.05f;
	Net.JitterMs = 4.f;
	Net.Rng.Initialize(1);

	FNsStateSyncServer Sv;
	FNsStateSyncClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	FNsStateSyncClient C1;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;

	const int8 Script[] = {1, 1, 0, -1, -1, 0};
	for (int32 S = 0; S < 240; ++S)
	{
		C0.LocalTick(Net, Script[S % 6]);
		C1.LocalTick(Net, Script[(S + 3) % 6]);
		NsPumpStateServer(Net, Sv);
		NsPumpStateClient(Net, C0);
		NsPumpStateClient(Net, C1);
		Net.Advance(Ns::SimDtMs);
	}

	for (int32 i = 0; i < 40; ++i)
	{
		NsPumpStateServer(Net, Sv);
		NsPumpStateClient(Net, C0);
		NsPumpStateClient(Net, C1);
		Net.Advance(Ns::SimDtMs);
	}

	if (C0.PredX != Sv.Pawns[0].X || C1.PredX != Sv.Pawns[1].X)
	{
		return Fail(TEXT("state-sync: prediction mismatch"));
	}
	if (!C0.bHasRemote)
	{
		return Fail(TEXT("state-sync: no remote lerp"));
	}
	if (Sv.LastAck[0] <= 0 || Sv.LastAck[1] <= 0)
	{
		return Fail(TEXT("state-sync: missing snap ack"));
	}
	if (!C0.bGotDelta && !C1.bGotDelta)
	{
		return Fail(TEXT("state-sync: no delta snapshot"));
	}
	(void)Sv.RewindX(0, 80);
	(void)Sv.RewindX(0, 500);

	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = FString::Printf(TEXT("state-sync tick=%d delta=%d ack=%d x=(%d,%d)"),
		Sv.Tick, Sv.DeltaSent, Sv.LastAck[0], Sv.Pawns[0].X, Sv.Pawns[1].X);
	return Ok;
}

FNsSelfTestResult NsRunRollbackSelfTest()
{
	FNsFakeNet Net;
	Net.RttMs = 80.f;
	Net.Drop = 0.05f;
	Net.JitterMs = 6.f;
	Net.Rng.Initialize(1);

	FNsRollbackPeer A;
	A.PlayerId = 0;
	A.Addr = ENsAddr::C0;
	A.Other = ENsAddr::C1;
	FNsRollbackPeer B;
	B.PlayerId = 1;
	B.Addr = ENsAddr::C1;
	B.Other = ENsAddr::C0;

	const int8 S0[] = {1, 1, 1, 0, -1, -1, 0, 1};
	const int8 S1[] = {0, -1, -1, 1, 1, 0, 0, -1};
	for (int32 S = 0; S < 120; ++S)
	{
		A.Advance(Net, S0[S % 8]);
		B.Advance(Net, S1[S % 8]);
		NsPumpRollbackPeer(Net, A);
		NsPumpRollbackPeer(Net, B);
		Net.Advance(Ns::RollbackDtMs);
	}

	for (int32 i = 0; i < 80; ++i)
	{
		A.Advance(Net, 0);
		B.Advance(Net, 0);
		NsPumpRollbackPeer(Net, A);
		NsPumpRollbackPeer(Net, B);
		Net.Advance(Ns::RollbackDtMs);
	}

	if (!A.World.Equals(B.World))
	{
		return Fail(TEXT("rollback: peers diverged"));
	}
	if (A.bWaiting || B.bWaiting)
	{
		return FailStr(FString::Printf(
			TEXT("rollback: still waiting after cooldown a(frame=%d conf=%d wait=%d) b(frame=%d conf=%d wait=%d)"),
			A.Frame, A.Confirmed, A.WaitCount, B.Frame, B.Confirmed, B.WaitCount));
	}
	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = FString::Printf(TEXT("rollback frame=%d wait=%d x=(%d,%d)"),
		A.Frame, A.WaitCount + B.WaitCount, A.World.X[0], A.World.X[1]);
	return Ok;
}

FNsSelfTestResult NsRunUdpLoopbackSelfTest()
{
	FNsUdpNet Net;
	if (!Net.BindLoopback())
	{
		return Fail(TEXT("udp: bind failed"));
	}
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Pkt.PlayerId = 1;
	Pkt.Dx = -1;
	Net.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
	TArray<FNsPacket> Got;
	NsDrain(Net, ENsAddr::Sv, Got, true);
	if (Got.Num() != 1)
	{
		return Fail(TEXT("udp: no datagram"));
	}
	if (Got[0].Type != ENsMsg::C2SInput || Got[0].PlayerId != 1 || Got[0].Dx != -1)
	{
		return Fail(TEXT("udp: payload mismatch"));
	}
	if (Got[0].Src != ENsAddr::C0 || Got[0].Seq <= 0)
	{
		return Fail(TEXT("udp: src/seq mismatch"));
	}
	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = FString::Printf(TEXT("udp ports=%d,%d,%d"),
		Net.BoundPort(ENsAddr::Sv), Net.BoundPort(ENsAddr::C0), Net.BoundPort(ENsAddr::C1));
	return Ok;
}

FNsSelfTestResult NsRunUdpLockstepSelfTest()
{
	FNsUdpNet Net;
	if (!Net.BindLoopback())
	{
		return Fail(TEXT("udp-lockstep: bind failed"));
	}
	FNsLockstepServer Sv;
	FNsLockstepClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	FNsLockstepClient C1;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;
	const int8 Script[][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, 1}};
	for (int32 S = 0; S < 45; ++S)
	{
		const int8* Pair = Script[S % 4];
		C0.SendInput(Net, Pair[0]);
		C1.SendInput(Net, Pair[1]);
		NsPumpLockstepServer(Net, Sv, true);
		NsPumpLockstepClient(Net, C0, true);
		NsPumpLockstepClient(Net, C1, true);
		Net.Advance(Ns::LogicDtMs);
	}
	for (int32 i = 0; i < 20; ++i)
	{
		NsPumpLockstepServer(Net, Sv, true);
		NsPumpLockstepClient(Net, C0, true);
		NsPumpLockstepClient(Net, C1, true);
		Net.Advance(Ns::LogicDtMs);
	}
	if (C0.ExecFrame <= 10)
	{
		return Fail(TEXT("udp-lockstep: too few frames"));
	}
	if (!C0.World.Equals(C1.World))
	{
		return Fail(TEXT("udp-lockstep: worlds diverged"));
	}
	if (Sv.bDesync)
	{
		return Fail(TEXT("udp-lockstep: checksum desync"));
	}
	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = FString::Printf(TEXT("udp-lockstep frames=%d x=(%d,%d)"),
		C0.ExecFrame, C0.World.X[0], C0.World.X[1]);
	return Ok;
}

FNsSelfTestResult NsRunUdpStateSyncSelfTest()
{
	FNsUdpNet Net;
	if (!Net.BindLoopback())
	{
		return Fail(TEXT("udp-state: bind failed"));
	}
	FNsStateSyncServer Sv;
	FNsStateSyncClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	FNsStateSyncClient C1;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;
	const int8 Script[] = {1, 1, 0, -1, -1, 0};
	for (int32 S = 0; S < 80; ++S)
	{
		C0.LocalTick(Net, Script[S % 6]);
		C1.LocalTick(Net, Script[(S + 3) % 6]);
		NsPumpStateServer(Net, Sv, true);
		NsPumpStateClient(Net, C0, true);
		NsPumpStateClient(Net, C1, true);
		Net.Advance(Ns::SimDtMs);
	}
	for (int32 i = 0; i < 20; ++i)
	{
		NsPumpStateServer(Net, Sv, true);
		NsPumpStateClient(Net, C0, true);
		NsPumpStateClient(Net, C1, true);
		Net.Advance(Ns::SimDtMs);
	}
	if (C0.PredX != Sv.Pawns[0].X || C1.PredX != Sv.Pawns[1].X)
	{
		return FailStr(FString::Printf(TEXT("udp-state: pred=(%d,%d) sv=(%d,%d)"),
			C0.PredX, C1.PredX, Sv.Pawns[0].X, Sv.Pawns[1].X));
	}
	if (Sv.LastAck[0] <= 0 || Sv.LastAck[1] <= 0)
	{
		return Fail(TEXT("udp-state: missing snap ack"));
	}
	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = FString::Printf(TEXT("udp-state tick=%d ack=%d x=(%d,%d)"),
		Sv.Tick, Sv.LastAck[0], Sv.Pawns[0].X, Sv.Pawns[1].X);
	return Ok;
}

FNsSelfTestResult NsRunUdpRollbackSelfTest()
{
	FNsUdpNet Net;
	if (!Net.BindLoopback())
	{
		return Fail(TEXT("udp-rollback: bind failed"));
	}
	FNsRollbackPeer A;
	A.PlayerId = 0;
	A.Addr = ENsAddr::C0;
	A.Other = ENsAddr::C1;
	FNsRollbackPeer B;
	B.PlayerId = 1;
	B.Addr = ENsAddr::C1;
	B.Other = ENsAddr::C0;
	const int8 S0[] = {1, 1, 1, 0, -1, -1, 0, 1};
	const int8 S1[] = {0, -1, -1, 1, 1, 0, 0, -1};
	for (int32 S = 0; S < 80; ++S)
	{
		A.Advance(Net, S0[S % 8]);
		B.Advance(Net, S1[S % 8]);
		NsPumpRollbackPeer(Net, A, true);
		NsPumpRollbackPeer(Net, B, true);
		Net.Advance(Ns::RollbackDtMs);
	}
	for (int32 i = 0; i < 40; ++i)
	{
		A.Advance(Net, 0);
		B.Advance(Net, 0);
		NsPumpRollbackPeer(Net, A, true);
		NsPumpRollbackPeer(Net, B, true);
		Net.Advance(Ns::RollbackDtMs);
	}
	if (!A.World.Equals(B.World))
	{
		return Fail(TEXT("udp-rollback: peers diverged"));
	}
	if (A.bWaiting || B.bWaiting)
	{
		return FailStr(FString::Printf(
			TEXT("udp-rollback: still waiting a(frame=%d conf=%d) b(frame=%d conf=%d)"),
			A.Frame, A.Confirmed, B.Frame, B.Confirmed));
	}
	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = FString::Printf(TEXT("udp-rollback frame=%d x=(%d,%d)"),
		A.Frame, A.World.X[0], A.World.X[1]);
	return Ok;
}

FNsSelfTestResult NsRunUdpPeersSelfTest()
{
	FNsUdpNet A;
	FNsUdpNet B;
	if (!A.Bind(ENsAddr::C0, 0, false) || !B.Bind(ENsAddr::Sv, 0, false))
	{
		return Fail(TEXT("udp-peers: bind failed"));
	}
	if (!A.SetPeer(ENsAddr::Sv, TEXT("127.0.0.1"), B.BoundPort(ENsAddr::Sv))
		|| !B.SetPeer(ENsAddr::C0, TEXT("127.0.0.1"), A.BoundPort(ENsAddr::C0)))
	{
		return Fail(TEXT("udp-peers: set peer failed"));
	}
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Pkt.PlayerId = 0;
	Pkt.Dx = 1;
	A.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
	TArray<FNsPacket> Got;
	NsDrain(B, ENsAddr::Sv, Got, true);
	if (Got.Num() != 1 || Got[0].Dx != 1 || Got[0].Src != ENsAddr::C0)
	{
		return Fail(TEXT("udp-peers: datagram mismatch"));
	}
	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = FString::Printf(TEXT("udp-peers c0=%d sv=%d"),
		A.BoundPort(ENsAddr::C0), B.BoundPort(ENsAddr::Sv));
	return Ok;
}

FNsSelfTestResult NsRunUdpSplitLockstepSelfTest()
{
	FNsUdpNet Host;
	FNsUdpNet Client;
	if (!Host.Bind(ENsAddr::Sv, 0, false) || !Host.Bind(ENsAddr::C0, 0, false)
		|| !Client.Bind(ENsAddr::C1, 0, false))
	{
		return Fail(TEXT("udp-split: bind failed"));
	}
	if (!Host.SetPeer(ENsAddr::C1, TEXT("127.0.0.1"), Client.BoundPort(ENsAddr::C1))
		|| !Client.SetPeer(ENsAddr::Sv, TEXT("127.0.0.1"), Host.BoundPort(ENsAddr::Sv))
		|| !Client.SetPeer(ENsAddr::C0, TEXT("127.0.0.1"), Host.BoundPort(ENsAddr::C0)))
	{
		return Fail(TEXT("udp-split: set peer failed"));
	}
	FNsLockstepServer Sv;
	FNsLockstepClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	FNsLockstepClient C1;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;
	const int8 Script[][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, 1}};
	for (int32 S = 0; S < 45; ++S)
	{
		const int8* Pair = Script[S % 4];
		C0.SendInput(Host, Pair[0]);
		C1.SendInput(Client, Pair[1]);
		NsPumpLockstepServer(Host, Sv, true);
		NsPumpLockstepClient(Host, C0, true);
		NsPumpLockstepClient(Client, C1, true);
		Host.Advance(Ns::LogicDtMs);
		Client.Advance(Ns::LogicDtMs);
	}
	for (int32 i = 0; i < 20; ++i)
	{
		NsPumpLockstepServer(Host, Sv, true);
		NsPumpLockstepClient(Host, C0, true);
		NsPumpLockstepClient(Client, C1, true);
		Host.Advance(Ns::LogicDtMs);
		Client.Advance(Ns::LogicDtMs);
	}
	if (C0.ExecFrame <= 10 || C1.ExecFrame <= 10)
	{
		return Fail(TEXT("udp-split: too few frames"));
	}
	if (!C0.World.Equals(C1.World))
	{
		return Fail(TEXT("udp-split: worlds diverged"));
	}
	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = FString::Printf(TEXT("udp-split frames=%d x=(%d,%d)"),
		C0.ExecFrame, C0.World.X[0], C0.World.X[1]);
	return Ok;
}

FNsSelfTestResult NsRunUdpSplitStateSyncSelfTest()
{
	FNsUdpNet Host;
	FNsUdpNet Client;
	if (!Host.Bind(ENsAddr::Sv, 0, false) || !Host.Bind(ENsAddr::C0, 0, false)
		|| !Client.Bind(ENsAddr::C1, 0, false))
	{
		return Fail(TEXT("udp-split-state: bind failed"));
	}
	if (!Host.SetPeer(ENsAddr::C1, TEXT("127.0.0.1"), Client.BoundPort(ENsAddr::C1))
		|| !Client.SetPeer(ENsAddr::Sv, TEXT("127.0.0.1"), Host.BoundPort(ENsAddr::Sv))
		|| !Client.SetPeer(ENsAddr::C0, TEXT("127.0.0.1"), Host.BoundPort(ENsAddr::C0)))
	{
		return Fail(TEXT("udp-split-state: set peer failed"));
	}
	FNsStateSyncServer Sv;
	FNsStateSyncClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	FNsStateSyncClient C1;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;
	const int8 Script[] = {1, 1, 0, -1, -1, 0};
	for (int32 S = 0; S < 40; ++S)
	{
		C0.LocalTick(Host, Script[S % 6]);
		C1.LocalTick(Client, Script[(S + 3) % 6]);
		NsPumpStateServer(Host, Sv, true);
		NsPumpStateClient(Host, C0, true);
		NsPumpStateClient(Client, C1, true);
		Host.Advance(Ns::SimDtMs);
		Client.Advance(Ns::SimDtMs);
	}
	for (int32 i = 0; i < 16; ++i)
	{
		NsPumpStateServer(Host, Sv, true);
		NsPumpStateClient(Host, C0, true);
		NsPumpStateClient(Client, C1, true);
		Host.Advance(Ns::SimDtMs);
		Client.Advance(Ns::SimDtMs);
	}
	if (C0.PredX != Sv.Pawns[0].X || C1.PredX != Sv.Pawns[1].X)
	{
		return FailStr(FString::Printf(TEXT("udp-split-state: pred=(%d,%d) sv=(%d,%d)"),
			C0.PredX, C1.PredX, Sv.Pawns[0].X, Sv.Pawns[1].X));
	}
	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = FString::Printf(TEXT("udp-split-state tick=%d x=(%d,%d)"),
		Sv.Tick, Sv.Pawns[0].X, Sv.Pawns[1].X);
	return Ok;
}

FNsSelfTestResult NsRunUdpSplitRollbackSelfTest()
{
	FNsUdpNet Host;
	FNsUdpNet Client;
	if (!Host.Bind(ENsAddr::C0, 0, false) || !Client.Bind(ENsAddr::C1, 0, false))
	{
		return Fail(TEXT("udp-split-rollback: bind failed"));
	}
	if (!Host.SetPeer(ENsAddr::C1, TEXT("127.0.0.1"), Client.BoundPort(ENsAddr::C1))
		|| !Client.SetPeer(ENsAddr::C0, TEXT("127.0.0.1"), Host.BoundPort(ENsAddr::C0)))
	{
		return Fail(TEXT("udp-split-rollback: set peer failed"));
	}
	FNsRollbackPeer A;
	A.PlayerId = 0;
	A.Addr = ENsAddr::C0;
	A.Other = ENsAddr::C1;
	FNsRollbackPeer B;
	B.PlayerId = 1;
	B.Addr = ENsAddr::C1;
	B.Other = ENsAddr::C0;
	const int8 S0[] = {1, 1, 1, 0, -1, -1, 0, 1};
	const int8 S1[] = {0, -1, -1, 1, 1, 0, 0, -1};
	for (int32 S = 0; S < 48; ++S)
	{
		A.Advance(Host, S0[S % 8]);
		B.Advance(Client, S1[S % 8]);
		NsPumpRollbackPeer(Host, A, true);
		NsPumpRollbackPeer(Client, B, true);
		Host.Advance(Ns::RollbackDtMs);
		Client.Advance(Ns::RollbackDtMs);
	}
	for (int32 i = 0; i < 24; ++i)
	{
		A.Advance(Host, 0);
		B.Advance(Client, 0);
		NsPumpRollbackPeer(Host, A, true);
		NsPumpRollbackPeer(Client, B, true);
		Host.Advance(Ns::RollbackDtMs);
		Client.Advance(Ns::RollbackDtMs);
	}
	if (!A.World.Equals(B.World))
	{
		return Fail(TEXT("udp-split-rollback: peers diverged"));
	}
	if (A.bWaiting || B.bWaiting)
	{
		return Fail(TEXT("udp-split-rollback: still waiting"));
	}
	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = FString::Printf(TEXT("udp-split-rollback frame=%d x=(%d,%d)"),
		A.Frame, A.World.X[0], A.World.X[1]);
	return Ok;
}

static FNsSelfTestResult RunFakeLockstep(float RttMs, float Drop, float JitterMs, int32 Frames, uint32 Seed,
	int32 Cooldown, const TCHAR* Tag)
{
	FNsFakeNet Net;
	Net.RttMs = RttMs;
	Net.Drop = Drop;
	Net.JitterMs = JitterMs;
	Net.Rng.Initialize(Seed);

	FNsLockstepServer Sv;
	FNsLockstepClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	FNsLockstepClient C1;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;

	const int8 Script[][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, 1}};
	const double End = static_cast<double>(Frames) * Ns::LogicDtMs;
	while (Net.Now < End)
	{
		const int32 Step = static_cast<int32>(Net.Now / Ns::LogicDtMs);
		const int8* Pair = Script[Step % 4];
		C0.SendInput(Net, Pair[0]);
		C1.SendInput(Net, Pair[1]);
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C0);
		NsPumpLockstepClient(Net, C1);
		Net.Advance(1.0);
	}

	for (int32 i = 0; i < Cooldown; ++i)
	{
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C0);
		NsPumpLockstepClient(Net, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	for (int32 i = 0; i < 48 && C0.ExecFrame != C1.ExecFrame; ++i)
	{
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C0);
		NsPumpLockstepClient(Net, C1);
		Net.Advance(Ns::LogicDtMs);
	}

	if (C0.ExecFrame <= Frames / 3)
	{
		return FailStr(FString::Printf(TEXT("%s: too few frames %d"), Tag, C0.ExecFrame));
	}
	if (FMath::Abs(C0.ExecFrame - C1.ExecFrame) > 20)
	{
		return FailStr(FString::Printf(TEXT("%s: exec mismatch %d vs %d"), Tag, C0.ExecFrame, C1.ExecFrame));
	}
	if (C0.ExecFrame != C1.ExecFrame)
	{
		return FailStr(FString::Printf(TEXT("%s: exec not aligned %d vs %d"), Tag, C0.ExecFrame, C1.ExecFrame));
	}
	if (!C0.World.Equals(C1.World))
	{
		return FailStr(FString::Printf(TEXT("%s: worlds diverged"), Tag));
	}
	if (C0.ExecFrame == Sv.Frame && !C0.World.Equals(Sv.World))
	{
		return FailStr(FString::Printf(TEXT("%s: client/server world mismatch"), Tag));
	}
	if (Sv.bDesync)
	{
		return FailStr(FString::Printf(TEXT("%s: checksum desync"), Tag));
	}
	if (Sv.Frame - C0.ExecFrame > 20)
	{
		return FailStr(FString::Printf(TEXT("%s: stalled exec=%d sv=%d"), Tag, C0.ExecFrame, Sv.Frame));
	}
	if (Drop <= 0.f && Sv.ChecksumOk <= 0)
	{
		return FailStr(FString::Printf(TEXT("%s: no checksum ack"), Tag));
	}
	return OkStr(FString::Printf(TEXT("%s frames=%d checksums=%d x=(%d,%d)"),
		Tag, C0.ExecFrame, Sv.ChecksumOk, C0.World.X[0], C0.World.X[1]));
}

static FNsSelfTestResult RunFakeStateSync(float RttMs, float Drop, float JitterMs, int32 Ticks, uint32 Seed,
	int32 Cooldown, const TCHAR* Tag)
{
	FNsFakeNet Net;
	Net.RttMs = RttMs;
	Net.Drop = Drop;
	Net.JitterMs = JitterMs;
	Net.Rng.Initialize(Seed);

	FNsStateSyncServer Sv;
	FNsStateSyncClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	FNsStateSyncClient C1;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;

	const int8 Script[] = {1, 1, 0, -1, -1, 0};
	for (int32 S = 0; S < Ticks; ++S)
	{
		C0.LocalTick(Net, Script[S % 6]);
		C1.LocalTick(Net, Script[(S + 3) % 6]);
		NsPumpStateServer(Net, Sv);
		NsPumpStateClient(Net, C0);
		NsPumpStateClient(Net, C1);
		Net.Advance(Ns::SimDtMs);
	}
	for (int32 i = 0; i < Cooldown; ++i)
	{
		NsPumpStateServer(Net, Sv);
		NsPumpStateClient(Net, C0);
		NsPumpStateClient(Net, C1);
		Net.Advance(Ns::SimDtMs);
	}

	if (C0.PredX != Sv.Pawns[0].X || C1.PredX != Sv.Pawns[1].X)
	{
		return FailStr(FString::Printf(TEXT("%s: prediction mismatch pred=(%d,%d) sv=(%d,%d) tick=%d ack=(%d,%d)"),
			Tag, C0.PredX, C1.PredX, Sv.Pawns[0].X, Sv.Pawns[1].X, Sv.Tick, Sv.LastAck[0], Sv.LastAck[1]));
	}
	if (!C0.bHasRemote || !C1.bHasRemote)
	{
		return FailStr(FString::Printf(TEXT("%s: no remote lerp"), Tag));
	}
	if (Sv.LastAck[0] <= 0 || Sv.LastAck[1] <= 0)
	{
		return FailStr(FString::Printf(TEXT("%s: missing snap ack"), Tag));
	}
	return OkStr(FString::Printf(TEXT("%s tick=%d delta=%d ack=%d x=(%d,%d)"),
		Tag, Sv.Tick, Sv.DeltaSent, Sv.LastAck[0], Sv.Pawns[0].X, Sv.Pawns[1].X));
}

static FNsSelfTestResult RunFakeRollback(float RttMs, float Drop, float JitterMs, int32 Steps, uint32 Seed,
	int32 Cooldown, const TCHAR* Tag)
{
	FNsFakeNet Net;
	Net.RttMs = RttMs;
	Net.Drop = Drop;
	Net.JitterMs = JitterMs;
	Net.Rng.Initialize(Seed);

	FNsRollbackPeer A;
	A.PlayerId = 0;
	A.Addr = ENsAddr::C0;
	A.Other = ENsAddr::C1;
	FNsRollbackPeer B;
	B.PlayerId = 1;
	B.Addr = ENsAddr::C1;
	B.Other = ENsAddr::C0;

	const int8 S0[] = {1, 1, 1, 0, -1, -1, 0, 1};
	const int8 S1[] = {0, -1, -1, 1, 1, 0, 0, -1};
	for (int32 S = 0; S < Steps; ++S)
	{
		A.Advance(Net, S0[S % 8]);
		B.Advance(Net, S1[S % 8]);
		NsPumpRollbackPeer(Net, A);
		NsPumpRollbackPeer(Net, B);
		Net.Advance(Ns::RollbackDtMs);
	}
	for (int32 i = 0; i < Cooldown; ++i)
	{
		A.Advance(Net, 0);
		B.Advance(Net, 0);
		NsPumpRollbackPeer(Net, A);
		NsPumpRollbackPeer(Net, B);
		Net.Advance(Ns::RollbackDtMs);
	}

	if (!A.World.Equals(B.World))
	{
		return FailStr(FString::Printf(TEXT("%s: peers diverged"), Tag));
	}
	if (A.bWaiting || B.bWaiting)
	{
		return FailStr(FString::Printf(
			TEXT("%s: still waiting a(frame=%d conf=%d wait=%d) b(frame=%d conf=%d wait=%d)"),
			Tag, A.Frame, A.Confirmed, A.WaitCount, B.Frame, B.Confirmed, B.WaitCount));
	}
	return OkStr(FString::Printf(TEXT("%s frame=%d wait=%d x=(%d,%d)"),
		Tag, A.Frame, A.WaitCount + B.WaitCount, A.World.X[0], A.World.X[1]));
}

FNsSelfTestResult NsRunWorldContractSelfTest()
{
	if (NsClampDx(-9) != -1 || NsClampDx(9) != 1 || NsClampDx(0) != 0)
	{
		return Fail(TEXT("world: clamp"));
	}

	FNsWorld A;
	FNsWorld B;
	const int8 Same[] = {1, -1};
	for (int32 i = 0; i < 1000; ++i)
	{
		A.Step(Same, Ns::LockstepSpeed);
		B.Step(Same, Ns::LockstepSpeed);
	}
	if (!A.Equals(B) || A.Checksum() != B.Checksum())
	{
		return Fail(TEXT("world: 1000-step mismatch"));
	}

	FNsWorld C;
	const int8 Other[] = {-1, 1};
	C.Step(Other, Ns::LockstepSpeed);
	if (C.Equals(A) || C.Checksum() == A.Checksum())
	{
		return Fail(TEXT("world: different input must diverge"));
	}

	A.Reset();
	if (A.X[0] != 0 || A.X[1] != 0 || A.Rng != 1)
	{
		return Fail(TEXT("world: reset"));
	}
	return OkStr(TEXT("world clamp+1000+diverge+reset"));
}

FNsSelfTestResult NsRunCodecContractSelfTest()
{
	auto Check = [](const TCHAR* Name, const FNsPacket& Src, auto&& Pred) -> FNsSelfTestResult
	{
		FNsPacket Dst;
		if (!RoundTripPacket(Src, Dst))
		{
			return FailStr(FString::Printf(TEXT("codec: %s roundtrip failed"), Name));
		}
		if (!Pred(Dst))
		{
			return FailStr(FString::Printf(TEXT("codec: %s fields"), Name));
		}
		return FNsSelfTestResult();
	};

	{
		FNsPacket Src;
		Src.Type = ENsMsg::C2SInput;
		Src.Seq = 3;
		Src.Ack = 1;
		Src.AckBits = 7;
		Src.PlayerId = 1;
		Src.Dx = -1;
		Src.SeqWindow = {10, 11, 12};
		Src.DxWindow = {1, 0, -1};
		const FNsSelfTestResult R = Check(TEXT("c2s"), Src, [](const FNsPacket& D)
		{
			return D.Type == ENsMsg::C2SInput && D.Seq == 3 && D.Ack == 1 && D.AckBits == 7
				&& D.PlayerId == 1 && D.Dx == -1 && D.SeqWindow.Num() == 3 && D.DxWindow[2] == -1;
		});
		if (!R.Detail.IsEmpty())
		{
			return R;
		}
	}
	{
		FNsPacket Src;
		Src.Type = ENsMsg::S2CSnapshot;
		Src.Tick = 30;
		Src.BaseTick = 12;
		Src.SnapX[0] = -4;
		Src.SnapX[1] = 9;
		Src.SnapSeq[0] = 8;
		Src.SnapSeq[1] = 7;
		const FNsSelfTestResult R = Check(TEXT("snap"), Src, [](const FNsPacket& D)
		{
			return D.Tick == 30 && D.BaseTick == 12 && D.SnapX[0] == -4 && D.SnapX[1] == 9
				&& D.SnapSeq[0] == 8 && D.SnapSeq[1] == 7;
		});
		if (!R.Detail.IsEmpty())
		{
			return R;
		}
	}
	{
		FNsPacket Src;
		Src.Type = ENsMsg::C2SSnapAck;
		Src.PlayerId = 0;
		Src.Tick = 44;
		const FNsSelfTestResult R = Check(TEXT("ack"), Src, [](const FNsPacket& D)
		{
			return D.PlayerId == 0 && D.Tick == 44;
		});
		if (!R.Detail.IsEmpty())
		{
			return R;
		}
	}
	{
		FNsPacket Src;
		Src.Type = ENsMsg::P2PInput;
		Src.RemoteDx.Add(4, 1);
		Src.RemoteDx.Add(5, -1);
		const FNsSelfTestResult R = Check(TEXT("p2p"), Src, [](const FNsPacket& D)
		{
			const int8* A = D.RemoteDx.Find(4);
			const int8* B = D.RemoteDx.Find(5);
			return A && B && *A == 1 && *B == -1;
		});
		if (!R.Detail.IsEmpty())
		{
			return R;
		}
	}
	{
		FNsPacket Src;
		Src.Type = ENsMsg::S2CFrame;
		FNsInputs In;
		In.Dx[0] = 1;
		In.Dx[1] = -1;
		Src.Frames.Add(3, In);
		Src.Frames.Add(4, In);
		const FNsSelfTestResult R = Check(TEXT("frame"), Src, [](const FNsPacket& D)
		{
			const FNsInputs* F3 = D.Frames.Find(3);
			const FNsInputs* F4 = D.Frames.Find(4);
			return D.Type == ENsMsg::S2CFrame && F3 && F4 && F3->Dx[0] == 1 && F4->Dx[1] == -1;
		});
		if (!R.Detail.IsEmpty())
		{
			return R;
		}
	}
	{
		FNsPacket Src;
		Src.Type = ENsMsg::S2CFrame;
		Src.Tick = 5;
		Src.BaseTick = 3;
		FNsInputs In;
		In.Dx[0] = 1;
		In.Dx[1] = -1;
		Src.Frames.Add(4, In);
		Src.TurnFpt.Add(4, 3);
		const FNsSelfTestResult R = Check(TEXT("frame-turn"), Src, [](const FNsPacket& D)
		{
			const FNsInputs* F4 = D.Frames.Find(4);
			const int32* Len = D.TurnFpt.Find(4);
			return D.Type == ENsMsg::S2CFrame && D.Tick == 5 && D.BaseTick == 3
				&& F4 && F4->Dx[0] == 1 && Len && *Len == 3;
		});
		if (!R.Detail.IsEmpty())
		{
			return R;
		}
	}
	{
		FNsPacket Src;
		Src.Type = ENsMsg::C2SChecksum;
		Src.PlayerId = 1;
		Src.Tick = 15;
		Src.Hash = 0xABu;
		const FNsSelfTestResult R = Check(TEXT("checksum"), Src, [](const FNsPacket& D)
		{
			return D.Type == ENsMsg::C2SChecksum && D.PlayerId == 1 && D.Tick == 15 && D.Hash == 0xABu;
		});
		if (!R.Detail.IsEmpty())
		{
			return R;
		}
	}
	{
		FNsPacket Src;
		Src.Type = ENsMsg::S2CJoinSnap;
		Src.Tick = 76;
		Src.SnapX[0] = 8;
		Src.SnapX[1] = -4;
		Src.SnapRng = 9;
		FNsInputs In;
		In.Dx[0] = 0;
		In.Dx[1] = 1;
		Src.Frames.Add(76, In);
		const FNsSelfTestResult R = Check(TEXT("join"), Src, [](const FNsPacket& D)
		{
			const FNsInputs* F = D.Frames.Find(76);
			return D.Tick == 76 && D.SnapX[0] == 8 && D.SnapRng == 9u && F && F->Dx[1] == 1;
		});
		if (!R.Detail.IsEmpty())
		{
			return R;
		}
	}
	{
		FNsPacket Src;
		Src.Type = ENsMsg::S2CDoorOpen;
		Src.DoorOpen = 1;
		const FNsSelfTestResult R = Check(TEXT("gate"), Src, [](const FNsPacket& D)
		{
			return D.Type == ENsMsg::S2CDoorOpen && D.DoorOpen == 1;
		});
		if (!R.Detail.IsEmpty())
		{
			return R;
		}
	}
	{
		FNsPacket Src;
		Src.Type = ENsMsg::C2SFrameNack;
		Src.PlayerId = 1;
		Src.SeqWindow = {8, 9, 10};
		const FNsSelfTestResult R = Check(TEXT("nack"), Src, [](const FNsPacket& D)
		{
			return D.Type == ENsMsg::C2SFrameNack && D.PlayerId == 1
				&& D.SeqWindow.Num() == 3 && D.SeqWindow[0] == 8 && D.SeqWindow[2] == 10;
		});
		if (!R.Detail.IsEmpty())
		{
			return R;
		}
	}
	{
		FNsPacket Src;
		Src.Type = ENsMsg::C2SFire;
		Src.PlayerId = 1;
		Src.Tick = 80;
		const FNsSelfTestResult R = Check(TEXT("fire"), Src, [](const FNsPacket& D)
		{
			return D.Type == ENsMsg::C2SFire && D.PlayerId == 1 && D.Tick == 80;
		});
		if (!R.Detail.IsEmpty())
		{
			return R;
		}
	}

	FNsPacket Empty;
	TArray<uint8> None;
	if (NsDecodePacket(None, Empty))
	{
		return Fail(TEXT("codec: empty must fail"));
	}
	TArray<uint8> Tiny;
	Tiny.AddZeroed(Ns::HeaderBytes - 1);
	if (NsDecodePacket(Tiny, Empty))
	{
		return Fail(TEXT("codec: truncated header must fail"));
	}

	FNsPacket Huge;
	Huge.Type = ENsMsg::S2CFrame;
	for (int32 i = 0; i < 256; ++i)
	{
		FNsInputs In;
		Huge.Frames.Add(i, In);
	}
	TArray<uint8> HugeBytes;
	if (NsEncodePacket(Huge, HugeBytes))
	{
		return Fail(TEXT("codec: 256 frames must fail"));
	}

	FNsPacket One;
	One.Type = ENsMsg::C2SSnapAck;
	One.Tick = 1;
	TArray<uint8> Bytes;
	if (!NsEncodePacket(One, Bytes))
	{
		return Fail(TEXT("codec: encode ack"));
	}
	Bytes.Add(0);
	if (NsDecodePacket(Bytes, Empty))
	{
		return Fail(TEXT("codec: trailing byte must fail"));
	}
	return OkStr(TEXT("codec all-types+reject"));
}

FNsSelfTestResult NsRunSeqWindowSelfTest()
{
	FNsSeqWindow W;
	if (!W.Accept(ENsAddr::Sv, ENsAddr::C0, 1) || W.Accept(ENsAddr::Sv, ENsAddr::C0, 1))
	{
		return Fail(TEXT("seq: dup 1"));
	}
	if (!W.Accept(ENsAddr::Sv, ENsAddr::C0, 2) || W.Accept(ENsAddr::Sv, ENsAddr::C0, 2))
	{
		return Fail(TEXT("seq: dup 2"));
	}
	if (!W.Accept(ENsAddr::Sv, ENsAddr::C1, 1))
	{
		return Fail(TEXT("seq: other src same seq"));
	}
	FNsPacket P;
	P.Dst = ENsAddr::Sv;
	W.Stamp(ENsAddr::C0, P);
	if (P.Seq != 1)
	{
		return Fail(TEXT("seq: stamp first"));
	}
	W.Stamp(ENsAddr::C0, P);
	if (P.Seq != 2)
	{
		return Fail(TEXT("seq: stamp second"));
	}
	if (!W.Accept(ENsAddr::Sv, ENsAddr::C0, 4) || !W.Accept(ENsAddr::Sv, ENsAddr::C0, 3))
	{
		return Fail(TEXT("seq: out-of-order hole fill"));
	}
	if (W.Accept(ENsAddr::Sv, ENsAddr::C0, 3))
	{
		return Fail(TEXT("seq: filled hole must stay dup"));
	}
	if (W.Accept(ENsAddr::Sv, ENsAddr::C0, 4 - 32))
	{
		return Fail(TEXT("seq: too old must reject"));
	}
	return OkStr(TEXT("seq window dup+stamp+hole"));
}

FNsSelfTestResult NsRunRouteGuardSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsStateSyncClient C;
	C.PlayerId = 0;
	C.Addr = ENsAddr::C0;

	FNsPacket Snapshot;
	Snapshot.Type = ENsMsg::S2CSnapshot;
	Snapshot.Tick = 3;
	Snapshot.SnapX[0] = 99;
	Net.Send(ENsAddr::C1, ENsAddr::C0, Snapshot);
	NsPumpStateClient(Net, C);
	if (C.LastAckedTick != 0 || C.PredX != 0)
	{
		return Fail(TEXT("route-guard: client accepted peer-forged snapshot"));
	}

	Snapshot.Tick = 4;
	Snapshot.SnapX[0] = 12;
	Net.Send(ENsAddr::Sv, ENsAddr::C0, Snapshot);
	NsPumpStateClient(Net, C);
	if (C.LastAckedTick != 4 || C.PredX != 12)
	{
		return Fail(TEXT("route-guard: valid server snapshot rejected"));
	}
	return OkStr(TEXT("route guard source+type"));
}

FNsSelfTestResult NsRunUdpSessionRestartSelfTest()
{
	FNsUdpNet Host;
	FNsUdpNet Client;
	if (!Host.Bind(ENsAddr::Sv, 0, false) || !Client.Bind(ENsAddr::C0, 0, false))
	{
		return Fail(TEXT("udp-session: bind failed"));
	}
	if (!Host.SetPeer(ENsAddr::C0, TEXT("127.0.0.1"), Client.BoundPort(ENsAddr::C0))
		|| !Client.SetPeer(ENsAddr::Sv, TEXT("127.0.0.1"), Host.BoundPort(ENsAddr::Sv)))
	{
		return Fail(TEXT("udp-session: peer setup failed"));
	}

	FNsPacket Input;
	Input.Type = ENsMsg::C2SInput;
	for (int32 i = 0; i < 40; ++i)
	{
		Client.Send(ENsAddr::C0, ENsAddr::Sv, Input);
	}
	TArray<FNsPacket> Warm;
	NsDrain(Host, ENsAddr::Sv, Warm, true);
	if (Warm.Num() < 33)
	{
		return FailStr(FString::Printf(TEXT("udp-session: warmup got=%d"), Warm.Num()));
	}

	Client.ResetSession();
	Client.Send(ENsAddr::C0, ENsAddr::Sv, Input);
	TArray<FNsPacket> Restarted;
	NsDrain(Host, ENsAddr::Sv, Restarted, true);
	if (Restarted.Num() != 1)
	{
		return FailStr(FString::Printf(TEXT("udp-session: restart got=%d"), Restarted.Num()));
	}
	return OkStr(TEXT("udp session restart accepted"));
}

FNsSelfTestResult NsRunFakeNetContractSelfTest()
{
	{
		FNsFakeNet Net;
		Net.Drop = 1.f;
		Net.RttMs = 0.f;
		Net.JitterMs = 0.f;
		Net.Rng.Initialize(1);
		FNsPacket Pkt;
		Pkt.Type = ENsMsg::C2SInput;
		Net.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
		Net.Advance(1000.0);
		TArray<FNsPacket> Got;
		Net.Drain(ENsAddr::Sv, Got);
		if (Got.Num() != 0)
		{
			return Fail(TEXT("fakenet: drop=1 leaked"));
		}
		Net.Drop = 0.f;
		Net.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
		Net.Advance(1.0);
		TArray<FNsPacket> After;
		Net.Drain(ENsAddr::Sv, After);
		if (After.Num() != 1 || After[0].Seq != 2)
		{
			return Fail(TEXT("fakenet: drop must still consume seq"));
		}
	}
	{
		FNsFakeNet Net;
		Net.Drop = 0.f;
		Net.RttMs = 80.f;
		Net.JitterMs = 0.f;
		Net.Rng.Initialize(1);
		FNsPacket Pkt;
		Pkt.Type = ENsMsg::C2SInput;
		Pkt.Dx = 1;
		Net.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
		Net.Advance(39.0);
		TArray<FNsPacket> Early;
		Net.Drain(ENsAddr::Sv, Early);
		if (Early.Num() != 0)
		{
			return Fail(TEXT("fakenet: delivered before half-rtt"));
		}
		Net.Advance(1.0);
		TArray<FNsPacket> OnTime;
		Net.Drain(ENsAddr::Sv, OnTime);
		if (OnTime.Num() != 1 || OnTime[0].Dx != 1)
		{
			return Fail(TEXT("fakenet: missing at half-rtt"));
		}
	}
	return OkStr(TEXT("fakenet drop+delay"));
}

FNsDropRateSample NsMeasureFakeNetDrop(float Drop, int32 Count, uint32 Seed)
{
	FNsDropRateSample Out;
	Out.Wanted = FMath::Clamp(Drop, 0.f, 1.f);
	Out.Sent = FMath::Clamp(Count, 1, 20000);
	FNsFakeNet Net;
	Net.Drop = Out.Wanted;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	Net.Rng.Initialize(Seed);
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Pkt.Dx = 1;
	for (int32 i = 0; i < Out.Sent; ++i)
	{
		Net.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
		Net.Advance(1.0);
		TArray<FNsPacket> Batch;
		Net.Drain(ENsAddr::Sv, Batch);
		Out.Got += Batch.Num();
	}
	Out.Measured = 1.f - static_cast<float>(Out.Got) / static_cast<float>(Out.Sent);
	return Out;
}

void NsLogFakeNetDropRate(float Drop, int32 Count)
{
	const FNsDropRateSample S = NsMeasureFakeNetDrop(Drop, Count, 1);
	UE_LOG(LogNetworkSync, Display,
		TEXT("ns.DropRate wanted=%.3f measured=%.3f sent=%d got=%d"),
		S.Wanted, S.Measured, S.Sent, S.Got);
}

FNsSelfTestResult NsRunFakeNetDropRateSelfTest()
{
	const float Rates[] = {0.f, 0.05f, 0.10f, 0.25f, 1.f};
	TArray<FString> Parts;
	for (float Wanted : Rates)
	{
		const FNsDropRateSample S = NsMeasureFakeNetDrop(Wanted, 2000, 1);
		const float Err = FMath::Abs(S.Measured - S.Wanted);
		const float Tol = (Wanted <= 0.f || Wanted >= 1.f) ? 0.f : 0.03f;
		if (Err > Tol + 1.e-6f)
		{
			return FailStr(FString::Printf(
				TEXT("drop-rate: wanted=%.3f measured=%.3f sent=%d got=%d (tol=%.3f)"),
				S.Wanted, S.Measured, S.Sent, S.Got, Tol));
		}
		Parts.Add(FString::Printf(TEXT("%.2f->%.3f"), S.Wanted, S.Measured));
	}
	return OkStr(FString::Printf(TEXT("drop-rate %s"), *FString::Join(Parts, TEXT(" "))));
}

FNsSelfTestResult NsRunLockstepCleanSelfTest()
{
	return RunFakeLockstep(80.f, 0.f, 0.f, 90, 1, 80, TEXT("lockstep-clean"));
}

FNsSelfTestResult NsRunLockstepHighDropSelfTest()
{
	return RunFakeLockstep(80.f, 0.15f, 8.f, 120, 1, 800, TEXT("lockstep-drop15"));
}

FNsSelfTestResult NsRunLockstepDesyncSelfTest()
{
	FNsLockstepServer Sv;
	Sv.Checksums.Add(15, 42u);
	Sv.OnChecksum(15, 99u);
	if (!Sv.bDesync)
	{
		return Fail(TEXT("lockstep-desync: mismatch not flagged"));
	}
	Sv.OnChecksum(15, 42u);
	if (Sv.ChecksumOk <= 0)
	{
		return Fail(TEXT("lockstep-desync: matching hash ignored"));
	}
	Sv.OnChecksum(99, 1u);
	return OkStr(TEXT("lockstep-desync flagged"));
}

FNsSelfTestResult NsRunSchemeSwitchSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepServer Sv;
	FNsLockstepClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	FNsLockstepClient C1;
	C1.PlayerId = 1;
	C1.Addr = ENsAddr::C1;
	for (int32 S = 0; S < 45; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, -1);
		NsPumpLockstepServer(Net, Sv);
		NsPumpLockstepClient(Net, C0);
		NsPumpLockstepClient(Net, C1);
		Net.Advance(Ns::LogicDtMs);
	}
	if (Sv.Frame < 20)
	{
		return FailStr(FString::Printf(TEXT("scheme-switch: lockstep frame=%d"), Sv.Frame));
	}

	Net.RttMs = 80.f;
	C0.SendInput(Net, 1);
	Net.ResetSession();
	if (Net.Now != 0.0)
	{
		return Fail(TEXT("scheme-switch: Now not cleared"));
	}
	TArray<FNsPacket> Leftover;
	Net.Drain(ENsAddr::Sv, Leftover);
	if (Leftover.Num() != 0)
	{
		return Fail(TEXT("scheme-switch: queue survived ResetSession"));
	}

	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepServer Fresh;
	NsPumpLockstepServer(Net, Fresh);
	if (Fresh.Frame > 2)
	{
		return FailStr(FString::Printf(TEXT("scheme-switch: lockstep stormed frame=%d"), Fresh.Frame));
	}

	FNsStateSyncServer Ss;
	FNsStateSyncClient Sc0;
	Sc0.PlayerId = 0;
	Sc0.Addr = ENsAddr::C0;
	FNsStateSyncClient Sc1;
	Sc1.PlayerId = 1;
	Sc1.Addr = ENsAddr::C1;
	for (int32 S = 0; S < 24; ++S)
	{
		Sc0.LocalTick(Net, 1);
		Sc1.LocalTick(Net, 0);
		NsPumpStateServer(Net, Ss);
		NsPumpStateClient(Net, Sc0);
		NsPumpStateClient(Net, Sc1);
		Net.Advance(Ns::SimDtMs);
	}
	if (Sc0.PredX != Ss.Pawns[0].X)
	{
		return FailStr(FString::Printf(TEXT("scheme-switch: state pred=%d sv=%d"), Sc0.PredX, Ss.Pawns[0].X));
	}
	return OkStr(FString::Printf(TEXT("scheme-switch lockstep=%d then state tick=%d"), Sv.Frame, Ss.Tick));
}

FNsSelfTestResult NsRunLockstepStressSelfTest()
{
	const double T0 = FPlatformTime::Seconds();
	FNsSelfTestResult R = RunFakeLockstep(80.f, 0.1f, 8.f, 600, 1, 400, TEXT("lockstep-stress"));
	const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
	if (!R.bOk)
	{
		return R;
	}
	if (Ms > 3000.0)
	{
		return FailStr(FString::Printf(TEXT("lockstep-stress: slow %.0fms"), Ms));
	}
	R.Detail += FString::Printf(TEXT(" %.0fms"), Ms);
	return R;
}

FNsSelfTestResult NsRunStateSyncCleanSelfTest()
{
	FNsSelfTestResult R = RunFakeStateSync(80.f, 0.f, 0.f, 240, 1, 40, TEXT("state-clean"));
	if (!R.bOk)
	{
		return R;
	}
	return R;
}

FNsSelfTestResult NsRunStateSyncRewindSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsStateSyncServer Sv;
	for (int32 S = 0; S < 80; ++S)
	{
		Sv.OnInput(0, S + 1, 1);
		Sv.Sim(Net);
		Net.Advance(Ns::SimDtMs);
	}
	if (Sv.Pawns[0].X != 80 * Ns::StateSpeed)
	{
		return FailStr(FString::Printf(TEXT("rewind: x=%d"), Sv.Pawns[0].X));
	}
	const int32 Cap = Sv.RewindX(0, 500);
	if (Cap != Sv.Pawns[0].X)
	{
		return Fail(TEXT("rewind: lag cap should return current x"));
	}
	const int32 Past = Sv.RewindX(0, 80);
	if (Past == Sv.Pawns[0].X)
	{
		return Fail(TEXT("rewind: ping 80 should look back"));
	}
	if (Sv.RewindX(-1, 80) != 0 || Sv.RewindX(2, 80) != 0)
	{
		return Fail(TEXT("rewind: bad player"));
	}
	return OkStr(FString::Printf(TEXT("rewind current=%d past=%d"), Cap, Past));
}

FNsSelfTestResult NsRunStateSyncFireSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsStateSyncServer Sv;
	for (int32 S = 0; S < 40; ++S)
	{
		Sv.Sim(Net);
		Net.Advance(Ns::SimDtMs);
	}
	if (!Sv.OnFire(0, 80) || Sv.Hits[0] != 1)
	{
		return Fail(TEXT("fire: close rewind should hit"));
	}
	if (Sv.OnFire(-1, 80) || Sv.OnFire(2, 80) || Sv.Hits[0] != 1)
	{
		return Fail(TEXT("fire: bad shooter"));
	}

	for (int32 S = 0; S < 40; ++S)
	{
		Sv.OnInput(1, S + 1, 1);
		Sv.Sim(Net);
		Net.Advance(Ns::SimDtMs);
	}
	if (Sv.OnFire(0, 80) || Sv.OnFire(0, 500) || Sv.Hits[0] != 1)
	{
		return FailStr(FString::Printf(TEXT("fire: far victim should miss x0=%d x1=%d rewind=%d"),
			Sv.Pawns[0].X, Sv.Pawns[1].X, Sv.RewindX(1, 80)));
	}

	FNsFakeNet Wire;
	Wire.Drop = 0.f;
	Wire.RttMs = 0.f;
	Wire.JitterMs = 0.f;
	FNsStateSyncServer WireSv;
	FNsStateSyncClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	for (int32 S = 0; S < 40; ++S)
	{
		NsPumpStateServer(Wire, WireSv);
		Wire.Advance(Ns::SimDtMs);
	}
	FNsPacket Spoof;
	Spoof.Type = ENsMsg::C2SFire;
	Spoof.PlayerId = 0;
	Spoof.Tick = 80;
	Wire.Send(ENsAddr::C1, ENsAddr::Sv, Spoof);
	NsPumpStateServer(Wire, WireSv);
	if (WireSv.Hits[0] != 0 || WireSv.Hits[1] != 1)
	{
		return FailStr(FString::Printf(TEXT("fire: spoof src hits=%d/%d"),
			WireSv.Hits[0], WireSv.Hits[1]));
	}
	C0.Fire(Wire, 80);
	NsPumpStateServer(Wire, WireSv);
	if (WireSv.Hits[0] != 1)
	{
		return Fail(TEXT("fire: wire C0 miss"));
	}
	return OkStr(TEXT("state-sync fire rewind+src"));
}

FNsSelfTestResult NsRunStateSyncNackSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsStateSyncServer Sv;
	FNsStateSyncClient C0;
	C0.PlayerId = 0;
	C0.Addr = ENsAddr::C0;
	for (int32 S = 0; S < 24; ++S)
	{
		C0.LocalTick(Net, 1);
		NsPumpStateServer(Net, Sv);
		NsPumpStateClient(Net, C0);
		Net.Advance(Ns::SimDtMs);
	}
	if (Sv.LastAck[0] <= 0)
	{
		return Fail(TEXT("state-nack: no first ack"));
	}
	C0.Store0.Reset();
	C0.Store1.Reset();
	C0.LastAckedTick = 0;
	for (int32 i = 0; i < 12; ++i)
	{
		C0.LocalTick(Net, 1);
		NsPumpStateServer(Net, Sv);
		NsPumpStateClient(Net, C0);
		Net.Advance(Ns::SimDtMs);
	}
	if (C0.Store0.Num() == 0)
	{
		return Fail(TEXT("state-nack: never recovered full snap"));
	}
	if (C0.PredX != Sv.Pawns[0].X)
	{
		return FailStr(FString::Printf(TEXT("state-nack: pred=%d sv=%d"), C0.PredX, Sv.Pawns[0].X));
	}
	return OkStr(TEXT("state-nack recovered"));
}

FNsSelfTestResult NsRunStateSyncInboxHoleSelfTest()
{
	FNsFakeNet Net;
	FNsStateSyncServer Sv;
	Sv.OnInput(0, 1, 1);
	Sv.OnInput(0, 3, 1);
	Sv.Sim(Net);
	if (Sv.Pawns[0].LastSeq != 1 || Sv.Pawns[0].X != Ns::StateSpeed)
	{
		return FailStr(FString::Printf(TEXT("state-inbox: skipped hole seq=%d x=%d"),
			Sv.Pawns[0].LastSeq, Sv.Pawns[0].X));
	}
	Sv.OnInput(0, 2, -1);
	Sv.Sim(Net);
	if (Sv.Pawns[0].LastSeq != 3 || Sv.Pawns[0].X != Ns::StateSpeed)
	{
		return FailStr(FString::Printf(TEXT("state-inbox: after fill seq=%d x=%d"),
			Sv.Pawns[0].LastSeq, Sv.Pawns[0].X));
	}
	return OkStr(TEXT("state-inbox hole then catch"));
}

FNsSelfTestResult NsRunStateSyncInboxCapSelfTest()
{
	FNsFakeNet Net;
	FNsStateSyncServer Sv;
	Sv.OnInput(0, Ns::MaxInboxAhead + 1, 1);
	Sv.Sim(Net);
	if (Sv.Pawns[0].LastSeq != 0 || Sv.Pawns[0].X != 0 || Sv.Inbox[0].Num() != 0)
	{
		return FailStr(FString::Printf(TEXT("state-inbox-cap: far seq stored seq=%d n=%d"),
			Sv.Pawns[0].LastSeq, Sv.Inbox[0].Num()));
	}
	Sv.OnInput(0, Ns::MaxInboxAhead, 1);
	if (Sv.Inbox[0].Num() != 1)
	{
		return Fail(TEXT("state-inbox-cap: LastSeq+MaxInboxAhead should store"));
	}
	Sv.OnInput(0, 1, 1);
	Sv.Sim(Net);
	if (Sv.Pawns[0].LastSeq != 1)
	{
		return FailStr(FString::Printf(TEXT("state-inbox-cap: in-window seq lost last=%d"), Sv.Pawns[0].LastSeq));
	}
	return OkStr(TEXT("state-inbox cap rejects far seq"));
}

FNsSelfTestResult NsRunStateSyncUnackedWindowSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsStateSyncClient C;
	C.PlayerId = 0;
	C.Addr = ENsAddr::C0;
	const int32 Ticks = Ns::InputWindow + 12;
	for (int32 i = 0; i < Ticks; ++i)
	{
		C.LocalTick(Net, 1);
	}
	if (C.UnackedSeq.Num() != Ticks || C.UnackedDx.Num() != Ticks)
	{
		return FailStr(FString::Printf(TEXT("state-unacked: n=%d want %d"),
			C.UnackedSeq.Num(), Ticks));
	}
	if (C.Seq != Ticks)
	{
		return FailStr(FString::Printf(TEXT("state-unacked: seq=%d"), C.Seq));
	}
	if (C.PredX != Ticks * Ns::StateSpeed)
	{
		return FailStr(FString::Printf(TEXT("state-unacked: pred=%d"), C.PredX));
	}
	return OkStr(TEXT("state-unacked retained until ack"));
}

FNsSelfTestResult NsRunStateSyncLongOutageSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 1.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsStateSyncServer Sv;
	FNsStateSyncClient C;
	C.PlayerId = 0;
	C.Addr = ENsAddr::C0;

	for (int32 i = 0; i < Ns::MaxInboxAhead; ++i)
	{
		C.LocalTick(Net, 1);
	}
	Net.Drop = 0.f;
	C.LocalTick(Net, 1);
	for (int32 Pump = 0;
		Pump < 256 && (Sv.Pawns[0].LastSeq < Ns::MaxInboxAhead || C.UnackedSeq.Num() > 0);
		++Pump)
	{
		NsPumpStateServer(Net, Sv);
		NsPumpStateClient(Net, C);
		Net.Advance(Ns::SimDtMs);
	}
	if (Sv.Pawns[0].LastSeq != Ns::MaxInboxAhead || C.UnackedSeq.Num() != 0)
	{
		return FailStr(FString::Printf(
			TEXT("state-outage: seq=%d unacked=%d want=%d"),
			Sv.Pawns[0].LastSeq, C.UnackedSeq.Num(), Ns::MaxInboxAhead));
	}
	const int32 ExpectedX = Ns::MaxInboxAhead * Ns::StateSpeed;
	if (Sv.Pawns[0].X != ExpectedX || C.PredX != ExpectedX)
	{
		return FailStr(FString::Printf(
			TEXT("state-outage: x server=%d client=%d want=%d"),
			Sv.Pawns[0].X, C.PredX, ExpectedX));
	}
	return OkStr(FString::Printf(TEXT("state-outage drained seq=%d"), Sv.Pawns[0].LastSeq));
}

FNsSelfTestResult NsRunStateSyncClockOffsetSelfTest()
{
	FNsFakeNet Net;
	FNsStateSyncClient C;
	C.PlayerId = 1;
	C.Addr = ENsAddr::C1;

	Net.Now = 5000.0;
	FNsPacket First;
	First.Type = ENsMsg::S2CSnapshot;
	First.Tick = 100;
	First.SnapX[0] = 0;
	C.OnSnap(Net, First);

	Net.Now = 5048.0;
	FNsPacket Second = First;
	Second.Tick = 103;
	Second.SnapX[0] = 48;
	C.OnSnap(Net, Second);
	C.UpdateRemoteDraw(5124.0);
	if (C.RemoteDrawn != 24)
	{
		return FailStr(FString::Printf(TEXT("state-clock: drawn=%d want 24"), C.RemoteDrawn));
	}
	return OkStr(TEXT("state clock offset interpolation"));
}

FNsSelfTestResult NsRunStateSyncOldSnapSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsStateSyncClient C;
	C.PlayerId = 0;
	C.Addr = ENsAddr::C0;
	FNsPacket P;
	P.Type = ENsMsg::S2CSnapshot;
	P.Tick = 12;
	P.BaseTick = 0;
	P.SnapX[0] = 40;
	P.SnapX[1] = 8;
	P.SnapSeq[0] = 3;
	C.OnSnap(Net, P);
	if (C.PredX != 40 || C.LastAckedTick != 12)
	{
		return FailStr(FString::Printf(TEXT("state-old: first snap pred=%d tick=%d"), C.PredX, C.LastAckedTick));
	}
	FNsPacket Old = P;
	Old.Tick = 9;
	Old.SnapX[0] = 0;
	C.OnSnap(Net, Old);
	if (C.PredX != 40 || C.LastAckedTick != 12)
	{
		return Fail(TEXT("state-old: older snap overwrote"));
	}
	return OkStr(TEXT("state-old snap ignored"));
}

FNsSelfTestResult NsRunStateSyncSpoofSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsStateSyncServer Sv;
	FNsPacket Spoof;
	Spoof.Type = ENsMsg::C2SInput;
	Spoof.PlayerId = 0;
	Spoof.SeqWindow.Add(1);
	Spoof.DxWindow.Add(1);
	Net.Send(ENsAddr::C1, ENsAddr::Sv, Spoof);
	Net.Advance(1.0);
	NsPumpStateServer(Net, Sv);
	if (Sv.Pawns[0].LastSeq != 0 || Sv.Pawns[0].X != 0)
	{
		return Fail(TEXT("state-spoof: payload PlayerId moved pawn 0"));
	}
	if (Sv.Pawns[1].LastSeq != 1 || Sv.Pawns[1].X != Ns::StateSpeed)
	{
		return FailStr(FString::Printf(TEXT("state-spoof: src pawn1 seq=%d x=%d"),
			Sv.Pawns[1].LastSeq, Sv.Pawns[1].X));
	}
	return OkStr(TEXT("state-spoof src wins"));
}

FNsSelfTestResult NsRunStateSyncStressSelfTest()
{
	const double T0 = FPlatformTime::Seconds();
	FNsSelfTestResult R = RunFakeStateSync(80.f, 0.05f, 4.f, 800, 1, 120, TEXT("state-stress"));
	const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
	if (!R.bOk)
	{
		return R;
	}
	if (Ms > 3000.0)
	{
		return FailStr(FString::Printf(TEXT("state-stress: slow %.0fms"), Ms));
	}
	R.Detail += FString::Printf(TEXT(" %.0fms"), Ms);
	return R;
}

FNsSelfTestResult NsRunRollbackCleanSelfTest()
{
	return RunFakeRollback(80.f, 0.f, 0.f, 120, 1, 80, TEXT("rollback-clean"));
}

FNsSelfTestResult NsRunRollbackWaitSelfTest()
{
	FNsRollbackPeer A;
	A.PlayerId = 0;
	A.Addr = ENsAddr::C0;
	A.Other = ENsAddr::C1;
	TMap<int32, int8> Packed;
	bool bHitWait = false;
	for (int32 i = 0; i < 20; ++i)
	{
		A.AdvanceLocal(1, Packed);
		if (A.bWaiting)
		{
			bHitWait = true;
			break;
		}
	}
	if (!bHitWait || A.WaitCount <= 0)
	{
		return Fail(TEXT("rollback-wait: never waited"));
	}
	TMap<int32, int8> Remote;
	for (int32 F = 0; F < A.Frame; ++F)
	{
		Remote.Add(F, 0);
	}
	A.OnRemote(Remote);
	A.AdvanceLocal(1, Packed);
	if (A.bWaiting)
	{
		return Fail(TEXT("rollback-wait: still waiting after remote fill"));
	}
	return OkStr(FString::Printf(TEXT("rollback-wait count=%d frame=%d"), A.WaitCount, A.Frame));
}

FNsSelfTestResult NsRunRollbackHoleSelfTest()
{
	FNsRollbackPeer A;
	A.PlayerId = 0;
	A.Addr = ENsAddr::C0;
	TMap<int32, int8> Packed;
	for (int32 i = 0; i < 6; ++i)
	{
		A.AdvanceLocal(1, Packed);
	}
	TMap<int32, int8> Remote;
	Remote.Add(2, 1);
	Remote.Add(3, 1);
	Remote.Add(4, 1);
	A.OnRemote(Remote);
	if (A.Confirmed >= 1)
	{
		return FailStr(FString::Printf(TEXT("rollback-hole: skipped missing frame confirmed=%d"), A.Confirmed));
	}
	return OkStr(FString::Printf(TEXT("rollback-hole confirmed=%d"), A.Confirmed));
}

FNsSelfTestResult NsRunRollbackMidHoleSelfTest()
{
	FNsRollbackPeer A;
	A.PlayerId = 0;
	A.Addr = ENsAddr::C0;
	TMap<int32, int8> Packed;
	for (int32 i = 0; i < 8; ++i)
	{
		A.AdvanceLocal(1, Packed);
	}
	TMap<int32, int8> Remote;
	Remote.Add(1, 1);
	Remote.Add(3, 1);
	Remote.Add(4, 1);
	A.OnRemote(Remote);
	if (A.Confirmed != 1)
	{
		return FailStr(FString::Printf(TEXT("rollback-midhole: confirmed=%d want 1"), A.Confirmed));
	}
	return OkStr(FString::Printf(TEXT("rollback-midhole confirmed=%d"), A.Confirmed));
}

FNsSelfTestResult NsRunRollbackConflictingInputSelfTest()
{
	FNsRollbackPeer A;
	A.PlayerId = 0;
	A.Addr = ENsAddr::C0;
	TMap<int32, int8> Packed;
	for (int32 i = 0; i < 16; ++i)
	{
		TMap<int32, int8> Remote;
		Remote.Add(i, 0);
		A.OnRemote(Remote);
		A.AdvanceLocal(1, Packed);
	}

	const int32 FrameBefore = A.Frame;
	TMap<int32, int8> Conflict;
	Conflict.Add(1, 1);
	A.OnRemote(Conflict);
	A.AdvanceLocal(1, Packed);
	if (!A.bWaiting || !A.bNeedsResync || A.ResyncFrame != 1 || A.Frame != FrameBefore)
	{
		return FailStr(FString::Printf(
			TEXT("rollback-conflict: frame=%d before=%d waiting=%d resync=%d at=%d"),
			A.Frame, FrameBefore, A.bWaiting ? 1 : 0,
			A.bNeedsResync ? 1 : 0, A.ResyncFrame));
	}
	if (!A.ConsumeResyncRequest() || A.ConsumeResyncRequest())
	{
		return Fail(TEXT("rollback-conflict: terminal request was not reported exactly once"));
	}
	return OkStr(TEXT("rollback conflicting input halted"));
}

FNsSelfTestResult NsRunRollbackStressSelfTest()
{
	const double T0 = FPlatformTime::Seconds();
	FNsSelfTestResult R = RunFakeRollback(80.f, 0.05f, 6.f, 400, 1, 200, TEXT("rollback-stress"));
	const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
	if (!R.bOk)
	{
		return R;
	}
	if (Ms > 3000.0)
	{
		return FailStr(FString::Printf(TEXT("rollback-stress: slow %.0fms"), Ms));
	}
	R.Detail += FString::Printf(TEXT(" %.0fms"), Ms);
	return R;
}

FNsSelfTestResult NsRunUdpBurstSelfTest()
{
	FNsUdpNet Net;
	if (!Net.BindLoopback())
	{
		return Fail(TEXT("udp-burst: bind failed"));
	}
	const int32 Count = 48;
	for (int32 i = 0; i < Count; ++i)
	{
		FNsPacket Pkt;
		Pkt.Type = ENsMsg::C2SInput;
		Pkt.PlayerId = 0;
		Pkt.Dx = static_cast<int8>((i % 3) - 1);
		Pkt.SeqWindow.Add(i + 1);
		Pkt.DxWindow.Add(Pkt.Dx);
		Net.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
	}
	TArray<FNsPacket> Got;
	for (int32 Try = 0; Try < 80; ++Try)
	{
		TArray<FNsPacket> Batch;
		Net.Drain(ENsAddr::Sv, Batch);
		Got.Append(Batch);
		if (Got.Num() >= Count)
		{
			break;
		}
		FPlatformProcess::Sleep(0.001f);
	}
	if (Got.Num() != Count)
	{
		return FailStr(FString::Printf(TEXT("udp-burst: got %d want %d"), Got.Num(), Count));
	}
	int32 Sum = 0;
	for (const FNsPacket& P : Got)
	{
		Sum += P.DxWindow.Num() > 0 ? P.DxWindow[0] : 0;
	}
	if (Sum != 0)
	{
		return FailStr(FString::Printf(TEXT("udp-burst: dx sum %d"), Sum));
	}
	return OkStr(FString::Printf(TEXT("udp-burst n=%d sum=%d"), Got.Num(), Sum));
}

FNsSelfTestResult NsRunCodecStressSelfTest()
{
	const double T0 = FPlatformTime::Seconds();
	FNsPacket Src;
	Src.Type = ENsMsg::S2CFrame;
	FNsInputs In;
	In.Dx[0] = 1;
	In.Dx[1] = -1;
	Src.Frames.Add(1, In);
	Src.Frames.Add(2, In);
	Src.Frames.Add(3, In);
	Src.Frames.Add(4, In);
	for (int32 i = 0; i < 10000; ++i)
	{
		Src.Seq = i;
		Src.Ack = i / 2;
		FNsPacket Dst;
		if (!RoundTripPacket(Src, Dst) || Dst.Seq != i || Dst.Frames.Num() != 4)
		{
			return FailStr(FString::Printf(TEXT("codec-stress: fail at %d"), i));
		}
	}
	const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
	if (Ms > 1500.0)
	{
		return FailStr(FString::Printf(TEXT("codec-stress: slow %.0fms"), Ms));
	}
	return OkStr(FString::Printf(TEXT("codec-stress 10000 %.0fms"), Ms));
}

FNsSelfTestResult NsRunFakeNetStressSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	Net.Rng.Initialize(1);
	const double T0 = FPlatformTime::Seconds();
	const int32 Count = 2500;
	TArray<FNsPacket> Got;
	for (int32 i = 0; i < Count; ++i)
	{
		FNsPacket Pkt;
		Pkt.Type = ENsMsg::C2SInput;
		Pkt.Dx = static_cast<int8>((i % 3) - 1);
		Net.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
		if ((i % 8) == 7)
		{
			Net.Advance(1.0);
			TArray<FNsPacket> Batch;
			Net.Drain(ENsAddr::Sv, Batch);
			Got.Append(Batch);
		}
	}
	Net.Advance(1.0);
	TArray<FNsPacket> Tail;
	Net.Drain(ENsAddr::Sv, Tail);
	Got.Append(Tail);
	if (Got.Num() != Count)
	{
		return FailStr(FString::Printf(TEXT("fakenet-stress: got %d"), Got.Num()));
	}
	const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
	if (Ms > 2000.0)
	{
		return FailStr(FString::Printf(TEXT("fakenet-stress: slow %.0fms"), Ms));
	}
	return OkStr(FString::Printf(TEXT("fakenet-stress n=%d %.0fms"), Count, Ms));
}

FNsSelfTestResult NsRunWorldStressSelfTest()
{
	FNsWorld A;
	FNsWorld B;
	const int8 Script[][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, 1}, {0, 0}, {-1, 0}};
	const int32 Steps = 10000;
	const double T0 = FPlatformTime::Seconds();
	for (int32 i = 0; i < Steps; ++i)
	{
		A.Step(Script[i % 6], Ns::LockstepSpeed);
		B.Step(Script[i % 6], Ns::RollbackSpeed);
	}
	FNsWorld C;
	for (int32 i = 0; i < Steps; ++i)
	{
		C.Step(Script[i % 6], Ns::LockstepSpeed);
	}
	if (!A.Equals(C))
	{
		return Fail(TEXT("world-stress: lockstep speed not deterministic"));
	}
	if (A.Equals(B))
	{
		return Fail(TEXT("world-stress: different speed must diverge"));
	}
	const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
	if (Ms > 250.0)
	{
		return FailStr(FString::Printf(TEXT("world-stress: slow %.0fms"), Ms));
	}
	return OkStr(FString::Printf(TEXT("world-stress 10000 %.0fms x=%d"), Ms, A.X[0]));
}

FNsSelfTestResult NsRunMtuSelfTest()
{
	if (Ns::MaxPacketBytes + Ns::Ipv6UdpOverheadBytes > Ns::MinPathMtuBytes)
	{
		return Fail(TEXT("mtu: ipv6 min path would fragment"));
	}

	FNsPacket Frame;
	Frame.Type = ENsMsg::S2CFrame;
	FNsInputs In;
	In.Dx[0] = 1;
	In.Dx[1] = -1;
	for (int32 i = 0; i < Ns::RedundantFrames + 1; ++i)
	{
		Frame.Frames.Add(i + 1, In);
	}
	const int32 Typical = NsWireBytes(Frame);
	if (Typical != 53)
	{
		return FailStr(FString::Printf(TEXT("mtu: typical S2CFrame %d"), Typical));
	}
	TArray<uint8> TypicalBytes;
	if (!NsEncodePacket(Frame, TypicalBytes) || TypicalBytes.Num() != Typical)
	{
		return Fail(TEXT("mtu: encode size != NsWireBytes"));
	}

	FNsPacket Snap;
	Snap.Type = ENsMsg::S2CSnapshot;
	if (NsWireBytes(Snap) != 45)
	{
		return Fail(TEXT("mtu: snapshot size"));
	}

	FNsPacket Join;
	Join.Type = ENsMsg::S2CJoinSnap;
	Join.Tick = 76;
	for (int32 i = 0; i < Ns::JoinSnapEvery; ++i)
	{
		Join.Frames.Add(76 + i, In);
	}
	const int32 JoinWire = NsWireBytes(Join);
	if (JoinWire != Ns::HeaderBytes + 17 + 6 * Ns::JoinSnapEvery || JoinWire > Ns::MaxPacketBytes)
	{
		return FailStr(FString::Printf(TEXT("mtu: join %d"), JoinWire));
	}

	FNsPacket Gate;
	Gate.Type = ENsMsg::S2CDoorOpen;
	Gate.DoorOpen = 1;
	if (NsWireBytes(Gate) != Ns::HeaderBytes + 4)
	{
		return Fail(TEXT("mtu: gate size"));
	}

	FNsPacket Huge;
	Huge.Type = ENsMsg::S2CFrame;
	const int32 HugeCount = Ns::MaxS2CFrameEntries + 40;
	for (int32 i = 0; i < HugeCount; ++i)
	{
		Huge.Frames.Add(i, In);
	}
	if (NsFitsMtu(Huge) || NsEncodePacket(Huge, TypicalBytes))
	{
		return Fail(TEXT("mtu: oversized single packet must not encode"));
	}
	TArray<FNsPacket> Parts;
	NsSplitForMtu(Huge, Parts);
	if (Parts.Num() < 2)
	{
		return Fail(TEXT("mtu: split produced one datagram"));
	}
	TSet<int32> Keys;
	for (const FNsPacket& Part : Parts)
	{
		TArray<uint8> Bytes;
		if (!NsEncodePacket(Part, Bytes) || Bytes.Num() > Ns::MaxPacketBytes)
		{
			return Fail(TEXT("mtu: split part over MTU"));
		}
		for (const TPair<int32, FNsInputs>& Kv : Part.Frames)
		{
			Keys.Add(Kv.Key);
		}
	}
	if (Keys.Num() != HugeCount)
	{
		return FailStr(FString::Printf(TEXT("mtu: split lost keys %d"), Keys.Num()));
	}

	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	Net.Rng.Initialize(1);
	Net.Send(ENsAddr::Sv, ENsAddr::C0, Huge);
	Net.Advance(1.0);
	TArray<FNsPacket> Got;
	Net.Drain(ENsAddr::C0, Got);
	TSet<int32> Wired;
	for (const FNsPacket& P : Got)
	{
		if (NsWireBytes(P) > Ns::MaxPacketBytes)
		{
			return Fail(TEXT("mtu: fakenet delivered oversize"));
		}
		for (const TPair<int32, FNsInputs>& Kv : P.Frames)
		{
			Wired.Add(Kv.Key);
		}
	}
	if (Got.Num() < 2 || Wired.Num() != HugeCount)
	{
		return FailStr(FString::Printf(TEXT("mtu: fakenet parts=%d keys=%d"), Got.Num(), Wired.Num()));
	}

	FNsPacket JoinHuge = Join;
	for (int32 i = Ns::JoinSnapEvery; i < HugeCount; ++i)
	{
		JoinHuge.Frames.Add(76 + i, In);
	}
	NsSplitForMtu(JoinHuge, Parts);
	if (Parts.Num() < 2)
	{
		return Fail(TEXT("mtu: join split"));
	}
	const int32 JoinParts = Parts.Num();
	for (const FNsPacket& Part : Parts)
	{
		if (Part.Type != ENsMsg::S2CJoinSnap || !NsFitsMtu(Part) || Part.Tick != 76)
		{
			return Fail(TEXT("mtu: join fragment must stay JoinSnap and fit"));
		}
	}

	FNsPacket HugeIn;
	HugeIn.Type = ENsMsg::C2SInput;
	const int32 InCount = Ns::MaxC2SInputEntries + 40;
	for (int32 i = 0; i < InCount; ++i)
	{
		HugeIn.SeqWindow.Add(i + 1);
		HugeIn.DxWindow.Add(static_cast<int8>((i % 3) - 1));
	}
	NsSplitForMtu(HugeIn, Parts);
	if (Parts.Num() < 2)
	{
		return Fail(TEXT("mtu: c2s split"));
	}
	int32 WinSum = 0;
	for (const FNsPacket& Part : Parts)
	{
		if (Part.Type != ENsMsg::C2SInput || !NsFitsMtu(Part))
		{
			return Fail(TEXT("mtu: c2s fragment"));
		}
		WinSum += Part.SeqWindow.Num();
	}
	if (WinSum != InCount)
	{
		return FailStr(FString::Printf(TEXT("mtu: c2s keys %d"), WinSum));
	}
	const int32 C2sParts = Parts.Num();

	FNsPacket HugeP2p;
	HugeP2p.Type = ENsMsg::P2PInput;
	const int32 P2pCount = Ns::MaxP2PInputEntries + 40;
	for (int32 i = 0; i < P2pCount; ++i)
	{
		HugeP2p.RemoteDx.Add(i, static_cast<int8>((i % 3) - 1));
	}
	NsSplitForMtu(HugeP2p, Parts);
	if (Parts.Num() < 2)
	{
		return Fail(TEXT("mtu: p2p split"));
	}
	int32 P2pSum = 0;
	for (const FNsPacket& Part : Parts)
	{
		if (Part.Type != ENsMsg::P2PInput || !NsFitsMtu(Part))
		{
			return Fail(TEXT("mtu: p2p fragment"));
		}
		P2pSum += Part.RemoteDx.Num();
	}
	if (P2pSum != P2pCount)
	{
		return FailStr(FString::Printf(TEXT("mtu: p2p keys %d"), P2pSum));
	}

	return OkStr(FString::Printf(TEXT("mtu typical=%d split=%d join-split=%d c2s-split=%d p2p-split=%d"),
		Typical, Got.Num(), JoinParts, C2sParts, Parts.Num()));
}

static FNsSelfTestResult RunOrStop(const FNsSelfTestResult& R)
{
	return R;
}

FNsSelfTestResult NsRunAllSelfTests()
{
	using FFn = FNsSelfTestResult(*)();
	const FFn Fns[] = {
		&NsRunWorldContractSelfTest,
		&NsRunCodecContractSelfTest,
		&NsRunSeqWindowSelfTest,
		&NsRunRouteGuardSelfTest,
		&NsRunFakeNetContractSelfTest,
		&NsRunFakeNetDropRateSelfTest,
		&NsRunMtuSelfTest,
		&NsRunLockstepSelfTest,
		&NsRunLockstepCleanSelfTest,
		&NsRunLockstepHighDropSelfTest,
		&NsRunLockstepJoinSelfTest,
		&NsRunLockstepLateJoinSelfTest,
		&NsRunLockstepNoSkipSelfTest,
		&NsRunLockstepNackSelfTest,
		&NsRunLockstepNackJoinSelfTest,
		&NsRunLockstepJoinFragSelfTest,
		&NsRunLockstepDesyncSelfTest,
		&NsRunSchemeSwitchSelfTest,
		&NsRunSchemeApplySelfTest,
		&NsRunLockstepResyncAlignSelfTest,
		&NsRunLockstepResyncForceSelfTest,
		&NsRunLockstepResyncIgnoreFrameSelfTest,
		&NsRunLockstepResyncDropSelfTest,
		&NsRunLockstepResyncApplyJoinSelfTest,
		&NsRunLockstepResyncStaleJoinSelfTest,
		&NsRunLockstepResyncGiveUpSelfTest,
		&NsRunLockstepResyncResumeSelfTest,
		&NsRunLockstepResyncAgainSelfTest,
		&NsRunLockstepResyncCleanSelfTest,
		&NsRunLockstepResyncWireSelfTest,
		&NsRunLockstepResyncUdpSelfTest,
		&NsRunLockstepResyncKickOffSelfTest,
		&NsRunLockstepResyncKickSelfTest,
		&NsRunLockstepDoorCleanSelfTest,
		&NsRunLockstepDoorDropOpenSelfTest,
		&NsRunLockstepDoorDropFrameSelfTest,
		&NsRunLockstepDoorIgnoreSnapSelfTest,
		&NsRunLockstepDoorNotInStepSelfTest,
		&NsRunLockstepDoorComposeSelfTest,
		&NsRunLockstepWaitDoorComposeSelfTest,
		&NsRunLockstepTurnDoorComposeSelfTest,
		&NsRunLockstepDelayDoorComposeSelfTest,
		&NsRunLockstepWaitCleanSelfTest,
		&NsRunLockstepWaitStallSelfTest,
		&NsRunLockstepWaitDropSelfTest,
		&NsRunLockstepWaitJoinSelfTest,
		&NsRunLockstepWaitNackSelfTest,
		&NsRunLockstepWaitNackJoinSelfTest,
		&NsRunLockstepWaitKickSelfTest,
		&NsRunLockstepWaitKickResumeSelfTest,
		&NsRunLockstepWaitResyncAlignSelfTest,
		&NsRunLockstepWaitResyncForceSelfTest,
		&NsRunLockstepWaitResyncIgnoreFrameSelfTest,
		&NsRunLockstepWaitResyncResumeSelfTest,
		&NsRunLockstepWaitResyncAgainSelfTest,
		&NsRunLockstepWaitResyncWireSelfTest,
		&NsRunLockstepWaitResyncUdpSelfTest,
		&NsRunLockstepWaitResyncKickOffSelfTest,
		&NsRunLockstepWaitResyncKickSelfTest,
		&NsRunLockstepTurnCleanSelfTest,
		&NsRunLockstepTurnLateSelfTest,
		&NsRunLockstepTurnDropSelfTest,
		&NsRunLockstepTurnSpeedSelfTest,
		&NsRunLockstepTurnLenDropSelfTest,
		&NsRunLockstepTurnLongRunSelfTest,
		&NsRunLockstepTurnRecoverySelfTest,
		&NsRunLockstepTurnResyncAlignSelfTest,
		&NsRunLockstepTurnResyncForceSelfTest,
		&NsRunLockstepTurnResyncIgnoreFrameSelfTest,
		&NsRunLockstepTurnResyncResumeSelfTest,
		&NsRunLockstepTurnResyncAgainSelfTest,
		&NsRunLockstepTurnResyncWireSelfTest,
		&NsRunLockstepTurnResyncUdpSelfTest,
		&NsRunLockstepTurnResyncKickOffSelfTest,
		&NsRunLockstepTurnResyncKickSelfTest,
		&NsRunLockstepDelayCleanSelfTest,
		&NsRunLockstepDelayRttSelfTest,
		&NsRunLockstepDelayHighRttSelfTest,
		&NsRunLockstepDelayFromRttSelfTest,
		&NsRunLockstepDelayAdaptSelfTest,
		&NsRunLockstepDelayRecoverySelfTest,
		&NsRunLockstepDelayNackSelfTest,
		&NsRunLockstepDelayResyncAlignSelfTest,
		&NsRunLockstepDelayResyncForceSelfTest,
		&NsRunLockstepDelayResyncIgnoreFrameSelfTest,
		&NsRunLockstepDelayResyncResumeSelfTest,
		&NsRunLockstepDelayResyncAgainSelfTest,
		&NsRunLockstepDelayResyncWireSelfTest,
		&NsRunLockstepDelayResyncUdpSelfTest,
		&NsRunLockstepDelayResyncKickOffSelfTest,
		&NsRunLockstepDelayResyncKickSelfTest,
		&NsRunStateSyncSelfTest,
		&NsRunStateSyncCleanSelfTest,
		&NsRunStateSyncRewindSelfTest,
		&NsRunStateSyncFireSelfTest,
		&NsRunStateSyncNackSelfTest,
		&NsRunStateSyncInboxHoleSelfTest,
		&NsRunStateSyncInboxCapSelfTest,
		&NsRunStateSyncUnackedWindowSelfTest,
		&NsRunStateSyncLongOutageSelfTest,
		&NsRunStateSyncClockOffsetSelfTest,
		&NsRunStateSyncOldSnapSelfTest,
		&NsRunStateSyncSpoofSelfTest,
		&NsRunRollbackSelfTest,
		&NsRunRollbackCleanSelfTest,
		&NsRunRollbackWaitSelfTest,
		&NsRunRollbackHoleSelfTest,
		&NsRunRollbackMidHoleSelfTest,
		&NsRunRollbackConflictingInputSelfTest,
		&NsRunUdpLoopbackSelfTest,
		&NsRunUdpLockstepSelfTest,
		&NsRunUdpStateSyncSelfTest,
		&NsRunUdpRollbackSelfTest,
		&NsRunUdpPeersSelfTest,
		&NsRunUdpSplitLockstepSelfTest,
		&NsRunUdpSplitStateSyncSelfTest,
		&NsRunUdpSplitRollbackSelfTest,
		&NsRunUdpBurstSelfTest,
		&NsRunUdpSessionRestartSelfTest,
		&NsRunStunBindSelfTest,
		&NsRunStunLoopbackSelfTest,
		&NsRunStunPunchSelfTest,
		&NsRunStunRendezvousSelfTest,
		&NsRunStunCheckSelfTest,
		&NsRunStunTurnSelfTest,
		&NsRunStunPermitSelfTest,
		&NsRunStunChannelSelfTest,
		&NsRunStunChannelPeersSelfTest,
		&NsRunStunPermitPeersSelfTest,
		&NsRunStunRendezvousOrderSelfTest,
		&NsRunStunChannelMtuSelfTest,
		&NsRunWorldStressSelfTest,
		&NsRunCodecStressSelfTest,
		&NsRunFakeNetStressSelfTest,
		&NsRunLockstepStressSelfTest,
		&NsRunStateSyncStressSelfTest,
		&NsRunRollbackStressSelfTest,
	};
	TArray<FString> Parts;
	for (FFn Fn : Fns)
	{
		const FNsSelfTestResult R = RunOrStop(Fn());
		if (!R.bOk)
		{
			return R;
		}
		Parts.Add(R.Detail);
	}
	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = FString::Join(Parts, TEXT(" | "));
	return Ok;
}

void NsRunSelfTestAndLog()
{
	const FNsSelfTestResult R = NsRunAllSelfTests();
	if (R.bOk)
	{
		UE_LOG(LogNetworkSync, Display, TEXT("NetworkSync self-test OK: %s"), *R.Detail);
	}
	else
	{
		UE_LOG(LogNetworkSync, Error, TEXT("NetworkSync self-test FAIL: %s"), *R.Detail);
	}
}
