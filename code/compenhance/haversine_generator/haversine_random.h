#include <math.h>

NO_WARNINGS_BEGIN
#include "lib/pcg/pcg.c"
NO_WARNINGS_END

#define CountLeadingZeroes64(Value)	__builtin_clzll(Value)

typedef pcg64_random_t random_series;

void Seed(random_series *Series, u64 RandomSeed)
{
 pcg64_srandom_r(Series, RandomSeed, RandomSeed);
}

u64 
RandomU64(random_series *Series)
{
 u64 Result = pcg64_random_r(Series);
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
f64
RandomF64(random_series *Series)
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

f64
RandomUnilateral(random_series *Series)
{
 return RandomF64(Series);
}

f64
RandomBilateral(random_series *Series)
{
 f64 Result = 2.0*RandomUnilateral(Series) - 1.0;
 return Result;
}

f64
RandomBetween(random_series *Series, f64 Min, f64 Max)
{
 f64 Range = Max - Min;
 f64 Result = Min + RandomUnilateral(Series)*Range;
 return Result;
}


static f64 RandomDegree(random_series *Series, f64 Center, f64 Radius, f64 MaxAllowed)
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
 
 f64 Result = RandomBetween(Series, MinVal, MaxVal);
 return Result;
}
