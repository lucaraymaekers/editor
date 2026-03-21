internal arena *
ArenaAlloc_(arena_alloc_params Params)
{
    arena *Arena = 0;
    
    u64 Size = Params.DefaultSize;
    if(Params.Size)
    {
        Size = Params.Size;
    }
    
    void *Base = OS_AllocateAtOffset(Size, Params.Offset);
    AsanPoisonMemoryRegion(Base, Size);
    
    u64 HeaderSize = sizeof(arena);
    AsanUnpoisonMemoryRegion(Base, HeaderSize);
    
    Arena = (arena *)Base;
    Arena->Base = Base;
    Arena->Pos = HeaderSize;
    Arena->Size = Size;
    
    return Arena;
}

internal arena *
PushArena(arena *Arena, u64 Size, b32 Zero)
{
    arena *Result = 0;
    
    Result = PushStruct(Arena, arena);
    Result->Size = Size;
    Result->Base = PushArray(Arena, u8, Result->Size);
    if(Zero) Result->Pos = 0;
    
    return Result;
}

internal void *
ArenaPush(arena *Arena, u64 Size, u64 Alignment, b32 Zero)
{
    u64 AlignedPos = AlignPow2(Arena->Pos, Alignment);
    u64 NewPos = AlignedPos + Size;
    
    Assert(NewPos <= Arena->Size);
    
    void *Result = (u8 *)Arena->Base + AlignedPos;
    
    AsanUnpoisonMemoryRegion(Result, Size);
    
    if(Zero)
    {
        MemorySet(Result, 0, Size);
    }
    
    Arena->Pos = NewPos;
    
    return Result;
}

internal u64 
BeginScratch(arena *Arena)
{
    u64 Result = Arena->Pos;
    return Result;
}

internal void 
EndScratch(arena *Arena, u64 BackPos)
{
    Arena->Pos = BackPos;
    
    AsanPoisonMemoryRegion((u8 *)Arena->Base + Arena->Pos,
                           Arena->Size - Arena->Pos);
}
