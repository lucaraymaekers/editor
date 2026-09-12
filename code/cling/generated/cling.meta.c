#define Cng_ConfigMatchers \
if(0) {} \
  else if(ConfigMatch(Name, S8("Asan"), Value)) Asan = 1; \
  else if(ConfigMatch(Name, S8("Debug"), Value)) Debug = 1; \
  else if(ConfigMatch(Name, S8("Clean"), Value)) Clean = 1; \
  else if(ConfigMatch(Name, S8("Clang"), Value)) Clang = 1; \
  else if(ConfigMatch(Name, S8("GCC"), Value)) GCC = 1; \
  else if(ConfigMatch(Name, S8("Slow"), Value)) Slow = 1; \
  else if(ConfigMatch(Name, S8("Wine"), Value)) Wine = 1; \
  else if(ConfigMatch(Name, S8("Meta"), Value)) Meta = 1; \
  else if(ConfigMatch(Name, S8("HaversineProcessor"), Value)) HaversineProcessor = 1; \
  else if(ConfigMatch(Name, S8("HaversineGenerator"), Value)) HaversineGenerator = 1; \
  else if(ConfigMatch(Name, S8("Sim86"), Value)) Sim86 = 1; \
  else if(ConfigMatch(Name, S8("Editor"), Value)) Editor = 1; \
  else if(ConfigMatch(Name, S8("Muze"), Value)) Muze = 1;
#define Cng_ConfigBools \
 \
  b32 Asan = false; \
  b32 Debug = false; \
  b32 Clean = false; \
  b32 Clang = false; \
  b32 GCC = false; \
  b32 Slow = false; \
  b32 Wine = false; \
  b32 Meta = false; \
  b32 HaversineProcessor = false; \
  b32 HaversineGenerator = false; \
  b32 Sim86 = false; \
  b32 Editor = false; \
  b32 Muze = false;

  internal field_hash_node *
  GetFieldHashNode(str8 String, u64 Seed, u64 ArraySize, field_hash_node *Array)
 {
  field_hash_node *Result = 0;
  
  u64 Key = U64HashFromSeedStr8(Seed, String);
  u64 Slot = Key%ArraySize;
  Result = Array + Slot;
  
  for(;Result && Result->Key != Key;)
  {
   Result = Result->Next;
  }
  
  return Result;
 }
 
 
  internal table_hash_node *
  GetTableHashNode(str8 String, u64 Seed, u64 ArraySize, table_hash_node *Array)
 {
  table_hash_node *Result = 0;
  
  u64 Key = U64HashFromSeedStr8(Seed, String);
  u64 Slot = Key%ArraySize;
  Result = Array + Slot;
  
  for(;Result && Result->Key != Key;)
  {
   Result = Result->Next;
  }
  
  return Result;
 }
 
 
  internal field_hash_node *
  AddFieldHashNode(arena *Arena, str8 String, u64 Seed, 
                     u64 ArraySize, field_hash_node *Array)
 {
  field_hash_node *Hash = 0;
  
  u64 Key = U64HashFromSeedStr8(Seed, String);
  u64 Slot = (Key%ArraySize);
  
  Hash = Array + Slot;
  
  for(;Hash->Key != 0;)
  {
   if(Hash->Key == Key)
   {
    // NOTE(luca): Collision.
    InvalidPath();
   }
   else
   {
    while(Hash->Next && Hash->Key != 0) Hash = Hash->Next;
    Hash->Next = PushArrayZero(Arena, field_hash_node, 1);
    Hash = Hash->Next;
   }
  }
  
  Hash->Key = Key;
  
  return Hash;
 }
 
  internal table_hash_node *
  AddTableHashNode(arena *Arena, str8 String, u64 Seed, 
                     u64 ArraySize, table_hash_node *Array)
 {
  table_hash_node *Hash = 0;
  
  u64 Key = U64HashFromSeedStr8(Seed, String);
  u64 Slot = (Key%ArraySize);
  
  Hash = Array + Slot;
  
  for(;Hash->Key != 0;)
  {
   if(Hash->Key == Key)
   {
    // NOTE(luca): Collision.
    InvalidPath();
   }
   else
   {
    while(Hash->Next && Hash->Key != 0) Hash = Hash->Next;
    Hash->Next = PushArrayZero(Arena, table_hash_node, 1);
    Hash = Hash->Next;
   }
  }
  
  Hash->Key = Key;
  
  return Hash;
 }
 
