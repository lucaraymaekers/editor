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

#define CLING_ARG_NOREBUILD "CLING_NOREBUILD"
#define CLING_ARG_PARENTPID "CLING_PARENTPID"

#include "base/base.h"

//~ Types
struct str8_array
{
 str8 *Strings;
 u64 Count;
 u64 Capacity;
};
typedef struct str8_array str8_array;

enum cng_os_kind
{
 CngOS_Windows,
 CngOS_Linux,
};
typedef enum cng_os_kind cng_os_kind;

typedef struct table_hash_node table_hash_node;
struct table_hash_node
{
 MD_Node *Table;
 
 u64 Key;
 table_hash_node *Next;
};

typedef struct field_hash_node field_hash_node; 
struct field_hash_node
{
 u64 Idx;
 
 u64 Key;
 field_hash_node *Next;
};

//~ Globals
global_variable arena *GlobalClingArena = 0;
global_variable str8_array *GlobalSelectedArray = 0;
global_variable char **GlobalEnv = 0;
global_variable cng_os_kind GlobalCurrentOS = 0;

//~ Platform API
// NOTE(luca): Implement these when adding a new platform.
internal void Cng_InitAndRebuildSelf(int ArgsCount, char *Args[], char *Env[]);
internal void Cng_RunCommand(str8 Command);

//~ Functions
internal b32
Cng_IsOs(cng_os_kind Kind)
{
 b32 Result = (GlobalCurrentOS == Kind);
 return Result;
}

//- Strings
internal void
Cng_SetArena(arena *Arena)
{
 GlobalClingArena = Arena;
}

internal void
Cng_SetSelectedArray(str8_array *Array)
{
 GlobalSelectedArray = Array;
}

internal str8_array *
Cng_PushStr8Array(u64 Capacity)
{
 str8_array *Result = PushArray(GlobalClingArena, str8_array, 1);
 Result->Capacity = Capacity;
 Result->Strings = PushArray(GlobalClingArena, str8, Result->Capacity);
 return Result;
}

#define Cng_Str8ArrayPushCount(Array) for(u64 _Count_ = Array->Count, _i_ = 0; \
_i_ == 0; \
_i_ = _i_ + 1, Array->Count = _Count_)

internal void 
Cng_Str8ArrayAppendTo(str8_array *Array, str8 String)
{
 if(String.Size > 0)
 {
  Assert(Array->Count < Array->Capacity);
  Array->Strings[Array->Count] = String;
  Array->Count += 1;
 }
}

internal void
Cng_Str8ArrayAppend(str8 String)
{
 Cng_Str8ArrayAppendTo(GlobalSelectedArray, String);
}

#define Cng_Str8ArrayAppendMultipleTo(Array, ...) \
do \
{ \
str8 Strings[] = {__VA_ARGS__}; \
_Cng_Str8ArrayAppendMultiple(Array, ArrayCount(Strings), Strings); \
} while(0);
void _Cng_Str8ArrayAppendMultiple(str8_array *Array, u64 Count, str8 *Strings)
{
 for EachIndex(At, Count)
 {
  Cng_Str8ArrayAppendTo(Array, Strings[At]);
 }
}

#define Cng_Str8ArrayAppendMultiple(...) Cng_Str8ArrayAppendMultipleTo(GlobalSelectedArray, __VA_ARGS__) 

internal str8 
Cng_Str8ArrayJoinFrom(str8_array *Array, u8 Char)
{
 str8 Result = {0};
 
 if(Array)
 {    
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
 }
 
 
 return Result;
}

internal str8 
Cng_Str8ArrayJoin(u8 Char)
{
 str8 Result = Cng_Str8ArrayJoinFrom(GlobalSelectedArray, Char);
 return Result;
}

