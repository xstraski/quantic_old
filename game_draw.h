/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#ifndef GAME_DRAW_H
#define GAME_DRAW_H

#include "game_math.h"
#include "game_image.h"

void DrawPixel(game_video_buffer *Buffer, v2 Pos, v4 Color);
void DrawImage(game_video_buffer *Buffer, image *Image, v2 Pos);

#endif // #ifndef GAME_DRAW_H
