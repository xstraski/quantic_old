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

game_state GameState = {};

static memory_partition TestPartition;
static image TestImage;

void
GameUpdate(game_update_type UpdateType) {
	switch (UpdateType) {
		///////////////////////////////////////////////////////////////////
		// Game initialization.
		///////////////////////////////////////////////////////////////////
	case GameUpdateType_Prepare: {
		InitializePartition(&GameState.PerFramePartition, Megabytes(64));
		
		InitializePartition(&TestPartition, 1024);
		TestImage = LoadImageBmp("/home/ia/src/quantic/base/master/test.bmp", &TestPartition);
	} break;

		///////////////////////////////////////////////////////////////////
		// Game de-initialization.
		///////////////////////////////////////////////////////////////////
	case GameUpdateType_Release: {
		if (GameState.KeyboardButtons[KeyCode_F].IsDown)
			PlatformQuit(0);
	} break;

		///////////////////////////////////////////////////////////////////
		// Game frame.
		///////////////////////////////////////////////////////////////////
	case GameUpdateType_Frame: {
		DrawImage(&GameState.VideoBuffer, &TestImage, V2(20.0f, 20.0f));
		
		ResetPartition(&GameState.PerFramePartition);
	} break;
	}
}
