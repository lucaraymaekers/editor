
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
  
  for(;Hash != 0;)
  {
   if(Hash->Key == 0)
   {
    break;
   }
   else if(Hash->Key == Key)
   {
    // NOTE(luca): Collision.
    InvalidPath();
   }
   else
   {
    if(Hash == 0)
    {
     Hash = PushArrayZero(Arena, field_hash_node, 1);
    }
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
  
  for(;Hash != 0;)
  {
   if(Hash->Key == 0)
   {
    break;
   }
   else if(Hash->Key == Key)
   {
    // NOTE(luca): Collision.
    InvalidPath();
   }
   else
   {
    if(Hash == 0)
    {
     Hash = PushArrayZero(Arena, table_hash_node, 1);
    }
    Hash = Hash->Next;
   }
  }
  
  Hash->Key = Key;
  
  return Hash;
 }
 
