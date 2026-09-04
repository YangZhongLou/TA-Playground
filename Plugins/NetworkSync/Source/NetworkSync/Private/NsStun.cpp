// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsStun.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/PlatformTime.h"

namespace
{
constexpr uint16 NsStunBindRequest = 0x0001;
constexpr uint16 NsStunBindIndication = 0x0011;
constexpr uint16 NsStunBindSuccess = 0x0101;
constexpr uint16 NsStunAllocateRequest = 0x0003;
constexpr uint16 NsStunAllocateSuccess = 0x0103;
constexpr uint16 NsStunCreatePermissionRequest = 0x0008;
constexpr uint16 NsStunCreatePermissionSuccess = 0x0108;
constexpr uint16 NsStunChannelBindRequest = 0x0009;
constexpr uint16 NsStunChannelBindSuccess = 0x0109;
constexpr uint16 NsStunAttrMapped = 0x0001;
constexpr uint16 NsStunAttrChannelNumber = 0x000C;
constexpr uint16 NsStunAttrXorMapped = 0x0020;
constexpr uint16 NsStunAttrXorPeer = 0x0012;
constexpr uint16 NsStunAttrXorRelayed = 0x0016;
constexpr uint16 NsStunAttrRequestedTransport = 0x0019;
constexpr uint16 NsStunAttrUseCandidate = 0x0025;
constexpr uint8 NsStunFamilyIpv4 = 0x01;
constexpr uint8 NsStunProtoUdp = 17;

void NsStunW16(TArray<uint8>& Out, uint16 Value)
{
	Out.Add(static_cast<uint8>(Value >> 8));
	Out.Add(static_cast<uint8>(Value));
}

void NsStunW32(TArray<uint8>& Out, uint32 Value)
{
	Out.Add(static_cast<uint8>(Value >> 24));
	Out.Add(static_cast<uint8>(Value >> 16));
	Out.Add(static_cast<uint8>(Value >> 8));
	Out.Add(static_cast<uint8>(Value));
}

bool NsStunR16(const uint8* Data, int32 Num, int32 At, uint16& Value)
{
	if (At < 0 || At + 2 > Num)
	{
		return false;
	}
	Value = static_cast<uint16>((Data[At] << 8) | Data[At + 1]);
	return true;
}

bool NsStunR32(const uint8* Data, int32 Num, int32 At, uint32& Value)
{
	if (At < 0 || At + 4 > Num)
	{
		return false;
	}
	Value = (static_cast<uint32>(Data[At]) << 24)
		| (static_cast<uint32>(Data[At + 1]) << 16)
		| (static_cast<uint32>(Data[At + 2]) << 8)
		| static_cast<uint32>(Data[At + 3]);
	return true;
}

bool NsStunReadHeader(const TArray<uint8>& Bytes, uint16& Type, uint16& Length, const uint8*& TxId)
{
	if (Bytes.Num() < NsStunHeaderBytes)
	{
		return false;
	}
	const uint8* Data = Bytes.GetData();
	uint32 Magic = 0;
	if (!NsStunR16(Data, Bytes.Num(), 0, Type)
		|| !NsStunR16(Data, Bytes.Num(), 2, Length)
		|| !NsStunR32(Data, Bytes.Num(), 4, Magic)
		|| Magic != NsStunMagic)
	{
		return false;
	}
	if (NsStunHeaderBytes + static_cast<int32>(Length) > Bytes.Num())
	{
		return false;
	}
	TxId = Data + 8;
	return true;
}
}

void NsStunFillTxId(uint8 TxId[NsStunTxIdBytes])
{
	uint32 Mix = static_cast<uint32>(FPlatformTime::Cycles());
	for (int32 i = 0; i < NsStunTxIdBytes; ++i)
	{
		Mix = Mix * 1103515245u + 12345u + static_cast<uint32>(FMath::Rand());
		TxId[i] = static_cast<uint8>(Mix >> 16);
	}
}

bool NsStunEncodeBindRequest(const uint8 TxId[NsStunTxIdBytes], TArray<uint8>& Out)
{
	if (!TxId)
	{
		return false;
	}
	Out.Reset();
	NsStunW16(Out, NsStunBindRequest);
	NsStunW16(Out, 0);
	NsStunW32(Out, NsStunMagic);
	Out.Append(TxId, NsStunTxIdBytes);
	return Out.Num() == NsStunHeaderBytes;
}

bool NsStunEncodeBindNominate(const uint8 TxId[NsStunTxIdBytes], TArray<uint8>& Out)
{
	if (!TxId)
	{
		return false;
	}
	Out.Reset();
	NsStunW16(Out, NsStunBindRequest);
	NsStunW16(Out, 4);
	NsStunW32(Out, NsStunMagic);
	Out.Append(TxId, NsStunTxIdBytes);
	NsStunW16(Out, NsStunAttrUseCandidate);
	NsStunW16(Out, 0);
	return Out.Num() == NsStunHeaderBytes + 4;
}

