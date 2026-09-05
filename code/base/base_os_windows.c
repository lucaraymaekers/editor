#include "base_os_windows.h"

global_variable f64 GlobalPerfCountFrequency;
global_variable thread_context *DEBUGThreadContext; 

//- Helpers 

internal inline u32
SafeTruncateU64(u64 Value)
{
 Assert(Value <= (u64)~0);
 u32 Result = (u32)Value;
 return(Result);
}

#define Win32Log() Win32Log_(__FILE__, __LINE__)
#define Win32LogIf(Expression) do { if(Expression) Win32Log(); } while(0)
internal void 
Win32Log_(char *File, s32 Line)
{
 LPVOID ErrorMessage = 0;
 DWORD ErrorCode = GetLastError();
 FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
               FORMAT_MESSAGE_FROM_SYSTEM | 
               FORMAT_MESSAGE_IGNORE_INSERTS, 
               0, ErrorCode, 
               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
               (LPTSTR)&ErrorMessage, 0, 0);
 if(ErrorCode)
 { 
  Log("%s(%d): ERROR(%d): %s\n", File, Line, ErrorCode, ErrorMessage);
  LocalFree(ErrorMessage);
  SetLastError(0);
 }
}

//- API 
internal b32
OS_FileExists(char *FileName)
{
 b32 Result = false;
 
 DWORD Attributes = GetFileAttributesA(FileName);
 
 Result = (Attributes != INVALID_FILE_ATTRIBUTES &&
           !(Attributes & FILE_ATTRIBUTE_DIRECTORY));
 
 return Result;
}

internal str8 
OS_ReadEntireFileIntoMemory(char *FileName)
{
 str8 Result = {0};
 
 HANDLE FileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
 if(FileHandle != INVALID_HANDLE_VALUE)
 {
  LARGE_INTEGER FileSize;
  if(GetFileSizeEx(FileHandle, &FileSize))
  {
   u32 FileSize32 = SafeTruncateU64(FileSize.QuadPart);
   Result.Data = (u8 *)VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
   if(Result.Data)
   {
    DWORD BytesRead;
    if(ReadFile(FileHandle, Result.Data, FileSize32, &BytesRead, 0) &&
       (FileSize32 == BytesRead))
    {
     // NOTE(casey): File read successfully
     Result.Size = FileSize32;
     CloseHandle(FileHandle);
    }
    else
    {                    
     ErrorLog("Could not read correctly from '%s'.\n", FileName);
     
     if(Result.Data)
     {
      VirtualFree(Result.Data, 0, MEM_RELEASE);
     }
     
     Result.Data = 0;
    }
   }
   else
   {
    Win32Log();
    ErrorLog("Could not allocate memory for reading '%s'.\n", FileName);
   }
  }
  else
  {
   Win32Log();
   ErrorLog("Could not open '%s' for reading.\n", FileName);
  }
 }
 else
 {
  Win32Log();
  ErrorLog("Could not open '%s' for reading.\n", FileName);
 }
 
 return Result;
}

internal b32
OS_WriteEntireFile(char *FileName, str8 File)
{
 b32 Result = false;
 
 HANDLE FileHandle = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
 if(FileHandle != INVALID_HANDLE_VALUE)
 {
  DWORD BytesWritten;
  if(WriteFile(FileHandle, File.Data, (DWORD)File.Size, &BytesWritten, 0))
  {
   // NOTE(casey): File read successfully
   Result = (BytesWritten == File.Size);
   // NOTE(luca): Silence the error from overwriting a file.
   SetLastError(0);
  }
  else
  {
   ErrorLog("Could not write to '%s'.", FileName);
  }
  
  CloseHandle(FileHandle);
 }
 else
 {
  Win32Log();
  ErrorLog("Could not open '%s' for writing.", FileName);
 }
 
 return Result;
}

internal void
OS_FreeFileMemory(str8 File)
{
 if(File.Data)
 {
  VirtualFree(File.Data, File.Size, MEM_RELEASE);
 }
}

