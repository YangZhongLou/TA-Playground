// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#if WITH_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "NsSelfTest.h"
#include "NsTypes.h"
#include "NsFakeNet.h"
#include "NsCodec.h"
#include "NsDoor.h"
#include "NsNetManager.h"
#include "NsReplicatedActor.h"
#include "NsInputProxy.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsWorld_Determinism, "NetworkSync.World.Determinism",
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_Lockstep, "NetworkSync.Lockstep.Drop10",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_Lockstep::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunLockstepSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_LockstepJoin, "NetworkSync.Lockstep.Join",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_LockstepJoin::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunLockstepJoinSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_StateSync, "NetworkSync.StateSync.Drop05",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_StateSync::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunStateSyncSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_Rollback, "NetworkSync.Rollback.Drop05",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_Rollback::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunRollbackSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsFakeNet_SeqIncreases, "NetworkSync.FakeNet.Seq",
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsCodec_RoundTrip, "NetworkSync.Codec.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsCodec_RoundTrip::RunTest(const FString& Parameters)
{
	FNsPacket Src;
	Src.Type = ENsMsg::S2CFrame;
	Src.Session = 0x12345678u;
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
	TestEqual(TEXT("session"), Dst.Session, 0x12345678u);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsCodec_RejectsBad, "NetworkSync.Codec.RejectsBad",
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_UdpLoopback, "NetworkSync.Udp.Loopback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_UdpLoopback::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunUdpLoopbackSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_UdpLockstep, "NetworkSync.Udp.Lockstep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_UdpLockstep::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunUdpLockstepSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_UdpPeers, "NetworkSync.Udp.Peers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNsSelfTest_UdpPeers::RunTest(const FString& Parameters)
{
	const FNsSelfTestResult R = NsRunUdpPeersSelfTest();
	TestTrue(R.Detail, R.bOk);
	return R.bOk;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsSelfTest_UdpSplit, "NetworkSync.Udp.Split",
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

NS_WRAP(FNsWorld_Contract, "NetworkSync.World.Contract", NsRunWorldContractSelfTest, NsAutoFlags)
NS_WRAP(FNsCodec_Contract, "NetworkSync.Codec.Contract", NsRunCodecContractSelfTest, NsAutoFlags)
NS_WRAP(FNsCodec_Mtu, "NetworkSync.Codec.Mtu", NsRunMtuSelfTest, NsAutoFlags)
NS_WRAP(FNsSeqWindow_Dup, "NetworkSync.FakeNet.SeqWindow", NsRunSeqWindowSelfTest, NsAutoFlags)
NS_WRAP(FNsRouteGuard, "NetworkSync.FakeNet.RouteGuard", NsRunRouteGuardSelfTest, NsAutoFlags)
NS_WRAP(FNsFakeNet_DropDelay, "NetworkSync.FakeNet.DropDelay", NsRunFakeNetContractSelfTest, NsAutoFlags)
NS_WRAP(FNsFakeNet_DropRate, "NetworkSync.FakeNet.DropRate", NsRunFakeNetDropRateSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_Clean, "NetworkSync.Lockstep.Clean", NsRunLockstepCleanSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_HighDrop, "NetworkSync.Lockstep.HighDrop", NsRunLockstepHighDropSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_LateJoin, "NetworkSync.Lockstep.LateJoin", NsRunLockstepLateJoinSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_NoSkip, "NetworkSync.Lockstep.NoSkip", NsRunLockstepNoSkipSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_Nack, "NetworkSync.Lockstep.Nack", NsRunLockstepNackSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_NackJoin, "NetworkSync.Lockstep.NackJoin", NsRunLockstepNackJoinSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_JoinFrag, "NetworkSync.Lockstep.JoinFrag", NsRunLockstepJoinFragSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_Desync, "NetworkSync.Lockstep.Desync", NsRunLockstepDesyncSelfTest, NsAutoFlags)
NS_WRAP(FNsScheme_Switch, "NetworkSync.Runtime.SchemeSwitch", NsRunSchemeSwitchSelfTest, NsAutoFlags)
NS_WRAP(FNsScheme_Apply, "NetworkSync.Runtime.SchemeApply", NsRunSchemeApplySelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_ResyncAlign, "NetworkSync.Lockstep.Resync.Align", NsRunLockstepResyncAlignSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_ResyncForce, "NetworkSync.Lockstep.Resync.Force", NsRunLockstepResyncForceSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_ResyncIgnore, "NetworkSync.Lockstep.Resync.IgnoreFrame", NsRunLockstepResyncIgnoreFrameSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_ResyncDrop, "NetworkSync.Lockstep.Resync.Drop", NsRunLockstepResyncDropSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_ResyncJoin, "NetworkSync.Lockstep.Resync.ApplyJoin", NsRunLockstepResyncApplyJoinSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_ResyncStale, "NetworkSync.Lockstep.Resync.StaleJoin", NsRunLockstepResyncStaleJoinSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_ResyncGiveUp, "NetworkSync.Lockstep.Resync.GiveUp", NsRunLockstepResyncGiveUpSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_ResyncResume, "NetworkSync.Lockstep.Resync.Resume", NsRunLockstepResyncResumeSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_ResyncAgain, "NetworkSync.Lockstep.Resync.Again", NsRunLockstepResyncAgainSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_ResyncClean, "NetworkSync.Lockstep.Resync.Clean", NsRunLockstepResyncCleanSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_ResyncWire, "NetworkSync.Lockstep.Resync.Wire", NsRunLockstepResyncWireSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_ResyncUdp, "NetworkSync.Lockstep.Resync.Udp", NsRunLockstepResyncUdpSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_ResyncKickOff, "NetworkSync.Lockstep.Resync.KickOff", NsRunLockstepResyncKickOffSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_ResyncKick, "NetworkSync.Lockstep.Resync.Kick", NsRunLockstepResyncKickSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstepDoor_Clean, "NetworkSync.LockstepDoor.Clean", NsRunLockstepDoorCleanSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstepDoor_DropOpen, "NetworkSync.LockstepDoor.DropOpen", NsRunLockstepDoorDropOpenSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstepDoor_DropFrame, "NetworkSync.LockstepDoor.DropFrame", NsRunLockstepDoorDropFrameSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstepDoor_IgnoreSnap, "NetworkSync.LockstepDoor.IgnoreSnap", NsRunLockstepDoorIgnoreSnapSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstepDoor_NotInStep, "NetworkSync.LockstepDoor.NotInStep", NsRunLockstepDoorNotInStepSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstepDoor_Compose, "NetworkSync.LockstepDoor.Compose", NsRunLockstepDoorComposeSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstepDoor_WaitCompose, "NetworkSync.LockstepDoor.WaitCompose", NsRunLockstepWaitDoorComposeSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstepDoor_TurnCompose, "NetworkSync.LockstepDoor.TurnCompose", NsRunLockstepTurnDoorComposeSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstepDoor_DelayCompose, "NetworkSync.LockstepDoor.DelayCompose", NsRunLockstepDelayDoorComposeSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitClean, "NetworkSync.Lockstep.Wait.Clean", NsRunLockstepWaitCleanSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitStall, "NetworkSync.Lockstep.Wait.Stall", NsRunLockstepWaitStallSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitDrop, "NetworkSync.Lockstep.Wait.Drop", NsRunLockstepWaitDropSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitJoin, "NetworkSync.Lockstep.Wait.Join", NsRunLockstepWaitJoinSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitNack, "NetworkSync.Lockstep.Wait.Nack", NsRunLockstepWaitNackSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitNackJoin, "NetworkSync.Lockstep.Wait.NackJoin", NsRunLockstepWaitNackJoinSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitKick, "NetworkSync.Lockstep.Wait.Kick", NsRunLockstepWaitKickSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitKickResume, "NetworkSync.Lockstep.Wait.KickResume", NsRunLockstepWaitKickResumeSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitResyncAlign, "NetworkSync.Lockstep.Wait.Resync.Align", NsRunLockstepWaitResyncAlignSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitResyncForce, "NetworkSync.Lockstep.Wait.Resync.Force", NsRunLockstepWaitResyncForceSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitResyncIgnore, "NetworkSync.Lockstep.Wait.Resync.IgnoreFrame", NsRunLockstepWaitResyncIgnoreFrameSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitResyncResume, "NetworkSync.Lockstep.Wait.Resync.Resume", NsRunLockstepWaitResyncResumeSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitResyncAgain, "NetworkSync.Lockstep.Wait.Resync.Again", NsRunLockstepWaitResyncAgainSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitResyncWire, "NetworkSync.Lockstep.Wait.Resync.Wire", NsRunLockstepWaitResyncWireSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitResyncUdp, "NetworkSync.Lockstep.Wait.Resync.Udp", NsRunLockstepWaitResyncUdpSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitResyncKickOff, "NetworkSync.Lockstep.Wait.Resync.KickOff", NsRunLockstepWaitResyncKickOffSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_WaitResyncKick, "NetworkSync.Lockstep.Wait.Resync.Kick", NsRunLockstepWaitResyncKickSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnClean, "NetworkSync.Lockstep.Turn.Clean", NsRunLockstepTurnCleanSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnLate, "NetworkSync.Lockstep.Turn.Late", NsRunLockstepTurnLateSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnDrop, "NetworkSync.Lockstep.Turn.Drop", NsRunLockstepTurnDropSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnSpeed, "NetworkSync.Lockstep.Turn.Speed", NsRunLockstepTurnSpeedSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnLenDrop, "NetworkSync.Lockstep.Turn.LenDrop", NsRunLockstepTurnLenDropSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnLongRun, "NetworkSync.Lockstep.Turn.LongRun", NsRunLockstepTurnLongRunSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnRecovery, "NetworkSync.Lockstep.Turn.Recovery", NsRunLockstepTurnRecoverySelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnResyncAlign, "NetworkSync.Lockstep.Turn.Resync.Align", NsRunLockstepTurnResyncAlignSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnResyncForce, "NetworkSync.Lockstep.Turn.Resync.Force", NsRunLockstepTurnResyncForceSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnResyncIgnore, "NetworkSync.Lockstep.Turn.Resync.IgnoreFrame", NsRunLockstepTurnResyncIgnoreFrameSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnResyncResume, "NetworkSync.Lockstep.Turn.Resync.Resume", NsRunLockstepTurnResyncResumeSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnResyncAgain, "NetworkSync.Lockstep.Turn.Resync.Again", NsRunLockstepTurnResyncAgainSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnResyncWire, "NetworkSync.Lockstep.Turn.Resync.Wire", NsRunLockstepTurnResyncWireSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnResyncUdp, "NetworkSync.Lockstep.Turn.Resync.Udp", NsRunLockstepTurnResyncUdpSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnResyncKickOff, "NetworkSync.Lockstep.Turn.Resync.KickOff", NsRunLockstepTurnResyncKickOffSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_TurnResyncKick, "NetworkSync.Lockstep.Turn.Resync.Kick", NsRunLockstepTurnResyncKickSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayClean, "NetworkSync.Lockstep.Delay.Clean", NsRunLockstepDelayCleanSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayRtt, "NetworkSync.Lockstep.Delay.Rtt", NsRunLockstepDelayRttSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayHighRtt, "NetworkSync.Lockstep.Delay.HighRtt", NsRunLockstepDelayHighRttSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayFromRtt, "NetworkSync.Lockstep.Delay.FromRtt", NsRunLockstepDelayFromRttSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayAdapt, "NetworkSync.Lockstep.Delay.Adapt", NsRunLockstepDelayAdaptSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayRecovery, "NetworkSync.Lockstep.Delay.Recovery", NsRunLockstepDelayRecoverySelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayNack, "NetworkSync.Lockstep.Delay.Nack", NsRunLockstepDelayNackSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayResyncAlign, "NetworkSync.Lockstep.Delay.Resync.Align", NsRunLockstepDelayResyncAlignSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayResyncForce, "NetworkSync.Lockstep.Delay.Resync.Force", NsRunLockstepDelayResyncForceSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayResyncIgnore, "NetworkSync.Lockstep.Delay.Resync.IgnoreFrame", NsRunLockstepDelayResyncIgnoreFrameSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayResyncResume, "NetworkSync.Lockstep.Delay.Resync.Resume", NsRunLockstepDelayResyncResumeSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayResyncAgain, "NetworkSync.Lockstep.Delay.Resync.Again", NsRunLockstepDelayResyncAgainSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayResyncWire, "NetworkSync.Lockstep.Delay.Resync.Wire", NsRunLockstepDelayResyncWireSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayResyncUdp, "NetworkSync.Lockstep.Delay.Resync.Udp", NsRunLockstepDelayResyncUdpSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayResyncKickOff, "NetworkSync.Lockstep.Delay.Resync.KickOff", NsRunLockstepDelayResyncKickOffSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_DelayResyncKick, "NetworkSync.Lockstep.Delay.Resync.Kick", NsRunLockstepDelayResyncKickSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_Clean, "NetworkSync.StateSync.Clean", NsRunStateSyncCleanSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_Rewind, "NetworkSync.StateSync.Rewind", NsRunStateSyncRewindSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_Fire, "NetworkSync.StateSync.Fire", NsRunStateSyncFireSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_Nack, "NetworkSync.StateSync.Nack", NsRunStateSyncNackSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_InboxHole, "NetworkSync.StateSync.InboxHole", NsRunStateSyncInboxHoleSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_InboxCap, "NetworkSync.StateSync.InboxCap", NsRunStateSyncInboxCapSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_Unacked, "NetworkSync.StateSync.UnackedWindow", NsRunStateSyncUnackedWindowSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_LongOutage, "NetworkSync.StateSync.LongOutage", NsRunStateSyncLongOutageSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_ClockOffset, "NetworkSync.StateSync.ClockOffset", NsRunStateSyncClockOffsetSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_OldSnap, "NetworkSync.StateSync.OldSnap", NsRunStateSyncOldSnapSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_Spoof, "NetworkSync.StateSync.Spoof", NsRunStateSyncSpoofSelfTest, NsAutoFlags)
NS_WRAP(FNsRollback_Clean, "NetworkSync.Rollback.Clean", NsRunRollbackCleanSelfTest, NsAutoFlags)
NS_WRAP(FNsRollback_Wait, "NetworkSync.Rollback.Wait", NsRunRollbackWaitSelfTest, NsAutoFlags)
NS_WRAP(FNsRollback_Hole, "NetworkSync.Rollback.Hole", NsRunRollbackHoleSelfTest, NsAutoFlags)
NS_WRAP(FNsRollback_MidHole, "NetworkSync.Rollback.MidHole", NsRunRollbackMidHoleSelfTest, NsAutoFlags)
NS_WRAP(FNsRollback_Conflict, "NetworkSync.Rollback.Conflict", NsRunRollbackConflictingInputSelfTest, NsAutoFlags)
NS_WRAP(FNsUdp_Burst, "NetworkSync.Udp.Burst", NsRunUdpBurstSelfTest, NsAutoFlags)
NS_WRAP(FNsUdp_StateSync, "NetworkSync.Udp.StateSync", NsRunUdpStateSyncSelfTest, NsAutoFlags)
NS_WRAP(FNsUdp_Rollback, "NetworkSync.Udp.Rollback", NsRunUdpRollbackSelfTest, NsAutoFlags)
NS_WRAP(FNsUdp_SplitState, "NetworkSync.Udp.SplitState", NsRunUdpSplitStateSyncSelfTest, NsAutoFlags)
NS_WRAP(FNsUdp_SplitRollback, "NetworkSync.Udp.SplitRollback", NsRunUdpSplitRollbackSelfTest, NsAutoFlags)
NS_WRAP(FNsUdp_SessionRestart, "NetworkSync.Udp.SessionRestart", NsRunUdpSessionRestartSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_Bind, "NetworkSync.Stun.Bind", NsRunStunBindSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_Loopback, "NetworkSync.Stun.Loopback", NsRunStunLoopbackSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_Punch, "NetworkSync.Stun.Punch", NsRunStunPunchSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_Rendezvous, "NetworkSync.Stun.Rendezvous", NsRunStunRendezvousSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_Check, "NetworkSync.Stun.Check", NsRunStunCheckSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_Turn, "NetworkSync.Stun.Turn", NsRunStunTurnSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_Permit, "NetworkSync.Stun.Permit", NsRunStunPermitSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_Channel, "NetworkSync.Stun.Channel", NsRunStunChannelSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_ChannelPeers, "NetworkSync.Stun.ChannelPeers", NsRunStunChannelPeersSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_PermitPeers, "NetworkSync.Stun.PermitPeers", NsRunStunPermitPeersSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_RendezvousOrder, "NetworkSync.Stun.RendezvousOrder", NsRunStunRendezvousOrderSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_ChannelMtu, "NetworkSync.Stun.ChannelMtu", NsRunStunChannelMtuSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_Relay, "NetworkSync.Stun.Relay", NsRunStunRelaySelfTest, NsAutoFlags)
NS_WRAP(FNsStun_Ice, "NetworkSync.Stun.Ice", NsRunStunIceSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_IceExchange, "NetworkSync.Stun.IceExchange", NsRunStunIceExchangeSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_IcePairs, "NetworkSync.Stun.IcePairs", NsRunStunIcePairsSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_IceNominate, "NetworkSync.Stun.IceNominate", NsRunStunIceNominateSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_Hub, "NetworkSync.Stun.Hub", NsRunStunHubSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_HubProcess, "NetworkSync.Stun.HubProcess", NsRunStunHubProcessSelfTest, NsAutoFlags)
NS_WRAP(FNsStun_IceSdp, "NetworkSync.Stun.IceSdp", NsRunStunIceSdpSelfTest, NsAutoFlags)

NS_WRAP(FNsWorld_Stress, "NetworkSync.Stress.World", NsRunWorldStressSelfTest, NsAutoFlags)
NS_WRAP(FNsCodec_Stress, "NetworkSync.Stress.Codec", NsRunCodecStressSelfTest, NsAutoFlags)
NS_WRAP(FNsFakeNet_Stress, "NetworkSync.Stress.FakeNet", NsRunFakeNetStressSelfTest, NsAutoFlags)
NS_WRAP(FNsLockstep_Stress, "NetworkSync.Stress.Lockstep", NsRunLockstepStressSelfTest, NsAutoFlags)
NS_WRAP(FNsStateSync_Stress, "NetworkSync.Stress.StateSync", NsRunStateSyncStressSelfTest, NsAutoFlags)
NS_WRAP(FNsRollback_Stress, "NetworkSync.Stress.Rollback", NsRunRollbackStressSelfTest, NsAutoFlags)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNsActors_Cdo, "NetworkSync.Actors.Cdo", NsAutoFlags)

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

	ANsNetManager* Manager = NewObject<ANsNetManager>(Pkg);
	TestNotNull(TEXT("manager"), Manager);
	TestEqual(TEXT("invalid player id"), Manager->GetPawnLocation(-1), Manager->GetActorLocation());

	ANsInputProxy* Proxy = NewObject<ANsInputProxy>(Pkg);
	TestNotNull(TEXT("proxy"), Proxy);
	TestTrue(TEXT("proxy replicates"), Proxy->GetIsReplicated());
	TestTrue(TEXT("proxy owner only"), Proxy->bOnlyRelevantToOwner);
	Proxy->ServerBumpCounter_Implementation();
	Proxy->ServerToggleDoor_Implementation();
	TestEqual(TEXT("proxy no world leaves counter"), Counter->Counter, 2);
	TestFalse(TEXT("proxy no world leaves door"), Door->bOpen);
	return true;
}

#undef NS_WRAP

#endif
