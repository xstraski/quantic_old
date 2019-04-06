/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#include "game_image.h"

// NOTE(ivan): BMP file header.
struct bmp_header {
	u16 FileType;
	u32 FileSize;
	u16 Reserved1;
	u16 Reserved2;
	u32 BitmapOffset;
	u32 Size;
	s32 Width;
	s32 Height;
	u16 Planes;
	u16 BitsPerPixel;
	u32 Compression;
	u32 SizeOfBitmap;
	s32 HorzResolution;
	s32 VertResolution;
	u32 ColorsUsed;
	u32 ColorsImportant;
};

// NOTE(ivan): BMP file bitfields masks
struct bmp_bitfields_masks {
	u32 RedMask;
	u32 GreenMask;
	u32 BlueMask;
};

image
LoadImageBMP(const char *FileName, memory_partition *Partition) {
	Assert(FileName);
	Assert(Partition);
	
	image Result = {};

	piece FilePiece = PlatformReadEntireFile(FileName);
	if (FilePiece.Base) {
		piece At = FilePiece;
		bmp_header *Header = ConsumeType(&At, bmp_header);
		if (Header->BitsPerPixel == 32) {
			if (Header->Compression == 3) {
				bmp_bitfields_masks *Masks = ConsumeType(&At, bmp_bitfields_masks);
				u32 AlphaMask = ~(Masks->RedMask | Masks->GreenMask | Masks->BlueMask);

				bit_scan_result RedShift = FindLeastSignificantBit(Masks->RedMask);
				bit_scan_result GreenShift = FindLeastSignificantBit(Masks->GreenMask);
				bit_scan_result BlueShift = FindLeastSignificantBit(Masks->BlueMask);
				bit_scan_result AlphaShift = FindLeastSignificantBit(AlphaMask);

				Assert(RedShift.IsFound);
				Assert(GreenShift.IsFound);
				Assert(BlueShift.IsFound);
				Assert(AlphaShift.IsFound);

				Result.Pixels = (u32 *)PushSize(Partition, Header->Width * Header->Height * 32);
				if (Result.Pixels) {
					Result.Width = Header->Width;
					Result.Height = Header->Height;
					Result.BytesPerPixel = 32;
					Result.Pitch = Header->Width * 32;

					u8 *DstRow = (u8 *)Result.Pixels;
					u8 *SrcRow = FilePiece.Base + Header->BitmapOffset;
					for (s32 Y = 0; Y < Header->Height; Y++) {
						u32 *DstPixel = (u32 *)DstRow;
						u32 *SrcPixel = (u32 *)SrcRow;
						for (s32 X = 0; X < Header->Width; X++) {
							u32 C = (u32)((((*SrcPixel >> AlphaShift.Index) & 0xFF) << 24) |
										  (((*SrcPixel >> RedShift.Index) & 0xFF) << 16) |
										  (((*SrcPixel >> GreenShift.Index) & 0xFF) << 8) |
										  (((*SrcPixel >> BlueShift.Index) & 0xFF) << 0));
							SrcPixel++;
							*DstPixel++ = C;
						}

						DstRow += Result.Pitch;
						SrcRow += Result.Pitch;
					}
				}
				
			} else {
				// NOTE(ivan): Compression other than bitfields encoding is not supported.
				// TODO(ivan): Diagnostics (Header->Compression == 0).
			}
		} else {
			// NOTE(ivan): Only 32-bit images are supported.
			// TODO(ivan): Diagnostics (Header->BitsPerPixel == 32).
		}
		
		PlatformFreeEntireFilePiece(&FilePiece);
	}

	return Result;
}
