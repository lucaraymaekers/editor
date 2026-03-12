/* TODO(luca): 
- Cng_PathFromExe should return char *
- Cng_InitAndRebuildSelf should be compressed between OS versions
- Cgn_RunCommand should take a str8_array instead of str8 since that seems to be the more common case
- Provide a path to execute a command asynchronously and check later if it has completed.
- Instead of passing argument "norebuild" set an environment variable
*/

//~ Libraries
// Standard library (TODO(luca): get rid of it) 
#include <stdio.h>
#include <string.h>

#if !defined(CLING_SOURCE_PATH)
# error "You must define CLING_SOURCE_PATH"
#endif
#if !defined(CLING_CODE_PATH)
# error "You must define CLING_CODE_PATH"
#endif
#if !defined(CLING_BUILD_PATH)
# error "You must define CLING_CODE_PATH"
#endif

#include "base/base.h"

//~ Types
struct str8_array
{
    str8 *Strings;
    u64 Count;
    u64 Capacity;
};
typedef struct str8_array str8_array;

//~ Globals
global_variable arena *GlobalClingArena = 0;
global_variable str8_array *GlobalSelectedArray = 0;
global_variable char **GlobalEnv = 0;

//~ Functions
//- Strings
internal void
SetClingArena(arena *Arena)
{
    GlobalClingArena = Arena;
}

internal void
SetSelectedArray(str8_array *Array)
{
    GlobalSelectedArray = Array;
}

internal str8_array *
PushStr8Array(u64 Capacity)
{
    str8_array *Result = PushStruct(GlobalClingArena, str8_array);
    Result->Capacity = Capacity;
    Result->Strings = PushArray(GlobalClingArena, str8, Result->Capacity);
    return Result;
}

#define Str8ArrayPushCount(Array) for(u64 _Count_ = Array->Count, _i_ = 0; \
_i_ == 0; \
_i_ = _i_ + 1, Array->Count = _Count_)

internal b32 
IsWhiteSpace(u8 Char)
{
    b32 Result = (Char == ' ' || Char == '\t' || Char == '\n' || Char == '\r');
    return Result;
}

internal void 
Str8ArrayAppendTo(str8_array *Array, str8 String)
{
    if(String.Size > 0)
    {
        Assert(Array->Count < Array->Capacity);
        Array->Strings[Array->Count] = String;
        Array->Count += 1;
    }
}

internal void
Str8ArrayAppend(str8 String)
{
    Str8ArrayAppendTo(GlobalSelectedArray, String);
}

#define Str8ArrayAppendMultipleTo(Array, ...) \
do \
{ \
str8 Strings[] = {__VA_ARGS__}; \
_Str8ArrayAppendMultiple(Array, ArrayCount(Strings), Strings); \
} while(0);
void _Str8ArrayAppendMultiple(str8_array *Array, u64 Count, str8 *Strings)
{
    for EachIndex(At, Count)
    {
        Str8ArrayAppendTo(Array, Strings[At]);
    }
}

#define Str8ArrayAppendMultiple(...) Str8ArrayAppendMultipleTo(GlobalSelectedArray, __VA_ARGS__) 

internal str8 
Str8ArrayJoinFrom(str8_array *Array, u8 Char)
{
    str8 Result = {};
    
    u64 TotalSize = 0;
    for EachIndex(Idx, Array->Count)
    {
        TotalSize += Array->Strings[Idx].Size;
    }
    
    
    if(Char)
    {
        TotalSize += (Array->Count - 1);
    }
    
    str8 Buffer = PushS8(GlobalClingArena, TotalSize);
    
    u64 BufferIndex = 0;
    for EachIndex(At, Array->Count)
    {
        str8 *StringAt = Array->Strings + At;
        
        MemoryCopy(Buffer.Data + BufferIndex, StringAt->Data, StringAt->Size);
        BufferIndex += StringAt->Size;
        if(Char)
        {
            if(At + 1 != Array->Count)
            {
                Buffer.Data[BufferIndex] = Char;
                BufferIndex += 1;
            }
        }
    }
    
    Result.Data = Buffer.Data;
    Result.Size = BufferIndex;
    
    return Result;
}