//~ Helpers
internal void 
Cng_CommonBuildCommand(b32 GCC, b32 Clang, b32 Debug, b32 Asan)
{
 // Exclusive arguments
 if(GCC) Clang = false;
 b32 Release = !Debug;
 
#if OS_LINUX
 str8 CommonCompilerFlags = S8("-fno-threadsafe-statics -nostdinc++ -D_GNU_SOURCE=1 -fno-exceptions -fno-rtti");
 // TODO(luca): nasr should fix his enums, so we can enable -Wswitch again.
 str8 CommonWarningFlags = S8("-Wall -Wextra -Wconversion -Wswitch -Wshadow " 
                              "-Wno-double-promotion -Wno-unused-but-set-variable -Wno-write-strings -Wno-pointer-arith "
                              "-Wno-missing-field-initializers "
                              "-Wno-initializer-overrides "
                              "-Wno-unused-parameter "
                              "-Wno-unused-variable "
                              "-Wno-unused-function "
                              "-Wno-unused-command-line-argument ");
 
 str8 LinkerFlags = S8("-lm");
 
 str8 Compiler = {0};
 if(0) {}
 else if(Clang) Compiler = S8("clang");
 else if(GCC) Compiler = S8("gcc");
 
 str8 ModeFlags = (Debug ? 
                   S8("-g -ggdb -g3 -fno-omit-frame-pointer") :
                   S8("-O3"));
 
 
 str8 Mode = {0};
 
 if(0) {}
 else if(Release)
 {
  Mode = S8("release");
 }
 else if(Debug)
 {
  Mode = S8("debug");
 }
 
 if(Asan)
 {
  Cng_Str8ArrayAppend(S8("-fsanitize=undefined,address"));
 }
 
 if(0) {}
 else if(Clang)
 {
  Compiler = S8("clang");
  Cng_Str8ArrayAppend(Compiler);
  if(Debug)
  {
   Cng_Str8ArrayAppend(S8("-g -ggdb -g3"));
  }
  else if(Release)
  {
   Cng_Str8ArrayAppend(S8("-O3"));
  }
  Cng_Str8ArrayAppend(CommonCompilerFlags);
  Cng_Str8ArrayAppend(S8("-fdiagnostics-absolute-paths -ftime-trace"));
  Cng_Str8ArrayAppend(CommonWarningFlags);
  Cng_Str8ArrayAppend(S8("-Wno-null-dereference -Wno-missing-braces -Wno-vla-cxx-extension -Wno-writable-strings -Wno-missing-designated-field-initializers -Wno-address-of-temporary -Wno-int-to-void-pointer-cast"));
 }
 else if(GCC)
 {
  Compiler = S8("g++");
  Cng_Str8ArrayAppend(Compiler);
  
  Cng_Str8ArrayAppend(S8("-Wno-cast-function-type -Wno-missing-field-initializers -Wno-int-to-pointer-cast"));
  
  if(Debug)
  {
   Cng_Str8ArrayAppend(S8("-g -ggdb -g3"));
  }
  else if(Release)
  {
   Cng_Str8ArrayAppend(S8("-O3"));
  }
  Cng_Str8ArrayAppend(CommonCompilerFlags);
  Cng_Str8ArrayAppend(CommonWarningFlags);
  Cng_Str8ArrayAppend(S8("-Wno-cast-function-type -Wno-missing-field-initializers -Wno-int-to-pointer-cast"));
 }
 
 Cng_Str8ArrayAppend(LinuxLinkerFlags);
#elif OS_WINDOWS    
 
#if defined(CLING_MSVC_SETUP)
 Cng_Str8ArrayAppend(S8("cmd /c \"" CLING_MSVC_SETUP));
 Cng_Str8ArrayAppend(S8("&&"));
#endif
 
 Cng_Str8ArrayAppend(S8("cl"));
 Cng_Str8ArrayAppend(S8("-MTd -Gm- -nologo -GR- -EHa- -Oi -FC -Z7"));
 Cng_Str8ArrayAppend(S8("-Zc:strictStrings-"));
 Cng_Str8ArrayAppend(S8("-WX -W4 -wd4459 -wd4456 -wd4201 -wd4100 -wd4101 -wd4189 -wd4505 -wd4996 -wd4389 -wd4244 -wd5287"));
 Cng_Str8ArrayAppend(S8("-I" CLING_CODE_PATH));
#endif
}

