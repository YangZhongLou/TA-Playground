// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsNet.h"
#include "Misc/Guid.h"

namespace
{
uint32 NsNewSession()
{
	const FGuid Guid = FGuid::NewGuid();
	const uint32 Session = Guid.A ^ Guid.B ^ Guid.C ^ Guid.D;
	return Session != 0 ? Session : 1u;
}
}

FNsSeqWindow::FNsSeqWindow()
{
	for (uint32& Session : SendSession)
	{
		Session = NsNewSession();
	}
}

void FNsSeqWindow::Stamp(ENsAddr Src, FNsPacket& Packet)
{
	const int32 Si = static_cast<int32>(Src);
	const int32 Di = static_cast<int32>(Packet.Dst);
	if (Si < 0 || Si >= 3 || Di < 0 || Di >= 3)
	{
		return;
	}
	if (NextSeq[Si] <= 0 || NextSeq[Si] == MAX_int32)
	{
		uint32 NewSession = NsNewSession();
		while (NewSession == SendSession[Si])
		{
			NewSession = NsNewSession();
		}
		SendSession[Si] = NewSession;
		NextSeq[Si] = 1;
	}
	Packet.Session = SendSession[Si];
	Packet.Seq = NextSeq[Si]++;
	Packet.Ack = RecvMax[Si][Di];
	Packet.AckBits = RecvBits[Si][Di];
}

bool FNsSeqWindow::Accept(ENsAddr Dst, ENsAddr Src, uint32 Session, int32 S)
{
	const int32 Di = static_cast<int32>(Dst);
	const int32 Si = static_cast<int32>(Src);
	if (Di < 0 || Di >= 3 || Si < 0 || Si >= 3 || Session == 0 || S <= 0)
	{
		return false;
	}
	uint32& CurrentSession = RecvSession[Di][Si];
	if (Session != CurrentSession)
	{
		TArray<uint32>& Retired = RetiredSessions[Di][Si];
		if (Retired.Contains(Session))
		{
			return false;
		}
		if (CurrentSession != 0)
		{
			Retired.Add(CurrentSession);
			if (Retired.Num() > 8)
			{
				Retired.RemoveAt(0);
			}
		}
		CurrentSession = Session;
		RecvMax[Di][Si] = 0;
		RecvBits[Di][Si] = 0;
	}
	int32& Max = RecvMax[Di][Si];
	uint32& Bits = RecvBits[Di][Si];
	if (S > Max)
	{
		const int32 Shift = S - Max;
		Bits = (Shift < 32) ? (Bits << Shift) : 0u;
		if (Shift > 0 && Shift < 32)
		{
			Bits |= 1u << (Shift - 1);
		}
		Max = S;
		return true;
	}
	if (S == Max)
	{
		return false;
	}
	const int32 Dist = Max - S;
	if (Dist >= 32)
	{
		return false;
	}
	const uint32 Bit = 1u << (Dist - 1);
	if (Bits & Bit)
	{
		return false;
	}
	Bits |= Bit;
	return true;
}

bool FNsSeqWindow::Accept(ENsAddr Dst, ENsAddr Src, int32 S)
{
	return Accept(Dst, Src, 1u, S);
}