internal str8 
Str8ArrayJoin(u8 Char)
{
    str8 Result = Str8ArrayJoinFrom(GlobalSelectedArray, Char);
    return Result;
}

//~ Helpers
internal void 
CommonBuildCommand(b32 GCC, b32 Clang, b32 Debug)
{
    str8_array *Command = GlobalSelectedArray;
    
    // Exclusive arguments
    if(GCC) Clang = false;
    b32 Release = !Debug;
    
#if OS_LINUX
    str8 CommonCompilerFlags = S8("-fsanitize-trap -nostdinc++ -D_GNU_SOURCE=1");
    str8 CommonWarningFlags = S8("-Wall -Wextra -Wconversion -Wdouble-promotion -Wno-sign-conversion -Wno-sign-compare -Wno-double-promotion -Wno-unused-but-set-variable -Wno-unused-variable -Wno-write-strings -Wno-missing-field-initializers -Wno-pointer-arith -Wno-unused-parameter -Wno-unused-function");
    
    str8 LinuxLinkerFlags = S8("-lpthread -lm");
    
    str8 Compiler = {};
    str8 Mode = {};
    
    if(0) {}
    else if(Release)
    {
        Mode = S8("release");
    }
    else if(Debug)
    {
        Mode = S8("debug");
    }
    
    if(0) {}
    else if(Clang)
    {
        Compiler = S8("clang");
        Str8ArrayAppend(Compiler);
        if(Debug)
        {
            Str8ArrayAppend(S8("-g -ggdb -g3"));
        }
        else if(Release)
        {
            Str8ArrayAppend(S8("-O3"));
        }
        Str8ArrayAppend(CommonCompilerFlags);
        Str8ArrayAppend(S8("-fdiagnostics-absolute-paths -ftime-trace"));
        Str8ArrayAppend(CommonWarningFlags);
        Str8ArrayAppend(S8("-Wno-null-dereference -Wno-missing-braces -Wno-vla-cxx-extension -Wno-writable-strings -Wno-missing-designated-field-initializers -Wno-address-of-temporary -Wno-int-to-void-pointer-cast"));
    }
    else if(GCC)
    {
        Compiler = S8("g++");
        Str8ArrayAppend(Compiler);
        
        if(Debug)
        {
            Str8ArrayAppend(S8("-g -ggdb -g3"));
        }
        else if(Release)
        {
            Str8ArrayAppend(S8("-O3"));
        }
        Str8ArrayAppend(CommonCompilerFlags);
        Str8ArrayAppend(CommonWarningFlags);
        Str8ArrayAppend(S8("-Wno-cast-function-type -Wno-missing-field-initializers -Wno-int-to-pointer-cast"));
    }
    
    Str8ArrayAppend(LinuxLinkerFlags);
#elif OS_WINDOWS    
    Str8ArrayAppend(S8("cmd /c \"C:\\msvc\\setup_x64.bat"));
    Str8ArrayAppend(S8("&&"));
    
    Str8ArrayAppend(S8("cl"));
    Str8ArrayAppend(S8("-MTd -Gm- -nologo -GR- -EHa- -Oi -FC -Z7"));
    Str8ArrayAppend(S8("-std:c++20 -Zc:strictStrings-"));
    Str8ArrayAppend(S8("-WX -W4 -wd4459 -wd4456 -wd4201 -wd4100 -wd4101 -wd4189 -wd4505 -wd4996 -wd4389 -wd4244 -wd5287"));
    Str8ArrayAppend(S8("-I" CLING_CODE_PATH));
#endif
}

//~ API
internal str8 Cng_PathFromExe(char *ExePath, str8 Path);
internal str8 Cng_GetBaseFileName(str8 FileName);
internal void Cng_InitAndRebuildSelf(int ArgsCount, char *Args[], char *Env[]);
internal void Cng_RunCommand(str8 Command);