internal str8
Cng_PathFromExe(str8 ExePath, str8 Path)
{
	str8 FileName = PushS8(GlobalClingArena, 512);
 
 MemoryCopy(FileName.Data, ExePath.Data, ExePath.Size);
 
 u64 OnePastLastSlash = 0;
 for EachIndex(Idx, ExePath.Size)
 {
  if(ExePath.Data[Idx] == OS_SlashChar)
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
 
 u64 OnePastLastSlash = 0;
 for EachIndex(Idx, FileName.Size)
 {
  if(FileName.Data[Idx] == OS_SlashChar)
  {
   OnePastLastSlash = Idx + 1;
  }
 }
 
 Result = S8From(FileName, OnePastLastSlash);
 
 return Result;
}

//~ Platform

#if OS_WINDOWS
//~ Constants
// NOTE(luca): Because windows cannot overwrite an opened file, we create a helper executable that will do the rest for us.
#define CLING_EXE "cling.exe"
#define CLING_TEMP_EXE "cling_temp.exe"

//~ Implementation

internal void 
Cng_RunCommandAsync(str8 Command, b32 Async)
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
  if(!Async)
  {
   WaitForSingleObject(ProcessInfo.hProcess, INFINITE);
  }
  CloseHandle(ProcessInfo.hProcess);
  CloseHandle(ProcessInfo.hThread);
 } 
 else 
 {
  Win32Log();
  Log("Command: %s\n", CommandCString);
 }
}

internal void
Cng_RunCommand(str8 Command)
{
 Cng_RunCommandAsync(Command, false);
}

internal str8
Win32GetEnvironmentVar(char *Name)
{
 u64 MaxSize = KB(2);
 str8 Result = PushS8(GlobalClingArena, MaxSize);
 
 DWORD Size = GetEnvironmentVariableA(Name, (char *)Result.Data, Result.Size);
 Result.Size = Size;
 
 ArenaFree(GlobalClingArena, MaxSize - Size);
 
 return Result;
}