bool NsStunEncodeBindIndication(const uint8 TxId[NsStunTxIdBytes], TArray<uint8>& Out)
{
	if (!TxId)
	{
		return false;
	}
	Out.Reset();
	NsStunW16(Out, NsStunBindIndication);
	NsStunW16(Out, 0);
	NsStunW32(Out, NsStunMagic);
	Out.Append(TxId, NsStunTxIdBytes);
	return Out.Num() == NsStunHeaderBytes;
}

bool NsStunEncodeXorMappedReply(
	const uint8 TxId[NsStunTxIdBytes], uint32 Ipv4Host, int32 Port, TArray<uint8>& Out)
{
	if (!TxId || Port <= 0 || Port > 65535)
	{
		return false;
	}
	const uint16 XPort = static_cast<uint16>(Port) ^ static_cast<uint16>(NsStunMagic >> 16);
	const uint32 XAddr = Ipv4Host ^ NsStunMagic;
	Out.Reset();
	NsStunW16(Out, NsStunBindSuccess);
	NsStunW16(Out, 12);
	NsStunW32(Out, NsStunMagic);
	Out.Append(TxId, NsStunTxIdBytes);
	NsStunW16(Out, NsStunAttrXorMapped);
	NsStunW16(Out, 8);
	Out.Add(0);
	Out.Add(NsStunFamilyIpv4);
	NsStunW16(Out, XPort);
	NsStunW32(Out, XAddr);
	return Out.Num() == NsStunHeaderBytes + 12;
}

bool NsStunEncodeAllocateRequest(const uint8 TxId[NsStunTxIdBytes], TArray<uint8>& Out)
{
	if (!TxId)
	{
		return false;
	}
	Out.Reset();
	NsStunW16(Out, NsStunAllocateRequest);
	NsStunW16(Out, 8);
	NsStunW32(Out, NsStunMagic);
	Out.Append(TxId, NsStunTxIdBytes);
	NsStunW16(Out, NsStunAttrRequestedTransport);
	NsStunW16(Out, 4);
	Out.Add(NsStunProtoUdp);
	Out.Add(0);
	Out.Add(0);
	Out.Add(0);
	return Out.Num() == NsStunHeaderBytes + 8;
}

bool NsStunEncodeXorRelayedReply(
	const uint8 TxId[NsStunTxIdBytes], uint32 Ipv4Host, int32 Port, TArray<uint8>& Out)
{
	if (!TxId || Port <= 0 || Port > 65535)
	{
		return false;
	}
	const uint16 XPort = static_cast<uint16>(Port) ^ static_cast<uint16>(NsStunMagic >> 16);
	const uint32 XAddr = Ipv4Host ^ NsStunMagic;
	Out.Reset();
	NsStunW16(Out, NsStunAllocateSuccess);
	NsStunW16(Out, 12);
	NsStunW32(Out, NsStunMagic);
	Out.Append(TxId, NsStunTxIdBytes);
	NsStunW16(Out, NsStunAttrXorRelayed);
	NsStunW16(Out, 8);
	Out.Add(0);
	Out.Add(NsStunFamilyIpv4);
	NsStunW16(Out, XPort);
	NsStunW32(Out, XAddr);
	return Out.Num() == NsStunHeaderBytes + 12;
}

bool NsStunEncodeCreatePermissionRequest(
	const uint8 TxId[NsStunTxIdBytes], uint32 PeerIpv4Host, int32 PeerPort, TArray<uint8>& Out)
{
	if (!TxId || PeerPort <= 0 || PeerPort > 65535)
	{
		return false;
	}
	const uint16 XPort = static_cast<uint16>(PeerPort) ^ static_cast<uint16>(NsStunMagic >> 16);
	const uint32 XAddr = PeerIpv4Host ^ NsStunMagic;
	Out.Reset();
	NsStunW16(Out, NsStunCreatePermissionRequest);
	NsStunW16(Out, 12);
	NsStunW32(Out, NsStunMagic);
	Out.Append(TxId, NsStunTxIdBytes);
	NsStunW16(Out, NsStunAttrXorPeer);
	NsStunW16(Out, 8);
	Out.Add(0);
	Out.Add(NsStunFamilyIpv4);
	NsStunW16(Out, XPort);
	NsStunW32(Out, XAddr);
	return Out.Num() == NsStunHeaderBytes + 12;
}

bool NsStunEncodeCreatePermissionSuccess(const uint8 TxId[NsStunTxIdBytes], TArray<uint8>& Out)
{
	if (!TxId)
	{
		return false;
	}
	Out.Reset();
	NsStunW16(Out, NsStunCreatePermissionSuccess);
	NsStunW16(Out, 0);
	NsStunW32(Out, NsStunMagic);
	Out.Append(TxId, NsStunTxIdBytes);
	return Out.Num() == NsStunHeaderBytes;
}