internal str8
Cng_PathFromExe(char *ExePath, str8 Path)
{
	str8 FileName = PushS8(GlobalClingArena, 512);
    
    u32 SizeOfFileName = (u32)StringLength(ExePath);
    MemoryCopy(FileName.Data, ExePath, SizeOfFileName);
    
    u64 OnePastLastSlash = 0;
    for EachIndex(Idx, SizeOfFileName)
    {
        if(ExePath[Idx] == OS_SlashChar)
        {
            OnePastLastSlash = Idx + 1;
        }
    }
    
    FileName.Size = OnePastLastSlash + Path.Size;
    
    u64 At = OnePastLastSlash;
    for EachIndex(Idx, Path.Size)
    {
        FileName.Data[At] = Path.Data[Idx];
        At += 1;
    }
    
    FileName.Data[FileName.Size] = 0;
    
    return FileName;
}

internal str8
Cng_GetBaseFileName(str8 FileName)
{
    str8 Result = {0};
    
    u32 OnePastLastSlash = 0;
    for EachIndex(Idx, FileName.Size)
    {
        if(FileName.Data[Idx] == OS_SlashChar)
        {
            OnePastLastSlash = (u32)Idx + 1;
        }
    }
    
    Result = S8From(FileName, OnePastLastSlash);
    
    return Result;
}

//~ Platform

#if OS_WINDOWS
//~ Constants
// NOTE(luca): Because windows cannot overwrite an opened file, we create a helper executable that will do the rest for us.
#define CLING_TEMP_EXE "cling_temp.exe"

//~ Implementation

internal void 
Cng_RunCommand(str8 Command)
{
    STARTUPINFOA StartupInfo = {0};
    StartupInfo.cb = sizeof(StartupInfo);
    
    
    PROCESS_INFORMATION ProcessInfo = {0};
    
    u8 *CommandCString = PushArray(GlobalClingArena, u8, Command.Size + 1);
    MemoryCopy(CommandCString, Command.Data, Command.Size);
    CommandCString[Command.Size] = 0;
    
    b32 Succeeded = CreateProcessA(0, 
                                   (char *)CommandCString, 
                                   0,
                                   0,
                                   false, // Inherit handle 
                                   0,
                                   0, 
                                   0,
                                   &StartupInfo, &ProcessInfo);
    if(Succeeded)
    {
        WaitForSingleObject(ProcessInfo.hProcess, INFINITE);
        CloseHandle(ProcessInfo.hProcess);
        CloseHandle(ProcessInfo.hThread);
    } 
    else 
    {
        Win32LogIfError();
        Log("Command: %s\n", CommandCString);
    }
}

internal void
Cng_InitAndRebuildSelf(int ArgsCount, char *Args[], char *Env[])
{
    GlobalEnv = Env;
    StringsScratch = ArenaAlloc();
    SetClingArena(ArenaAlloc());
    
    b32 Rebuild = true;
    for(int ArgsIndex = 1;
        ArgsIndex < ArgsCount;
        ArgsIndex++)
    {
        if(!strcmp(Args[ArgsIndex], "norebuild"))
        {
            Rebuild = false;
        }
    }
    
    char *ExePath = 0;
    {
        str8 Path = PushS8(GlobalClingArena, MAX_PATH);
        Path.Size = GetModuleFileNameA(0, (char *)Path.Data, Path.Size);
        Path.Data[Path.Size] = 0;
        ExePath = (char *)Path.Data;
    }
    
    str8 OutputFileName = Cng_PathFromExe(ExePath, S8(CLING_BUILD_PATH CLING_TEMP_EXE));
    str8 BuildDirPath = Cng_PathFromExe(ExePath, S8(CLING_BUILD_PATH));
    OS_ChangeDirectory((char *)BuildDirPath.Data);
    
    if(Rebuild)
    {
        str8 ClingSourcePath = Cng_PathFromExe(ExePath, S8(CLING_SOURCE_PATH));
        str8 ClingCodePath = Cng_PathFromExe(ExePath, S8(CLING_CODE_PATH));
        
        // Build self 
        {
            Log("[self build]\n");
            
            SetSelectedArray(PushStr8Array(256));
            CommonBuildCommand(false, true, true);
            
            Str8ArrayAppendMultiple(S8("-I"), ClingCodePath);
            
            Str8ArrayAppend(ClingSourcePath);
            
            Str8ArrayAppend(S8Cat(S8("/link /out:"), OutputFileName));
            
            str8 BuildCommand = Str8ArrayJoin(' ');
            Cng_RunCommand(BuildCommand);
        }
        
        // Run new cling.exe
        {
            SetSelectedArray(PushStr8Array(256));
            
            Str8ArrayAppend(OutputFileName);
            
            for(s32 At = 1;
                At < ArgsCount;
                At++)
            {
                // Skip the rebuilding argument
                if(strcmp(Args[At], "rebuild"))
                {
                    str8 Arg = S8FromCString(Args[At]);
                    if(Arg.Size) Str8ArrayAppend(Arg); 
                }
            }
            
            Str8ArrayAppend(S8("norebuild"));
            
            str8 Command = Str8ArrayJoin(' ');
            Cng_RunCommand(Command);
        }
        
        ExitProcess(0);
    }
}

