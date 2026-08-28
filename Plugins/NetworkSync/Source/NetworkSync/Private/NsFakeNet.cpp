// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsFakeNet.h"

void FNsFakeNet::Send(ENsAddr Src, ENsAddr Dst, const FNsPacket& Packet)
{
	if (Rng.FRand() < Drop)
	{
		return;
	}
	FNsPacket Copy = Packet;
	Copy.Src = Src;
	Copy.Dst = Dst;
	const int32 Si = static_cast<int32>(Src);
	Copy.Seq = NextSeq[Si]++;
	Copy.Ack = RecvMax[Si];
	Copy.AckBits = RecvBits[Si];
	Copy.DeliverAt = Now + RttMs * 0.5 + Rng.FRandRange(0.f, JitterMs);
	Queue.Add(MoveTemp(Copy));
}

bool FNsFakeNet::AcceptSeq(ENsAddr Dst, int32 S)
{
	const int32 Di = static_cast<int32>(Dst);
	int32& Max = RecvMax[Di];
	uint32& Bits = RecvBits[Di];
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

void FNsFakeNet::Drain(ENsAddr Dst, TArray<FNsPacket>& Out)
{
	Out.Reset();
	for (int32 i = Queue.Num() - 1; i >= 0; --i)
	{
		if (Queue[i].Dst == Dst && Queue[i].DeliverAt <= Now)
		{
			if (AcceptSeq(Dst, Queue[i].Seq))
			{
				Out.Add(Queue[i]);
			}
			Queue.RemoveAtSwap(i);
		}
	}
}

void FNsFakeNet::Advance(double Ms)
{
	Now += Ms;
}
