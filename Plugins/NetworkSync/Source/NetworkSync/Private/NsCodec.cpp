// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsCodec.h"

namespace
{
	void WriteU8(TArray<uint8>& Out, uint8 V)
	{
		Out.Add(V);
	}

	void WriteU16(TArray<uint8>& Out, uint16 V)
	{
		Out.Add(static_cast<uint8>(V));
		Out.Add(static_cast<uint8>(V >> 8));
	}

	void WriteU32(TArray<uint8>& Out, uint32 V)
	{
		Out.Add(static_cast<uint8>(V));
		Out.Add(static_cast<uint8>(V >> 8));
		Out.Add(static_cast<uint8>(V >> 16));
		Out.Add(static_cast<uint8>(V >> 24));
	}

	void WriteI8(TArray<uint8>& Out, int8 V)
	{
		WriteU8(Out, static_cast<uint8>(V));
	}

	void WriteI32(TArray<uint8>& Out, int32 V)
	{
		WriteU32(Out, static_cast<uint32>(V));
	}

	struct FNsReader
	{
		const uint8* Data = nullptr;
		int32 Len = 0;
		int32 Off = 0;
		bool bOk = true;

		uint8 U8()
		{
			if (Off >= Len)
			{
				bOk = false;
				return 0;
			}
			return Data[Off++];
		}

		uint16 U16()
		{
			const uint16 A = U8();
			const uint16 B = U8();
			return static_cast<uint16>(A | (B << 8));
		}

		uint32 U32()
		{
			const uint32 A = U8();
			const uint32 B = U8();
			const uint32 C = U8();
			const uint32 D = U8();
			return A | (B << 8) | (C << 16) | (D << 24);
		}

		int8 I8()
		{
			return static_cast<int8>(U8());
		}

		int32 I32()
		{
			return static_cast<int32>(U32());
		}
	};

	void SortedIntKeys(const TMap<int32, FNsInputs>& Map, TArray<int32>& Out)
	{
		Out.Reset();
		for (const TPair<int32, FNsInputs>& Kv : Map)
		{
			Out.Add(Kv.Key);
		}
		Out.Sort();
	}

	void SortedIntKeys(const TMap<int32, int8>& Map, TArray<int32>& Out)
	{
		Out.Reset();
		for (const TPair<int32, int8>& Kv : Map)
		{
			Out.Add(Kv.Key);
		}
		Out.Sort();
	}

	bool NsRsvCarriesTurnFpt(uint8 Rsv)
	{
		const int32 Hi = static_cast<int32>(Rsv >> 4);
		const int32 Lo = static_cast<int32>(Rsv & 0xF);
		if (NsIsTurnFpt(Hi) && NsIsTurnFpt(Lo))
		{
			return true;
		}
		return NsIsTurnFpt(static_cast<int32>(Rsv));
	}

