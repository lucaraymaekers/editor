/* ========================================================================

   (C) Copyright 2023 by Molly Rocket, Inc., All Rights Reserved.
   
   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any damages
   arising from the use of this software.
   
   Please see https://computerenhance.com for more information
   
   ======================================================================== */

/* ========================================================================
   LISTING 65
   ======================================================================== */

static f64 RadiansFromDegrees(f64 Degrees)
{
 f64 Result = 0.01745329251994329577 * Degrees;
 return Result;
}

// NOTE(casey): EarthRadius is generally expected to be 6372.8
static f64 ReferenceHaversine(f64 X0, f64 Y0, f64 X1, f64 Y1, f64 EarthRadius)
{
 /* NOTE(casey): This is not meant to be a "good" way to calculate the Haversine distance.
    Instead, it attempts to follow, as closely as possible, the formula used in the real-world
    question on which these homework exercises are loosely based.
 */
 
 f64 lat1 = Y0;
 f64 lat2 = Y1;
 f64 lon1 = X0;
 f64 lon2 = X1;
 
 f64 dLat = RadiansFromDegrees(lat2 - lat1);
 f64 dLon = RadiansFromDegrees(lon2 - lon1);
 lat1 = RadiansFromDegrees(lat1);
 lat2 = RadiansFromDegrees(lat2);
 
 f64 a = SquareF64(SinF64(dLat/2.0)) + CosF64(lat1)*CosF64(lat2)*SquareF64(SinF64(dLon/2.0));
 f64 c = 2.0*ASinF64(SqrtF64(a));
 
 f64 Result = EarthRadius * c;
 
 return Result;
}

/* ========================================================================

   (C) Copyright 2023 by Molly Rocket, Inc., All Rights Reserved.
   
   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any damages
   arising from the use of this software.
   
   Please see https://computerenhance.com for more information
   
   ======================================================================== */

/* ========================================================================
   LISTING 70
   ======================================================================== */

#if _WIN32

#include <intrin.h>
#include <windows.h>

static u64 GetOSTimerFreq(void)
{
	LARGE_INTEGER Freq;
	QueryPerformanceFrequency(&Freq);
	return Freq.QuadPart;
}

static u64 ReadOSTimer(void)
{
	LARGE_INTEGER Value;
	QueryPerformanceCounter(&Value);
	return Value.QuadPart;
}

#else

#include <x86intrin.h>
#include <sys/time.h>

static u64 GetOSTimerFreq(void)
{
	return 1000000;
}

static u64 ReadOSTimer(void)
{
	// NOTE(casey): The "struct" keyword is not necessary here when compiling in C++,
	// but just in case anyone is using this file from C, I include it.
	struct timeval Value;
	gettimeofday(&Value, 0);
	
	u64 Result = GetOSTimerFreq()*(u64)Value.tv_sec + (u64)Value.tv_usec;
	return Result;
}

#endif

/* NOTE(casey): This does not need to be "inline", it could just be "static"
   because compilers will inline it anyway. But compilers will warn about 
   static functions that aren't used. So "inline" is just the simplest way 
   to tell them to stop complaining about that. */
inline u64 ReadCPUTimer(void)
{
	// NOTE(casey): If you were on ARM, you would need to replace __rdtsc
	// with one of their performance counter read instructions, depending
	// on which ones are available on your platform.
	
	return __rdtsc();
}

