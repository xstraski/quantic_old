/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#include "game_draw.h"

// NOTE(ivan): Blending resulting color.
struct blending_result {
	u8 R;
	u8 G;
	u8 B;
};

inline blending_result
DoLinearBlending(f32 AlphaChannel,
				 u8 DstR, u8 DstG, u8 DstB,
				 u8 SrcR, u8 SrcG, u8 SrcB) {
	blending_result Result;

	Result.R = (u8)roundf((1.0f - AlphaChannel) * DstR + AlphaChannel * SrcR);
	Result.G = (u8)roundf((1.0f - AlphaChannel) * DstG + AlphaChannel * SrcG);
	Result.B = (u8)roundf((1.0f - AlphaChannel) * DstB + AlphaChannel * SrcB);

	return Result;
}

void
DrawPixel(game_video_buffer *Buffer, v2 Pos, v4 Color) {
	Assert(Buffer);

	s32 PosX = (s32)roundf(Pos.X);
	s32 PosY = (s32)roundf(Pos.Y);

	u8 ColorR = (u8)roundf(Color.R);
	u8 ColorG = (u8)roundf(Color.G);
	u8 ColorB = (u8)roundf(Color.B);
	u8 ColorA = (u8)roundf(Color.A);
	u32 Color32 = ((ColorA << 24) |
				   (ColorR << 16) |
				   (ColorG << 8) |
				   (ColorB << 0));

	if (PosX < 0)
		return;
	if (PosX >= Buffer->Width)
		return;
	if (PosY < 0)
		return;
	if (PosY >= Buffer->Height)
		return;

	u8 *DstRow = ((u8 *)Buffer->Pixels + (PosY * Buffer->Pitch));
	u32 *DstPixel = (u32 *)(DstRow + (PosX * Buffer->BytesPerPixel));

	u32 SrcC = Color32;
	u32 DstC = *DstPixel;

	// TODO(ivan): Premultiplied alpha!!!
	blending_result BlendingResult = DoLinearBlending(((SrcC >> 24) & 0xFF) / 255.0f,
													  (u8)((DstC >> 16) & 0xFF),
													  (u8)((DstC >> 8) & 0xFF),
													  (u8)((DstC >> 0) & 0xFF),
													  (u8)((SrcC >> 16) & 0xFF),
													  (u8)((SrcC >> 8) & 0xFF),
													  (u8)((SrcC >> 0) & 0xFF));
	*DstPixel =(((u32)BlendingResult.R << 16) |
				((u32)BlendingResult.G << 8) |
				((u32)BlendingResult.B << 0));
}

void
DrawImage(game_video_buffer *Buffer, image *Image, v2 Pos) {
	Assert(Buffer);
	Assert(Image);

	point Pos1;
	point Pos2;

	Pos1.X = (s32)roundf(Pos.X);
	Pos1.Y = (s32)roundf(Pos.Y);

	Pos2.X = Pos1.X + Image->Width;
	Pos2.Y = Pos1.Y + Image->Height;

	for (s32 Y = Pos1.Y; Y < Pos2.Y; Y++) {
		if (Y < 0)
			continue;
		if (Y >= Buffer->Height)
			continue;

		u8 *DstRow = ((u8 *)Buffer->Pixels + (Y * Buffer->Pitch));
		u8 *SrcRow = (u8 *)Image->Pixels + ((Y - Pos1.Y) * Image->Pitch);

		for (s32 X = Pos1.X; X < Pos2.X; X++) {
			if (X < 0)
				continue;
			if (X >= Buffer->Width)
				continue;

			u32 *DstPixel = (u32 *)(DstRow + (X * Buffer->BytesPerPixel));
			u32 *SrcPixel = (u32 *)(SrcRow + ((X - Pos1.X) * Image->BytesPerPixel));

			u32 DstC = *DstPixel;
			u32 SrcC = *SrcPixel;

			// TODO(ivan): Premultiplied alpha!
			blending_result BlendingResult = DoLinearBlending(((SrcC >> 24) & 0xFF) / 255.0f,
															  (u8)((DstC >> 16) & 0xFF),
															  (u8)((DstC >> 8)  & 0xFF),
															  (u8)((DstC >> 0)  & 0xFF),
															  (u8)((SrcC >> 16) & 0xFF),
															  (u8)((SrcC >> 8)  & 0xFF),
															  (u8)((SrcC >> 0)  & 0xFF));
			*DstPixel = (((u32)BlendingResult.R << 16) |
						 ((u32)BlendingResult.G << 8) |
						 ((u32)BlendingResult.B << 0));
		}
	}
}