#elif OS_LINUX

#include <unistd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/limits.h>

//- Error wrappers 
internal void 
LinuxErrorWrapperPipe(int Pipe[2])
{
    int Ret = pipe(Pipe);
    if(Ret == -1)
    {
        perror("pipe");
        Assert(0);
    }
}

internal void 
LinuxErrorWrapperDup2(int OldFile, int NewFile)
{
    int Ret = dup2(OldFile, NewFile);
    if(Ret == -1)
    {
        perror("dup2");
        Assert(0);
    }
}

internal int 
LinuxErrorWrapperGetAvailableBytesToRead(int File)
{
    int BytesToRead = 0;
    
    int Ret = ioctl(File, FIONREAD, &BytesToRead);
    if(Ret == -1)
    {
        perror("read(ioctl)");
        Assert(0);
    }
    
    return BytesToRead;
}

internal smm 
LinuxErrorWrapperRead(int File, void *Buffer, umm BytesToRead)
{
    smm BytesRead = read(File, Buffer, BytesToRead);
    if(BytesRead == -1)
    {
        perror("read");
        Assert(0);
    }
    Assert(BytesRead == BytesRead);
    
    return BytesRead;
}

internal str8 
LinuxFindCommandInPATH(umm BufferSize, u8 *Buffer, char *Command, char *Env[])
{
    char **VarAt = Env;
    char Search[] = "PATH=";
    int MatchedSearch = false;
    
    str8 Result = {};
    Result.Data = Buffer;
    
    while(*VarAt && !MatchedSearch)
    {
        MatchedSearch = true;
        
        for(u32 At = 0;
            (At < sizeof(Search) - 1) && (VarAt[At]);
            At++)
        {
            if(Search[At] != VarAt[0][At])
            {
                MatchedSearch = false;
                break;
            }
        }
        
        VarAt++;
    }
    
    if(MatchedSearch)
    {
        VarAt--;
        char *Scan = VarAt[0];
        while(*Scan && *Scan != '=') Scan++;
        Scan++;
        
        while((*Scan) && (Scan != VarAt[1]))
        {
            int Len = 0;
            while(Scan[Len] && Scan[Len] != ':' && 
                  (Scan+Len != VarAt[1])) Len++;
            
            // Add the PATH entry
            int At = 0;
            for(; At < Len; At += 1)
            {
                Result.Data[At] = Scan[At];
            }
            Result.Data[At++] = '/';
            
            // Add the executable name
            for(char *CharAt = Command;
                *CharAt;
                CharAt++)
            {
                Result.Data[At++] = *CharAt;
            }
            Result.Data[At] = 0;
            
            // Check if it exists
            int AccessMode = F_OK | X_OK;
            int Ret = access((char *)Result.Data, AccessMode);
            if(Ret == 0)
            {
                Result.Size = At;
                break;
            }
            
            Scan += Len + 1;
        }
    }
    
    return Result;
}

internal void 
LinuxRunCommand(char *Args[])
{
    int Ret = 0;
    int WaitStatus = 0;
    b32 Error = false;
    
    pid_t ChildPID = fork();
    if(ChildPID != -1)
    {
        if(ChildPID == 0)
        {
            if(execve(Args[0], Args, __environ) == -1)
            {
                perror("exec");
            }
        }
        else
        {
            wait(&WaitStatus);
        }
    }
    else
    {
        perror("fork");
    }
    
}

