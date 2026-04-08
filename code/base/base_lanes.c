#include "base_arenas.h"

internal void 
ThreadContextSelect(thread_context *Context)
{
    ThreadContext = Context;
}

internal arena *
GetScratch(void)
{
    arena *Arena = ThreadContext->Arena; 
    return Arena;
}

internal void 
Iceberg(void)
{
    OS_BarrierWait(ThreadContext->Barrier);
}

internal void 
LaneSyncU64(u64 *Value, s64 SourceIdx)
{
    if(LaneIndex() == SourceIdx)
    {
        *(u64 *)ThreadContext->SharedStorage = *(u64 *)Value;
    }
    Iceberg();
    
    if(LaneIndex() != SourceIdx)
    {
        *(u64 *)Value = *(u64 *)ThreadContext->SharedStorage;
    }
    Iceberg();
}

internal range_s64 
LaneRange(s64 ValuesCount)
{
    range_s64 Result = {0};
    
    s64 ValuesPerThread = ValuesCount/LaneCount();
    
    s64 LeftoverValuesCount = ValuesCount%LaneCount();
    b32 ThreadHasLeftover = (LaneIndex() < LeftoverValuesCount);
    s64 LeftoversBeforeThisThreadIdx = ((ThreadHasLeftover) ? 
                                        LaneIndex(): 
                                        LeftoverValuesCount);
    
    Result.Min = (ValuesPerThread*LaneIndex()+
                  LeftoversBeforeThisThreadIdx);
    Result.Max = (Result.Min + ValuesPerThread + !!ThreadHasLeftover);
    
    return Result;
}

internal void 
ThreadInit(thread_context *ContextToSelect)
{
    ThreadContextSelect(ContextToSelect);
    
    ThreadContext->Arena = ArenaAlloc();
    
    u8 ThreadNameBuffer[16] = {0};
    str8 ThreadName = {0};
    ThreadName.Data = ThreadNameBuffer;
    ThreadName.Size = 1;
    ThreadName.Data[0] = (u8)LaneIndex() + '0';
    OS_SetThreadName(ThreadName);
}
