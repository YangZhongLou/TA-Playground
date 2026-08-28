// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#if WITH_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "NsSelfTest.h"
#include "NsTypes.h"
#include "NsFakeNet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsWorld_Determinism, "TA.NetworkSync.World.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsWorld_Determinism::RunTest(const FString& Parameters)
{
	FNsWorld A;
	FNsWorld B;
	const int8 Script[][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, 1}};
	for (int32 i = 0; i < 200; ++i)
	{
		A.Step(Script[i % 4], Ns::LockstepSpeed);
		B.Step(Script[i % 4], Ns::LockstepSpeed);
	}
	TestTrue(TEXT("worlds equal"), A.Equals(B));
	TestEqual(TEXT("checksum"), A.Checksum(), B.Checksum());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_Lockstep, "TA.NetworkSync.Lockstep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_Lockstep::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunLockstepSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_StateSync, "TA.NetworkSync.StateSync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_StateSync::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunStateSyncSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_Rollback, "TA.NetworkSync.Rollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_Rollback::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunRollbackSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsFakeNet_SeqIncreases, "TA.NetworkSync.FakeNet.Seq",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsFakeNet_SeqIncreases::RunTest(const FString& Parameters)
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Net.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
	Net.Advance(1.0);
	TArray<FNsPacket> First;
	Net.Drain(ENsAddr::Sv, First);
	TestEqual(TEXT("first deliver"), First.Num(), 1);
	Net.Send(ENsAddr::C0, ENsAddr::Sv, Pkt);
	Net.Advance(1.0);
	TArray<FNsPacket> Second;
	Net.Drain(ENsAddr::Sv, Second);
	TestEqual(TEXT("second deliver"), Second.Num(), 1);
	TestTrue(TEXT("seq increased"), Second[0].Seq > First[0].Seq);
	return true;
}

#endif
