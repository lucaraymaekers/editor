//~ Libraries
#define BASE_FORCE_THREADS_COUNT 1
#define BASE_CONSOLE_APPLICATION 1
#include "base/base.h"
#include "base/base.c"

NO_WARNINGS_BEGIN
#define STB_SPRINTF_IMPLEMENTATION
#include "lib/stb_sprintf.h"
#include <math.h>
#include "listing.c"
NO_WARNINGS_END

#include "haversine_random.h"
#include "generated/haversine.meta.c"

//~ Helpers

void WriteMemoryTofile(umm Size, void *Memory, umm PairCount, char *Name, char *Extension)
{
 char FileName[256] = {};
 stbsp_sprintf(FileName, "data_%lu_%s.%s", PairCount, Name, Extension);
 
#if OS_LINUX
 int File = open(FileName, O_RDWR|O_CREAT|O_TRUNC, 0600);
 if(File != -1)
 {
  smm BytesWritten = write(File, Memory, Size);
  Assert((umm)BytesWritten == Size);
 }
 else
 {
  // TODO(luca): Logging
 }
 
#elif OS_WINDOWS
 HANDLE FileHandle = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
 if(FileHandle != INVALID_HANDLE_VALUE)
 {
  DWORD BytesWritten;
  if(WriteFile(FileHandle, Memory, (DWORD)Size, &BytesWritten, 0))
  {
   // NOTE(casey): File written successfully
   Assert(BytesWritten == Size);
  }
  else
  {
   // TODO(casey): Logging
  }
  
  CloseHandle(FileHandle);
 }
 else
 {
  // TODO(casey): Logging
 }
#endif
}

internal void *
PlatformGetMemory(umm Size)
{
 void *Result = 0;
 
#if OS_LINUX
 Result = mmap(0, Size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_SHARED, -1, 0);
#elif OS_WINDOWS
 Result = VirtualAlloc(0, Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
#endif
 
 return Result;
}


//~ Main
ENTRY_POINT(EntryPoint)
{
 if(LaneIndex() == 0)
 {
  // 1. haversine_generator [uniform/cluster] [random seed] [number of pairs to generate]
  
  char **Args = Params->Args;
  
  if(Params->ArgsCount >= 4)
  {
   u32 Method = 0;
   u64 Seed = 0;;
   u64 PairCount = 0;
   
   char *MethodName = Args[1];
   char *SeedString = Args[2];
   
   u64 ClusterCountLeft = U64Max;
   
   f64 MaxAllowedX = 180.0;
   f64 MaxAllowedY = 90.0;
   f64 XCenter = 0.0;
   f64 YCenter = 0.0;
   f64 XRadius = MaxAllowedX;
   f64 YRadius = MaxAllowedY;
   
   if(!strcmp(MethodName, "cluster"))
   {
    ClusterCountLeft = 0;
   }
   else if(strcmp(MethodName, "uniform"))
   {
    MethodName = "uniform";
    Log("Warning: Unknown method '%s', uniform used.\n", MethodName);
   }
   
   Seed = (u64)atoll(Args[2]);
   PairCount = (u64)atoll(Args[3]);
   
   u64 MaxPairCount = (1LL << 34);
   if(PairCount < MaxPairCount)
   {
    umm JsonMemorySize = GB(4);
    u8 *JsonMemory = (u8 *)PlatformGetMemory(JsonMemorySize);
    u8 *JsonOut = JsonMemory;
    
    umm BinMemorySize = GB(4);
    u8 *BinMemory = (u8 *)PlatformGetMemory(BinMemorySize);;
    u8 *BinOut = BinMemory;
    
    JsonOut += stbsp_sprintf((char *)JsonOut, 
                             "{\n"
                             " \"pairs\":\n"
                             "  [\n");
    
    haversine_random Series = {};
    Random64Seed(&Series, (u64)&printf + Seed, (u64)&sin + Seed, (u64)&cos, (u64)&tan);
    
    u64 ClusterCountMax = 1 + (PairCount / 64);
    
    f64 SumCoefficient = 1.0/(f64)PairCount;
    f64 Sum = 0;
    
    for(u32 PairIndex = 0;
        PairIndex < PairCount;
        PairIndex += 1)
    {
     if(ClusterCountLeft-- == 0)
     {
      ClusterCountLeft = ClusterCountMax;
      
      XCenter = Random64Between(&Series, -MaxAllowedX, MaxAllowedX);
      YCenter = Random64Between(&Series, -MaxAllowedY, MaxAllowedY);
      XRadius = Random64Between(&Series, 0, MaxAllowedX);
      YRadius = Random64Between(&Series, 0, MaxAllowedY);
     }
     
     f64 X0 = Random64Degree(&Series, XCenter, XRadius, MaxAllowedX);
     f64 Y0 = Random64Degree(&Series, YCenter, YRadius, MaxAllowedY);
     f64 X1 = Random64Degree(&Series, XCenter, XRadius, MaxAllowedX);
     f64 Y1 = Random64Degree(&Series, YCenter, YRadius, MaxAllowedY);
     
     f64 EarthRadius = 6372.8;
     f64 Distance = ReferenceHaversine(X0, Y0, X1, Y1, EarthRadius);
     Sum += SumCoefficient*Distance;
     
     *(f64 *)BinOut = Distance;
     BinOut += sizeof(Distance);
     
     const char *Separator = (PairIndex == PairCount - 1) ? "\n" : ",\n";
     JsonOut += stbsp_sprintf((char *)JsonOut, 
                              "   { \"x0\": %.16f, \"y0\": %.16f, \"x1\": %.16f, \"y1\": %.16f }%s", 
                              X0, Y0, X1, Y1, Separator);
    }
    
    *(f64 *)BinOut = Sum;
    BinOut += sizeof(Sum);
    
    Log("Method: %s\n"
        "Random seed: %lu\n"
        "Pairs count: %lu\n"
        , MethodName, Seed, PairCount);
    Log("Average sum: %.16f\n", Sum);
    
    JsonOut += stbsp_sprintf((char *)JsonOut, 
                             "  ]\n"
                             "}\n");
    WriteMemoryTofile((umm)(JsonOut - JsonMemory), JsonMemory, PairCount, "flex", "json");
    WriteMemoryTofile((umm)(BinOut - BinMemory), BinMemory, PairCount, "haveranswer", "f64");
   }
   else
   {
    Log("Massive files unsupported. Number of pairs must be less than %lu.\n", MaxPairCount);
   }
  }
  else
  {
   Log("Usage: %s [uniform/cluster] [random seed] [number of pairs to generate]\n", 
       Args[0]);
  }
 }
 
 return 0;
}