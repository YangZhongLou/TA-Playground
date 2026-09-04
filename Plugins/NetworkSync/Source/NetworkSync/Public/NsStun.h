// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

constexpr uint32 NsStunMagic = 0x2112A442u;
constexpr int32 NsStunHeaderBytes = 20;
constexpr int32 NsStunTxIdBytes = 12;

NETWORKSYNC_API void NsStunFillTxId(uint8 TxId[NsStunTxIdBytes]);
NETWORKSYNC_API bool NsStunEncodeBindRequest(const uint8 TxId[NsStunTxIdBytes], TArray<uint8>& Out);
NETWORKSYNC_API bool NsStunEncodeBindNominate(const uint8 TxId[NsStunTxIdBytes], TArray<uint8>& Out);
NETWORKSYNC_API bool NsStunEncodeBindIndication(const uint8 TxId[NsStunTxIdBytes], TArray<uint8>& Out);
NETWORKSYNC_API bool NsStunEncodeXorMappedReply(
	const uint8 TxId[NsStunTxIdBytes], uint32 Ipv4Host, int32 Port, TArray<uint8>& Out);
NETWORKSYNC_API bool NsStunDecodeMapped(
	const TArray<uint8>& Bytes, const uint8 TxId[NsStunTxIdBytes], uint32& OutIpv4Host, int32& OutPort);
NETWORKSYNC_API bool NsStunIsBindIndication(const TArray<uint8>& Bytes);
NETWORKSYNC_API bool NsStunIsBindRequest(const TArray<uint8>& Bytes);
NETWORKSYNC_API bool NsStunHasUseCandidate(const TArray<uint8>& Bytes);
NETWORKSYNC_API bool NsStunEncodeAllocateRequest(const uint8 TxId[NsStunTxIdBytes], TArray<uint8>& Out);
NETWORKSYNC_API bool NsStunEncodeXorRelayedReply(
	const uint8 TxId[NsStunTxIdBytes], uint32 Ipv4Host, int32 Port, TArray<uint8>& Out);
NETWORKSYNC_API bool NsStunDecodeRelayed(
	const TArray<uint8>& Bytes, const uint8 TxId[NsStunTxIdBytes], uint32& OutIpv4Host, int32& OutPort);
NETWORKSYNC_API bool NsStunIsAllocateRequest(const TArray<uint8>& Bytes);
NETWORKSYNC_API bool NsStunEncodeCreatePermissionRequest(
	const uint8 TxId[NsStunTxIdBytes], uint32 PeerIpv4Host, int32 PeerPort, TArray<uint8>& Out);
NETWORKSYNC_API bool NsStunEncodeCreatePermissionSuccess(const uint8 TxId[NsStunTxIdBytes], TArray<uint8>& Out);
NETWORKSYNC_API bool NsStunDecodePeer(
	const TArray<uint8>& Bytes, const uint8 TxId[NsStunTxIdBytes], uint32& OutIpv4Host, int32& OutPort);
NETWORKSYNC_API bool NsStunDecodePermissionSuccess(const TArray<uint8>& Bytes, const uint8 TxId[NsStunTxIdBytes]);
NETWORKSYNC_API bool NsStunIsCreatePermissionRequest(const TArray<uint8>& Bytes);
NETWORKSYNC_API bool NsStunEncodeChannelBindRequest(
	const uint8 TxId[NsStunTxIdBytes], uint16 Channel, uint32 PeerIpv4Host, int32 PeerPort, TArray<uint8>& Out);
NETWORKSYNC_API bool NsStunEncodeChannelBindSuccess(const uint8 TxId[NsStunTxIdBytes], TArray<uint8>& Out);
NETWORKSYNC_API bool NsStunDecodeChannelBind(
	const TArray<uint8>& Bytes, const uint8 TxId[NsStunTxIdBytes],
	uint16& OutChannel, uint32& OutIpv4Host, int32& OutPort);
NETWORKSYNC_API bool NsStunDecodeChannelBindSuccess(const TArray<uint8>& Bytes, const uint8 TxId[NsStunTxIdBytes]);
NETWORKSYNC_API bool NsStunIsChannelBindRequest(const TArray<uint8>& Bytes);
NETWORKSYNC_API bool NsEncodeChannelData(uint16 Channel, const TArray<uint8>& Payload, TArray<uint8>& Out);
NETWORKSYNC_API bool NsDecodeChannelData(const TArray<uint8>& Bytes, uint16& OutChannel, TArray<uint8>& OutPayload);
NETWORKSYNC_API bool NsStunReadTxId(const TArray<uint8>& Bytes, uint8 TxId[NsStunTxIdBytes]);
NETWORKSYNC_API FString NsStunIpv4ToString(uint32 Ipv4Host);
NETWORKSYNC_API bool NsStunParseIpv4(const FString& Host, uint32& OutIpv4Host);

constexpr uint32 NsRendezvousMagic = 0x4E535256u;
constexpr int32 NsRendezvousBytes = 12;
constexpr uint16 NsTurnChannelMin = 0x4000;
constexpr uint16 NsTurnChannelMax = 0x7FFF;
constexpr int32 NsChannelDataHeaderBytes = 4;

NETWORKSYNC_API bool NsRendezvousEncode(uint8 Slot, uint32 Ipv4Host, int32 Port, TArray<uint8>& Out);
NETWORKSYNC_API bool NsRendezvousDecode(
	const TArray<uint8>& Bytes, uint8& OutSlot, uint32& OutIpv4Host, int32& OutPort);

enum class ENsIceType : uint8
{
	Host = 0,
	Srflx = 1,
	Relay = 2,
};

struct FNsIceCandidate
{
	ENsIceType Type = ENsIceType::Host;
	uint32 Ipv4 = 0;
	int32 Port = 0;
};

constexpr uint32 NsIceMagic = 0x4E534943u;
constexpr int32 NsIceHeaderBytes = 6;
constexpr int32 NsIceCandBytes = 7;
constexpr int32 NsIceMaxCandidates = 3;

NETWORKSYNC_API bool NsIceEncode(uint8 Slot, const TArray<FNsIceCandidate>& Cands, TArray<uint8>& Out);
NETWORKSYNC_API bool NsIceDecode(
	const TArray<uint8>& Bytes, uint8& OutSlot, TArray<FNsIceCandidate>& OutCands);
NETWORKSYNC_API bool NsIceSdpEncode(uint8 Slot, const TArray<FNsIceCandidate>& Cands, FString& Out);
NETWORKSYNC_API bool NsIceSdpDecode(
	const FString& Text, uint8& OutSlot, TArray<FNsIceCandidate>& OutCands);

struct FNsIcePair
{
	FNsIceCandidate Local;
	FNsIceCandidate Remote;
};

NETWORKSYNC_API bool NsIceFormPairs(
	const TArray<FNsIceCandidate>& Local, const TArray<FNsIceCandidate>& Remote, TArray<FNsIcePair>& Out);
