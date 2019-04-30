/* =====================================================================
   $File: $
   $Date: $
   $Revision: $
   $Author: Ivan Avdonin $
   $Notice: Copyright (C) 2019, Ivan Avdonin. All Rights Reserved. $
   ===================================================================== */
#ifndef GAME_PLATFORM_H
#define GAME_PLATFORM_H

//
// NOTE(ivan): Compiler detection.
//
#if defined(_MSC_VER)
#define MSVC 1
#define GNUC 0
#elif defined(__GNUC__)
#define MSVC 0
#define GNUC 1
#else
#error "Unsupported compiler!"
#endif

//
// NOTE(ivan): C standard includes.
//
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <math.h>

//
// NOTE(ivan): Compiler intrinsics includes.
//
#if MSVC
#include <intrin.h>
#elif GNUC
#include <x86intrin.h>
#endif

// NOTE(ivan): Detect target CPU architecture.
#if MSVC
#if defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64)
#define INTEL86 1
#define INTELORDER 1
#define AMIGAORDER 0
#if defined(_M_X64) || defined(_M_AMD64)
#define X32CPU 0
#define X64CPU 1
#else
#define X32CPU 1
#define X64CPU 0
#endif
#else
#error Unsupported target CPU architecture!
#endif
#elif GNUC
#if defined(__i386__) || defined(__x86_64__) || defined(__amd64__)
#define INTEL86 1
#define INTELORDER 1
#define AMIGAORDER 0
#if defined(__x86_64__) || defined(__amd64__)
#define X32CPU 0
#define X64CPU 1
#else
#define X32CPU 1
#define X64CPU 0
#endif
#else
#error Unsupported target CPU architecture!
#endif
#endif

// NOTE(ivan): Base integer types.
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

// NOTE(ivan): Integers large enough to hold any pointer on target processor platform.
typedef size_t uptr;
typedef ptrdiff_t sptr;

// NOTE(ivan): Integers of a maximum size of a target processor platform.
typedef uintmax_t umax;
typedef intmax_t smax;

// NOTE(ivan): Float types.
typedef float f32;
typedef double f64;

typedef u32 b32; // NOTE(ivan): Forget about standard C++ bool, wa want our boolean type to be 32-bit.

// NOTE(ivan): Used to shut up the compiler complaining about an unused variable.
#define UnusedParam(Param) (Param)

// NOTE(ivan): Adds quotes.
#define XSTRING2(X) #X
#define XSTRING(String) XSTRING2(String)

// NOTE(ivan): Calculates a given array's elements count. Do not place functions/expressions as the 'arr' parameter, cuz they will be exeute twice!!!
template <typename T, u32 N> inline constexpr u32
CountOf(T (&Array)[N]) {
	UnusedParam(Array);
	return N;
}

// NOTE(ivan): Calculates an offset value of a structure's member.
#define OffsetOf(Type, Member) ((uptr)&(((Type *)0)->Member))

// NOTE(ivan): Min/max.
template <typename T> inline T
Min(T A, T B) {
	return A <= B ? A : B;
}
template <typename T> inline T
Max(T A, T B) {
	return A > B ? A : B;
}

// NOTE(ivan): Swaps data.
template <typename T> inline void
Swap(T *A, T *B) {
	T Tmp = *A;
	*A = *B;
	*B = Tmp;
}

// NOTE(ivan): Debug assertions.
#define BreakDebugger() do {*((s32 *)0) = 1;} while(0)
#if SLOWCODE
#define Assert(Expression) do {if (!(Expression)) BreakDebugger();} while(0)
#else
#define Assert(Expression)
#endif
#if SLOWCODE
#define Verify(Expression) Assert(Expression)
#else
#define Verify(Expression) (Expression)
#endif

// NOTE(ivan): Does not allow to compile the program in public release builds.
#if INTERNAL
#define NotImplemented() Assert(!"Not implemented!!!")
#else
#define NotImplemented() NotImplemented!!!!!!!!!!!!!
#endif

// NOTE(ivan): Helper macros for code pieces that are not intended to run at all.
#if INTERNAL
#define InvalidCodePath() Assert(!"Invalid code path!!!")
#else
#define InvalidCodePath() do {} while(0)
#endif
#define InvalidDefaultCase default: {InvalidCodePath();} break