	bool WritePayload(const FNsPacket& Packet, TArray<uint8>& Out)
	{
		switch (Packet.Type)
		{
		case ENsMsg::C2SInput:
		{
			const int32 Win = FMath::Min(Packet.SeqWindow.Num(), Packet.DxWindow.Num());
			if (Win > 255)
			{
				return false;
			}
			WriteU8(Out, static_cast<uint8>(Packet.PlayerId));
			WriteI8(Out, Packet.Dx);
			WriteU8(Out, static_cast<uint8>(Win));
			for (int32 i = 0; i < Win; ++i)
			{
				WriteU32(Out, static_cast<uint32>(Packet.SeqWindow[i]));
				WriteI8(Out, Packet.DxWindow[i]);
			}
			return true;
		}
		case ENsMsg::S2CFrame:
		{
			TArray<int32> Keys;
			SortedIntKeys(Packet.Frames, Keys);
			if (Keys.Num() > 255)
			{
				return false;
			}
			const int32 Latest = (Keys.Num() > 0) ? Keys.Last() : 0;
			WriteU32(Out, static_cast<uint32>(Latest));
			WriteU8(Out, static_cast<uint8>(Keys.Num()));
			for (int32 K : Keys)
			{
				const FNsInputs* Found = Packet.Frames.Find(K);
				if (!Found)
				{
					return false;
				}
				WriteU32(Out, static_cast<uint32>(K));
				WriteI8(Out, Found->Dx[0]);
				WriteI8(Out, Found->Dx[1]);
				if (NsIsTurnFpt(Packet.Tick))
				{
					int32 Len = 0;
					if (const int32* FoundLen = Packet.TurnFpt.Find(K))
					{
						Len = *FoundLen;
					}
					WriteU8(Out, static_cast<uint8>(NsIsTurnFpt(Len) ? Len : 0));
				}
			}
			return true;
		}
		case ENsMsg::S2CSnapshot:
		{
			WriteU32(Out, static_cast<uint32>(Packet.Tick));
			WriteU32(Out, static_cast<uint32>(Packet.BaseTick));
			WriteU8(Out, static_cast<uint8>(Ns::PlayerCount));
			for (int32 i = 0; i < Ns::PlayerCount; ++i)
			{
				WriteI32(Out, Packet.SnapX[i]);
				WriteU32(Out, static_cast<uint32>(Packet.SnapSeq[i]));
			}
			return true;
		}
		case ENsMsg::C2SSnapAck:
		case ENsMsg::C2SFire:
			WriteU8(Out, static_cast<uint8>(Packet.PlayerId));
			WriteU32(Out, static_cast<uint32>(Packet.Tick));
			return true;
		case ENsMsg::P2PInput:
		{
			TArray<int32> Keys;
			SortedIntKeys(Packet.RemoteDx, Keys);
			if (Keys.Num() > 255)
			{
				return false;
			}
			WriteU8(Out, static_cast<uint8>(Keys.Num()));
			for (int32 K : Keys)
			{
				const int8* Found = Packet.RemoteDx.Find(K);
				if (!Found)
				{
					return false;
				}
				WriteU32(Out, static_cast<uint32>(K));
				WriteI8(Out, *Found);
			}
			return true;
		}
		case ENsMsg::C2SChecksum:
			WriteU8(Out, static_cast<uint8>(Packet.PlayerId));
			WriteU32(Out, static_cast<uint32>(Packet.Tick));
			WriteU32(Out, Packet.Hash);
			return true;
		case ENsMsg::S2CJoinSnap:
		{
			TArray<int32> Keys;
			SortedIntKeys(Packet.Frames, Keys);
			if (Keys.Num() > 255)
			{
				return false;
			}
			WriteU32(Out, static_cast<uint32>(Packet.Tick));
			WriteI32(Out, Packet.SnapX[0]);
			WriteI32(Out, Packet.SnapX[1]);
			WriteU32(Out, Packet.SnapRng);
			WriteU8(Out, static_cast<uint8>(Keys.Num()));
			for (int32 K : Keys)
			{
				const FNsInputs* Found = Packet.Frames.Find(K);
				if (!Found)
				{
					return false;
				}
				WriteU32(Out, static_cast<uint32>(K));
				WriteI8(Out, Found->Dx[0]);
				WriteI8(Out, Found->Dx[1]);
			}
			return true;
		}
		case ENsMsg::S2CDoorOpen:
			WriteI32(Out, Packet.DoorOpen);
			return true;
		case ENsMsg::C2SFrameNack:
		{
			if (Packet.SeqWindow.Num() > 255)
			{
				return false;
			}
			WriteU8(Out, static_cast<uint8>(Packet.PlayerId));
			WriteU8(Out, static_cast<uint8>(Packet.SeqWindow.Num()));
			for (int32 F : Packet.SeqWindow)
			{
				WriteU32(Out, static_cast<uint32>(F));
			}
			return true;
		}
		default:
			return false;
		}
	}

