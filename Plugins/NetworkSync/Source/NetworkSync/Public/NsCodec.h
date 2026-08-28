// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NsFakeNet.h"

NETWORKSYNC_API bool NsEncodePacket(const FNsPacket& Packet, TArray<uint8>& OutBytes);
NETWORKSYNC_API bool NsDecodePacket(const TArray<uint8>& Bytes, FNsPacket& OutPacket);
NETWORKSYNC_API int32 NsPayloadBytes(const FNsPacket& Packet);
NETWORKSYNC_API int32 NsWireBytes(const FNsPacket& Packet);
NETWORKSYNC_API bool NsFitsMtu(const FNsPacket& Packet);
NETWORKSYNC_API int32 NsMaxEntriesPerPacket(ENsMsg Type);
NETWORKSYNC_API void NsSplitForMtu(const FNsPacket& Packet, TArray<FNsPacket>& Out);
