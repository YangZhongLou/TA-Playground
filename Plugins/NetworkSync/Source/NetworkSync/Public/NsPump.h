// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsLockstep.h"
#include "NsStateSync.h"
#include "NsRollback.h"

NETWORKSYNC_API void NsDrain(INsNet& Net, ENsAddr Dst, TArray<FNsPacket>& Out, bool bWait = false);
NETWORKSYNC_API void NsPumpLockstepServer(INsNet& Net, FNsLockstepServer& Sv, bool bWait = false);
NETWORKSYNC_API void NsPumpLockstepClient(INsNet& Net, FNsLockstepClient& C, bool bWait = false);
NETWORKSYNC_API void NsPumpStateServer(INsNet& Net, FNsStateSyncServer& Sv, bool bWait = false);
NETWORKSYNC_API void NsPumpStateClient(INsNet& Net, FNsStateSyncClient& C, bool bWait = false);
NETWORKSYNC_API void NsPumpRollbackPeer(INsNet& Net, FNsRollbackPeer& Peer, bool bWait = false);
