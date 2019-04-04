/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#ifndef GAME_MATH_H
#define GAME_MATH_H

#include "game_platform.h"

// NOTE(ivan): Pi.
#define PI32 3.14159265359f
#define PI64 3.14159265359

//
// NOTE(ivan): 2D point.
//
struct point {
	s32 X;
	s32 Y;
};

//
// NOTE(ivan): 2D rectangle.
//
struct rectangle {
	s32 Width;
	s32 Height;
};

//
// NOTE(ivan): 2D vector.
//
struct v2 {
	union {
		struct {
			f32 X;
			f32 Y;
		};
		struct {
			f32 U;
			f32 V;
		};
		f32 E[2];
	};
};

#endif // #ifndef GAME_MATH_H
