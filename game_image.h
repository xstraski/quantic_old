/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#ifndef GAME_IMAGE_H
#define GAME_IMAGE_H

#include "game_memory.h"

// NOTE(ivan): Image container.
struct image {
	u32 *Pixels; // NOTE(ivan): Format - 0xAARRGGBB.
	s32 Width;
	s32 Height;
	s32 BytesPerPixel;
	s32 Pitch;
};

image LoadImageBMP(const char *FileName, memory_partition *Partition);

#endif // #ifndef GAME_IMAGE_H