bool NsStunEncodeChannelBindRequest(
	const uint8 TxId[NsStunTxIdBytes], uint16 Channel, uint32 PeerIpv4Host, int32 PeerPort, TArray<uint8>& Out)
{
	if (!TxId || PeerPort <= 0 || PeerPort > 65535
		|| Channel < NsTurnChannelMin || Channel > NsTurnChannelMax)
	{
		return false;
	}
	const uint16 XPort = static_cast<uint16>(PeerPort) ^ static_cast<uint16>(NsStunMagic >> 16);
	const uint32 XAddr = PeerIpv4Host ^ NsStunMagic;
	Out.Reset();
	NsStunW16(Out, NsStunChannelBindRequest);
	NsStunW16(Out, 20);
	NsStunW32(Out, NsStunMagic);
	Out.Append(TxId, NsStunTxIdBytes);
	NsStunW16(Out, NsStunAttrChannelNumber);
	NsStunW16(Out, 4);
	NsStunW16(Out, Channel);
	NsStunW16(Out, 0);
	NsStunW16(Out, NsStunAttrXorPeer);
	NsStunW16(Out, 8);
	Out.Add(0);
	Out.Add(NsStunFamilyIpv4);
	NsStunW16(Out, XPort);
	NsStunW32(Out, XAddr);
	return Out.Num() == NsStunHeaderBytes + 20;
}

bool NsStunEncodeChannelBindSuccess(const uint8 TxId[NsStunTxIdBytes], TArray<uint8>& Out)
{
	if (!TxId)
	{
		return false;
	}
	Out.Reset();
	NsStunW16(Out, NsStunChannelBindSuccess);
	NsStunW16(Out, 0);
	NsStunW32(Out, NsStunMagic);
	Out.Append(TxId, NsStunTxIdBytes);
	return Out.Num() == NsStunHeaderBytes;
}

bool NsStunDecodeMapped(
	const TArray<uint8>& Bytes, const uint8 TxId[NsStunTxIdBytes], uint32& OutIpv4Host, int32& OutPort)
{
	OutIpv4Host = 0;
	OutPort = 0;
	if (!TxId)
	{
		return false;
	}
	uint16 Type = 0;
	uint16 Length = 0;
	const uint8* GotTx = nullptr;
	if (!NsStunReadHeader(Bytes, Type, Length, GotTx) || Type != NsStunBindSuccess)
	{
		return false;
	}
	if (FMemory::Memcmp(GotTx, TxId, NsStunTxIdBytes) != 0)
	{
		return false;
	}

	const uint8* Data = Bytes.GetData();
	int32 At = NsStunHeaderBytes;
	const int32 End = NsStunHeaderBytes + static_cast<int32>(Length);
	bool bHit = false;
	while (At + 4 <= End)
	{
		uint16 Attr = 0;
		uint16 AttrLen = 0;
		if (!NsStunR16(Data, Bytes.Num(), At, Attr) || !NsStunR16(Data, Bytes.Num(), At + 2, AttrLen))
		{
			return false;
		}
		const int32 Val = At + 4;
		const int32 Next = Val + ((AttrLen + 3) & ~3);
		if (Val + AttrLen > End)
		{
			return false;
		}
		if ((Attr == NsStunAttrXorMapped || Attr == NsStunAttrMapped) && AttrLen >= 8)
		{
			if (Data[Val + 1] != NsStunFamilyIpv4)
			{
				At = Next;
				continue;
			}
			uint16 WirePort = 0;
			uint32 WireAddr = 0;
			if (!NsStunR16(Data, Bytes.Num(), Val + 2, WirePort)
				|| !NsStunR32(Data, Bytes.Num(), Val + 4, WireAddr))
			{
				return false;
			}
			if (Attr == NsStunAttrXorMapped)
			{
				OutPort = static_cast<int32>(WirePort ^ static_cast<uint16>(NsStunMagic >> 16));
				OutIpv4Host = WireAddr ^ NsStunMagic;
			}
			else
			{
				OutPort = static_cast<int32>(WirePort);
				OutIpv4Host = WireAddr;
			}
			bHit = OutPort > 0;
		}
		At = Next;
	}
	return bHit;
}

