// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsSelfTest.h"
#include "NsTypes.h"
#include "NsFakeNet.h"
#include "NsLockstep.h"
#include "NsStateSync.h"
#include "NsRollback.h"

DEFINE_LOG_CATEGORY_STATIC(LogNetworkSync, Log, All);

static FNsSelfTestResult Fail(const TCHAR* Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static void PumpLockstepServer(FNsFakeNet& Net, FNsLockstepServer& Sv)
{
	TArray<FNsPacket> ToSv;
	Net.Drain(ENsAddr::Sv, ToSv);
	for (const FNsPacket& P : ToSv)
	{
		if (P.Type == ENsMsg::C2SInput)
		{
			Sv.OnInput(P.PlayerId, P.Dx);
		}
		else if (P.Type == ENsMsg::C2SChecksum)
		{
			Sv.OnChecksum(P.Tick, P.Hash);
		}
	}
	Sv.Tick(Net);
}

static void PumpLockstepClient(FNsFakeNet& Net, FNsLockstepClient& C)
{
	TArray<FNsPacket> ToC;
	Net.Drain(C.Addr, ToC);
	for (const FNsPacket& P : ToC)
	{
		if (P.Type == ENsMsg::S2CFrame)
		{
			C.OnS2C(P.Frames);
		}
	}
	C.Logic(Net);
}

static void PumpStateServer(FNsFakeNet& Net, FNsStateSyncServer& Sv)
{
	TArray<FNsPacket> ToSv;
	Net.Drain(ENsAddr::Sv, ToSv);
	for (const FNsPacket& P : ToSv)
	{
		if (P.Type == ENsMsg::C2SInput)
		{
			for (int32 i = 0; i < P.SeqWindow.Num(); ++i)
			{
				Sv.OnInput(P.PlayerId, P.SeqWindow[i], P.DxWindow[i]);
			}
		}
		else if (P.Type == ENsMsg::C2SSnapAck)
		{
			Sv.OnAck(P.PlayerId, P.Tick);
		}
	}
	Sv.Sim(Net);
}

static void PumpStateClient(FNsFakeNet& Net, FNsStateSyncClient& C)
{
	TArray<FNsPacket> ToC;
	Net.Drain(C.Addr, ToC);
	for (const FNsPacket& P : ToC)
	{
		if (P.Type == ENsMsg::S2CSnapshot)
		{
			C.OnSnap(Net, P);
		}
	}
	C.UpdateRemoteDraw(Net.Now);
}

static void PumpRollback(FNsFakeNet& Net, FNsRollbackPeer& A, FNsRollbackPeer& B)
{
	TArray<FNsPacket> ToA;
	Net.Drain(ENsAddr::C0, ToA);
	for (const FNsPacket& P : ToA)
	{
		if (P.Type == ENsMsg::P2PInput)
		{
			A.OnRemote(P.RemoteDx);
		}
	}
	TArray<FNsPacket> ToB;
	Net.Drain(ENsAddr::C1, ToB);
	for (const FNsPacket& P : ToB)
	{
		if (P.Type == ENsMsg::P2PInput)
		{
			B.OnRemote(P.RemoteDx);
		}
	}
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
		PumpLockstepServer(Net, Sv);
		PumpLockstepClient(Net, C0);
		PumpLockstepClient(Net, C1);
		Net.Advance(1.0);
	}

	for (int32 i = 0; i < 200; ++i)
	{
		PumpLockstepServer(Net, Sv);
		PumpLockstepClient(Net, C0);
		PumpLockstepClient(Net, C1);
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
		PumpStateServer(Net, Sv);
		PumpStateClient(Net, C0);
		PumpStateClient(Net, C1);
		Net.Advance(Ns::SimDtMs);
	}

	for (int32 i = 0; i < 40; ++i)
	{
		PumpStateServer(Net, Sv);
		PumpStateClient(Net, C0);
		PumpStateClient(Net, C1);
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
		PumpRollback(Net, A, B);
		Net.Advance(Ns::RollbackDtMs);
	}

	for (int32 i = 0; i < 40; ++i)
	{
		PumpRollback(Net, A, B);
		Net.Advance(Ns::RollbackDtMs);
	}

	if (!A.World.Equals(B.World))
	{
		return Fail(TEXT("rollback: peers diverged"));
	}
	if (A.bWaiting || B.bWaiting)
	{
		return Fail(TEXT("rollback: still waiting after cooldown"));
	}
	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = FString::Printf(TEXT("rollback frame=%d wait=%d x=(%d,%d)"),
		A.Frame, A.WaitCount + B.WaitCount, A.World.X[0], A.World.X[1]);
	return Ok;
}

FNsSelfTestResult NsRunAllSelfTests()
{
	const FNsSelfTestResult A = NsRunLockstepSelfTest();
	if (!A.bOk)
	{
		return A;
	}
	const FNsSelfTestResult B = NsRunStateSyncSelfTest();
	if (!B.bOk)
	{
		return B;
	}
	const FNsSelfTestResult C = NsRunRollbackSelfTest();
	if (!C.bOk)
	{
		return C;
	}
	FNsSelfTestResult Ok;
	Ok.bOk = true;
	Ok.Detail = A.Detail + TEXT(" | ") + B.Detail + TEXT(" | ") + C.Detail;
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
