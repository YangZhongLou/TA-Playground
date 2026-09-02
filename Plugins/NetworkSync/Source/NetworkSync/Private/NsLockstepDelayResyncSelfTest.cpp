// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsSelfTest.h"
#include "NsFakeNet.h"
#include "NsUdpNet.h"
#include "NsLockstepDoor.h"
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

FNsSelfTestResult NsRunLockstepDelayResyncUdpSelfTest()
{
	FNsUdpNet Host;
	FNsUdpNet Client;
	if (!Host.Bind(ENsAddr::Sv, 0, false) || !Host.Bind(ENsAddr::C0, 0, false)
		|| !Client.Bind(ENsAddr::C1, 0, false))
	{
		return DrFail(TEXT("lockstep-delay-resync-udp: bind failed"));
	}
	if (!Host.SetPeer(ENsAddr::C1, TEXT("127.0.0.1"), Client.BoundPort(ENsAddr::C1))
		|| !Client.SetPeer(ENsAddr::Sv, TEXT("127.0.0.1"), Host.BoundPort(ENsAddr::Sv))
		|| !Client.SetPeer(ENsAddr::C0, TEXT("127.0.0.1"), Host.BoundPort(ENsAddr::C0)))
	{
		return DrFail(TEXT("lockstep-delay-resync-udp: set peer failed"));
	}
	FNsLockstepDelayServer Sv;
	FNsLockstepDelayClient C0;
	FNsLockstepDelayClient C1;
	DrInit(C0, C1);
	FNsLockstepResync Repair;
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	const int8 Script[][2] = {{1, 0}, {1, -1}, {0, -1}, {-1, 1}};
	for (int32 S = 0; S < 24; ++S)
	{
		const int8* Pair = Script[S % 4];
		C0.SendInput(Host, Pair[0]);
		C1.SendInput(Client, Pair[1]);
		NsPumpLockstepDelayResyncServer(Host, Sv, Repair, true);
		NsPumpLockstepDelayResyncClient(Host, C0, V0, true);
		NsPumpLockstepDelayResyncClient(Client, C1, V1, true);
		Host.Advance(Ns::LogicDtMs);
		Client.Advance(Ns::LogicDtMs);
	}
	if (!DrForceDesync(Sv))
	{
		return DrFail(TEXT("lockstep-delay-resync-udp: no checksum record"));
	}
	const int32 HaltAt = Sv.Frame;
	for (int32 i = 0; i < 16; ++i)
	{
		NsPumpLockstepDelayResyncServer(Host, Sv, Repair, true);
		NsPumpLockstepDelayResyncClient(Host, C0, V0, true);
		NsPumpLockstepDelayResyncClient(Client, C1, V1, true);
		Host.Advance(1.0);
		Client.Advance(1.0);
		if (V0.HaltTick == HaltAt && V1.HaltTick == HaltAt
			&& C0.World.Equals(Sv.World) && C1.World.Equals(Sv.World)
			&& C0.ExecFrame == HaltAt && C1.ExecFrame == HaltAt)
		{
			break;
		}
	}
	if (V1.HaltTick != HaltAt || !C1.World.Equals(Sv.World) || C1.ExecFrame != HaltAt)
	{
		return DrFailStr(FString::Printf(
			TEXT("lockstep-delay-resync-udp: C1 not halted from packet halt=%d snap=%d x=%d/%d"),
			V1.HaltTick, HaltAt, C1.World.X[0], Sv.World.X[0]));
	}
	for (int32 i = 0; i < 16; ++i)
	{
		C0.SendInput(Host, 1);
		C1.SendInput(Client, -1);
		NsPumpLockstepDelayResyncClient(Host, C0, V0, true);
		NsPumpLockstepDelayResyncClient(Client, C1, V1, true);
		Host.Advance(Ns::LogicDtMs);
		Client.Advance(Ns::LogicDtMs);
		NsPumpLockstepDelayResyncServer(Host, Sv, Repair, true);
		NsPumpLockstepDelayResyncClient(Host, C0, V0, true);
		NsPumpLockstepDelayResyncClient(Client, C1, V1, true);
		if (Repair.bResumed && V0.HaltTick < 0 && V1.HaltTick < 0
			&& C0.ExecFrame == Sv.Frame && C1.ExecFrame == Sv.Frame
			&& C0.World.Equals(C1.World) && C0.World.Equals(Sv.World)
			&& Sv.Frame > HaltAt)
		{
			return DrOk(FString::Printf(TEXT("lockstep-delay-resync-udp snap=%d frame=%d"), HaltAt, Sv.Frame));
		}
	}
	return DrFailStr(FString::Printf(
		TEXT("lockstep-delay-resync-udp: no resume resumed=%d v0=%d v1=%d sv=%d c0=%d c1=%d"),
		Repair.bResumed ? 1 : 0, V0.HaltTick, V1.HaltTick, Sv.Frame, C0.ExecFrame, C1.ExecFrame));
}

static void DrSendChecksum(INsNet& Net, ENsAddr Src, int32 Tick, uint32 Hash)
{
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SChecksum;
	Pkt.PlayerId = NsPlayerIdFromAddr(Src);
	Pkt.Tick = Tick;
	Pkt.Hash = Hash;
	Net.Send(Src, ENsAddr::Sv, Pkt);
}