bool NsStunDecodeRelayed(
	const TArray<uint8>& Bytes, const uint8 TxId[NsStunTxIdBytes], uint32& OutIpv4Host, int32& OutPort)
{
	OutIpv4Host = 0;
	OutPort = 0;
	if (!TxId)
	{
		return false;
	}
	uint16 Type = 0;
	uint16 Length = 0;
	const uint8* GotTx = nullptr;
	if (!NsStunReadHeader(Bytes, Type, Length, GotTx) || Type != NsStunAllocateSuccess)
	{
		return false;
	}
	if (FMemory::Memcmp(GotTx, TxId, NsStunTxIdBytes) != 0)
	{
		return false;
	}

	const uint8* Data = Bytes.GetData();
	int32 At = NsStunHeaderBytes;
	const int32 End = NsStunHeaderBytes + static_cast<int32>(Length);
	bool bHit = false;
	while (At + 4 <= End)
	{
		uint16 Attr = 0;
		uint16 AttrLen = 0;
		if (!NsStunR16(Data, Bytes.Num(), At, Attr) || !NsStunR16(Data, Bytes.Num(), At + 2, AttrLen))
		{
			return false;
		}
		const int32 Val = At + 4;
		const int32 Next = Val + ((AttrLen + 3) & ~3);
		if (Val + AttrLen > End)
		{
			return false;
		}
		if (Attr == NsStunAttrXorRelayed && AttrLen >= 8)
		{
			if (Data[Val + 1] != NsStunFamilyIpv4)
			{
				At = Next;
				continue;
			}
			uint16 WirePort = 0;
			uint32 WireAddr = 0;
			if (!NsStunR16(Data, Bytes.Num(), Val + 2, WirePort)
				|| !NsStunR32(Data, Bytes.Num(), Val + 4, WireAddr))
			{
				return false;
			}
			OutPort = static_cast<int32>(WirePort ^ static_cast<uint16>(NsStunMagic >> 16));
			OutIpv4Host = WireAddr ^ NsStunMagic;
			bHit = OutPort > 0;
		}
		At = Next;
	}
	return bHit;
}

bool NsStunDecodePeer(
	const TArray<uint8>& Bytes, const uint8 TxId[NsStunTxIdBytes], uint32& OutIpv4Host, int32& OutPort)
{
	OutIpv4Host = 0;
	OutPort = 0;
	if (!TxId)
	{
		return false;
	}
	uint16 Type = 0;
	uint16 Length = 0;
	const uint8* GotTx = nullptr;
	if (!NsStunReadHeader(Bytes, Type, Length, GotTx) || Type != NsStunCreatePermissionRequest)
	{
		return false;
	}
	if (FMemory::Memcmp(GotTx, TxId, NsStunTxIdBytes) != 0)
	{
		return false;
	}

	const uint8* Data = Bytes.GetData();
	int32 At = NsStunHeaderBytes;
	const int32 End = NsStunHeaderBytes + static_cast<int32>(Length);
	bool bHit = false;
	while (At + 4 <= End)
	{
		uint16 Attr = 0;
		uint16 AttrLen = 0;
		if (!NsStunR16(Data, Bytes.Num(), At, Attr) || !NsStunR16(Data, Bytes.Num(), At + 2, AttrLen))
		{
			return false;
		}
		const int32 Val = At + 4;
		const int32 Next = Val + ((AttrLen + 3) & ~3);
		if (Val + AttrLen > End)
		{
			return false;
		}
		if (Attr == NsStunAttrXorPeer && AttrLen >= 8)
		{
			if (Data[Val + 1] != NsStunFamilyIpv4)
			{
				At = Next;
				continue;
			}
			uint16 WirePort = 0;
			uint32 WireAddr = 0;
			if (!NsStunR16(Data, Bytes.Num(), Val + 2, WirePort)
				|| !NsStunR32(Data, Bytes.Num(), Val + 4, WireAddr))
			{
				return false;
			}
			OutPort = static_cast<int32>(WirePort ^ static_cast<uint16>(NsStunMagic >> 16));
			OutIpv4Host = WireAddr ^ NsStunMagic;
			bHit = OutPort > 0;
		}
		At = Next;
	}
	return bHit;
}

bool NsStunDecodePermissionSuccess(const TArray<uint8>& Bytes, const uint8 TxId[NsStunTxIdBytes])
{
	if (!TxId)
	{
		return false;
	}
	uint16 Type = 0;
	uint16 Length = 0;
	const uint8* GotTx = nullptr;
	if (!NsStunReadHeader(Bytes, Type, Length, GotTx) || Type != NsStunCreatePermissionSuccess)
	{
		return false;
	}
	return FMemory::Memcmp(GotTx, TxId, NsStunTxIdBytes) == 0;
}

bool NsStunIsBindIndication(const TArray<uint8>& Bytes)
{
	uint16 Type = 0;
	uint16 Length = 0;
	const uint8* TxId = nullptr;
	return NsStunReadHeader(Bytes, Type, Length, TxId) && Type == NsStunBindIndication;
}

bool NsStunIsBindRequest(const TArray<uint8>& Bytes)
{
	uint16 Type = 0;
	uint16 Length = 0;
	const uint8* TxId = nullptr;
	return NsStunReadHeader(Bytes, Type, Length, TxId) && Type == NsStunBindRequest;
}

