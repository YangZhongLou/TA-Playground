// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

constexpr uint32 NsStunMagic = 0x2112A442u;
constexpr int32 NsStunHeaderBytes = 20;
constexpr int32 NsStunTxIdBytes = 12;

NETWORKSYNC_API void NsStunFillTxId(uint8 TxId[NsStunTxIdBytes]);
NETWORKSYNC_API bool NsStunEncodeBindRequest(const uint8 TxId[NsStunTxIdBytes], TArray<uint8>& Out);
NETWORKSYNC_API bool NsStunEncodeBindIndication(const uint8 TxId[NsStunTxIdBytes], TArray<uint8>& Out);
NETWORKSYNC_API bool NsStunEncodeXorMappedReply(
	const uint8 TxId[NsStunTxIdBytes], uint32 Ipv4Host, int32 Port, TArray<uint8>& Out);
NETWORKSYNC_API bool NsStunDecodeMapped(
	const TArray<uint8>& Bytes, const uint8 TxId[NsStunTxIdBytes], uint32& OutIpv4Host, int32& OutPort);
NETWORKSYNC_API bool NsStunIsBindIndication(const TArray<uint8>& Bytes);
NETWORKSYNC_API FString NsStunIpv4ToString(uint32 Ipv4Host);
NETWORKSYNC_API bool NsStunParseIpv4(const FString& Host, uint32& OutIpv4Host);

constexpr uint32 NsRendezvousMagic = 0x4E535256u;
constexpr int32 NsRendezvousBytes = 12;

NETWORKSYNC_API bool NsRendezvousEncode(uint8 Slot, uint32 Ipv4Host, int32 Port, TArray<uint8>& Out);
NETWORKSYNC_API bool NsRendezvousDecode(
	const TArray<uint8>& Bytes, uint8& OutSlot, uint32& OutIpv4Host, int32& OutPort);