// NOTE(ivan): Safe truncation.
inline u32
SafeTruncateU64(u64 Value) {
	Assert(Value <= 0xFFFFFFFF);
	return (u32)Value;
}
inline u16
SafeTruncateU32(u32 Value) {
	Assert(Value <= 0xFFFF);
	return (u16)Value;
}
inline u8
SafeTruncateU16(u16 Value) {
	Assert(Value <= 0xFF);
	return (u8)Value;
}

// NOTE(ivan): Measurings.
#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

// NOTE(ivan): Is power of two?
template <typename T> inline b32
IsPow2(T Value) {
	return ((Value & ~(Value - 1)) == Value);
}

// NOTE(ivan): Values alignment.
template <typename T> inline T
AlignPow2(T Value, T Alignment) {
	Assert(IsPow2(Alignment));
	return ((Value + (Alignment - 1)) & ~(Alignment - 1));
}
#define Align4(Value) (((Value) + 3) & ~3)
#define Align8(Value) (((Value) + 7) & ~7)
#define Align16(Value) (((Value) + 15) & ~15)

// NOTE(ivan): Bit scan result.
struct bit_scan_result {
	b32 IsFound;
	u32 Index;
};

// NOTE(ivan): Bit scan.
inline bit_scan_result
FindLeastSignificantBit(u32 Value)
{
	bit_scan_result Result = {};
	
#if MSVC
	Result.IsFound = _BitScanForward((unsigned long *)&Result.Index, Value);
#else
	for (u32 Test = 0; Test < 32; Test++) {
		if (Value & (1 << Test)) {
			Result.IsFound = true;
			Result.Index = Test;
			break;
		}
	}
#endif	

	return Result;
}
inline bit_scan_result
FindMostSignificantBit(u32 Value)
{
	bit_scan_result Result = {};

#if MSVC
	Result.IsFound = _BitScanReverse((unsigned long *)&Result.Index, Value);
#else
	for (s32 Test = 31; Test >= 0; Test++) {
		if (Value & (1 << Test)) {
			Result.IsFound = true;
			Result.Index = Test;
			break;
		}
	}
#endif

	return Result;
}

// NOTE(ivan): Generic-purpose structure for holding a memory piece of information.
struct piece {
	u8 *Base;
	uptr Size;
};

// NOTE(ivan): Helper functions for reading from buffers.
#define ConsumeType(Piece, Type) (Type *)ConsumeSize(Piece, sizeof(Type))
#define ConsumeTypeArray(Piece, Type, Count) (Type *)ConsumeSize(Piece, sizeof(Type) * Count)
inline void *
ConsumeSize(piece *Piece, uptr Size) {
	Assert(Piece);
	Assert(Piece->Size >= Size);
	
	void *Result = Piece->Base;
	Piece->Base = (Piece->Base + Size);
	Piece->Size -= Size;

	return Result;
}

// NOTE(ivan): Endian-ness utilities.
inline void
SwapEndianU32(u32 *Value)
{
	Assert(Value);

#if MSVC
	*Value = _byteswap_ulong(*Value);
#else	
	u32 V = *Value;
	*Value = ((V << 24) | ((V & 0xFF00) << 8) | ((V >> 8) & 0xFF00) | (V >> 24));
#endif	
}
inline void
SwapEndianU16(u16 *Value)
{
	Assert(Value);

#if MSVC
	*Value = _byteswap_ushort(*Value);
#else
	u16 V = *Value;
	*Value = ((V << 8) || (V  >> 8));
#endif
}

// NOTE(ivan): 4-character-code.
#define FourCC(String) ((u32)((String[3] << 0) | (String[2] << 8) | (String[1] << 16) | (String[0] << 24)))
#define FastFourCC(String) (*(u32 *)(String)) // NOTE(ivan): Does not work with switch/case.

