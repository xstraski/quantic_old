/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#if WIN32
#include "game_platform_win32.cpp"
#else
#error Unsupported target platform!
#endif
#include "game_misc.cpp"
#include "game_memory.cpp"
#include "game_image.cpp"
#include "game_asset.cpp"

game_state GameState = {};

void
GameUpdate(game_update_type UpdateType) {
	switch (UpdateType) {
		///////////////////////////////////////////////////////////////////
		// Game initialization.
		///////////////////////////////////////////////////////////////////
	case GameUpdateType_Prepare: {
		InitializePartition(&GameState.PerFramePartition, Megabytes(64));
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
		ResetPartition(&GameState.PerFramePartition);
	} break;
	}
}
