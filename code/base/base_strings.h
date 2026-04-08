/* date = December 21st 2025 0:25 pm */

#if OS_WINDOWS
# define RADDBG_MARKUP_IMPLEMENTATION
#else
# define RADDBG_MARKUP_STUBS
#endif
#include "lib/raddbg_markup.h"

#if !defined(BASE_STRINGS_H)
#define BASE_STRINGS_H

typedef struct str8 str8;
struct str8
{
    u8 *Data;
    u64 Size;
};

internal str8 S8SkipLastSlash(str8 String);
internal b32  S8Match(str8 A, str8 B, b32 AIsPrefix);
internal u64  StringLength(char *String);

#if LANG_CPP
# define S8Cast str8
#else
# define S8Cast (str8)
#endif

#define S8(String)                   S8Cast{.Data = (u8 *)(String), .Size = (sizeof((String)) - 1)}
#define S8From(String, Start)        S8Cast{.Data = (u8 *)(String).Data + (Start), .Size = ((String).Size - (Start))}
#define S8To(String, End)            S8Cast{.Data = (u8 *)(String).Data, .Size = (End)}
#define S8FromTo(String, Start, End) S8Cast{.Data = (u8 *)(String).Data + (Start), .Size = ((End) - (Start))}
#define S8FromCString(String)        S8Cast{.Data = (u8 *)(String), .Size = StringLength((char *)(String))}
#define S8FromArray(Array)           S8Cast{.Data = (u8 *)(Array), .Size = sizeof((Array))}

#define S8Fmt "%.*s" 
#define S8Arg(String) (int)((String).Size), (char *)(String).Data

#define PushS8_(Arena, Count) S8Cast{.Data = (PushArray((Arena), u8, (Count))), .Size = (Count)} 
#define PushS8(Arena, Size) PushS8_(Arena, Size)

#endif //BASE_STRINGS_H
