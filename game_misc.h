/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#ifndef GAME_MISC_H
#define GAME_MISC_H

#include "game_platform.h"

void
ExtractFileExtension(char *Buffer, uptr Size, const char *FileName) {
	Assert(Buffer);
	Assert(Size);
	Assert(FileName);

	const char *Dot = strrchr(FileName, '.');
	if (Dot) {
		strncpy(Buffer, Dot + 1, Size);
	}
}


#endif // #ifndef GAME_MISC_H
