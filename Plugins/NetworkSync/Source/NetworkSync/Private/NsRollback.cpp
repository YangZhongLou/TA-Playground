// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsRollback.h"

FNsInputs FNsRollbackPeer::Pair(int32 F) const
{
	FNsInputs Out;
	const int8 Loc = Local.Contains(F) ? Local[F] : 0;
	const int8 Rem = RemoteOrPred(F);
	if (PlayerId == 0)
	{
		Out.Dx[0] = Loc;
		Out.Dx[1] = Rem;
	}
	else
	{
		Out.Dx[0] = Rem;
		Out.Dx[1] = Loc;
	}
	return Out;
}

int8 FNsRollbackPeer::RemoteOrPred(int32 F) const
{
	if (const int8* Found = RealRemote.Find(F))
	{
		return *Found;
	}
	if (const FNsInputs* Prev = Pred.Find(F - 1))
	{
		return (PlayerId == 0) ? Prev->Dx[1] : Prev->Dx[0];
	}
	return 0;
}

void FNsRollbackPeer::CollectPacked(int32 EndF, TMap<int32, int8>& Out) const
{
	Out.Reset();
	const int32 First = FMath::Max(0, EndF - 3);
	for (int32 i = First; i <= EndF; ++i)
	{
		if (const int8* Found = Local.Find(i))
		{
			Out.Add(i, *Found);
		}
	}
}

void FNsRollbackPeer::RaiseConfirmed()
{
	int32 Start = Ns::InputDelay - 1;
	int32 MinKept = MAX_int32;
	for (const TPair<int32, int8>& Kv : RealRemote)
	{
		MinKept = FMath::Min(MinKept, Kv.Key);
	}
	if (MinKept != MAX_int32)
	{
		Start = FMath::Max(Start, MinKept - 1);
	}
	int32 C = Start;
	for (int32 F = Start + 1; F < Frame; ++F)
	{
		if (!RealRemote.Contains(F))
		{
			break;
		}
		C = F;
	}
	Confirmed = C;
}

void FNsRollbackPeer::AdvanceLocal(int8 Dx, TMap<int32, int8>& OutPacked)
{
	RaiseConfirmed();
	if (Frame - Confirmed > Ns::MaxRollback)
	{
		bWaiting = true;
		++WaitCount;
		CollectPacked(FMath::Max(0, Frame + Ns::InputDelay - 1), OutPacked);
		return;
	}
	bWaiting = false;
	const int32 SendF = Frame + Ns::InputDelay;
	Local.Add(SendF, NsClampDx(Dx));
	CollectPacked(SendF, OutPacked);
	const FNsInputs In = Pair(Frame);
	Saves.Add(Frame, World);
	Pred.Add(Frame, In);
	World.Step(In.Dx, Ns::RollbackSpeed);
	++Frame;
	Trim();
}

void FNsRollbackPeer::Advance(INsNet& Net, int8 Dx)
{
	TMap<int32, int8> Packed;
	AdvanceLocal(Dx, Packed);
	if (Packed.Num() == 0)
	{
		return;
	}
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::P2PInput;
	Pkt.RemoteDx = Packed;
	Net.Send(Addr, Other, Pkt);
}

void FNsRollbackPeer::OnRemote(const TMap<int32, int8>& Packed)
{
	for (const TPair<int32, int8>& PairIt : Packed)
	{
		const int32 F = PairIt.Key;
		const int8 Dx = NsClampDx(PairIt.Value);
		RealRemote.Add(F, Dx);
		if (const FNsInputs* Guess = Pred.Find(F))
		{
			const int8 Guessed = (PlayerId == 0) ? Guess->Dx[1] : Guess->Dx[0];
			if (Guessed != Dx && F < Frame)
			{
				RollbackFrom(F);
			}
		}
	}
	RaiseConfirmed();
}

void FNsRollbackPeer::RollbackFrom(int32 F)
{
	if (Frame - F > Ns::MaxRollback)
	{
		return;
	}
	if (!Saves.Contains(F))
	{
		return;
	}
	bInRollback = true;
	World = Saves[F];
	for (int32 K = F; K < Frame; ++K)
	{
		int8 D0;
		int8 D1;
		if (PlayerId == 0)
		{
			D0 = Local.Contains(K) ? Local[K] : 0;
			D1 = RealRemote.Contains(K) ? RealRemote[K] : (Pred.Contains(K) ? Pred[K].Dx[1] : 0);
		}
		else
		{
			D0 = RealRemote.Contains(K) ? RealRemote[K] : (Pred.Contains(K) ? Pred[K].Dx[0] : 0);
			D1 = Local.Contains(K) ? Local[K] : 0;
		}
		FNsInputs In;
		In.Dx[0] = D0;
		In.Dx[1] = D1;
		Saves.Add(K, World);
		Pred.Add(K, In);
		World.Step(In.Dx, Ns::RollbackSpeed);
	}
	bInRollback = false;
}

void FNsRollbackPeer::Trim()
{
	const int32 Keep = FMath::Min(Confirmed, Frame - Ns::MaxRollback - Ns::InputDelay - 2);
	auto TrimMap = [Keep](auto& Map)
	{
		TArray<int32> Dead;
		for (const auto& Kv : Map)
		{
			if (Kv.Key < Keep)
			{
				Dead.Add(Kv.Key);
			}
		}
		for (int32 K : Dead)
		{
			Map.Remove(K);
		}
	};
	TrimMap(Saves);
	TrimMap(Pred);
	TrimMap(Local);
	TrimMap(RealRemote);
}
