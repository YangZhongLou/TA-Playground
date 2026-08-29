// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsNet.h"

void FNsSeqWindow::Stamp(ENsAddr Src, FNsPacket& Packet)
{
	const int32 Si = static_cast<int32>(Src);
	const int32 Di = static_cast<int32>(Packet.Dst);
	Packet.Seq = NextSeq[Si]++;
	Packet.Ack = RecvMax[Si][Di];
	Packet.AckBits = RecvBits[Si][Di];
}

bool FNsSeqWindow::Accept(ENsAddr Dst, ENsAddr Src, int32 S)
{
	const int32 Di = static_cast<int32>(Dst);
	const int32 Si = static_cast<int32>(Src);
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
