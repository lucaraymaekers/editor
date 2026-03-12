/* date = December 6th 2025 2:11 pm */

#if !defined(ARENAS_H)
#define ARENAS_H

typedef struct arena arena;
struct arena
{
    void *Base;
    u64 Pos;
    u64 Size;
};

typedef struct arena_alloc_params arena_alloc_params;
struct arena_alloc_params
{
    u64 DefaultSize;
    u64 Size;
    u64 Offset;
};

#define ArenaAllocDefaultSize MB(64)

#if COMPILER_MSVC
# if LANG_CPP
#  define StructCast(t) t
# elif LANG_C
#  define StructCast(t) (t)
# endif
#else
# define StructCast(t) (t)
#endif

#define ArenaAlloc(...) ArenaAlloc_(StructCast(arena_alloc_params){.DefaultSize = ArenaAllocDefaultSize, ##__VA_ARGS__})
internal arena *ArenaAlloc_(arena_alloc_params Params);
internal void  *ArenaPush(arena *Arena, u64 Size, u64 Alignment, b32 Zero);
internal u64 BeginScratch(arena *Arena);
internal void EndScratch(arena *Arena, u64 BackPos);

#define Scratch(Arena) for(u64 _J_ = BeginScratch((Arena)), _I_ = 0; !_I_; _I_ += 1, (EndScratch(Arena, _J_)))

#define PushArray(Arena, t, Count) (t *)ArenaPush((Arena), (Count)*(sizeof(t)), AlignOf(t), false)
#define PushStruct(Arena, t) PushArray(Arena, t, 1)
#define PushArrayZero(Arena, t, Count) (t *)ArenaPush((Arena), (Count)*(sizeof(t)), AlignOf(t), true)
#define PushStructZero(Arena, t) PushArrayZero(Arena, t, 1)

#endif //ARENAS_H
