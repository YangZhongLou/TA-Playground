// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#if WITH_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "NsSelfTest.h"
#include "NsTypes.h"
#include "NsFakeNet.h"
#include "NsCodec.h"
#include "NsDoor.h"
#include "NsReplicatedActor.h"
#include "UObject/Package.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_Lockstep, "TA.NetworkSync.Lockstep.Drop10",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_Lockstep::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunLockstepSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_LockstepJoin, "TA.NetworkSync.Lockstep.Join",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_LockstepJoin::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunLockstepJoinSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_StateSync, "TA.NetworkSync.StateSync.Drop05",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_StateSync::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunStateSyncSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_Rollback, "TA.NetworkSync.Rollback.Drop05",
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsCodec_RoundTrip, "TA.NetworkSync.Codec.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsCodec_RoundTrip::RunTest(const FString& Parameters)
{
	FNsPacket Src;
	Src.Type = ENsMsg::S2CFrame;
	Src.Seq = 9;
	Src.Ack = 4;
	Src.AckBits = 0xA5A5A5A5u;
	FNsInputs In;
	In.Dx[0] = -1;
	In.Dx[1] = 1;
	Src.Frames.Add(3, In);
	Src.Frames.Add(4, In);

	TArray<uint8> Bytes;
	TestTrue(TEXT("encode"), NsEncodePacket(Src, Bytes));
	TestTrue(TEXT("header size"), Bytes.Num() >= Ns::HeaderBytes);
	TestEqual(TEXT("magic0"), static_cast<int32>(Bytes[0]), static_cast<int32>(Ns::PacketMagic & 0xFFu));

	FNsPacket Dst;
	TestTrue(TEXT("decode"), NsDecodePacket(Bytes, Dst));
	TestEqual(TEXT("type"), static_cast<uint8>(Dst.Type), static_cast<uint8>(ENsMsg::S2CFrame));
	TestEqual(TEXT("seq"), Dst.Seq, 9);
	TestEqual(TEXT("ack"), Dst.Ack, 4);
	TestEqual(TEXT("ackbits"), Dst.AckBits, 0xA5A5A5A5u);
	TestEqual(TEXT("frames"), Dst.Frames.Num(), 2);
	const FNsInputs* F3 = Dst.Frames.Find(3);
	TestTrue(TEXT("frame3"), F3 && F3->Dx[0] == -1 && F3->Dx[1] == 1);

	FNsPacket Join;
	Join.Type = ENsMsg::S2CJoinSnap;
	Join.Tick = 76;
	Join.SnapX[0] = 40;
	Join.SnapX[1] = -16;
	Join.SnapRng = 12345;
	Join.Frames.Add(76, In);
	TArray<uint8> JoinBytes;
	TestTrue(TEXT("encode join"), NsEncodePacket(Join, JoinBytes));
	FNsPacket JoinOut;
	TestTrue(TEXT("decode join"), NsDecodePacket(JoinBytes, JoinOut));
	TestEqual(TEXT("join tick"), JoinOut.Tick, 76);
	TestEqual(TEXT("join x0"), JoinOut.SnapX[0], 40);
	TestEqual(TEXT("join rng"), JoinOut.SnapRng, 12345u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsCodec_RejectsBad, "TA.NetworkSync.Codec.RejectsBad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsCodec_RejectsBad::RunTest(const FString& Parameters)
{
	FNsPacket Src;
	Src.Type = ENsMsg::C2SChecksum;
	Src.PlayerId = 1;
	Src.Tick = 15;
	Src.Hash = 0x11u;
	TArray<uint8> Bytes;
	TestTrue(TEXT("encode"), NsEncodePacket(Src, Bytes));

	TArray<uint8> BadMagic = Bytes;
	BadMagic[0] ^= 1;
	FNsPacket Out;
	TestFalse(TEXT("bad magic"), NsDecodePacket(BadMagic, Out));

	TArray<uint8> BadType = Bytes;
	BadType[4] = 99;
	TestFalse(TEXT("unknown type"), NsDecodePacket(BadType, Out));

	TArray<uint8> BadLen = Bytes;
	BadLen[6] = 0;
	BadLen[7] = 0;
	TestFalse(TEXT("payload len"), NsDecodePacket(BadLen, Out));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_UdpLoopback, "TA.NetworkSync.Udp.Loopback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_UdpLoopback::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunUdpLoopbackSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_UdpLockstep, "TA.NetworkSync.Udp.Lockstep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_UdpLockstep::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunUdpLockstepSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_UdpPeers, "TA.NetworkSync.Udp.Peers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_UdpPeers::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunUdpPeersSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_UdpSplit, "TA.NetworkSync.Udp.Split",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_UdpSplit::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunUdpSplitLockstepSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

static constexpr EAutomationTestFlags NsAutoFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

#define NS_WRAP(ClassName, TestPath, Fn, Flags) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, TestPath, Flags) \
	bool ClassName::RunTest(const FString& Parameters) \
	{ \
		const FNsSelfTestResult R = Fn(); \
		TestTrue(R.Detail, R.bOk); \
		return R.bOk; \
	}

NS_WRAP(FNsWorld_Contract, "TA.NetworkSync.World.Contract", NsRunWorldContractSelfTest, NsAutoFlags)
NS_WRAP(FNsCodec_Contract, "TA.NetworkSync.Codec.Contract", NsRunCodecContractSelfTest, NsAutoFlags)
NS_WRAP(FNsCodec_Mtu, "TA.NetworkSync.Codec.Mtu", NsRunMtuSelfTest, NsAutoFlags)
NS_WRAP(FNsSeqWindow_Dup, "TA.NetworkSync.FakeNet.SeqWindow", NsRunSeqWindowSelfTest, NsAutoFlags)
NS_WRAP(FNsFakeNet_DropDelay, "TA.NetworkSync.FakeNet.DropDelay", NsRunFakeNetContractSelfTest, NsAutoFlags)
NS_WRAP(FNsFakeNet_DropRate, "TA.NetworkSync.FakeNet.DropRate", NsRunFakeNetDropRateSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_Clean, "TA.NetworkSync.Lockstep.Clean", NsRunLockstepCleanSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_HighDrop, "TA.NetworkSync.Lockstep.HighDrop", NsRunLockstepHighDropSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_LateJoin, "TA.NetworkSync.Lockstep.LateJoin", NsRunLockstepLateJoinSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_NoSkip, "TA.NetworkSync.Lockstep.NoSkip", NsRunLockstepNoSkipSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_JoinFrag, "TA.NetworkSync.Lockstep.JoinFrag", NsRunLockstepJoinFragSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_Desync, "TA.NetworkSync.Lockstep.Desync", NsRunLockstepDesyncSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_Clean, "TA.NetworkSync.StateSync.Clean", NsRunStateSyncCleanSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_Rewind, "TA.NetworkSync.StateSync.Rewind", NsRunStateSyncRewindSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_Nack, "TA.NetworkSync.StateSync.Nack", NsRunStateSyncNackSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_InboxHole, "TA.NetworkSync.StateSync.InboxHole", NsRunStateSyncInboxHoleSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_OldSnap, "TA.NetworkSync.StateSync.OldSnap", NsRunStateSyncOldSnapSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_Spoof, "TA.NetworkSync.StateSync.Spoof", NsRunStateSyncSpoofSelfTest, NsAutoFlags)
NS_WRAP(FNsRollback_Clean, "TA.NetworkSync.Rollback.Clean", NsRunRollbackCleanSelfTest, NsAutoFlags)
NS_WRAP(FNsRollback_Wait, "TA.NetworkSync.Rollback.Wait", NsRunRollbackWaitSelfTest, NsAutoFlags)
NS_WRAP(FNsRollback_Hole, "TA.NetworkSync.Rollback.Hole", NsRunRollbackHoleSelfTest, NsAutoFlags)
NS_WRAP(FNsRollback_MidHole, "TA.NetworkSync.Rollback.MidHole", NsRunRollbackMidHoleSelfTest, NsAutoFlags)
NS_WRAP(FNsUdp_Burst, "TA.NetworkSync.Udp.Burst", NsRunUdpBurstSelfTest, NsAutoFlags)

NS_WRAP(FNsWorld_Stress, "TA.NetworkSync.Stress.World", NsRunWorldStressSelfTest, NsAutoFlags)
NS_WRAP(FNsCodec_Stress, "TA.NetworkSync.Stress.Codec", NsRunCodecStressSelfTest, NsAutoFlags)
NS_WRAP(FNsFakeNet_Stress, "TA.NetworkSync.Stress.FakeNet", NsRunFakeNetStressSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_Stress, "TA.NetworkSync.Stress.Lockstep", NsRunLockstepStressSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_Stress, "TA.NetworkSync.Stress.StateSync", NsRunStateSyncStressSelfTest, NsAutoFlags)
NS_WRAP(FNsRollback_Stress, "TA.NetworkSync.Stress.Rollback", NsRunRollbackStressSelfTest, NsAutoFlags)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsActors_Cdo, "TA.NetworkSync.Actors.Cdo", NsAutoFlags)

bool FNsActors_Cdo::RunTest(const FString& Parameters)
{
	UPackage* Pkg = GetTransientPackage();
	ANsDoor* Door = NewObject<ANsDoor>(Pkg);
	TestNotNull(TEXT("door"), Door);
	TestTrue(TEXT("door replicates"), Door->GetIsReplicated());
	Door->ServerSetOpen_Implementation(true);
	TestTrue(TEXT("door open"), Door->bOpen);
	Door->ServerSetOpen_Implementation(false);
	TestFalse(TEXT("door closed"), Door->bOpen);

	ANsReplicatedActor* Counter = NewObject<ANsReplicatedActor>(Pkg);
	TestNotNull(TEXT("counter"), Counter);
	TestTrue(TEXT("counter replicates"), Counter->GetIsReplicated());
	TestEqual(TEXT("counter start"), Counter->Counter, 0);
	Counter->ServerBump_Implementation();
	Counter->ServerBump_Implementation();
	TestEqual(TEXT("counter bump"), Counter->Counter, 2);
	return true;
}

#undef NS_WRAP

#endif
