// Copyright (c) 2026 TA-Playground. All Rights Reserved.

#include "NsLockstepDoor.h"
#include "NsPump.h"

void FNsLockstepDoorServer::BroadcastDoorOpen(INsNet& Net) const
{
	FNsPacket Pkt;
	Pkt.Type = ENsMsg::S2CDoorOpen;
	Pkt.DoorOpen = Door.Open;
	Net.Send(ENsAddr::Sv, ENsAddr::C0, Pkt);
	Net.Send(ENsAddr::Sv, ENsAddr::C1, Pkt);
}

void FNsLockstepDoorServer::SetOpen(INsNet& Net, int32 Open)
{
	Door.Open = Open;
	BroadcastDoorOpen(Net);
}

void NsPumpLockstepDoorServer(INsNet& Net, FNsLockstepDoorServer& Sv, bool bWait)
{
	NsPumpLockstepServer(Net, Sv.Ls, bWait);
	Sv.BroadcastDoorOpen(Net);
}

void NsPumpLockstepDoorClient(INsNet& Net, FNsLockstepDoorClient& C, bool bWait)
{
	TArray<FNsPacket> ToC;
	NsDrain(Net, C.Ls.Addr, ToC, bWait);
	for (const FNsPacket& P : ToC)
	{
		if (P.Type == ENsMsg::S2CJoinSnap)
		{
			C.Ls.ApplyJoin(P);
		}
		else if (P.Type == ENsMsg::S2CFrame)
		{
			C.Ls.OnS2C(P.Frames);
		}
		else if (P.Type == ENsMsg::S2CDoorOpen)
		{
			C.Door.Open = P.DoorOpen;
		}
	}
	C.Ls.Logic(Net);
}
