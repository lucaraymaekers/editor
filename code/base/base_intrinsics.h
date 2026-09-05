/* date = April 14th 2026 3:51 pm */

#ifndef BASE_INTRINSICS_H
#define BASE_INTRINSICS_H

#include "math.h"

#if COMPILER_GNU || COMPILER_CLANG
# define AtomicAddEvalU64(Pointer, Value) \
(__sync_fetch_and_add((Pointer), (Value), __ATOMIC_SEQ_CST) + (Value))
#elif COMPILER_MSVC
# define AtomicAddEvalU64(Pointer, Value) \
InterlockedAdd64((__int64 *)(Pointer), (Value))
#endif


#if COMPILER_GNU || COMPILER_CLANG
# define CountLeadingZeroes64(Value)	__builtin_clzll(Value)
#elif COMPILER_MSVC
# define CountLeadingZeroes64(Value) __lzcnt64(Value)
#endif

internal inline f64
SinF64(f64 X)
{
 f64 Result = sin(X);
 return Result;
}

internal inline f64
CosF64(f64 X)
{
 f64 Result = cos(X);
 return Result;
}

internal inline f64
ASinF64(f64 X)
{
 f64 Result = asin(X);
 return Result;
}

internal inline f64
SqrtF64(f64 X)
{
 f64 Result = (X);
 return Result;
}

internal inline f32 
FloorF32(f32 X)
{
 f32 Result = floorf(X);
 return Result;
}

internal inline f32
AbsF32(f32 X)
{
 f32 Result = fabsf(X);
 return Result;
}

internal inline f32
CeilF32(f32 X)
{
 f32 Result = ceilf(X);
 return Result;
}

internal inline f32
RoundF32(f32 X)
{
 f32 Result = roundf(X);
 return Result;
}

internal inline f32
PowF32(f32 Base, f32 Exp)
{
 f32 Result = powf(Base, Exp);
 return Result;
}

internal inline f32
LogF32(f32 X)
{
 f32 Result = logf(X);
 return Result;
}

internal inline f32
Log2F32(f32 X)
{
 f32 Result = log2f(X);
 return Result;
}

internal inline f32
Log10F32(f32 X)
{
 f32 Result = log10f(X);
 return Result;
}

internal inline f32
ExpF32(f32 X)
{
 f32 Result = PowF32(Euler32, X);
 return Result;
}

#endif //BASE_INTRINSICS_H
