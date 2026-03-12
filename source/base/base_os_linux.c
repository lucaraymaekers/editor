// Standard
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Linux
#include <errno.h>
#include <linux/prctl.h> 
#include <linux/limits.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
// UNIX & POSIX
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <link.h>

#include "base_arenas.h"
#include "base_os_linux_errno_to_str8.c"

#define ERRNO_FMT "Errno(%d): " S8Fmt
#define ERRNO_ARG errno, S8Arg(ErrnoToStr8(errno))

//~ Types
typedef void *pthread_entry_point_func(void *);

//~ Helpers
internal s64
LinuxTimeSpecToNS(struct timespec Counter)
{
    s64 Result = (s64)Counter.tv_sec*1000000000 + (s64)Counter.tv_nsec;
    return Result;
}

//~ Syscalls
#define AssertErrno(Expression) do { if(!(Expression)) { TrapMsg(ERRNO_FMT, ERRNO_ARG); }; } while(0)

internal b32
OS_FileExists(char *FileName)
{
	b32 Result = false;
	int AccessMode = F_OK;
 Result = (access(FileName, AccessMode) == 0);
	return Result;
}

internal str8 
OS_ReadEntireFileIntoMemory(char *FileName)
{
    str8 Result = {};
    
    if(FileName)
    {
        int File = open(FileName, O_RDONLY);
        
        if(File != -1)
        {
            struct stat StatBuffer = {};
            int Error = fstat(File, &StatBuffer);
            AssertErrno(Error != -1);
            
            Result.Size = (u64 )StatBuffer.st_size;
            
            if(Result.Size != 0)
            {                
                Result.Data = (u8 *)mmap(0, Result.Size, PROT_READ, MAP_PRIVATE, File, 0);
                AssertErrno(Result.Data != MAP_FAILED);
            }
            
            close(File);
        }
        else
        {
            DebugBreak();
            ErrorLog("Could not read file '%s', " ERRNO_FMT, FileName, ERRNO_ARG);
        }
        
    }
    
    return Result;
}

internal void
OS_FreeFileMemory(str8 File)
{
    munmap(File.Data, File.Size);
}