	bool ReadPayload(ENsMsg Type, FNsReader& R, FNsPacket& Out, bool bTurnFpt)
	{
		switch (Type)
		{
		case ENsMsg::C2SInput:
		{
			Out.PlayerId = static_cast<int32>(R.U8());
			Out.Dx = R.I8();
			const int32 Win = R.U8();
			Out.SeqWindow.Reset();
			Out.DxWindow.Reset();
			for (int32 i = 0; i < Win; ++i)
			{
				Out.SeqWindow.Add(static_cast<int32>(R.U32()));
				Out.DxWindow.Add(R.I8());
			}
			return R.bOk;
		}
		case ENsMsg::S2CFrame:
		{
			(void)R.U32();
			const int32 Count = R.U8();
			Out.Frames.Reset();
			Out.TurnFpt.Reset();
			for (int32 i = 0; i < Count; ++i)
			{
				const int32 Frame = static_cast<int32>(R.U32());
				FNsInputs In;
				In.Dx[0] = R.I8();
				In.Dx[1] = R.I8();
				Out.Frames.Add(Frame, In);
				if (bTurnFpt)
				{
					const int32 Len = static_cast<int32>(R.U8());
					if (NsIsTurnFpt(Len))
					{
						Out.TurnFpt.Add(Frame, Len);
					}
				}
			}
			return R.bOk;
		}
		case ENsMsg::S2CSnapshot:
		{
			Out.Tick = static_cast<int32>(R.U32());
			Out.BaseTick = static_cast<int32>(R.U32());
			const int32 Count = R.U8();
			if (Count != Ns::PlayerCount)
			{
				return false;
			}
			for (int32 i = 0; i < Ns::PlayerCount; ++i)
			{
				Out.SnapX[i] = R.I32();
				Out.SnapSeq[i] = static_cast<int32>(R.U32());
			}
			return R.bOk;
		}
		case ENsMsg::C2SSnapAck:
		case ENsMsg::C2SFire:
			Out.PlayerId = static_cast<int32>(R.U8());
			Out.Tick = static_cast<int32>(R.U32());
			return R.bOk;
		case ENsMsg::P2PInput:
		{
			const int32 Count = R.U8();
			Out.RemoteDx.Reset();
			for (int32 i = 0; i < Count; ++i)
			{
				const int32 Frame = static_cast<int32>(R.U32());
				Out.RemoteDx.Add(Frame, R.I8());
			}
			return R.bOk;
		}
		case ENsMsg::C2SChecksum:
			Out.PlayerId = static_cast<int32>(R.U8());
			Out.Tick = static_cast<int32>(R.U32());
			Out.Hash = R.U32();
			return R.bOk;
		case ENsMsg::S2CJoinSnap:
		{
			Out.Tick = static_cast<int32>(R.U32());
			Out.SnapX[0] = R.I32();
			Out.SnapX[1] = R.I32();
			Out.SnapRng = R.U32();
			const int32 Count = R.U8();
			Out.Frames.Reset();
			for (int32 i = 0; i < Count; ++i)
			{
				const int32 Frame = static_cast<int32>(R.U32());
				FNsInputs In;
				In.Dx[0] = R.I8();
				In.Dx[1] = R.I8();
				Out.Frames.Add(Frame, In);
			}
			return R.bOk;
		}
		case ENsMsg::S2CDoorOpen:
			Out.DoorOpen = R.I32();
			return R.bOk;
		case ENsMsg::C2SFrameNack:
		{
			Out.PlayerId = static_cast<int32>(R.U8());
			const int32 Count = R.U8();
			Out.SeqWindow.Reset();
			for (int32 i = 0; i < Count; ++i)
			{
				Out.SeqWindow.Add(static_cast<int32>(R.U32()));
			}
			return R.bOk;
		}
		default:
			return false;
		}
	}
}

bool NsEncodePacket(const FNsPacket& Packet, TArray<uint8>& OutBytes)
{
	TArray<uint8> Payload;
	if (!WritePayload(Packet, Payload))
	{
		return false;
	}
	if (Payload.Num() > 0xFFFF)
	{
		return false;
	}
	if (Ns::HeaderBytes + Payload.Num() > Ns::MaxPacketBytes)
	{
		return false;
	}
	OutBytes.Reset();
	WriteU32(OutBytes, Ns::PacketMagic);
	WriteU8(OutBytes, static_cast<uint8>(Packet.Type));
	uint8 Rsv = 0;
	if (Packet.Type == ENsMsg::S2CFrame && NsIsTurnFpt(Packet.Tick))
	{
		if (NsIsTurnFpt(Packet.BaseTick))
		{
			Rsv = static_cast<uint8>((Packet.BaseTick << 4) | Packet.Tick);
		}
		else
		{
			Rsv = static_cast<uint8>(Packet.Tick);
		}
	}
	WriteU8(OutBytes, Rsv);
	WriteU16(OutBytes, static_cast<uint16>(Payload.Num()));
	WriteU32(OutBytes, Packet.Session);
	WriteU32(OutBytes, static_cast<uint32>(Packet.Seq));
	WriteU32(OutBytes, static_cast<uint32>(Packet.Ack));
	WriteU32(OutBytes, Packet.AckBits);
	OutBytes.Append(Payload);
	return true;
}