internal void
Cng_InitAndRebuildSelf(int ArgsCount, char *Args[], char *Env[])
{
 // Globals
 {
  GlobalCurrentOS = CngOS_Windows;
  GlobalEnv = Env;
  SetStringsScratch(ArenaAlloc());
  Cng_SetArena(ArenaAlloc());
 }
 
 str8 NoRebuildString = Win32GetEnvironmentVar(CLING_ARG_NOREBUILD);
 str8 ParentPidString = Win32GetEnvironmentVar(CLING_ARG_PARENTPID);
 
 str8 ExePath = {0};
 {
  ExePath = PushS8(GlobalClingArena, MAX_PATH);
  ExePath.Size = GetModuleFileNameA(0, (char *)ExePath.Data, ExePath.Size);
  ExePath.Data[ExePath.Size] = 0;
 }
 
 // NOTE(luca): Wait on parent to close so we can write to the file.
 { 
  DWORD ParentPid = 0;
  // Convert PidString to number 
  {
   for EachIndex(Idx, ParentPidString.Size)
   {
    u8 Digit = ParentPidString.Data[Idx] - '0';
    ParentPid *= 10;
    ParentPid += Digit;
   }
  }
  
  if(ParentPid != 0)
  {
   HANDLE Parent = OpenProcess(SYNCHRONIZE, false, ParentPid);
   if(Parent != 0)
   {
    DWORD Result = WaitForSingleObject(Parent, INFINITE);
    CloseHandle(Parent);
   }
   else
   {
    // NOTE(luca): This will mostly result in incorrect parameter if the parent has already exit.
    //Win32Log();
   }
  }
 }
 
 // Rebuild self
 {
  // NOTE(luca): These are the operations that need to happen.
  // old: copy self to temp
  // old: run temp and exit
  // temp: make new replacing old
  // temp: run new and exit
  // new: delete temp
  
  b32 IsOld = false;
  b32 IsTemp = false;
  
  str8 NewPath = Cng_PathFromExe(ExePath, S8(CLING_EXE));
  str8 TempPath = Cng_PathFromExe(ExePath, S8(CLING_TEMP_EXE));
  
  IsTemp = S8Match(TempPath, ExePath, false);
  IsOld = (NoRebuildString.Size == 0);
  str8 RunPath = {0};
  
  if(IsTemp)
  {
   // Make new
   {
    str8 BuildDirPath = Cng_PathFromExe(ExePath, S8(CLING_BUILD_PATH));
    str8 ClingSourcePath = Cng_PathFromExe(ExePath, S8(CLING_SOURCE_PATH));
    str8 ClingCodePath = Cng_PathFromExe(ExePath, S8(CLING_CODE_PATH));
    
    Log("[self build]\n");
    
    // NOTE(luca): So that build artifacts end up in the right directory.
    OS_ChangeDirectory((char *)BuildDirPath.Data);
    
    Cng_SetSelectedArray(Cng_PushStr8Array(256));
    Cng_CommonBuildCommand(false, true, true, false);
    
    Cng_Str8ArrayAppendMultiple(S8("-I"), ClingCodePath);
    
    Cng_Str8ArrayAppend(ClingSourcePath);
    
    Cng_Str8ArrayAppend(S8Cat(S8("/link -opt:ref -incremental:no /out:"), NewPath));
    
    str8 BuildCommand = Cng_Str8ArrayJoin(' ');
    Cng_RunCommand(BuildCommand);
   }
   
   RunPath = NewPath;
  }
  else if(IsOld)
  {
   // Copy to temp
   Win32LogIf(!CopyFile((char *)NewPath.Data, 
                        (char *)TempPath.Data,
                        
                        false));
   RunPath = TempPath;
  }
  else
  {
   // Delete temp
   DeleteFile((char *)TempPath.Data);
  }
  
  if(IsOld || IsTemp)
  {
   // Run temp or new depending on RunPath
   {
    DWORD ParentPid = GetCurrentProcessId();
    char *ParentPidCString = (char *)Str8Fmt("%u", ParentPid).Data;
    SetEnvironmentVariableA(CLING_ARG_PARENTPID, ParentPidCString);
    
    if(IsTemp) SetEnvironmentVariableA(CLING_ARG_NOREBUILD, "1");
   }
   
   Cng_RunCommandAsync(RunPath, true);
   
   ExitProcess(0);
  }
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
 
 str8 Result = {0};
 Result.Data = Buffer;
 
 while(*VarAt && !MatchedSearch)
 {
  MatchedSearch = true;
  
  for(u64 At = 0;
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
 char *Args[64] = {0};
 
 u8 ArgsBuffer[1024] = {0};
 u64 ArgsBufferIndex = 0;
 
 // 1. split on whitespace into null-terminated strings.
 //    TODO: skip quotes
 s32 ArgsCount = 0;
 
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
  u8 Buffer[PATH_MAX] = {0};
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
 // Globals
 {
#if OS_WINDOWS
  GlobalCurrentOS = CngOS_Windows;
#elif OS_LINUX
  GlobalCurrentOS = CngOS_Linux;
#endif
  GlobalEnv = Env;
  SetStringsScratch(ArenaAlloc());
  Cng_SetArena(ArenaAlloc());
 }
 
 arena *Arena = GlobalClingArena;
 str8 OutputBuffer = PushS8(Arena, KB(2));
 str8 TempBuffer = PushS8(Arena, KB(2));
 
 str8 CommandName = S8FromCString(Args[0]);
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
  
  Cng_SetSelectedArray(Cng_PushStr8Array(256));
  Cng_CommonBuildCommand(false, true, true, false);
  
  str8 ClingSourcePath = Cng_PathFromExe(CommandName, S8(CLING_SOURCE_PATH));
  str8 ClingCodePath = Cng_PathFromExe(CommandName, S8(CLING_CODE_PATH));
  
  Log("%S\n", Cng_GetBaseFileName(ClingSourcePath));
  
  Cng_Str8ArrayAppendMultiple(S8("-o"), CommandName,
                              S8("-I"), ClingCodePath,
                              ClingSourcePath);
  str8 BuildCommand = Cng_Str8ArrayJoin(' ');
  
  //Log("%*s\n", (int)BuildCommand.Size, BuildCommand.Data);
  
  Cng_RunCommand(BuildCommand);
  
  // Run without rebuilding
  char *Arguments[64] = {0};
  s32 At;
  for(At = 1;
      At < ArgsCount;
      At++)
  {
   if(strcmp(Args[At], "rebuild"))
   {
    Arguments[At] = Args[At];
   }
  }
  
  Arguments[0] = (char *)CommandName.Data;
  Arguments[At++] = "norebuild";
  Assert(At < ArrayCount(Arguments));
  
  LinuxRunCommand(Arguments);
  
  _exit(0);
 }
 
}

#endif