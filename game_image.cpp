/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#include "game_image.h"

// NOTE(ivan): BMP file header.
#pragma pack(push, 1)
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
#pragma pack(pop)

// NOTE(ivan): BMP file bitfields masks
#pragma pack(push, 1)
struct bmp_bitfields_masks {
	u32 RedMask;
	u32 GreenMask;
	u32 BlueMask;
};
#pragma pack(pop)

image
LoadImageBmp(const char *FileName, memory_heap *Heap) {
	Assert(FileName);
	Assert(Heap);

	// NOTE(ivan): WARNING - this loader is FAR from being universal. It supports only 32-bit A8R8G8B8 format,
	// compression with value 3 (bitfields encoding), and alpha channel must be present.
	// Without alpha channel no image will be shown (alpha channel will be always 0).
	// Tested with GIMP exporter.
	// This loader is written only for test purposes, it is unlikely will be used in a shipping product,
	// so there is no reason to make it complete BMP loader at all.

	// TODO(ivan): Detect somehow whether the alpha channel is present. Fail if not.
	
	image Result = {};

	GameTLState.LastError = ErrorCode_NoError;
	
	DEBUGPlatformOutf("Loading BMP: %s", FileName);

	piece FilePiece = PlatformReadEntireFile(FileName);
	if (FilePiece.Base) {
		piece At = FilePiece;
		bmp_header *Header = ConsumeType(&At, bmp_header);

		const u8 BmpSignature[] = {0x42, 0x4d}; // NOTE(ivan): "BM".
		if (Header->FileType == *(u16 *)BmpSignature) {
			if (Header->BitsPerPixel == 32) {
				if (Header->Compression == 3) {
					bmp_bitfields_masks *Masks = ConsumeType(&At, bmp_bitfields_masks);
					
					bit_scan_result RedShift = FindLeastSignificantBit(Masks->RedMask);
					bit_scan_result GreenShift = FindLeastSignificantBit(Masks->GreenMask);
					bit_scan_result BlueShift = FindLeastSignificantBit(Masks->BlueMask);
					bit_scan_result AlphaShift = FindLeastSignificantBit(~(Masks->RedMask | Masks->GreenMask | Masks->BlueMask));

					Assert(RedShift.IsFound);
					Assert(GreenShift.IsFound);
					Assert(BlueShift.IsFound);
					Assert(AlphaShift.IsFound);

					Result.Pixels = (u32 *)PushSize(Partition, Header->Width * Header->Height * (Header->BitsPerPixel / 8));
					if (Result.Pixels) {
						Result.Width = Header->Width;
						Result.Height = Header->Height;
						Result.BytesPerPixel = Header->BitsPerPixel / 8;
						Result.Pitch = Header->Width * (Header->BitsPerPixel / 8);

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
					GameTLState.LastError = ErrorCode_BMPLoader_CompressionNot3;
				}
			} else {
				// NOTE(ivan): Only 32-bit images are supported.
				GameTLState.LastError = ErrorCode_BMPLoader_BitnessNot32;
			}
		} else {
			// NOTE(ivan): File is not BMP.
			GameTLState.LastError = ErrorCode_WrongSignature;
		}
		
		PlatformFreeEntireFilePiece(&FilePiece);
	} else {
		// NOTE(ivan): Not found.
		// NOTE(ivan): LastError is already set to GameError_NotFound.
	}

	return Result;
}
