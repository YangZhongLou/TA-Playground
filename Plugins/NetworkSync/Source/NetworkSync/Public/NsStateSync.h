// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsTypes.h"
#include "NsFakeNet.h"

struct FNsPawn
{
	int32 X = 0;
	int32 LastSeq = 0;
};

class NETWORKSYNC_API FNsStateSyncServer
{
public:
	int32 Tick = 0;
	FNsPawn Pawns[Ns::PlayerCount];
	int32 PendingSeq[Ns::PlayerCount] = {0, 0};
	int8 PendingDx[Ns::PlayerCount] = {0, 0};
	bool bHasPending[Ns::PlayerCount] = {false, false};
	int32 LastAck[Ns::PlayerCount] = {0, 0};
	int32 HistX[Ns::PlayerCount][Ns::HistoryTicks] = {};
	int32 DeltaSent = 0;

	void OnInput(int32 PlayerId, int32 Seq, int8 Dx);
	void OnAck(int32 PlayerId, int32 AckTick);
	void Sim(FNsFakeNet& Net);
	int32 RewindX(int32 PlayerId, int32 PingMs) const;

private:
	TMap<int32, int32> SnapX0;
	TMap<int32, int32> SnapX1;
	void SendSnap(FNsFakeNet& Net, ENsAddr Dst, int32 PlayerId);
};

class NETWORKSYNC_API FNsStateSyncClient
{
public:
	int32 PlayerId = 0;
	ENsAddr Addr = ENsAddr::C0;
	int32 Seq = 0;
	int32 PredX = 0;
	TArray<int32> UnackedSeq;
	TArray<int8> UnackedDx;
	TArray<int32> SnapTick;
	TArray<int32> SnapX0;
	TArray<int32> SnapX1;
	TMap<int32, int32> Store0;
	TMap<int32, int32> Store1;
	int32 RemoteDrawn = 0;
	int32 LastAckedTick = 0;
	bool bHasRemote = false;
	bool bGotDelta = false;

	void LocalTick(FNsFakeNet& Net, int8 Dx);
	void OnSnap(FNsFakeNet& Net, const FNsPacket& P);
	void UpdateRemoteDraw(double NowMs);
};
