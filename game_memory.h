/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#ifndef GAME_MEMORY_H
#define GAME_MEMORY_H

#include "game_platform.h"

// NOTE(ivan): Memory block.
struct memory_partition_block {
	piece Piece;
	uptr Marker;

	memory_partition_block *PrevBlock;
};

// NOTE(ivan): Memory region/partition.
struct memory_partition {
	memory_partition_block *CurrentBlock;
	uptr MinBlockSize;

	ticket_mutex Mutex;
};

b32 InitializePartition(memory_partition *Partition, uptr InitSize, uptr MinBlockSize = 0);
void ResetPartition(memory_partition *Partition);

#define PushType(Partition, Type) (Type *)PushSize(Partition, sizeof(Type))
#define PushTypeArray(Partition, Type, Count) (Type *)PushSize(Partition, sizeof(Type) * Count)
void * PushSize(memory_partition *Partition, uptr Size);

#endif // #ifndef GAME_MEMORY_H
