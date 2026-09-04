//~ Libraries
#define BASE_FORCE_THREADS_COUNT 1
#define BASE_CONSOLE_APPLICATION 1
#include "base/base.h"
#include "base/base.c"

//- External  
NO_WARNINGS_BEGIN
#define STB_SPRINTF_IMPLEMENTATION
#include "lib/stb_sprintf.h"
NO_WARNINGS_END
#include "listing.c"
//- Project 
#include "haversine_processor.h"
#include "profiler.c"
#include "json.c"

//- Main 
ENTRY_POINT(EntryPoint)
{
 if(LaneIndex() == 0)
 {
  char *JSONFileName = 0;
  char *AnswersFileName = 0;
  u64 PairCount = 0;
  u64 OSFreq = GetOSTimerFreq();
  u64 OSStart = ReadOSTimer();
  
  char **Args = Params->Args;
  s32 ArgsCount = Params->ArgsCount;
  
  // Parse command line
  {    
   if(ArgsCount >= 2)
   {
    JSONFileName = Args[1];
   }
   
   if(ArgsCount >= 3)
   {
    AnswersFileName = Args[2];
   }
  }
  
  if(JSONFileName)
  {
   str8 In = {};
   Prof_TimeScope("Read")
   {
    In = OS_ReadEntireFileIntoMemory(JSONFileName);
   }
   
   if(In.Size)
   {    
    u64 Pos = 0;
    json_token *JSON = 0;
    
    Prof_TimeScope("Parse")
    {                
     JSON = JSON_ParseElement(In, &Pos);
    }
    
    json_token *PairsArray = 0;
    haversine_pair *HaversinePairs = 0;
    
    // Parse haversine pairs
    Prof_TimeScope("MiscSetup")
    {
     PairsArray = JSON_LookupIdentifierValue(JSON, S8("\"pairs\""));
     
     // 30 bytes(characters) is the minimum for one pair
     u64 MaxPairCount = In.Size/30;
     HaversinePairs = (haversine_pair *)malloc(sizeof(haversine_pair)*MaxPairCount);
     
     for(json_token *Pair = PairsArray->Child;
         Pair;
         Pair = Pair->Next)
     {
      haversine_pair *HaversinePair = HaversinePairs + PairCount;
      HaversinePair->X0 = JSON_StringToFloat(JSON_LookupIdentifierValue(Pair, S8("\"x0\""))->Value);
      HaversinePair->Y0 = JSON_StringToFloat(JSON_LookupIdentifierValue(Pair, S8("\"y0\""))->Value);
      HaversinePair->X1 = JSON_StringToFloat(JSON_LookupIdentifierValue(Pair, S8("\"x1\""))->Value);
      HaversinePair->Y1 = JSON_StringToFloat(JSON_LookupIdentifierValue(Pair, S8("\"y1\""))->Value);
      
      PairCount += 1;
      
      if(Pair->Next)
      {
       Assert(Pair->Next->Type == JSON_TokenType_Comma);
       Pair = Pair->Next;
      }
     }
    }
    JSON_FreeTokens(JSON);
    
    // Compute the sum
    Prof_TimeScope("Sum")
    {
     f64 AverageSum = 0;
     f64 AverageCoefficient = (1.0/(f64)PairCount);
     
     for(u64 PairIndex = 0;
         PairIndex < PairCount;
         PairIndex += 1)
     {
      haversine_pair *HaversinePair = HaversinePairs + PairIndex;
      
      f64 EarthRadius = 6372.8;
      f64 Sum = ReferenceHaversine(HaversinePair->X0, HaversinePair->Y0, 
                                   HaversinePair->X1, HaversinePair->Y1,
                                   EarthRadius);
      AverageSum += AverageCoefficient*Sum;
     }
     
     Prof_TimeScope("MiscOutput")
     {                    
      // TODO(luca): print average and difference between answer file 
      
      Log("Average: %.16f\n", AverageSum);
     }
     
    }
    
    free(HaversinePairs);
    
   }
   else
   {
    Log("ERROR: Cannot find file '%s' or its size is zero.\n", JSONFileName);
   }
  }
  else
  {
   Log("ERROR: No file provided.\n");
   Log("Usage: %s <data.json> [data.f64]\n", Args[0]); 
  }
 }
 
 return 0;
}