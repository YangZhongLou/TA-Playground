// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Ns
{
	constexpr int32 PlayerCount = 2;
	constexpr int32 LogicDtMs = 66;
	constexpr int32 LockstepSpeed = 8;
	constexpr int32 RedundantFrames = 3;
	constexpr int32 LockstepNackMax = 8;
	constexpr int32 ChecksumEvery = 15;
	constexpr int32 SimDtMs = 16;
	constexpr int32 SendEvery = 3;
	constexpr int32 StateSpeed = 4;
	constexpr int32 RollbackDtMs = 16;
	constexpr int32 MaxRollback = 8;
	constexpr int32 RollbackSpeed = 3;
	constexpr int32 InputWindow = 8;
	constexpr int32 InterpDelayMs = 100;
	constexpr int32 HistoryTicks = 64;
	constexpr int32 MaxInboxAhead = HistoryTicks;
	constexpr int32 InputDelay = 1;
	constexpr int32 LagCompCapMs = 220;
	constexpr int32 JoinSnapEvery = 75;
	constexpr int32 ResyncGiveUpPumps = 32;
	constexpr uint32 PacketMagic = 0x54414E53;
	constexpr int32 HeaderBytes = 24;
	constexpr int32 Ipv4UdpOverheadBytes = 28;
	constexpr int32 Ipv6UdpOverheadBytes = 48;
	constexpr int32 MinPathMtuBytes = 1280;
	constexpr int32 EthernetMtuBytes = 1500;
	constexpr int32 MaxPacketBytes = 1200;
	constexpr int32 MaxPayloadBytes = MaxPacketBytes - HeaderBytes;
	constexpr int32 MaxS2CFrameEntries = (MaxPayloadBytes - 5) / 6;
	constexpr int32 MaxS2CTurnFrameEntries = (MaxPayloadBytes - 5) / 7;
	constexpr int32 MaxJoinSnapEntries = (MaxPayloadBytes - 17) / 6;
	constexpr int32 MaxP2PInputEntries = (MaxPayloadBytes - 1) / 5;
	constexpr int32 MaxC2SInputEntries = (MaxPayloadBytes - 3) / 5;
}

static_assert(Ns::MaxPacketBytes + Ns::Ipv6UdpOverheadBytes <= Ns::MinPathMtuBytes,
	"UDP payload must fit in the IPv6 minimum MTU without IP fragmentation");
static_assert(Ns::MaxPacketBytes + Ns::Ipv4UdpOverheadBytes <= Ns::EthernetMtuBytes,
	"UDP payload must fit in Ethernet MTU without IP fragmentation");
static_assert(Ns::MaxS2CFrameEntries >= Ns::RedundantFrames + 1,
	"one lockstep datagram must hold the working redundant window");
static_assert(Ns::MaxS2CTurnFrameEntries >= 17,
	"one turn datagram must hold the closed-turn window");

inline int8 NsClampDx(int32 Dx)
{
	if (Dx < -1)
	{
		return -1;
	}
	if (Dx > 1)
	{
		return 1;
	}
	return static_cast<int8>(Dx);
}

struct NETWORKSYNC_API FNsWorld
{
	int32 X[Ns::PlayerCount] = {0, 0};
	uint32 Rng = 1;

	void Reset();
	void Step(const int8 Dxs[Ns::PlayerCount], int32 Speed);
	uint32 Checksum() const;
	bool Equals(const FNsWorld& Other) const;
};

struct NETWORKSYNC_API FNsInputs
{
	int8 Dx[Ns::PlayerCount] = {0, 0};
};