bool NsStunHasUseCandidate(const TArray<uint8>& Bytes)
{
	uint16 Type = 0;
	uint16 Length = 0;
	const uint8* TxId = nullptr;
	if (!NsStunReadHeader(Bytes, Type, Length, TxId) || Type != NsStunBindRequest)
	{
		return false;
	}
	const uint8* Data = Bytes.GetData();
	int32 At = NsStunHeaderBytes;
	const int32 End = NsStunHeaderBytes + static_cast<int32>(Length);
	while (At + 4 <= End)
	{
		uint16 Attr = 0;
		uint16 AttrLen = 0;
		if (!NsStunR16(Data, Bytes.Num(), At, Attr) || !NsStunR16(Data, Bytes.Num(), At + 2, AttrLen))
		{
			return false;
		}
		const int32 Val = At + 4;
		const int32 Next = Val + ((AttrLen + 3) & ~3);
		if (Val + AttrLen > End)
		{
			return false;
		}
		if (Attr == NsStunAttrUseCandidate && AttrLen == 0)
		{
			return true;
		}
		At = Next;
	}
	return false;
}

bool NsStunIsAllocateRequest(const TArray<uint8>& Bytes)
{
	uint16 Type = 0;
	uint16 Length = 0;
	const uint8* TxId = nullptr;
	return NsStunReadHeader(Bytes, Type, Length, TxId) && Type == NsStunAllocateRequest;
}

bool NsStunIsCreatePermissionRequest(const TArray<uint8>& Bytes)
{
	uint16 Type = 0;
	uint16 Length = 0;
	const uint8* TxId = nullptr;
	return NsStunReadHeader(Bytes, Type, Length, TxId) && Type == NsStunCreatePermissionRequest;
}

bool NsStunDecodeChannelBind(
	const TArray<uint8>& Bytes, const uint8 TxId[NsStunTxIdBytes],
	uint16& OutChannel, uint32& OutIpv4Host, int32& OutPort)
{
	OutChannel = 0;
	OutIpv4Host = 0;
	OutPort = 0;
	if (!TxId)
	{
		return false;
	}
	uint16 Type = 0;
	uint16 Length = 0;
	const uint8* GotTx = nullptr;
	if (!NsStunReadHeader(Bytes, Type, Length, GotTx) || Type != NsStunChannelBindRequest)
	{
		return false;
	}
	if (FMemory::Memcmp(GotTx, TxId, NsStunTxIdBytes) != 0)
	{
		return false;
	}

	const uint8* Data = Bytes.GetData();
	int32 At = NsStunHeaderBytes;
	const int32 End = NsStunHeaderBytes + static_cast<int32>(Length);
	bool bCh = false;
	bool bPeer = false;
	while (At + 4 <= End)
	{
		uint16 Attr = 0;
		uint16 AttrLen = 0;
		if (!NsStunR16(Data, Bytes.Num(), At, Attr) || !NsStunR16(Data, Bytes.Num(), At + 2, AttrLen))
		{
			return false;
		}
		const int32 Val = At + 4;
		const int32 Next = Val + ((AttrLen + 3) & ~3);
		if (Val + AttrLen > End)
		{
			return false;
		}
		if (Attr == NsStunAttrChannelNumber && AttrLen >= 4)
		{
			uint16 Channel = 0;
			if (!NsStunR16(Data, Bytes.Num(), Val, Channel)
				|| Channel < NsTurnChannelMin || Channel > NsTurnChannelMax)
			{
				return false;
			}
			OutChannel = Channel;
			bCh = true;
		}
		else if (Attr == NsStunAttrXorPeer && AttrLen >= 8)
		{
			if (Data[Val + 1] != NsStunFamilyIpv4)
			{
				At = Next;
				continue;
			}
			uint16 WirePort = 0;
			uint32 WireAddr = 0;
			if (!NsStunR16(Data, Bytes.Num(), Val + 2, WirePort)
				|| !NsStunR32(Data, Bytes.Num(), Val + 4, WireAddr))
			{
				return false;
			}
			OutPort = static_cast<int32>(WirePort ^ static_cast<uint16>(NsStunMagic >> 16));
			OutIpv4Host = WireAddr ^ NsStunMagic;
			bPeer = OutPort > 0;
		}
		At = Next;
	}
	return bCh && bPeer;
}

bool NsStunDecodeChannelBindSuccess(const TArray<uint8>& Bytes, const uint8 TxId[NsStunTxIdBytes])
{
	if (!TxId)
	{
		return false;
	}
	uint16 Type = 0;
	uint16 Length = 0;
	const uint8* GotTx = nullptr;
	if (!NsStunReadHeader(Bytes, Type, Length, GotTx) || Type != NsStunChannelBindSuccess)
	{
		return false;
	}
	return FMemory::Memcmp(GotTx, TxId, NsStunTxIdBytes) == 0;
}

bool NsStunIsChannelBindRequest(const TArray<uint8>& Bytes)
{
	uint16 Type = 0;
	uint16 Length = 0;
	const uint8* TxId = nullptr;
	return NsStunReadHeader(Bytes, Type, Length, TxId) && Type == NsStunChannelBindRequest;
}