bool NsDecodePacket(const TArray<uint8>& Bytes, FNsPacket& OutPacket)
{
	if (Bytes.Num() < Ns::HeaderBytes)
	{
		return false;
	}
	FNsReader R;
	R.Data = Bytes.GetData();
	R.Len = Bytes.Num();
	const uint32 Magic = R.U32();
	if (Magic != Ns::PacketMagic)
	{
		return false;
	}
	const uint8 TypeRaw = R.U8();
	const uint8 Rsv = R.U8();
	const uint16 PayloadLen = R.U16();
	const uint32 Session = R.U32();
	const int32 Seq = static_cast<int32>(R.U32());
	const int32 Ack = static_cast<int32>(R.U32());
	const uint32 AckBits = R.U32();
	if (!R.bOk)
	{
		return false;
	}
	if (Bytes.Num() != Ns::HeaderBytes + static_cast<int32>(PayloadLen))
	{
		return false;
	}
	const ENsMsg Type = static_cast<ENsMsg>(TypeRaw);
	FNsPacket Decoded;
	Decoded.Type = Type;
	Decoded.Session = Session;
	Decoded.Seq = Seq;
	Decoded.Ack = Ack;
	Decoded.AckBits = AckBits;
	if (!ReadPayload(Type, R, Decoded, Type == ENsMsg::S2CFrame && NsRsvCarriesTurnFpt(Rsv)) || !R.bOk)
	{
		return false;
	}
	if (Type == ENsMsg::S2CFrame)
	{
		const int32 Hi = static_cast<int32>(Rsv >> 4);
		const int32 Lo = static_cast<int32>(Rsv & 0xF);
		if (NsIsTurnFpt(Hi) && NsIsTurnFpt(Lo))
		{
			Decoded.BaseTick = Hi;
			Decoded.Tick = Lo;
		}
		else if (NsIsTurnFpt(static_cast<int32>(Rsv)))
		{
			Decoded.Tick = static_cast<int32>(Rsv);
		}
	}
	if (R.Off != Bytes.Num())
	{
		return false;
	}
	OutPacket = MoveTemp(Decoded);
	return true;
}

int32 NsPayloadBytes(const FNsPacket& Packet)
{
	switch (Packet.Type)
	{
	case ENsMsg::C2SInput:
	{
		const int32 Win = FMath::Min(Packet.SeqWindow.Num(), Packet.DxWindow.Num());
		if (Win > 255)
		{
			return -1;
		}
		return 3 + 5 * Win;
	}
	case ENsMsg::S2CFrame:
		if (Packet.Frames.Num() > 255)
		{
			return -1;
		}
		return 5 + (NsIsTurnFpt(Packet.Tick) ? 7 : 6) * Packet.Frames.Num();
	case ENsMsg::S2CSnapshot:
		return 21;
	case ENsMsg::C2SSnapAck:
	case ENsMsg::C2SFire:
		return 5;
	case ENsMsg::P2PInput:
		if (Packet.RemoteDx.Num() > 255)
		{
			return -1;
		}
		return 1 + 5 * Packet.RemoteDx.Num();
	case ENsMsg::C2SChecksum:
		return 9;
	case ENsMsg::S2CJoinSnap:
		if (Packet.Frames.Num() > 255)
		{
			return -1;
		}
		return 17 + 6 * Packet.Frames.Num();
	case ENsMsg::S2CDoorOpen:
		return 4;
	case ENsMsg::C2SFrameNack:
		if (Packet.SeqWindow.Num() > 255)
		{
			return -1;
		}
		return 2 + 4 * Packet.SeqWindow.Num();
	default:
		return -1;
	}
}

