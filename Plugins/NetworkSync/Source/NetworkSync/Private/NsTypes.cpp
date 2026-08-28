// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsTypes.h"

void FNsWorld::Reset()
{
	X[0] = 0;
	X[1] = 0;
	Rng = 1;
}

void FNsWorld::Step(const int8 Dxs[Ns::PlayerCount], int32 Speed)
{
	for (int32 i = 0; i < Ns::PlayerCount; ++i)
	{
		X[i] += static_cast<int32>(NsClampDx(Dxs[i])) * Speed;
	}
	Rng = (Rng * 1103515245u + 12345u) & 0x7FFFFFFFu;
}

uint32 FNsWorld::Checksum() const
{
	uint32 H = Rng;
	for (int32 i = 0; i < Ns::PlayerCount; ++i)
	{
		const uint32 Term = static_cast<uint32>(static_cast<int64>(X[i]) * 0x45D9F3B);
		H ^= Term;
		H *= 0x45D9F3Bu;
	}
	return H;
}

bool FNsWorld::Equals(const FNsWorld& Other) const
{
	return X[0] == Other.X[0] && X[1] == Other.X[1] && Rng == Other.Rng;
}
