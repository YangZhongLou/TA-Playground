// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsSelfTest.h"
#include "NsNetManager.h"
#include "NsPacket.h"
#include "UObject/Package.h"

static FNsSelfTestResult SaFail(const TCHAR* Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult SaFailStr(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = false;
	R.Detail = Msg;
	return R;
}

static FNsSelfTestResult SaOk(const FString& Msg)
{
	FNsSelfTestResult R;
	R.bOk = true;
	R.Detail = Msg;
	return R;
}

void NsSchemeApplyDirty(ANsNetManager& M)
{
	M.Fake.RttMs = 0.f;
	M.Fake.Drop = 0.f;
	M.Fake.JitterMs = 0.f;
	M.Fake.Now = 5000.0;
	M.LsSv.Frame = 40;
	M.LsSv.World.X[0] = 777;
	M.LsSv.World.X[1] = -777;
	M.LsC0.World.X[0] = 777;
	M.LsC0.ExecFrame = 40;
	M.WaitSv.Frame = 11;
	M.TurnSv.Frame = 22;
	M.DelaySv.Frame = 33;
	M.SsC0.PredX = 888;
	M.SsC1.PredX = 888;
	M.SsSv.Pawns[0].X = 888;
	M.DoorSv.Open = 1;
	M.LsResync.bCaptured = true;
	M.AccumMs = 50.0;
	FNsPacket Junk;
	Junk.Type = ENsMsg::C2SInput;
	Junk.Dx = 1;
	M.Fake.Send(ENsAddr::C0, ENsAddr::Sv, Junk);
}

FNsSelfTestResult NsRunSchemeApplySelfTest()
{
	ANsNetManager* M = NewObject<ANsNetManager>(GetTransientPackage());
	if (!M)
	{
		return SaFail(TEXT("scheme-apply: NewObject"));
	}

	NsSchemeApplyDirty(*M);
	M->Scheme = ENsScheme::Lockstep;
	M->LockstepKind = ENsLockstepKind::Optimistic;
	M->ApplyScheme(ENsScheme::Lockstep);
	if (M->AppliedScheme != ENsScheme::Lockstep
		|| M->AppliedLockstepKind != ENsLockstepKind::Optimistic)
	{
		return SaFail(TEXT("scheme-apply: applied lockstep"));
	}
	if (M->Fake.Now != 0.0 || M->LsSv.NextMs != 0.0 || M->AccumMs != 0.0)
	{
		return SaFailStr(FString::Printf(
			TEXT("scheme-apply: clock now=%.0f next=%.0f accum=%.0f"),
			M->Fake.Now, M->LsSv.NextMs, M->AccumMs));
	}
	TArray<FNsPacket> Leftover;
	M->Fake.Drain(ENsAddr::Sv, Leftover);
	if (Leftover.Num() != 0)
	{
		return SaFail(TEXT("scheme-apply: queue survived ApplyScheme"));
	}
	if (M->LsSv.Frame != 0 || M->LsC0.ExecFrame != 0
		|| M->LsSv.World.X[0] != 0 || M->LsC0.World.X[0] != 0)
	{
		return SaFailStr(FString::Printf(
			TEXT("scheme-apply: lockstep leftover sv=%d x=%d c0=%d"),
			M->LsSv.Frame, M->LsSv.World.X[0], M->LsC0.World.X[0]));
	}
	if (M->SsC0.PredX != 0 || M->SsC1.PredX != 0 || M->SsSv.Pawns[0].X != 0)
	{
		return SaFailStr(FString::Printf(
			TEXT("scheme-apply: pred leftover pred=%d pawn=%d"),
			M->SsC0.PredX, M->SsSv.Pawns[0].X));
	}
	if (M->DoorSv.Open != 0 || M->LsResync.bCaptured)
	{
		return SaFail(TEXT("scheme-apply: door/resync leftover"));
	}
	if (M->GetPawnLocation(0).X != 0.f)
	{
		return SaFailStr(FString::Printf(
			TEXT("scheme-apply: pawn x=%.0f"), M->GetPawnLocation(0).X));
	}

	NsSchemeApplyDirty(*M);
	M->Scheme = ENsScheme::StateSync;
	M->ApplyScheme(ENsScheme::StateSync);
	if (M->SsC0.PredX != 0 || M->SsSv.Pawns[0].X != 0)
	{
		return SaFailStr(FString::Printf(
			TEXT("scheme-apply: state inherited pred=%d x=%d"),
			M->SsC0.PredX, M->SsSv.Pawns[0].X));
	}
	if (M->LsSv.Frame != 0 || M->LsC0.World.X[0] != 0)
	{
		return SaFail(TEXT("scheme-apply: state cut left lockstep"));
	}

	M->SsC0.PredX = 888;
	M->SsSv.Pawns[0].X = 888;
	M->Scheme = ENsScheme::Lockstep;
	M->LockstepKind = ENsLockstepKind::Optimistic;
	M->ApplyScheme(ENsScheme::Lockstep);
	if (M->LsC0.World.X[0] == 888 || M->LsSv.World.X[0] == 888
		|| M->GetPawnLocation(0).X != 0.f)
	{
		return SaFailStr(FString::Printf(
			TEXT("scheme-apply: lockstep inherited PredX x=%d pawn=%.0f"),
			M->LsC0.World.X[0], M->GetPawnLocation(0).X));
	}
	if (M->SsC0.PredX != 0)
	{
		return SaFail(TEXT("scheme-apply: PredX survived lockstep cut"));
	}

	NsSchemeApplyDirty(*M);
	M->Scheme = ENsScheme::Lockstep;
	M->LockstepKind = ENsLockstepKind::Conservative;
	M->ApplyScheme(ENsScheme::Lockstep);
	if (M->AppliedLockstepKind != ENsLockstepKind::Conservative
		|| M->WaitSv.Frame != 0 || M->WaitC0.ExecFrame != 0)
	{
		return SaFailStr(FString::Printf(
			TEXT("scheme-apply: wait leftover kind=%d frame=%d"),
			static_cast<int32>(M->AppliedLockstepKind), M->WaitSv.Frame));
	}

	return SaOk(TEXT("scheme-apply"));
}
