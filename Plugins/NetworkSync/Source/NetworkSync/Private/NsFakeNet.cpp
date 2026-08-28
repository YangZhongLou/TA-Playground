// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsFakeNet.h"
#include "NsCodec.h"

void FNsSeqWindow::Stamp(ENsAddr Src, FNsPacket& Packet)
{
	const int32 Si = static_cast<int32>(Src);
	Packet.Seq = NextSeq[Si]++;
	Packet.Ack = RecvMax[Si];
	Packet.AckBits = RecvBits[Si];
}

bool FNsSeqWindow::Accept(ENsAddr Dst, int32 S)
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

void FNsFakeNet::Send(ENsAddr Src, ENsAddr Dst, const FNsPacket& Packet)
{
	if (Rng.FRand() < Drop)
	{
		return;
	}
	FNsPacket Copy = Packet;
	Copy.Src = Src;
	Copy.Dst = Dst;
	Seq.Stamp(Src, Copy);
	Copy.DeliverAt = Now + RttMs * 0.5 + Rng.FRandRange(0.f, JitterMs);

	TArray<uint8> Bytes;
	if (!NsEncodePacket(Copy, Bytes))
	{
		return;
	}
	FNsPacket Wired;
	if (!NsDecodePacket(Bytes, Wired))
	{
		return;
	}
	Wired.Src = Src;
	Wired.Dst = Dst;
	Wired.DeliverAt = Copy.DeliverAt;
	Queue.Add(MoveTemp(Wired));
}

void FNsFakeNet::Drain(ENsAddr Dst, TArray<FNsPacket>& Out)
{
	Out.Reset();
	for (int32 i = Queue.Num() - 1; i >= 0; --i)
	{
		if (Queue[i].Dst == Dst && Queue[i].DeliverAt <= Now)
		{
			if (Seq.Accept(Dst, Queue[i].Seq))
			{
				Out.Add(Queue[i]);
			}
			Queue.RemoveAtSwap(i);
		}
	}
}
