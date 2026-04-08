/* date = December 21st 2025 1:11 pm */

#ifndef EDITOR_RANDOM_H
#define EDITOR_RANDOM_H

//~ Random 
#include <math.h>

#define PCG_MULTIPLIER_64 6364136223846793005ULL
#define PCG_INCREMENT_64  1442695040888963407ULL

typedef struct random_series random_series;
struct random_series
{
    u64 State;
};

internal void
RandomStep(random_series *Series)
{
    u64 NewState = (Series->State) * PCG_MULTIPLIER_64 + PCG_INCREMENT_64;
    Series->State = NewState;
}

internal void
RandomSeed(random_series *Series, u64 Seed)
{
    Series->State = 0;
    RandomStep(Series);
    Series->State += Seed;
    RandomStep(Series);
}

internal void
RandomLeap(random_series *Series, u64 Delta)
{
    u64 Increment = PCG_INCREMENT_64;
    u64 Multiplier = PCG_MULTIPLIER_64; 
    u64 AccumulatedMult = 1u;
    u64 AccumulatedInc = 0u;
    while(Delta > 0)
    {
        if(Delta & 1)
        {
            AccumulatedMult *= Multiplier;
            AccumulatedInc = AccumulatedInc * Multiplier + Increment;
        }
        Increment = (Multiplier + 1) * Increment;
        Multiplier *= Multiplier;
        Delta /= 2;
    }
    
    u64 NewState = AccumulatedMult * Series->State + AccumulatedInc;
    Series->State = NewState;
}

internal u32
RandomNext(random_series *Series)
{
    u32 Result = 0;
    
    Result = (u32)((Series->State ^ (Series->State >> 22)) >> (22 + (Series->State >> 61)));
    RandomStep(Series);
    
    return Result;
}

internal f32 
RandomF32(random_series *Series)
{
    f32 Result = ldexpf((f32)RandomNext(Series), -32);
    return Result;
}

internal f32
RandomUnilateral(random_series *Series)
{
    f32 Result = RandomF32(Series);
    return Result;
}

internal f32
RandomBilateral(random_series *Series)
{
    f32 Result = 2.0f*RandomUnilateral(Series) - 1.0f;
    return Result;
}

internal f32
RandomBetween(random_series *Series, f32 Min, f32 Max)
{
    f32 Range = Max - Min;
    f32 Result = Min + RandomUnilateral(Series)*Range;
    return Result;
}

#endif //EDITOR_RANDOM_H
