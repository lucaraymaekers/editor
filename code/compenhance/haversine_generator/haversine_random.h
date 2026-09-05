#include <math.h>

/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *     http://www.pcg-random.org
 */

/*
 * This file was mechanically generated from tests/check-pcg32.c
 */

/*
 * This code shows how you can cope if you're on a 32-bit platform (or a
 * 64-bit platform with a mediocre compiler) that doesn't support 128-bit math,
 * or if you're using the basic version of the library which only provides
 * 32-bit generation.
 *
 * Here we build a 64-bit generator by tying together two 32-bit generators.
 * Note that we can do this because we set up the generators so that each
 * 32-bit generator has a *totally different* different output sequence
 * -- if you tied together two identical generators, that wouldn't be nearly
 * as good.
 *
 * For simplicity, we keep the period fixed at 2^64.  The state space is
 * approximately 2^254 (actually  2^64 * 2^64 * 2^63 * (2^63 - 1)), which
 * is huge.
 */

#include "rl/rl_random.h"

typedef struct pcg32x2_random_t pcg32x2_random_t; 
struct pcg32x2_random_t 
{
 random_series gen[2];
};

typedef struct pcg32x2_random_t haversine_random;

internal void pcg32x2_srandom_r(haversine_random* rng, 
                                u64 seed1, u64 seed2,
                                u64 seq1,  u64 seq2)
{
 u64 mask = ~0ull >> 1;
 // The stream for each of the two generators *must* be distinct
 if ((seq1 & mask) == (seq2 & mask)) 
  seq2 = ~seq2;
 
 // TODO(luca): Use seq
 RandomSeed(rng->gen+0, seed1);
 RandomSeed(rng->gen+1, seed2);
}

internal u64 
pcg32x2_random_r(haversine_random* rng)
{
 u64 Result = ((u64)(RandomNext(rng->gen)) << 32)| RandomNext(rng->gen+1);
 return Result; 
}

internal void 
Random64Seed(haversine_random *Series, u64 Seed1, u64 Seed2, u64 Seq1, u64 Seq2)
{
 pcg32x2_srandom_r(Series, Seed1, Seed2, Seq1, Seq2);
}

internal u64 
RandomU64(haversine_random *Series)
{
 u64 Result = pcg32x2_random_r(Series);
 return Result;
}

//~ Random 64 bit float

// From: https://mumble.net/~campbell/tmp/random_real.c
/*
 *    Copyright (c) 2014, Taylor R Campbell
*
*    Verbatim copying and distribution of this entire article are
*    permitted worldwide, without royalty, in any medium, provided
*    this notice, and the copyright notice, are preserved.
*
*/

/*
 * random_real: Generate a stream of bits uniformly at random and
 * interpret it as the fractional part of the binary expansion of a
 * number in [0, 1], 0.00001010011111010100...; then round it.
 */
internal f64
RandomF64(haversine_random *Series)
{
	s32 Exponent = -64;
	u64 Significand;
	s32 Shift;
 
	/*
	 * Read zeros into the exponent until we hit a one; the rest
	 * will go into the significand.
	 */
	while((Significand = RandomU64(Series)) == 0) 
 {
		Exponent -= 64;
		/*
		 * If the exponent falls below -1074 = emin + 1 - p,
		 * the exponent of the smallest subnormal, we are
		 * guaranteed the result will be rounded to zero.  This
		 * case is so unlikely it will happen in realistic
		 * terms only if RandomU64 is broken.
		 */
		if ((Exponent < -1074))
			return 0;
	}
 
	/*
	 * There is a 1 somewhere in significand, not necessarily in
	 * the most significant position.  If there are leading zeros,
	 * shift them into the exponent and refill the less-significant
	 * bits of the significand.  Can't predict one way or another
	 * whether there are leading zeros: there's a fifty-fifty
	 * chance, if RandomU64() is uniformly distributed.
	 */
	Shift = CountLeadingZeroes64(Significand);
	if (Shift != 0) {
		Exponent -= Shift;
		Significand <<= Shift;
		Significand |= (RandomU64(Series) >> (64 - Shift));
	}
 
	/*
	 * Set the sticky bit, since there is almost surely another 1
	 * in the bit stream.  Otherwise, we might round what looks
	 * like a tie to even when, almost surely, were we to look
	 * further in the bit stream, there would be a 1 breaking the
	 * tie.
	 */
	Significand |= 1;
 
	/*
	 * Finally, convert to f64 (rounding) and scale by
	 * 2^exponent.
	 */
	return ldexp((f64)Significand, Exponent);
}

internal f64
Random64Unilateral(haversine_random *Series)
{
 return RandomF64(Series);
}

internal f64
Random64Bilateral(haversine_random *Series)
{
 f64 Result = 2.0*Random64Unilateral(Series) - 1.0;
 return Result;
}

internal f64
Random64Between(haversine_random *Series, f64 Min, f64 Max)
{
 f64 Range = Max - Min;
 f64 Result = Min + Random64Unilateral(Series)*Range;
 return Result;
}

internal f64 
Random64Degree(haversine_random *Series, f64 Center, f64 Radius, f64 MaxAllowed)
{
 f64 MinVal = Center - Radius;
 if(MinVal < -MaxAllowed)
 {
  MinVal = -MaxAllowed;
 }
 
 f64 MaxVal = Center + Radius;
 if(MaxVal > MaxAllowed)
 {
  MaxVal = MaxAllowed;
 }
 
 f64 Result = Random64Between(Series, MinVal, MaxVal);
 return Result;
}