internal void
OS_BarrierWait(barrier Barrier)
{
 EnterSynchronizationBarrier((LPSYNCHRONIZATION_BARRIER)Barrier, 0);
}

internal void 
OS_SetThreadName(str8 ThreadName)
{
 wchar_t *Name = PushArrayZero(ThreadContext->Arena, wchar_t, ThreadName.Size+ 1);
 for EachIndex(Idx, ThreadName.Size)
 {
  Name[Idx] = ThreadName.Data[Idx];
 }
 SetThreadDescription((HANDLE)ThreadContext->Handle, Name);
}

internal void *
OS_AllocateAtOffset(u64 Size, u64 Offset)
{
 void *Result = VirtualAlloc((LPVOID)Offset, Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
 return Result;
}

internal void *
OS_Allocate(u64 Size)
{
 void *Result = OS_AllocateAtOffset(Size, 0);
 return Result;
}

internal void 
OS_MarkReadonly(void *Memory, u64 Size)
{
 DWORD OldProtection = {0};
 if(VirtualProtect(Memory, Size, PAGE_READONLY, &OldProtection) == 0)
 {
  Win32Log();
 }
}

internal f64
OS_GetWallClock(void)
{
 f64 Result = 0;
 
 LARGE_INTEGER Counter;
 QueryPerformanceCounter(&Counter);
 Result = (f64)(Counter.QuadPart)/GlobalPerfCountFrequency;
 
 return Result;
}

internal void
OS_Sleep(u32 MicroSeconds)
{
 HANDLE Timer;
 LARGE_INTEGER DueTime;
 
 Timer = CreateWaitableTimer(NULL, TRUE, NULL);
 
 // Negative value means relative time
 // Time is in 100-nanosecond intervals
 DueTime.QuadPart = (-(s32)(MicroSeconds * 10));
 
 SetWaitableTimer(Timer, &DueTime, 0, NULL, NULL, FALSE);
 WaitForSingleObject(Timer, INFINITE);
 CloseHandle(Timer);
}

//~ Filesystem operations
internal void
OS_ChangeDirectory(char *Path)
{
 Win32LogIf(SetCurrentDirectory(Path) == 0);
}


internal void
OS_MakeDirectory(char *Path)
{
 CreateDirectoryA(Path, 0);
}

internal os_dir *
OS_WalkDirPush(str8 Path, os_dir *Current)
{
 arena *Arena = GetScratch();
 os_dir *Result = 0;
 
 if(Current)
 {
  Path = Str8Fmt("%S/%S", Current->Path, Path);
 }
 
 str8 SearchPath = Str8Fmt("%S\\*", Path);
 
 WIN32_FIND_DATAA FindData = {0};
 HANDLE Handle = FindFirstFileExA((char *)SearchPath.Data, 
                                  FindExInfoBasic, 
                                  &FindData, 
                                  FindExSearchNameMatch, 
                                  0, 
                                  FIND_FIRST_EX_LARGE_FETCH);
 if(Handle == INVALID_HANDLE_VALUE) Win32Log();
 
 b32 IsDirectory = (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
 Assert(IsDirectory);
 
 if(IsDirectory)
 {
  Result = PushArrayZero(Arena, os_dir, 1);
  Result->Opaque = PushArrayZero(Arena, os_dir_opaque, 1);
  Result->Opaque->Handle = Handle;
  
  Result->Path = Path;
  Result->Parent = Current;
 }
 
 return Result;
}

internal os_dir_entry *
OS_WalkDirGetNext(os_dir *Dir)
{
 os_dir_entry *Result = 0;
 
 WIN32_FIND_DATAA FindData = {0};
 b32 Found = FindNextFileA(Dir->Opaque->Handle, &FindData);
 
 if(Found)
 {
  arena *Arena = GetScratch();
  Result = PushArrayZero(Arena, os_dir_entry, 1);
  
  Result->Name = Str8DupCString(Arena, FindData.cFileName);
  
  Result->Kind = OS_DirEntryKind_File;
  if(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) Result->Kind = OS_DirEntryKind_Directory;
 }
 else if(GetLastError() != ERROR_NO_MORE_FILES)
 {
  Win32Log();
 }
 
 return Result;
}

internal os_dir *
OS_WalkDirPop(os_dir *Dir)
{
 FindClose(Dir->Opaque->Handle);
 Dir = Dir->Parent;
 
 return Dir;
}


//~ Entrypoint
DWORD WINAPI 
ThreadInitEntryPoint(LPVOID FuncParams)
{
 entry_point_params *Params = (entry_point_params *)FuncParams;
 
 ThreadInit(&Params->Context);
 
 DEBUGThreadContext = ThreadContext;
 
#if !BASE_NO_ENTRYPOINT
 EntryPoint(Params);
#endif
 return 0;
}

#if !BASE_NO_ENTRYPOINT

#if BASE_CONSOLE_APPLICATION
int
main(int ArgsCount, char **Args, char **Env)
#else
int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
#endif
{
 GlobalDebuggerIsAttached = raddbg_is_attached();
 
#if !BASE_CONSOLE_APPLICATION    
 if(!GetStdHandle(STD_OUTPUT_HANDLE))
 {
  if(AttachConsole(ATTACH_PARENT_PROCESS))
  {
   freopen("CONOUT$","wb", stdout);
   freopen("CONOUT$","wb", stderr);
  }
  else 
  {
   AllocConsole();
   freopen("CONOUT$", "w", stdout);
   freopen("CONOUT$", "w", stderr);
  }
 }
#endif
 
 LARGE_INTEGER PerfCountFrequencyResult;
 QueryPerformanceFrequency(&PerfCountFrequencyResult);
 GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;
 
#if BASE_FORCE_THREADS_COUNT
 u64 ThreadsCount = BASE_FORCE_THREADS_COUNT;
#else
 u64 ThreadsCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
 SetLastError(0);
#endif
 
 arena *Arena = ArenaAlloc();
 SetStringsScratch(Arena);
 
 os_thread *Threads = PushArray(Arena, os_thread, ThreadsCount);
 
 u64 SharedStorage = 0;
 
 SYNCHRONIZATION_BARRIER Barrier = {0};
 
 BOOL BarrierInitialized = InitializeSynchronizationBarrier(&Barrier, (long)ThreadsCount, -1);
 Win32LogIf(!BarrierInitialized);
 
#if !BASE_CONSOLE_APPLICATION    
 // TODO(luca): Real command line parsing
 int ArgsCount = 2;
 char *Args[2] = {0};
 
 str8 ExePath = PushS8(Arena, MAX_PATH);
 ExePath.Size = GetModuleFileNameA(0, (char *)ExePath.Data, ExePath.Size);
 ExePath.Data[ExePath.Size] = 0;
 
 Args[0] = (char *)ExePath.Data;
 Args[1] = CommandLine;
#endif
 
 for EachIndex(Idx, ThreadsCount)
 {
  entry_point_params *Params = &Threads[Idx].Params;
  Params->Context.LaneIndex = Idx;
  Params->Context.LaneCount = ThreadsCount;
  Params->Context.Barrier = (barrier)&Barrier;
  Params->Context.SharedStorage = &SharedStorage;
  Params->Args = Args;
  Params->ArgsCount = ArgsCount;
  
  Params->Context.Handle = (thread_handle)CreateThread(0, 0, ThreadInitEntryPoint, Params, 0, 0);
  
  if(!Params->Context.Handle)
  {
   Win32Log();
  }
 }
 
 for EachIndex(Idx, ThreadsCount)
 {
  entry_point_params *Params = &Threads[Idx].Params;
  
  WaitForSingleObject((HANDLE)Params->Context.Handle, INFINITE);
  
  CloseHandle((HANDLE)Params->Context.Handle);
 }
 
 
 return 0;
}
#endif