internal b32
OS_WriteEntireFile(char *FileName, str8 File)
{
    b32 Result = false;
    
    int Handle = open(FileName, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if(Handle != -1)
    {
        smm BytesWritten = write(Handle, File.Data, File.Size);
        if(BytesWritten == (smm)File.Size)
        {
            Result = true;
        }
        else
        {
            ErrorLog("Could not write to '%s'.", FileName);
        }
    }
    else
    {
        ErrorLog("Could not open '%s' for writing.", FileName);
    }
    
    return Result;
}

internal void 
OS_PrintFormat(char *Format, ...)
{
    va_list Args;
    va_start(Args, Format);
#if 0 
    // TODO(luca): stb_sprintf would be nice, but takes long time to compile.
    int Length = vsprintf((char *)LogBuffer, Format, Args);
    smm BytesWritten = write(STDOUT_FILENO, LogBuffer, Length);
    AssertErrno(BytesWritten == Length);
#else
    vprintf(Format, Args);
#endif
}

//~ Threads
internal void 
OS_BarrierWait(barrier Barrier)
{
    s32 Ret = pthread_barrier_wait((pthread_barrier_t *)Barrier);
    
    AssertErrno(Ret == 0 || Ret == PTHREAD_BARRIER_SERIAL_THREAD);
}

internal void 
OS_SetThreadName(str8 ThreadName)
{
    Assert(ThreadName.Size <= 16 -1);
    s32 Ret = prctl(PR_SET_NAME, ThreadName.Data);
    AssertErrno(Ret != -1);
}

internal void *
OS_AllocateAtOffset(u64 Size, u64 Offset)
{
    void *Result = mmap((void *)Offset, Size, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    return Result;
}

internal void *
OS_Allocate(u64 Size)
{
    void *Result = OS_AllocateAtOffset(Size, 0);
    return Result;
}

internal f64 
OS_GetWallClock(void)
{
    f64 Result = 0;
    
    struct timespec Counter;
    clock_gettime(CLOCK_MONOTONIC, &Counter);
    Result = (f64)LinuxTimeSpecToNS(Counter)*1e-9;
    
    return Result;
}

internal void
OS_Sleep(u32 MicroSeconds)
{
    usleep(MicroSeconds);
}

internal void
OS_ChangeDirectory(char *Path)
{
    if(chdir(Path) == -1)
    {
        perror("chdir");
    }
}

//~ Entrypoint
global_variable thread_context *DEBUGThreadContext; 

ENTRY_POINT(ThreadInitEntryPoint)
{
    ThreadInit(&Params->Context);
    DEBUGThreadContext = ThreadContext;
#if !BASE_NO_ENTRYPOINT
    EntryPoint(Params);
#endif
    
    return 0;
}

internal void
LinuxSigHandler(int Signal, siginfo_t *Info, void *Arg)
{
    Log("\nSignal received: %s (%d). The process is terminating.\n", strsignal(Signal), Signal);
#if !ANDROID
    Log("Callstack:\n");
    
    void *IPs[4096] = {0};
    int IPsCount = backtrace(IPs, ArrayCount(IPs));
    
    for EachIndex(Idx, IPsCount)
    {
        Dl_info Info = {0};
        dladdr(IPs[Idx], &Info);
        char CMD[2048];
        snprintf(CMD, sizeof(CMD), "llvm-symbolizer --relative-address -f -e %s %lu", Info.dli_fname, (unsigned long)IPs[Idx] - (unsigned long)Info.dli_fbase);
        FILE *Out = popen(CMD, "r");
        if(Out)
        {
            char FuncName[256], FileName[256];
            if(fgets(FuncName, sizeof(FuncName), Out) && fgets(FileName, sizeof(FileName), Out))
            {
                str8 Func = S8FromCString(FuncName);
                if(Func.Size > 0) Func.Size -= 1;
                str8 Module = S8SkipLastSlash(S8FromCString(Info.dli_fname));
                str8 File = S8SkipLastSlash(S8FromCString(FileName));
                if(File.Size > 0) File.Size -= 1;
                Log("%lu. " S8Fmt ", " S8Fmt " " S8Fmt "\n",
                    Idx, S8Arg(Module), S8Arg(Func), S8Arg(File));
            }
        }
    }
    
    _exit(1);
#endif
}

void
LinuxSetDebuggerAttached()
{
    s32 TracerPid = 0;
    
    u8 FileBuffer[KB(2)] = {0};
    int File = open("/proc/self/status", O_RDONLY);
    smm Size = read(File, FileBuffer, sizeof(FileBuffer));
    str8 Out = {FileBuffer, (u64 )Size};
    
    str8 TracerPidKey = S8("TracerPid:\t");
    
    for EachIndex(Idx, Out.Size)
    {
        str8 Search = S8From(Out, Idx);
        if(S8Match(Search, TracerPidKey, true))
        {
            Idx += TracerPidKey.Size;
            
            while(Idx < Out.Size && 
                  (Out.Data[Idx] >= '0' && Out.Data[Idx] <= '9') &&
                  Out.Data[Idx] != '\n')
            {
                s32 Digit = (s32)(Out.Data[Idx] - '0');
                TracerPid = 10*TracerPid + Digit;
                Idx += 1;
            }
            
            break;
        }
        
        while(Idx < Out.Size && Out.Data[Idx] != '\n') Idx += 1;
    }
    
    GlobalDebuggerIsAttached = !!TracerPid;
}

internal void 
LinuxMainEntryPoint(int ArgsCount, char **Args, char **Env)
{
    LinuxSetDebuggerAttached();
    
    // Install signal handler for crash with callstacks
    {
        struct sigaction Handler = {0};
        Handler.sa_sigaction = LinuxSigHandler;
        Handler.sa_flags = SA_SIGINFO;
        sigfillset(&Handler.sa_mask);
        sigaction(SIGILL, &Handler, NULL);
        sigaction(SIGTRAP, &Handler, NULL);
        sigaction(SIGABRT, &Handler, NULL);
        sigaction(SIGFPE, &Handler, NULL);
        sigaction(SIGBUS, &Handler, NULL);
        sigaction(SIGSEGV, &Handler, NULL);
        sigaction(SIGQUIT, &Handler, NULL);
    }
    
    arena *Arena = ArenaAlloc();
    
    char ThreadName[16] = "Main";
    
#if BASE_FORCE_THREADS_COUNT
    s64 ThreadsCount = BASE_FORCE_THREADS_COUNT;
#else
    s64 ThreadsCount = get_nprocs();
#endif
    
    os_thread *Threads = PushArray(Arena, os_thread, (u64)ThreadsCount);
    s32 Ret = 0;
    
    Ret = prctl(PR_SET_NAME, ThreadName);
    AssertErrno(Ret != -1);
    
    u64 SharedStorage = 0;
    
    barrier Barrier = (barrier)PushStruct(Arena, pthread_barrier_t);
    
    Ret = pthread_barrier_init((pthread_barrier_t *)Barrier, 0, (u32)ThreadsCount);
    Assert(Ret == 0);
    
    for EachIndex(Idx, ThreadsCount)
    {
        entry_point_params *Params = &Threads[Idx].Params;
        Params->Context.LaneIndex = Idx;
        Params->Context.LaneCount = ThreadsCount;
        Params->Context.Barrier   = Barrier;
        Params->Context.SharedStorage = &SharedStorage;
        Params->Args = Args;
        Params->Env = Env;
        Params->ArgsCount = ArgsCount;
        
        pthread_t Handle;
        Ret = pthread_create(&Handle, 0, (pthread_entry_point_func *)ThreadInitEntryPoint, Params);
        Assert(Ret == 0);
        Params->Context.Handle = Handle;
    }
    
    for EachIndex(Idx, ThreadsCount)
    {
        Ret = pthread_join(Threads[Idx].Params.Context.Handle, &Threads[Idx].Result);
        Assert(Ret == 0);
    }
    
}

internal void 
AndroidMainEntryPoint(int ArgsCount, char **Args)
{
    entry_point_params Params = {0};
    Params.ArgsCount = ArgsCount;
    Params.Args = Args;
    Params.Context.LaneIndex = 0;
    Params.Context.LaneCount = 1;
    
    ThreadInitEntryPoint(&Params);
}

#if !BASE_NO_ENTRYPOINT
int main(int ArgsCount, char **Args, char **Env)
{
#if OS_ANDROID
    AndroidMainEntryPoint(ArgsCount, Args);
#else
    LinuxMainEntryPoint(ArgsCount, Args, Env);
#endif
    return 0;
}
#endif
