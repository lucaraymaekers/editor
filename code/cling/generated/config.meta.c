
  internal ui_box *
  GetBoxHashNode(str8 String, u64 Seed, u64 ArraySize, ui_box *Array)
 {
  ui_box *Result = 0;
  
  u64 Key = U64HashFromSeedStr8(Seed, String);
  u64 Slot = Key%ArraySize;
  Result = Array + Slot;
  
  for(;Result && Result->Key != Key;)
  {
   Result = Result->Next;
  }
  
  return Result;
 }
 
 