// NOTE(ivan): Interlocked operations.
#if MSVC
inline u32 AtomicIncrementU32(volatile u32 *Value) {return _InterlockedIncrement((volatile long *)Value);}
inline u64 AtomicIncrementU64(volatile u64 *Value) {return _InterlockedIncrement64((volatile __int64 *)Value);}
inline u32 AtomicDecrementU32(volatile u32 *Value) {return _InterlockedDecrement((volatile long *)Value);}
inline u64 AtomicDecrementU64(volatile u64 *Value) {return _InterlockedDecrement64((volatile __int64 *)Value);}
inline u32 AtomicExchangeU32(volatile u32 *Target, u32 Value) {return _InterlockedExchange((volatile long *)Target, Value);}
inline u64 AtomicExchangeU64(volatile u64 *Target, u64 Value) {return _InterlockedExchange64((volatile __int64 *)Target, Value);}
inline u32 AtomicCompareExchangeU32(volatile u32 *Value, u32 NewValue, u32 Exp) {return _InterlockedCompareExchange((volatile long *)Value, NewValue, Exp);}
inline u64 AtomicCompareExchangeU64(volatile u64 *Value, u64 NewValue, u64 Exp) {return _InterlockedCompareExchange64((volatile __int64 *)Value, NewValue, Exp);}
#elif GNUC
inline u32 AtomicIncrementU32(volatile u32 *Value) {return __sync_fetch_and_add(Value, 1);}
inline u64 AtomicIncrementU64(volatile u64 *Value) {return __sync_fetch_and_add(Value, 1);}
inline u32 AtomicDecrementU32(volatile u32 *Value) {return __sync_fetch_and_sub(Value, 1);}
inline u64 AtomicDecrementU64(volatile u64 *Value) {return __sync_fetch_and_sub(Value, 1);}
inline u32 AtomicExchangeU32(volatile u32 *Target, u32 Value) {return __sync_lock_test_and_set(Target, Value);}
inline u64 AtomicExchangeU64(volatile u64 *Target, u64 Value) {return __sync_lock_test_and_set(Target, Value);}
inline u32 AtomicCompareExchangeU32(volatile u32 *Value, u32 NewValue, u32 Exp) {return __sync_val_compare_and_swap(Value, Exp, NewValue);}
inline u64 AtomicCompareExchangeU64(volatile u64 *Value, u64 NewValue, u64 Exp) {return __sync_val_compare_and_swap(Value, Exp, NewValue);}
#endif

// NOTE(ivan): Yield processor, give its time to other threads.
#if MSVC
inline void YieldProcessor(void) {_mm_pause();}
#elif GNUC
inline void YieldProcessor(void) {_mm_pause();}
#endif

// NOTE(ivan): Cross-platform ticket-mutex.
// NOTE(ivan): Any instance of this structure MUST be ZERO-initialized for proper functioning of EnterTicketMutex()/LeaveTicketMutex() macros.
struct ticket_mutex {
	volatile u64 Ticket;
	volatile u64 Serving;
};

// NOTE(ivan): Ticket-mutex locking/unlocking.
inline void
EnterTicketMutex(ticket_mutex *Mutex) {
	Assert(Mutex);
	AtomicIncrementU64(&Mutex->Ticket);
	
	u64 Ticket = Mutex->Ticket - 1;
	while (Ticket != Mutex->Serving)
		YieldProcessor();
}
inline void
LeaveTicketMutex(ticket_mutex *Mutex) {
	Assert(Mutex);
	AtomicIncrementU64(&Mutex->Serving);
}

//
// NOTE(ivan): Platform-specific interface.
//
void PlatformQuit(s32 QuitCode);

#define PARAM_MISSING ((s32)-1)
s32 PlatformCheckParam(const char *Param);
const char * PlatformCheckParamValue(const char *Param);

piece PlatformReadEntireFile(const char *FileName);
b32 PlatformWriteEntireFile(const char *FileName, void *Base, uptr Size);
void PlatformFreeEntireFilePiece(piece *Piece);

//
// NOTE(ivan): Platform-specific debug-only interface.
//
#if INTERNAL
void DEBUGPlatformOutf(const char *Format, ...);
#else
inline void DEBUGPlatformOutf(const char *Format, ...) {}
#endif

#endif // #ifndef GAME_PLATFORM_H