bool NsEncodeChannelData(uint16 Channel, const TArray<uint8>& Payload, TArray<uint8>& Out)
{
	if (Channel < NsTurnChannelMin || Channel > NsTurnChannelMax || Payload.Num() > 0xffff)
	{
		return false;
	}
	Out.Reset();
	NsStunW16(Out, Channel);
	NsStunW16(Out, static_cast<uint16>(Payload.Num()));
	Out.Append(Payload);
	while ((Out.Num() & 3) != 0)
	{
		Out.Add(0);
	}
	return true;
}

bool NsDecodeChannelData(const TArray<uint8>& Bytes, uint16& OutChannel, TArray<uint8>& OutPayload)
{
	OutChannel = 0;
	OutPayload.Reset();
	if (Bytes.Num() < NsChannelDataHeaderBytes)
	{
		return false;
	}
	const uint8* Data = Bytes.GetData();
	uint16 Channel = 0;
	uint16 Len = 0;
	if (!NsStunR16(Data, Bytes.Num(), 0, Channel) || !NsStunR16(Data, Bytes.Num(), 2, Len))
	{
		return false;
	}
	if (Channel < NsTurnChannelMin || Channel > NsTurnChannelMax
		|| Bytes.Num() < NsChannelDataHeaderBytes + static_cast<int32>(Len))
	{
		return false;
	}
	OutChannel = Channel;
	OutPayload.Append(Data + NsChannelDataHeaderBytes, Len);
	return true;
}

bool NsStunReadTxId(const TArray<uint8>& Bytes, uint8 TxId[NsStunTxIdBytes])
{
	uint16 Type = 0;
	uint16 Length = 0;
	const uint8* Got = nullptr;
	if (!TxId || !NsStunReadHeader(Bytes, Type, Length, Got))
	{
		return false;
	}
	FMemory::Memcpy(TxId, Got, NsStunTxIdBytes);
	return true;
}

FString NsStunIpv4ToString(uint32 Ipv4Host)
{
	return FString::Printf(TEXT("%u.%u.%u.%u"),
		(Ipv4Host >> 24) & 0xffu,
		(Ipv4Host >> 16) & 0xffu,
		(Ipv4Host >> 8) & 0xffu,
		Ipv4Host & 0xffu);
}

bool NsStunParseIpv4(const FString& Host, uint32& OutIpv4Host)
{
	TArray<FString> Parts;
	Host.ParseIntoArray(Parts, TEXT("."), true);
	if (Parts.Num() != 4)
	{
		return false;
	}
	uint32 Acc = 0;
	for (int32 i = 0; i < 4; ++i)
	{
		const int32 Octet = FCString::Atoi(*Parts[i]);
		if (Octet < 0 || Octet > 255)
		{
			return false;
		}
		Acc = (Acc << 8) | static_cast<uint32>(Octet);
	}
	OutIpv4Host = Acc;
	return true;
}

bool NsRendezvousEncode(uint8 Slot, uint32 Ipv4Host, int32 Port, TArray<uint8>& Out)
{
	if (Slot > 2 || Port <= 0 || Port > 65535)
	{
		return false;
	}
	Out.Reset();
	Out.Add(static_cast<uint8>(NsRendezvousMagic));
	Out.Add(static_cast<uint8>(NsRendezvousMagic >> 8));
	Out.Add(static_cast<uint8>(NsRendezvousMagic >> 16));
	Out.Add(static_cast<uint8>(NsRendezvousMagic >> 24));
	Out.Add(Slot);
	Out.Add(0);
	const uint16 WirePort = static_cast<uint16>(Port);
	Out.Add(static_cast<uint8>(WirePort));
	Out.Add(static_cast<uint8>(WirePort >> 8));
	Out.Add(static_cast<uint8>(Ipv4Host));
	Out.Add(static_cast<uint8>(Ipv4Host >> 8));
	Out.Add(static_cast<uint8>(Ipv4Host >> 16));
	Out.Add(static_cast<uint8>(Ipv4Host >> 24));
	return Out.Num() == NsRendezvousBytes;
}

bool NsRendezvousDecode(const TArray<uint8>& Bytes, uint8& OutSlot, uint32& OutIpv4Host, int32& OutPort)
{
	if (Bytes.Num() != NsRendezvousBytes)
	{
		return false;
	}
	const uint8* Data = Bytes.GetData();
	const uint32 Magic = static_cast<uint32>(Data[0])
		| (static_cast<uint32>(Data[1]) << 8)
		| (static_cast<uint32>(Data[2]) << 16)
		| (static_cast<uint32>(Data[3]) << 24);
	if (Magic != NsRendezvousMagic || Data[5] != 0 || Data[4] > 2)
	{
		return false;
	}
	const int32 Port = static_cast<int32>(Data[6] | (static_cast<uint16>(Data[7]) << 8));
	if (Port <= 0)
	{
		return false;
	}
	OutSlot = Data[4];
	OutPort = Port;
	OutIpv4Host = static_cast<uint32>(Data[8])
		| (static_cast<uint32>(Data[9]) << 8)
		| (static_cast<uint32>(Data[10]) << 16)
		| (static_cast<uint32>(Data[11]) << 24);
	return true;
}