//- OS implementation

internal void 
Cng_RunCommand(str8 Command)
{
    char *Args[64] = {};
    
    u8 ArgsBuffer[1024] = {};
    u64 ArgsBufferIndex = 0;
    
    // 1. split on whitespace into null-terminated strings.
    //    TODO: skip quotes
    u32 ArgsCount = 0;
    
    for(u64 At = 0; At < Command.Size;)
    {
        u64 Start = At;
        while(!IsWhiteSpace(Command.Data[At]) && At < Command.Size) At += 1;
        
        u64 End = At;
        
        // Set pointer to ArgsBuffer into Args
        {            
            Args[ArgsCount] = (char *)(ArgsBuffer + ArgsBufferIndex);
            ArgsCount += 1;
            Assert(ArgsCount < ArrayCount(Args));
        }
        
        // Create null terminated string and copy it into ArgsBuffer
        {            
            u64 Size = End - Start;
            MemoryCopy(ArgsBuffer + ArgsBufferIndex, Command.Data + Start, Size);
            
            ArgsBufferIndex += Size;
            ArgsBuffer[ArgsBufferIndex] = 0;
            ArgsBufferIndex += 1;
            Assert(ArgsBufferIndex < ArrayCount(ArgsBuffer));
        }
        
        while(IsWhiteSpace(Command.Data[At]) && At < Command.Size) At += 1;
    }
    
    if(Args[0] && Args[0][0] != '/')
    {
        u8 Buffer[PATH_MAX] = {};
        str8 ExePath = LinuxFindCommandInPATH(sizeof(Buffer), Buffer, Args[0], GlobalEnv);
        if(ExePath.Size)
        {
            Args[0] = (char *)ExePath.Data;
        }
    }
    
    LinuxRunCommand(Args);
}

internal void
Cng_InitAndRebuildSelf(int ArgsCount, char *Args[], char *Env[])
{
    GlobalEnv = Env;
    StringsScratch = ArenaAlloc();
    SetClingArena(ArenaAlloc());
    
    arena *Arena = GlobalClingArena;
    str8 OutputBuffer = PushS8(Arena, KB(2));
    str8 TempBuffer = PushS8(Arena, KB(2));
    
    char *CommandName = Args[0];
    b32 Rebuild = true;
    for(int ArgsIndex = 1;
        ArgsIndex < ArgsCount;
        ArgsIndex++)
    {
        if(!strcmp(Args[ArgsIndex], "norebuild"))
        {
            Rebuild = false;
        }
    }
    
    if(Rebuild)
    {
        Log("[self build]\n");
        
        SetSelectedArray(PushStr8Array(256));
        CommonBuildCommand(false, true, true);
        
        str8 ClingSourcePath = Cng_PathFromExe(CommandName, S8(CLING_SOURCE_PATH));
        str8 ClingCodePath = Cng_PathFromExe(CommandName, S8(CLING_CODE_PATH));
        
        Log(S8Fmt "\n", S8Arg(Cng_GetBaseFileName(ClingSourcePath)));
        
        Str8ArrayAppendMultiple(S8("-o"), S8FromCString(CommandName),
                                S8("-I"), ClingCodePath,
                                ClingSourcePath);
        str8 BuildCommand = Str8ArrayJoin(' ');
        
        //Log("%*s\n", (int)BuildCommand.Size, BuildCommand.Data);
        
        Cng_RunCommand(BuildCommand);
        
        // Run without rebuilding
        char *Arguments[64] = {};
        u32 At;
        for(At = 1;
            At < ArgsCount;
            At++)
        {
            if(strcmp(Args[At], "rebuild"))
            {
                Arguments[At] = Args[At];
            }
        }
        
        Arguments[0] = CommandName;
        Arguments[At++] = "norebuild";
        Assert(At < ArrayCount(Arguments));
        
        LinuxRunCommand(Arguments);
        
        _exit(0);
    }
    
}

#endif