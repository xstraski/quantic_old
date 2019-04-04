/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#include "game_memory.h"

b32
InitializePartition(memory_partition *Partition, uptr InitSize, uptr MinBlockSize) {
	Assert(Partition);
	Assert(InitSize);
	Assert(!Partition->Piece.Base); // NOTE(ivan): Must be zero-initialized, and must not be called for an already initialized partition.

	b32 Result = false;

	if (InitSize < GameState.Hunk.Size) {
		EnterTicketMutex(&GameState.HunkMutex);		
		Partition->Piece.Base = (u8 *)ConsumeSize(&GameState.Hunk, InitSize);
		LeaveTicketMutex(&GameState.HunkMutex);
		
		Partition->Piece.Size = InitSize;
		Partition->Marker = 0;
		Partition->MinBlockSize = MinBlockSize;

		Partition->NextPiece = 0;

		// NOTE(ivan): Memory partition might be reseted before, so it should be zero-cleaned,
		// because we always allocate new memory being zero-initialized.
		uptr At = Partition->Piece.Size;
		while (At--)
			Partition->Piece.Base[At] = 0;
		
		Result = true;
	}

	return Result;
}

void
ResetPartition(memory_partition *Partition) {
	Assert(Partition);

	EnterTicketMutex(&Partition->Mutex);

	Partition->Marker = 0;
	for (memory_partition *Piece = Partition->NextPiece; Piece; Piece = Piece->NextPiece) {
		EnterTicketMutex(&Piece->Mutex);
		Piece->Marker = 0;
		LeaveTicketMutex(&Piece->Mutex);
	}

	LeaveTicketMutex(&Partition->Mutex);
}

void *
PushSize(memory_partition *Partition, uptr Size) {
	Assert(Partition);
	Assert(Size);

	void *Result = 0;

	EnterTicketMutex(&Partition->Mutex);
	
	// NOTE(ivan): Check whether a current partition piece can handle the size.
	if (Size < (Partition->Piece.Size - Partition->Marker)) {
		Result = (u8 *)((uptr)Partition->Piece.Base + Partition->Marker);
		Partition->Marker += Size;
	}

	LeaveTicketMutex(&Partition->Mutex);
	
	return Result;
}
