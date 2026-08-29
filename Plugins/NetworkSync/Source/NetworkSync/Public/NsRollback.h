// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsTypes.h"
#include "NsNet.h"

class NETWORKSYNC_API FNsRollbackPeer
{
public:
	int32 PlayerId = 0;
	ENsAddr Addr = ENsAddr::C0;
	ENsAddr Other = ENsAddr::C1;
	int32 Frame = 0;
	int32 Confirmed = -1;
	int32 WaitCount = 0;
	bool bWaiting = false;
	bool bInRollback = false;
	FNsWorld World;
	TMap<int32, FNsWorld> Saves;
	TMap<int32, FNsInputs> Pred;
	TMap<int32, int8> Local;
	TMap<int32, int8> RealRemote;

	void AdvanceLocal(int8 Dx, TMap<int32, int8>& OutPacked);
	void Advance(INsNet& Net, int8 Dx);
	void OnRemote(const TMap<int32, int8>& Packed);

private:
	FNsInputs Pair(int32 F) const;
	int8 RemoteOrPred(int32 F) const;
	bool RollbackFrom(int32 F);
	void Trim();
	void RaiseConfirmed();
	void CollectPacked(int32 EndF, TMap<int32, int8>& Out) const;
};