bool NsIceEncode(uint8 Slot, const TArray<FNsIceCandidate>& Cands, TArray<uint8>& Out)
{
	if (Slot > 2 || Cands.Num() < 1 || Cands.Num() > NsIceMaxCandidates)
	{
		return false;
	}
	for (const FNsIceCandidate& Cand : Cands)
	{
		const uint8 Type = static_cast<uint8>(Cand.Type);
		if (Type > static_cast<uint8>(ENsIceType::Relay) || Cand.Port <= 0 || Cand.Port > 65535)
		{
			return false;
		}
	}
	Out.Reset();
	Out.Add(static_cast<uint8>(NsIceMagic));
	Out.Add(static_cast<uint8>(NsIceMagic >> 8));
	Out.Add(static_cast<uint8>(NsIceMagic >> 16));
	Out.Add(static_cast<uint8>(NsIceMagic >> 24));
	Out.Add(Slot);
	Out.Add(static_cast<uint8>(Cands.Num()));
	for (const FNsIceCandidate& Cand : Cands)
	{
		Out.Add(static_cast<uint8>(Cand.Type));
		const uint16 WirePort = static_cast<uint16>(Cand.Port);
		Out.Add(static_cast<uint8>(WirePort));
		Out.Add(static_cast<uint8>(WirePort >> 8));
		Out.Add(static_cast<uint8>(Cand.Ipv4));
		Out.Add(static_cast<uint8>(Cand.Ipv4 >> 8));
		Out.Add(static_cast<uint8>(Cand.Ipv4 >> 16));
		Out.Add(static_cast<uint8>(Cand.Ipv4 >> 24));
	}
	return Out.Num() == NsIceHeaderBytes + NsIceCandBytes * Cands.Num();
}

bool NsIceDecode(const TArray<uint8>& Bytes, uint8& OutSlot, TArray<FNsIceCandidate>& OutCands)
{
	OutSlot = 0;
	OutCands.Reset();
	if (Bytes.Num() < NsIceHeaderBytes)
	{
		return false;
	}
	const uint8* Data = Bytes.GetData();
	const uint32 Magic = static_cast<uint32>(Data[0])
		| (static_cast<uint32>(Data[1]) << 8)
		| (static_cast<uint32>(Data[2]) << 16)
		| (static_cast<uint32>(Data[3]) << 24);
	const int32 Count = static_cast<int32>(Data[5]);
	if (Magic != NsIceMagic || Data[4] > 2 || Count < 1 || Count > NsIceMaxCandidates
		|| Bytes.Num() != NsIceHeaderBytes + NsIceCandBytes * Count)
	{
		return false;
	}
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Off = NsIceHeaderBytes + NsIceCandBytes * i;
		const uint8 Type = Data[Off];
		const int32 Port = static_cast<int32>(Data[Off + 1] | (static_cast<uint16>(Data[Off + 2]) << 8));
		if (Type > static_cast<uint8>(ENsIceType::Relay) || Port <= 0)
		{
			OutCands.Reset();
			return false;
		}
		FNsIceCandidate Cand;
		Cand.Type = static_cast<ENsIceType>(Type);
		Cand.Port = Port;
		Cand.Ipv4 = static_cast<uint32>(Data[Off + 3])
			| (static_cast<uint32>(Data[Off + 4]) << 8)
			| (static_cast<uint32>(Data[Off + 5]) << 16)
			| (static_cast<uint32>(Data[Off + 6]) << 24);
		OutCands.Add(Cand);
	}
	OutSlot = Data[4];
	return true;
}

bool NsIceFormPairs(
	const TArray<FNsIceCandidate>& Local, const TArray<FNsIceCandidate>& Remote, TArray<FNsIcePair>& Out)
{
	Out.Reset();
	auto Rank = [](ENsIceType Type) -> int32
	{
		if (Type == ENsIceType::Host)
		{
			return 0;
		}
		if (Type == ENsIceType::Srflx)
		{
			return 1;
		}
		return 9;
	};
	for (const FNsIceCandidate& L : Local)
	{
		if (L.Type == ENsIceType::Relay || L.Port <= 0)
		{
			continue;
		}
		for (const FNsIceCandidate& R : Remote)
		{
			if (R.Type == ENsIceType::Relay || R.Port <= 0)
			{
				continue;
			}
			FNsIcePair Pair;
			Pair.Local = L;
			Pair.Remote = R;
			Out.Add(Pair);
		}
	}
	if (Out.IsEmpty())
	{
		return false;
	}
	Out.Sort([&Rank](const FNsIcePair& A, const FNsIcePair& B)
	{
		const int32 Ra = Rank(A.Local.Type) + Rank(A.Remote.Type);
		const int32 Rb = Rank(B.Local.Type) + Rank(B.Remote.Type);
		return Ra < Rb;
	});
	return true;
}

