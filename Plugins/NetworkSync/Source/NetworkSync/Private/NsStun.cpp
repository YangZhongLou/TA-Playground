// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsStun.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/PlatformTime.h"

namespace
{
constexpr uint16 NsStunBindRequest = 0x0001;
constexpr uint16 NsStunBindSuccess = 0x0101;
constexpr uint16 NsStunAttrMapped = 0x0001;
constexpr uint16 NsStunAttrXorMapped = 0x0020;
constexpr uint8 NsStunFamilyIpv4 = 0x01;

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

FString NsStunIpv4ToString(uint32 Ipv4Host)
{
	return FString::Printf(TEXT("%u.%u.%u.%u"),
		(Ipv4Host >> 24) & 0xffu,
		(Ipv4Host >> 16) & 0xffu,
		(Ipv4Host >> 8) & 0xffu,
		Ipv4Host & 0xffu);
}
