/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#ifndef GAME_H
#define GAME_H

#include "game_platform.h"
#include "game_keys.h"
#include "game_math.h"
#include "game_memory.h"

// NOTE(ivan): Game title, should be one simple UpperCamelCase word.
#define GAMENAME "Quantic"

// NOTE(ivan): Game video buffer.
struct game_video_buffer {
	u32 *Pixels;
	s32 Width;
	s32 Height;
	s32 BytesPerPixel;
	s32 Pitch;
};

// NOTE(ivan): Game audio buffer.
struct game_audio_buffer {
	s16 *Samples;
	u32 SamplesPerSecond;
	u32 SampleCount;
};

// NOTE(ivan): Game input button.
struct game_input_button {
	b32 WasDown;
	b32 IsDown;
	b32 IsNew; // NOTE(ivan): Has the state been changed in current frame?
};

inline b32
IsNewlyPressed(game_input_button *Button) {
	Assert(Button);
	return !Button->WasDown && Button->IsDown && Button->IsNew;
}

// NOTE(ivan): Game Xbox controller.
struct game_input_xbox_controller {
	b32 IsConnected;
	
	game_input_button Start;
	game_input_button Back;

	game_input_button A;
	game_input_button B;
	game_input_button X;
	game_input_button Y;

	game_input_button Up;
	game_input_button Down;
	game_input_button Left;
	game_input_button Right;

	game_input_button LeftBumper;
	game_input_button RightBumper;

	game_input_button LeftStick;
	game_input_button RightStick;

	u8 LeftTrigger;
	u8 RightTrigger;

	v2 LeftStickPos;
	v2 RightStickPos; 
};

// NOTE(ivan): Game state.
struct game_state {
	// NOTE(ivan): Main memory block. Must be zero-initialized at startup.
	piece Hunk;
	ticket_mutex HunkMutex;

	// NOTE(ivan): Temp memory buffer that holds data per-frame.
	memory_partition PerFramePartition;
	
	// NOTE(ivan): Video buffer to write graphics in.
	game_video_buffer VideoBuffer;
	// NOTE(ivan): Audio buffer to write sounds in.
	game_audio_buffer AudioBuffer;

	// NOTE(ivan): Input state.
	game_input_button KeyboardButtons[KeyCode_MaxCount];
	game_input_button MouseButtons[5];
	point MousePos;
	s32 MouseWheel; // NOTE(ivan): Number of scrolls per frame. Negative value indicates the wheel was rotated backward, toward the user.
	game_input_xbox_controller XboxControllers[4];

	// NOTE(ivan): Clocks.
	f64 CyclesPerFrame;
	f64 SecondsPerFrame;
	f64 FramesPerSecond;
};
extern game_state GameState;

// NOTE(ivan): Game update type.
enum game_update_type {
	GameUpdateType_Prepare,
	GameUpdateType_Release,
	GameUpdateType_Frame
};

void GameUpdate(game_update_type UpdateType);

#endif // #ifndef GAME_H
