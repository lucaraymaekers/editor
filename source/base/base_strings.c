//~ Strings

#include <stdarg.h>
#include <stdio.h>

internal b32 
IsPrintable(u8 Char)
{
    b32 Result = (Char >= ' ' && Char <= '~');
    return Result;
}

internal b32
IsDigit(u8 Char)
{
    b32 Result = (Char >= '0' && Char <= '9');
    return Result;
}

internal b32
IsAlpha(u8 Char)
{
    b32 Result = ((Char >= 'A' && Char <= 'Z') ||
                  (Char >= 'a' && Char <= 'z'));
    return Result;
}

internal str8 
S8SkipLastSlash(str8 String)
{
	u64 LastSlash = 0;
	for EachIndex(i, String.Size)
	{
		if(String.Data[i] == '/')
		{
			LastSlash = i;
		}
	}
	LastSlash += 1;
    
	str8 Result = S8From(String, LastSlash);
	return Result;
}

internal b32
S8Match(str8 A, str8 B, b32 AIsPrefix)
{
    b32 Match = false;
    
    b32 Requirements = false;
    Requirements |= ( AIsPrefix && (A.Size <= B.Size));
    Requirements |= (!AIsPrefix && (A.Size == B.Size));
    
    if(Requirements)
    {
        Match = true;
        for EachIndex(Idx, A.Size)
        {
            if((A.Data[Idx] != B.Data[Idx]))
            {
                Match = false;
                break;
            }
        }
    }
    
    return Match;
}

internal u64
StringLength(char *String)
{
    u64 Result = 0;
    
    while(*String)
    {
        String += 1;
        Result += 1;
    }
    
    return Result;
}

internal str8
StringCat(arena *Arena, str8 Prefix, str8 Suffix)
{
    str8 Result = PushS8(Arena, Prefix.Size + Suffix.Size);
    
    for EachIndex(Idx, Prefix.Size)
    {
        Result.Data[Idx] = Prefix.Data[Idx];
    }
    
    for EachIndex(Idx, Suffix.Size)
    {
        Result.Data[Idx] = Suffix.Data[Idx];
    }
    
    return Result;
}

internal str8
StringFormat(arena *Arena, char *Format, ...)
{
    str8 Result = {0};
    
    Result.Data = PushArray(Arena, u8, 256);
    
    va_list Args;
    va_start(Args, Format);
    
    Result.Size = vsprintf((char *)Result.Data, Format, Args);
    
    return Result;
}

#if !defined(BASE_EXTERNAL_LIBS)
# if !defined(XXH_IMPLEMENTATION)
# define XXH_INLINE_ALL
# define XXH_STATIC_LINKING_ONLY
# define XXH_IMPLEMENTATION
# endif
#endif

#include "lib/xxHash/xxhash.h"

internal u64
U64HashFromSeedStr8(u64 Seed, str8 String)
{
    u64 Result = XXH3_64bits_withSeed(String.Data, String.Size, Seed);
    return Result;
}

internal u64
U64HashFromStr8(str8 String)
{
    u64 Result = U64HashFromSeedStr8(5381, String);
    return Result;
}