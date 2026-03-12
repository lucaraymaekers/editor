/* date = December 6th 2025 2:34 pm */

#if !defined(OS_H)
#define OS_H

//~ Types
typedef struct entry_point_params entry_point_params;
struct entry_point_params
{
    thread_context Context;
    int ArgsCount;
    char **Args;
    char **Env;
};

typedef struct os_profiler os_profiler;
struct os_profiler
{
    f64 Start;
    f64 End;
};

typedef struct os_file_handle os_file_handle;
struct os_file_handle
{
    u64 Handle;
};

typedef struct os_thread os_thread;
struct os_thread
{
    void *Result;
    
    entry_point_params Params;
};

//~ Globals
global_variable u8 LogBuffer[KB(64)];
global_variable os_profiler GlobalProfiler = {}; 
global_variable char *GlobalProfilerPrefix = ""; 

#if BASE_PROFILE
global_variable b32 GlobalIsProfiling = true;
#else
global_variable b32 GlobalIsProfiling = false;
#endif

//~ Functions
#define ENTRY_POINT(Name) void *Name(entry_point_params *Params)
typedef ENTRY_POINT(entry_point_func);

C_LINKAGE ENTRY_POINT(EntryPoint);

#if OS_LINUX
# include "base_os_linux.h"
#endif

internal str8  OS_ReadEntireFileIntoMemory(char *FileName);
internal b32   OS_FileExists(char *FileName);
internal void  OS_FreeFileMemory(str8 File);
internal b32   OS_WriteEntireFile(char *FileName, str8 File);
internal void  OS_PrintFormat(char *Format, ...) PrintfFunc(1, 2);
internal void  OS_BarrierWait(barrier Barrier);
internal void  OS_SetThreadName(str8 ThreadName);
internal void *OS_AllocateAtOffset(u64 Size, u64 Offset);
internal void *OS_Allocate(u64  Size);
internal void  OS_BarrierWait(barrier Barrier);
internal f64   OS_GetWallClock(void);
internal void  OS_Sleep(u32 MicroSeconds);
internal void  OS_ChangeDirectory(char *Path);
//- OS agnostic, implemented in `base_os.c`.
internal f64  OS_SecondsElapsed(f64 Start, f64 End);
internal f64  OS_MSElapsed(f64 Start, f64 End);
internal void OS_ProfileInit(char *Prefix);
internal void OS_ProfileAndPrint(char *Label);

#define Log(Format, ...)      OS_PrintFormat((char *)(Format), ##__VA_ARGS__)
// NOTE(luca): Append '\n', because this macro might be redefined into a visual error log.
#define ErrorLog(Format, ...) Log(ERROR_FMT Format "\n", ERROR_ARG, ##__VA_ARGS__)

#endif //OS_H