int32 NsWireBytes(const FNsPacket& Packet)
{
	const int32 Payload = NsPayloadBytes(Packet);
	if (Payload < 0)
	{
		return -1;
	}
	return Ns::HeaderBytes + Payload;
}

bool NsFitsMtu(const FNsPacket& Packet)
{
	const int32 Wire = NsWireBytes(Packet);
	return Wire > 0 && Wire <= Ns::MaxPacketBytes;
}

int32 NsMaxEntriesPerPacket(ENsMsg Type)
{
	switch (Type)
	{
	case ENsMsg::C2SInput:
		return Ns::MaxC2SInputEntries;
	case ENsMsg::S2CFrame:
		return Ns::MaxS2CFrameEntries;
	case ENsMsg::P2PInput:
		return Ns::MaxP2PInputEntries;
	case ENsMsg::S2CJoinSnap:
		return Ns::MaxJoinSnapEntries;
	default:
		return 0;
	}
}

void NsSplitForMtu(const FNsPacket& Packet, TArray<FNsPacket>& Out)
{
	Out.Reset();
	if (NsFitsMtu(Packet))
	{
		Out.Add(Packet);
		return;
	}

	const int32 Cap = NsMaxEntriesPerPacket(Packet.Type);
	if (Cap <= 0)
	{
		return;
	}

	if (Packet.Type == ENsMsg::C2SInput)
	{
		const int32 Win = FMath::Min(Packet.SeqWindow.Num(), Packet.DxWindow.Num());
		for (int32 i = 0; i < Win; )
		{
			FNsPacket Part = Packet;
			Part.SeqWindow.Reset();
			Part.DxWindow.Reset();
			const int32 Take = FMath::Min(Cap, Win - i);
			for (int32 k = 0; k < Take; ++k)
			{
				Part.SeqWindow.Add(Packet.SeqWindow[i + k]);
				Part.DxWindow.Add(Packet.DxWindow[i + k]);
			}
			i += Take;
			if (!NsFitsMtu(Part))
			{
				Out.Reset();
				return;
			}
			Out.Add(Part);
		}
		return;
	}

	if (Packet.Type == ENsMsg::P2PInput)
	{
		TArray<int32> Keys;
		SortedIntKeys(Packet.RemoteDx, Keys);
		for (int32 i = 0; i < Keys.Num(); )
		{
			FNsPacket Part = Packet;
			Part.RemoteDx.Reset();
			const int32 Take = FMath::Min(Cap, Keys.Num() - i);
			for (int32 k = 0; k < Take; ++k)
			{
				const int32 Key = Keys[i + k];
				if (const int8* Found = Packet.RemoteDx.Find(Key))
				{
					Part.RemoteDx.Add(Key, *Found);
				}
			}
			i += Take;
			if (!NsFitsMtu(Part))
			{
				Out.Reset();
				return;
			}
			Out.Add(Part);
		}
		return;
	}

	TArray<int32> Keys;
	SortedIntKeys(Packet.Frames, Keys);
	int32 FrameCap = Cap;
	if (Packet.Type == ENsMsg::S2CFrame && NsIsTurnFpt(Packet.Tick))
	{
		FrameCap = Ns::MaxS2CTurnFrameEntries;
	}
	for (int32 i = 0; i < Keys.Num(); )
	{
		FNsPacket Part = Packet;
		Part.Frames.Reset();
		Part.TurnFpt.Reset();
		const int32 Take = FMath::Min(FrameCap, Keys.Num() - i);
		for (int32 k = 0; k < Take; ++k)
		{
			const int32 Key = Keys[i + k];
			if (const FNsInputs* Found = Packet.Frames.Find(Key))
			{
				Part.Frames.Add(Key, *Found);
			}
			if (const int32* FoundLen = Packet.TurnFpt.Find(Key))
			{
				Part.TurnFpt.Add(Key, *FoundLen);
			}
		}
		i += Take;
		if (!NsFitsMtu(Part))
		{
			Out.Reset();
			return;
		}
		Out.Add(Part);
	}
}
