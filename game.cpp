/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#if WIN32
#include "game_platform_win32.cpp"
#elif LINUX
#include "game_platform_linux.cpp"
#else
#error Unsupported target platform!
#endif
#include "game_misc.cpp"
#include "game_memory.cpp"
#include "game_image.cpp"
#include "game_draw.cpp"
#include "game_asset.cpp"

void
GameUpdate(game_update_type UpdateType, game_state *State, game_tl_state *TLState) {
	switch (UpdateType) {
		///////////////////////////////////////////////////////////////////
		// Game initialization.
		///////////////////////////////////////////////////////////////////
	case GameUpdateType_Prepare: {
	} break;

		///////////////////////////////////////////////////////////////////
		// Game de-initialization.
		///////////////////////////////////////////////////////////////////
	case GameUpdateType_Release: {
	} break;

		///////////////////////////////////////////////////////////////////
		// Game frame.
		///////////////////////////////////////////////////////////////////
	case GameUpdateType_Frame: {
	} break;
	}
}