namespace
{
const TCHAR* NsIceSdpTypeName(ENsIceType Type)
{
	if (Type == ENsIceType::Host)
	{
		return TEXT("host");
	}
	if (Type == ENsIceType::Srflx)
	{
		return TEXT("srflx");
	}
	if (Type == ENsIceType::Relay)
	{
		return TEXT("relay");
	}
	return nullptr;
}

uint32 NsIceSdpPriority(ENsIceType Type)
{
	uint32 Pref = 0;
	if (Type == ENsIceType::Host)
	{
		Pref = 126;
	}
	else if (Type == ENsIceType::Srflx)
	{
		Pref = 110;
	}
	return (Pref << 24) | (65535u << 8) | 255u;
}

bool NsIceSdpParseType(const FString& Name, ENsIceType& OutType)
{
	if (Name == TEXT("host"))
	{
		OutType = ENsIceType::Host;
		return true;
	}
	if (Name == TEXT("srflx"))
	{
		OutType = ENsIceType::Srflx;
		return true;
	}
	if (Name == TEXT("relay"))
	{
		OutType = ENsIceType::Relay;
		return true;
	}
	return false;
}
}

bool NsIceSdpEncode(uint8 Slot, const TArray<FNsIceCandidate>& Cands, FString& Out)
{
	Out.Reset();
	if (Slot > 2 || Cands.Num() < 1 || Cands.Num() > NsIceMaxCandidates)
	{
		return false;
	}
	for (const FNsIceCandidate& Cand : Cands)
	{
		if (!NsIceSdpTypeName(Cand.Type) || Cand.Port <= 0 || Cand.Port > 65535)
		{
			return false;
		}
	}
	Out = FString::Printf(
		TEXT("v=0\r\no=- %u 0 IN IP4 0.0.0.0\r\ns=-\r\nt=0 0\r\nm=application 9 UDP ICE\r\nc=IN IP4 0.0.0.0\r\n"),
		static_cast<uint32>(Slot));
	for (int32 i = 0; i < Cands.Num(); ++i)
	{
		const FNsIceCandidate& Cand = Cands[i];
		Out += FString::Printf(
			TEXT("a=candidate:%d 1 UDP %u %s %d typ %s\r\n"),
			i + 1,
			NsIceSdpPriority(Cand.Type),
			*NsStunIpv4ToString(Cand.Ipv4),
			Cand.Port,
			NsIceSdpTypeName(Cand.Type));
	}
	return true;
}

bool NsIceSdpDecode(const FString& Text, uint8& OutSlot, TArray<FNsIceCandidate>& OutCands)
{
	OutSlot = 0;
	OutCands.Reset();
	FString Norm = Text;
	Norm.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
	Norm.ReplaceInline(TEXT("\r"), TEXT("\n"));
	TArray<FString> Lines;
	Norm.ParseIntoArray(Lines, TEXT("\n"), true);
	if (Lines.Num() < 1)
	{
		return false;
	}
	for (FString& Line : Lines)
	{
		Line.TrimStartAndEndInline();
	}
	if (Lines[0] != TEXT("v=0"))
	{
		return false;
	}
	bool bGotOrigin = false;
	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("o=")))
		{
			TArray<FString> Parts;
			Line.Mid(2).ParseIntoArray(Parts, TEXT(" "), true);
			if (bGotOrigin || Parts.Num() < 6 || Parts[0] != TEXT("-")
				|| Parts[1].Len() != 1 || Parts[1][0] < TEXT('0') || Parts[1][0] > TEXT('2'))
			{
				OutCands.Reset();
				OutSlot = 0;
				return false;
			}
			OutSlot = static_cast<uint8>(Parts[1][0] - TEXT('0'));
			bGotOrigin = true;
			continue;
		}
		if (!Line.StartsWith(TEXT("a=candidate:")))
		{
			continue;
		}
		TArray<FString> Parts;
		Line.Mid(12).ParseIntoArray(Parts, TEXT(" "), true);
		ENsIceType Type = ENsIceType::Host;
		uint32 Ipv4 = 0;
		const int32 Port = Parts.Num() >= 8 ? FCString::Atoi(*Parts[5]) : 0;
		if (Parts.Num() < 8 || Parts[0].IsEmpty() || Parts[1] != TEXT("1")
			|| !Parts[2].Equals(TEXT("UDP"), ESearchCase::IgnoreCase)
			|| Parts[6] != TEXT("typ") || !NsIceSdpParseType(Parts[7], Type)
			|| !NsStunParseIpv4(Parts[4], Ipv4) || Port <= 0 || Port > 65535
			|| OutCands.Num() >= NsIceMaxCandidates)
		{
			OutCands.Reset();
			OutSlot = 0;
			return false;
		}
		FNsIceCandidate Cand;
		Cand.Type = Type;
		Cand.Ipv4 = Ipv4;
		Cand.Port = Port;
		OutCands.Add(Cand);
	}
	if (!bGotOrigin || OutCands.Num() < 1)
	{
		OutCands.Reset();
		OutSlot = 0;
		return false;
	}
	return true;
}
