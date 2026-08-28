// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsStateSync.h"

void FNsStateSyncServer::OnInput(int32 PlayerId, int32 Seq, int8 Dx)
{
	if (PlayerId < 0 || PlayerId >= Ns::PlayerCount)
	{
		return;
	}
	if (Seq <= Pawns[PlayerId].LastSeq)
	{
		return;
	}
	Inbox[PlayerId].Add(Seq, NsClampDx(Dx));
}

void FNsStateSyncServer::OnAck(int32 PlayerId, int32 AckTick)
{
	if (PlayerId < 0 || PlayerId >= Ns::PlayerCount)
	{
		return;
	}
	if (AckTick <= 0)
	{
		LastAck[PlayerId] = 0;
		return;
	}
	if (AckTick > LastAck[PlayerId])
	{
		LastAck[PlayerId] = AckTick;
	}
}

int32 FNsStateSyncServer::RewindX(int32 PlayerId, int32 PingMs) const
{
	if (PlayerId < 0 || PlayerId >= Ns::PlayerCount)
	{
		return 0;
	}
	int32 BackMs = PingMs / 2 + Ns::InterpDelayMs;
	if (BackMs > Ns::LagCompCapMs)
	{
		return Pawns[PlayerId].X;
	}
	const int32 BackTicks = BackMs / Ns::SimDtMs;
	const int32 T = FMath::Max(0, Tick - BackTicks);
	return HistX[PlayerId][T % Ns::HistoryTicks];
}

void FNsStateSyncServer::SendSnap(INsNet& Net, ENsAddr Dst, int32 PlayerId)
{
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::S2CSnapshot;
	Pkt.Tick = Tick;
	const int32 Ack = LastAck[PlayerId];
	const int32* Base0 = SnapX0.Find(Ack);
	const int32* Base1 = SnapX1.Find(Ack);
	if (Ack > 0 && Base0 && Base1)
	{
		Pkt.BaseTick = Ack;
		Pkt.SnapX[0] = Pawns[0].X - *Base0;
		Pkt.SnapX[1] = Pawns[1].X - *Base1;
		++DeltaSent;
	}
	else
	{
		Pkt.BaseTick = 0;
		Pkt.SnapX[0] = Pawns[0].X;
		Pkt.SnapX[1] = Pawns[1].X;
	}
	for (int32 i = 0; i < Ns::PlayerCount; ++i)
	{
		Pkt.SnapSeq[i] = Pawns[i].LastSeq;
	}
	Net.Send(ENsAddr::Sv, Dst, Pkt);
}

void FNsStateSyncServer::Sim(INsNet& Net)
{
	++Tick;
	for (int32 i = 0; i < Ns::PlayerCount; ++i)
	{
		for (;;)
		{
			const int32 Next = Pawns[i].LastSeq + 1;
			const int8* Found = Inbox[i].Find(Next);
			if (!Found)
			{
				break;
			}
			Pawns[i].X += static_cast<int32>(*Found) * Ns::StateSpeed;
			Pawns[i].LastSeq = Next;
			Inbox[i].Remove(Next);
		}
		HistX[i][Tick % Ns::HistoryTicks] = Pawns[i].X;
	}
	if (Tick % Ns::SendEvery == 0)
	{
		SnapX0.Add(Tick, Pawns[0].X);
		SnapX1.Add(Tick, Pawns[1].X);
		SendSnap(Net, ENsAddr::C0, 0);
		SendSnap(Net, ENsAddr::C1, 1);
		const int32 KeepAfter = Tick - Ns::HistoryTicks;
		TArray<int32> Dead;
		for (const TPair<int32, int32>& Kv : SnapX0)
		{
			if (Kv.Key < KeepAfter)
			{
				Dead.Add(Kv.Key);
			}
		}
		for (int32 K : Dead)
		{
			SnapX0.Remove(K);
			SnapX1.Remove(K);
		}
	}
}

void FNsStateSyncClient::LocalTick(INsNet& Net, int8 Dx)
{
	++Seq;
	const int8 D = NsClampDx(Dx);
	UnackedSeq.Add(Seq);
	UnackedDx.Add(D);
	PredX += static_cast<int32>(D) * Ns::StateSpeed;
	const int32 Start = FMath::Max(0, UnackedSeq.Num() - Ns::InputWindow);
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::C2SInput;
	Pkt.PlayerId = PlayerId;
	for (int32 i = Start; i < UnackedSeq.Num(); ++i)
	{
		Pkt.SeqWindow.Add(UnackedSeq[i]);
		Pkt.DxWindow.Add(UnackedDx[i]);
	}
	Net.Send(Addr, ENsAddr::Sv, Pkt);
}

