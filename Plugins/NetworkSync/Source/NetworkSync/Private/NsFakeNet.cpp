// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsFakeNet.h"
#include "NsCodec.h"

void FNsFakeNet::Send(ENsAddr Src, ENsAddr Dst, const FNsPacket& Packet)
{
	TArray<FNsPacket> Parts;
	NsSplitForMtu(Packet, Parts);
	for (const FNsPacket& Part : Parts)
	{
		FNsPacket Copy = Part;
		Copy.Src = Src;
		Copy.Dst = Dst;
		Seq.Stamp(Src, Copy);
		Copy.DeliverAt = Now + RttMs * 0.5 + Rng.FRandRange(0.f, JitterMs);
		if (Rng.FRand() < Drop)
		{
			continue;
		}

		TArray<uint8> Bytes;
		if (!NsEncodePacket(Copy, Bytes))
		{
			continue;
		}
		FNsPacket Wired;
		if (!NsDecodePacket(Bytes, Wired))
		{
			continue;
		}
		Wired.Src = Src;
		Wired.Dst = Dst;
		Wired.DeliverAt = Copy.DeliverAt;
		Queue.Add(MoveTemp(Wired));
	}
}

void FNsFakeNet::Drain(ENsAddr Dst, TArray<FNsPacket>& Out)
{
	Out.Reset();
	for (int32 i = Queue.Num() - 1; i >= 0; --i)
	{
		if (Queue[i].Dst == Dst && Queue[i].DeliverAt <= Now)
		{
			if (Seq.Accept(Dst, Queue[i].Src, Queue[i].Seq))
			{
				Out.Add(Queue[i]);
			}
			Queue.RemoveAtSwap(i);
		}
	}
}