FNsSelfTestResult NsRunLockstepDelayResyncKickOffSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepDelayServer Sv;
	FNsLockstepDelayClient C0;
	FNsLockstepDelayClient C1;
	DrInit(C0, C1);
	DrWarm(Net, Sv, C0, C1, 24);
	const uint32* Found = Sv.Checksums.Find(Ns::ChecksumEvery);
	if (!Found)
	{
		return DrFail(TEXT("lockstep-delay-resync-kick-off: no checksum record"));
	}
	const int32 FrameAt = Sv.Frame;
	FNsLockstepResync Repair;
	DrSendChecksum(Net, ENsAddr::C1, Ns::ChecksumEvery, *Found ^ 1u);
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	if (!Sv.bDesync || Sv.Frame != FrameAt || !Repair.Alive[1])
	{
		return DrFailStr(FString::Printf(
			TEXT("lockstep-delay-resync-kick-off: desync=%d frame=%d was=%d alive1=%d"),
			Sv.bDesync ? 1 : 0, Sv.Frame, FrameAt, Repair.Alive[1] ? 1 : 0));
	}
	return DrOk(TEXT("lockstep-delay-resync-kick-off"));
}

FNsSelfTestResult NsRunLockstepDelayResyncKickSelfTest()
{
	FNsFakeNet Net;
	Net.Drop = 0.f;
	Net.RttMs = 0.f;
	Net.JitterMs = 0.f;
	FNsLockstepDelayServer Sv;
	FNsLockstepDelayClient C0;
	FNsLockstepDelayClient C1;
	DrInit(C0, C1);
	DrWarm(Net, Sv, C0, C1, 24);
	const uint32* Found = Sv.Checksums.Find(Ns::ChecksumEvery);
	if (!Found)
	{
		return DrFail(TEXT("lockstep-delay-resync-kick: no checksum record"));
	}
	FNsLockstepResync Repair;
	Repair.bKickDesyncer = true;
	DrSendChecksum(Net, ENsAddr::C1, Ns::ChecksumEvery, *Found ^ 1u);
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	if (Sv.bDesync || Repair.Alive[1] || !Repair.Alive[0])
	{
		return DrFailStr(FString::Printf(
			TEXT("lockstep-delay-resync-kick: desync=%d alive0=%d alive1=%d"),
			Sv.bDesync ? 1 : 0, Repair.Alive[0] ? 1 : 0, Repair.Alive[1] ? 1 : 0));
	}

	const int32 X0 = Sv.World.X[0];
	const int32 X1 = Sv.World.X[1];
	const int32 FrameAt = Sv.Frame;
	FNsLockstepResyncClient V0;
	FNsLockstepResyncClient V1;
	for (int32 S = 0; S < 8; ++S)
	{
		C0.SendInput(Net, 1);
		C1.SendInput(Net, 1);
		NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
		NsPumpLockstepDelayResyncClient(Net, C0, V0);
		NsPumpLockstepDelayResyncClient(Net, C1, V1);
		Net.Advance(Ns::LogicDtMs);
	}
	if (Sv.Frame <= FrameAt || Sv.World.X[0] == X0 || Sv.World.X[1] != X1)
	{
		return DrFailStr(FString::Printf(
			TEXT("lockstep-delay-resync-kick: frame=%d was=%d x0=%d/%d x1=%d/%d"),
			Sv.Frame, FrameAt, Sv.World.X[0], X0, Sv.World.X[1], X1));
	}
	if (!C0.World.Equals(C1.World) || !C0.World.Equals(Sv.World))
	{
		return DrFail(TEXT("lockstep-delay-resync-kick: worlds"));
	}
	return DrOk(FString::Printf(TEXT("lockstep-delay-resync-kick frame=%d"), Sv.Frame));
}

FNsSelfTestResult NsRunLockstepDelayDoorComposeSelfTest()
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
	const int32 X0 = C0.World.X[0];
	FNsDoorOpen DoorC0;
	FNsDoorOpen DoorC1;
	NsBroadcastDoorOpen(Net, 1);
	NsPumpLockstepDelayResyncClient(Net, C0, V0, false, &DoorC0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1, false, &DoorC1);
	if (DoorC0.Open != 1 || DoorC1.Open != 1)
	{
		return DrFail(TEXT("lockstep-delay-door-compose: open not applied"));
	}
	if (C0.World.X[0] != X0)
	{
		return DrFail(TEXT("lockstep-delay-door-compose: open wrote X"));
	}

	if (!DrForceDesync(Sv))
	{
		return DrFail(TEXT("lockstep-delay-door-compose: no checksum record"));
	}
	FNsLockstepResync Repair;
	NsPumpLockstepDelayResyncServer(Net, Sv, Repair);
	NsBroadcastDoorOpen(Net, 0);
	NsPumpLockstepDelayResyncClient(Net, C0, V0, false, &DoorC0);
	NsPumpLockstepDelayResyncClient(Net, C1, V1, false, &DoorC1);
	if (!DrAligned(Sv, Repair, C0, C1))
	{
		return DrFail(TEXT("lockstep-delay-door-compose: halt align"));
	}
	if (DoorC0.Open != 0 || DoorC1.Open != 0)
	{
		return DrFail(TEXT("lockstep-delay-door-compose: halt ignored door"));
	}
	return DrOk(TEXT("lockstep-delay-door-compose"));
}
