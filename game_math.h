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

// NOTE(ivan): 2D point.
struct point {
	s32 X;
	s32 Y;
};

// NOTE(ivan): 2D rectangle.
struct rectangle {
	s32 Width;
	s32 Height;
};

// NOTE(ivan): 2D vector.
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

inline v2
V2(f32 E0, f32 E1) {
	v2 Result;

	Result.E[0] = E0;
	Result.E[1] = E1;

	return Result;
}

// NOTE(ivan): 4D vector.
struct v4 {
	union {
		struct {
			f32 X;
			f32 Y;
			f32 Z;
			f32 W;
		};
		struct {
			f32 R;
			f32 G;
			f32 B;
			f32 A;
		};
		f32 E[4];
	};
};

inline v4
V4(f32 E0, f32 E1, f32 E2, f32 E3) {
	v4 Result;

	Result.E[0] = E0;
	Result.E[1] = E1;
	Result.E[2] = E2;
	Result.E[3] = E3;

	return Result;
}

#endif // #ifndef GAME_MATH_H
