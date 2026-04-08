/* date = December 6th 2025 3:00 am */

#if !defined(LANES_H)
#define LANES_H

#include "base_arenas.h"

typedef u64 barrier;

typedef u64  thread_handle;

typedef struct thread_context thread_context;
struct thread_context
{
    s64 LaneCount;
    s64 LaneIndex;
    
    thread_handle Handle;
    
    u64 *SharedStorage;
    barrier Barrier;
    
    arena *Arena;
};

#define AtomicAddEvalU64(Pointer, Value) \
(__sync_fetch_and_add((Pointer), (Value), __ATOMIC_SEQ_CST) + (Value));

thread_static thread_context *ThreadContext;

#define LaneCount() (ThreadContext->LaneCount)
#define LaneIndex() (ThreadContext->LaneIndex)

internal void ThreadInit(thread_context *ContextToSelect);
internal arena *GetScratch(void);

#endif //LANES_H
