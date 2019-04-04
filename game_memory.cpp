/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#include "game_memory.h"

static memory_partition_block *
AllocPartitionBlock(memory_partition_block *PrevBlock, uptr InitSize) {
	Assert(InitSize);
	
	memory_partition_block *Result = 0;

	if (sizeof(memory_partition_block) < GameState.Hunk.Size) {
		Result = ConsumeType(&GameState.Hunk, memory_partition_block);

		if (InitSize < GameState.Hunk.Size) {
			Result->Piece.Base = (u8 *)ConsumeSize(&GameState.Hunk, InitSize);
			Result->Piece.Size = InitSize;

			Result->PrevBlock = PrevBlock;
		}
	}

	return Result;
}

b32
InitializePartition(memory_partition *Partition, uptr InitSize, uptr MinBlockSize) {
	Assert(Partition);
	Assert(InitSize);

	b32 Result = false;

	EnterTicketMutex(&Partition->Mutex);
	
	// NOTE(ivan): A given partition is required to be zero-initialized, and this function
	// must not be called for a partition that is already initialized.
	Assert(!Partition->CurrentBlock);
	
	Partition->CurrentBlock = AllocPartitionBlock(Partition->CurrentBlock, InitSize < MinBlockSize ? MinBlockSize : InitSize);
	if (Partition->CurrentBlock) {
		if (!MinBlockSize)
			MinBlockSize = AlignPow2(InitSize, (uptr)4);
		Partition->MinBlockSize = MinBlockSize;
		Result = true;
	}

	LeaveTicketMutex(&Partition->Mutex);
	return Result;
}

void
ResetPartition(memory_partition *Partition) {
	Assert(Partition);

	EnterTicketMutex(&Partition->Mutex);

	for (memory_partition_block *Block = Partition->CurrentBlock; Block; Block = Block->PrevBlock)
		Block->Marker = 0;

	LeaveTicketMutex(&Partition->Mutex);
}

void *
PushSize(memory_partition *Partition, uptr Size) {
	Assert(Partition);
	Assert(Size);

	void *Result = 0;

	EnterTicketMutex(&Partition->Mutex);

	// NOTE(ivan): Search for a block that can handle the size.
	memory_partition_block *Block;
	for (Block = Partition->CurrentBlock; Block; Block = Block->PrevBlock) {
		if (Size < (Block->Piece.Size - Block->Marker))
			break;
	}

	// NOTE(ivan): No block can handle - allocate a new one.
	if (!Block)
		Block = AllocPartitionBlock(Partition->CurrentBlock, Size > Partition->MinBlockSize ? AlignPow2(Size, (uptr)4) : Partition->MinBlockSize);
	
	if (Block) {
		Result = (void *)(Block->Piece.Base + Block->Marker);
		Block->Marker += Size;
	}

	LeaveTicketMutex(&Partition->Mutex);
			
	return Result;
}