void FNsStateSyncClient::OnSnap(INsNet& Net, const FNsPacket& P)
{
	if (LastAckedTick > 0 && P.Tick <= LastAckedTick)
	{
		return;
	}
	int32 Xs[Ns::PlayerCount];
	if (P.BaseTick == 0)
	{
		Xs[0] = P.SnapX[0];
		Xs[1] = P.SnapX[1];
	}
	else
	{
		const int32* B0 = Store0.Find(P.BaseTick);
		const int32* B1 = Store1.Find(P.BaseTick);
		if (!B0 || !B1)
		{
			FNsPacket Nack;
			Nack.Type = ENsMsg::C2SSnapAck;
			Nack.PlayerId = PlayerId;
			Nack.Tick = 0;
			Net.Send(Addr, ENsAddr::Sv, Nack);
			Net.Send(Addr, ENsAddr::Sv, Nack);
			return;
		}
		Xs[0] = *B0 + P.SnapX[0];
		Xs[1] = *B1 + P.SnapX[1];
		bGotDelta = true;
	}
	Store0.Add(P.Tick, Xs[0]);
	Store1.Add(P.Tick, Xs[1]);
	auto TrimStore = [](TMap<int32, int32>& Store)
	{
		while (Store.Num() > Ns::HistoryTicks)
		{
			int32 MinK = MAX_int32;
			for (const TPair<int32, int32>& Kv : Store)
			{
				MinK = FMath::Min(MinK, Kv.Key);
			}
			if (MinK == MAX_int32)
			{
				break;
			}
			Store.Remove(MinK);
		}
	};
	TrimStore(Store0);
	TrimStore(Store1);
	SnapTick.Add(P.Tick);
	SnapX0.Add(Xs[0]);
	SnapX1.Add(Xs[1]);
	while (SnapTick.Num() > 32)
	{
		SnapTick.RemoveAt(0);
		SnapX0.RemoveAt(0);
		SnapX1.RemoveAt(0);
	}
	const int32 LastSeq = P.SnapSeq[PlayerId];
	int32 Keep = 0;
	for (int32 i = 0; i < UnackedSeq.Num(); ++i)
	{
		if (UnackedSeq[i] > LastSeq)
		{
			UnackedSeq[Keep] = UnackedSeq[i];
			UnackedDx[Keep] = UnackedDx[i];
			++Keep;
		}
	}
	UnackedSeq.SetNum(Keep);
	UnackedDx.SetNum(Keep);
	PredX = Xs[PlayerId];
	for (int8 D : UnackedDx)
	{
		PredX += static_cast<int32>(D) * Ns::StateSpeed;
	}
	bHasRemote = true;
	LastAckedTick = P.Tick;
	FNsPacket Ack;
	Ack.Type = ENsMsg::C2SSnapAck;
	Ack.PlayerId = PlayerId;
	Ack.Tick = P.Tick;
	Net.Send(Addr, ENsAddr::Sv, Ack);
	Net.Send(Addr, ENsAddr::Sv, Ack);
}

void FNsStateSyncClient::UpdateRemoteDraw(double NowMs)
{
	const int32 Other = 1 - PlayerId;
	if (SnapTick.Num() == 0)
	{
		return;
	}
	auto XAt = [this, Other](int32 i) { return (Other == 0) ? SnapX0[i] : SnapX1[i]; };
	const double FirstT = static_cast<double>(SnapTick[0]) * Ns::SimDtMs;
	const double LastT = static_cast<double>(SnapTick.Last()) * Ns::SimDtMs;
	const double TShow = NowMs - Ns::InterpDelayMs;
	if (SnapTick.Num() == 1 || TShow <= FirstT)
	{
		RemoteDrawn = XAt(0);
		return;
	}
	if (TShow >= LastT)
	{
		RemoteDrawn = XAt(SnapTick.Num() - 1);
		return;
	}
	for (int32 i = 0; i < SnapTick.Num() - 1; ++i)
	{
		const double T0 = static_cast<double>(SnapTick[i]) * Ns::SimDtMs;
		const double T1 = static_cast<double>(SnapTick[i + 1]) * Ns::SimDtMs;
		if (T0 <= TShow && TShow <= T1)
		{
			const double U = (T1 > T0) ? (TShow - T0) / (T1 - T0) : 0.0;
			const int32 A = XAt(i);
			const int32 B = XAt(i + 1);
			RemoteDrawn = A + static_cast<int32>((B - A) * U);
			return;
		}
	}
	RemoteDrawn = XAt(SnapTick.Num() - 1);
}
