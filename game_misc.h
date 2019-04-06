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
CopyString(char *DestString, const char *SrcString) {
	Assert(DestString);
	Assert(SrcString);

	while (*SrcString)
		*DestString++ = *SrcString++;
}

void
CopyStringN(char *DestString, const char *SrcString, uptr MaxCount) {
	Assert(DestString);
	Assert(SrcString);

	if (MaxCount == 0)
		return;

	while (MaxCount) {
		*DestString++ = *SrcString++;
		MaxCount--;
	}
}

inline b32
AreStringsEqual(const char *String1, const char *String2) {
	Assert(String1);
	Assert(String2);

	b32 Result = true;

	while (*String1 && *String2) {
		if (*String1 != *String2) {
			Result = false;
			break;
		}
	}

	return Result;
}

#endif // #ifndef GAME_MISC_H
