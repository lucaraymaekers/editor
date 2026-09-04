//~ Libraries
#define BASE_FORCE_THREADS_COUNT 1
#define BASE_CONSOLE_APPLICATION 1
#include "base/base.h"
#include "base/base.c"

NO_WARNINGS_BEGIN
#include "reference_decoder/sim86_lib.cpp"
#define STB_SPRINTF_IMPLEMENTATION
#include "lib/stb_sprintf.h"
NO_WARNINGS_END
#include "clocks_table.inl"

#include "generated/sim86.meta.c"

//~ Macro's
#define SIM86_INTERNAL 1

//~ Globals 
global_variable u8 GlobalMemory[1*1024*1024] = {};

//~ Functions
internal void
SetOrUnsetFlag(u32 *FlagsRegister, b32 Condition, flags_8086 Flag)
{
 if(Condition)
 {
  *FlagsRegister |= Flag;
 }
 else
 {
  *FlagsRegister &= (~(u32)Flag);
 }
}

internal u32
FlagsToString(u8 *Buffer, u32 Flags)
{
 u32 Length = 0;
 
 for EachElement(MappingIndex, flags_8086_strings)
 {
  u8 Char = (u8)(flags_8086_strings[MappingIndex][0]);
  b32 IsBitSet = (Flags & 1);
  Flags >>= 1;
  if(IsBitSet)
  {
   Buffer[Length++] = Char;
  }
 }
 
 return Length;
}

// Sets flags by checking the destination value of the register.
internal void
FlagsFromValue(u32 *FlagsRegister, u32 InstructionFlags, s32 Value)
{
 u32 OldFlagsRegister = *FlagsRegister;
 
 u32 SignMask = (1 << ((InstructionFlags & Inst_Wide) ? 15 : 7));
 SetOrUnsetFlag(FlagsRegister, (Value & (s32)SignMask), Flag_Sign);
 SetOrUnsetFlag(FlagsRegister, (Value == 0),       Flag_Zero);
 
 // NOTE(luca): Parity flag is set when in lower 8 bits have an even number of set bits.
 u32 OneBitsCount = 0;
 for(u32 BitsIndex = 0;
     BitsIndex < 8;
     BitsIndex++)
 {
  OneBitsCount += (Value & 1);
  Value >>= 1;
 }
 SetOrUnsetFlag(FlagsRegister, (!(OneBitsCount & 1)), Flag_Parity);
 
 // TODO(luca): Overflow flag
 // Were we adding positive numbers but produced a negative number?
 
 // TODO(luca): Carry flag 
 // When twot numbers are added and the 17th bit would be set?
 
 // TODO(luca): Auxiliary carry flag
 // Carry for bottom 8 bits
 if(*FlagsRegister != OldFlagsRegister)
 {
  char OldFlagsString[ArrayCount(flags_8086_strings)] = {};
  char FlagsString[ArrayCount(flags_8086_strings)] = {};
  FlagsToString((u8 *)OldFlagsString, OldFlagsRegister);
  FlagsToString((u8 *)FlagsString, *FlagsRegister);
  
  printf(" flags:%s->%s", OldFlagsString, FlagsString);
 }
}

s32 GetCompleteDisplacement(s32 *Registers, instruction_operand *Operand)
{
 s32 CompleteDisplacement = Operand->Address.Displacement;
 
 u32 Count = Operand->Address.Terms[0].Register.Count;
 u32 Mask = ((u32)((-1)) >> (16 + (16 - Count*8)));
 CompleteDisplacement += 
 (Registers[Operand->Address.Terms[0].Register.Index] & Mask) +
 (Registers[Operand->Address.Terms[1].Register.Index] & Mask);
 
 return CompleteDisplacement;
}

internal s32 *
OperandToValue(s32 *Registers, u8 *Memory, instruction_operand *Operand)
{
 s32 *Result = 0;
 
 if(0) {}
 else if(Operand->Type == Operand_Register)
 {
  Result = Registers + Operand->Register.Index;
 }
 else if(Operand->Type == Operand_Memory)
 {
  s32 CompleteDisplacement = GetCompleteDisplacement(Registers, Operand);
  Result = (s32 *)((u8 *)Memory + CompleteDisplacement);
 }
 else if(Operand->Type == Operand_Immediate)
 {
  Result = &Operand->Immediate.Value;
 }
 else if(Operand->Type != Operand_None)
 {
  Assert(0);
 }
 
 return Result;
}

b32 IsAccumulator(instruction_operand *Operand)
{
 b32 Result = ((Operand->Type == Operand_Register) &&
               (Operand->Register.Index == Register_a));
 return Result;
}

b32 IsMatchingOp(instruction_operand *Operand, instruction_clocks_operand_type Type)
{
 b32 Matching = false;
 
 Matching = Matching || (Type == InstructionClocksOperand_Memory && Operand->Type == Operand_Memory);
 Matching = Matching || (Type == InstructionClocksOperand_Register && Operand->Type == Operand_Register);
 Matching = Matching || (Type == InstructionClocksOperand_Accumulator && IsAccumulator(Operand));
 Matching = Matching || (Type == InstructionClocksOperand_Immediate && Operand->Type == Operand_Immediate);
 
 return Matching;
}

internal void
Run8086(umm MemorySize, u8 *Memory)
{
 s32 Registers[Register_count] = {}; 
 u32 FlagsRegister = 0;
 u32 IPRegister = 0;
 u32 ElapsedClocks = 0;
 
 while(IPRegister < MemorySize)
 {
  instruction Decoded;
  Sim86_Decode8086Instruction((u32)(MemorySize - IPRegister), Memory + IPRegister, &Decoded);
  if(Decoded.Op)
  {
   u32 OldIPRegister = IPRegister;
   
#if SIM86_INTERNAL           
   printf("%s ;", Sim86_MnemonicFromOperationType(Decoded.Op));
#endif
   
   instruction_operand *DestinationOperand = Decoded.Operands + 0;
   instruction_operand *SourceOperand      = Decoded.Operands + 1;
   
   u32 AddedClocks = 0;
   for(u32 ClocksIndex = 0;
       ClocksIndex < ArrayCount(ClocksTable);
       ClocksIndex++)
   {
    instruction_clocks *Clocks = ClocksTable + ClocksIndex;
    
    b32 Matching = Decoded.Op == Clocks->Op; 
    Matching = Matching && IsMatchingOp(DestinationOperand, Clocks->Operands[0]);
    Matching = Matching && IsMatchingOp(SourceOperand, Clocks->Operands[1]);
    if(Matching)
    {
     AddedClocks += Clocks->Clocks;
     
     if(Clocks->EffectiveAddress)
     {
      instruction_operand *MemoryOperand = ((DestinationOperand->Type == Operand_Memory) ? DestinationOperand : SourceOperand);
      u32 FirstIndex = MemoryOperand->Address.Terms[0].Register.Index;
      u32 SecondIndex = MemoryOperand->Address.Terms[1].Register.Index;
      
      // Only displacement
      if(FirstIndex == 0 && SecondIndex == 0)
      {
       AddedClocks += 6;
      }
      // Base or index
      else if(FirstIndex == 0 || SecondIndex == 0)
      {
       // Base or index only
       if(MemoryOperand->Address.Displacement == 0)
       {
        AddedClocks += 5;
       }
       // Base or index + displacement
       else
       {
        AddedClocks += 9;
       }
      }
      // Base + index
      else if(MemoryOperand->Address.Displacement == 0)
      {
       /*
       bp+di bx+si 7
           bp+si bx+di 8
           */
       if((FirstIndex == Register_bp && SecondIndex == Register_di) ||
          (FirstIndex == Register_b && SecondIndex == Register_si))
       {
        AddedClocks += 7;
       }
       else
       {
        AddedClocks += 8;
       }
      }
      // Base + index + displacement
      else
      {
       /*
       bp+di bx+si 11
           bp+si bx+di 12
           */
       if((FirstIndex == Register_bp && SecondIndex == Register_di) ||
          (FirstIndex == Register_b && SecondIndex == Register_si))
       {
        AddedClocks += 11;
       }
       else
       {
        AddedClocks += 12;
       }
      }
      
     }
     
     // Add transfer penalty
     if(Clocks->Transfers && (Decoded.Flags & Inst_Wide))
     {
      instruction_operand *MemoryOperand = ((DestinationOperand->Type == Operand_Memory) ? DestinationOperand : SourceOperand);
      s32 Displacement = GetCompleteDisplacement(Registers, MemoryOperand);
      if(Displacement & 1)
      {
       AddedClocks += 4*Clocks->Transfers;
      }
     }
     
     break;
    }
   }
   
#if 0            
   Assert(AddedClocks);
#endif
   
   ElapsedClocks += AddedClocks;
   printf(" clocks: +%d = %d", AddedClocks, ElapsedClocks);
   
   s32 *Destination = OperandToValue(Registers, Memory, DestinationOperand);
   s32 *Source      = OperandToValue(Registers, Memory, SourceOperand);
   
   if(0) {}
   else if(Decoded.Op == Op_int3)
   {
    Assert(0);
   }
   else if(Decoded.Op == Op_mov)
   {
    s32 Old = *Destination;
    if(Decoded.Flags & Inst_Wide)
    {
     *(u16 *)Destination = *(u16 *)Source;
    }
    else
    {
     Assert((SourceOperand->Type == Operand_Immediate ||
             SourceOperand->Type == Operand_Memory) &&
            (SourceOperand->Register.Offset == 0));
     Assert((DestinationOperand->Type == Operand_Immediate || DestinationOperand->Type == Operand_Memory) && (DestinationOperand->Register.Offset == 0));
     
     u8 *SourceByte = (u8 *)Source + SourceOperand->Register.Offset;
     u8 *DestByte   = (u8 *)Destination + DestinationOperand->Register.Offset;
     
     *DestByte = *SourceByte;
    }
    
#if SIM86_INTERNAL                    
    if(DestinationOperand->Type == Operand_Register)
    {
     printf(" %s:0x%x->0x%x", Sim86_RegisterNameFromOperand(&DestinationOperand->Register),
            Old, *Destination);
    }
#endif
   }
   else if(Decoded.Op == Op_ret)
   {
    printf("\n");
    printf("STOPONRET: Return encountered at address %d.\n", IPRegister);
    
    break;
   }
   else if(Decoded.Op == Op_inc)
   {
    Assert(DestinationOperand->Type == Operand_Register);
    Assert(SourceOperand->Type == Operand_None);
    *Destination += 1;
   }
   else if(Decoded.Op == Op_test)
   {
    
    Assert(DestinationOperand->Type == Operand_Register);
    Assert(SourceOperand->Type == Operand_Register || SourceOperand->Type == Operand_Immediate);
    
    s32 Value =((Decoded.Flags & Inst_Wide) ? 
                (u16)((u16)*Destination & ((u16)*Source)) :
                (u8)((u8)*Destination & ((u8)*Source))); 
    FlagsFromValue(&FlagsRegister, Decoded.Flags, Value);
   }
   else if(Decoded.Op == Op_xor)
   {
    
    Assert(DestinationOperand->Type == Operand_Register);
    Assert(SourceOperand->Type == Operand_Register || SourceOperand->Type == Operand_Immediate);
    
    s32 Value =((Decoded.Flags & Inst_Wide) ? 
                (u16)((u16)*Destination ^ ((u16)*Source)) :
                (u8)((u8)*Destination ^ ((u8)*Source))); 
    FlagsFromValue(&FlagsRegister, Decoded.Flags, Value);
    *Destination = Value;
   }
   else if(Decoded.Op == Op_cmp)
   {
    Assert(DestinationOperand->Type == Operand_Register);
    Assert(SourceOperand->Type == Operand_Register || SourceOperand->Type == Operand_Immediate);
    
    s32 Value = ((Decoded.Flags & Inst_Wide) ? 
                 (u16)((u16)*Destination - ((u16)*Source)) :
                 (u8)((u8)*Destination - ((u8)*Source)));
    
    FlagsFromValue(&FlagsRegister, Decoded.Flags, Value);
   }
   else if(Decoded.Op == Op_sub)
   {
    Assert(DestinationOperand->Type == Operand_Register);
    Assert(SourceOperand->Type == Operand_Register || SourceOperand->Type == Operand_Immediate);
    
    s32 Old = *Destination;
    *Destination = ((Decoded.Flags & Inst_Wide) ? 
                    (u16)((u16)*Destination - ((u16)*Source)) :
                    (u8)((u8)*Destination - ((u8)*Source)));
    
    printf(" %s:0x%x->0x%x", 
           Sim86_RegisterNameFromOperand(&DestinationOperand->Register),
           Old, *Destination);
    
    FlagsFromValue(&FlagsRegister, Decoded.Flags, *Destination);
    
   }
   else if(Decoded.Op == Op_add)
   {
    s32 Old = *Destination;
    
    *Destination = ((Decoded.Flags & Inst_Wide) ? 
                    (u16)((u16)*Destination + ((u16)*Source)) :
                    (u8)((u8)*Destination + ((u8)*Source)));
    
    printf(" %s:0x%x->0x%x", 
           Sim86_RegisterNameFromOperand(&DestinationOperand->Register),
           Old, *Destination);
    
    FlagsFromValue(&FlagsRegister, Decoded.Flags, *Destination);
   }
   else if(Decoded.Op == Op_jne)
   {
    if(!(FlagsRegister & Flag_Zero))
    {
     IPRegister += *Destination;
    }
   }
   else if(Decoded.Op == Op_je)
   {
    if((FlagsRegister & Flag_Zero))
    {
     IPRegister += *Destination;
    }
   }
   else
   {
    Assert(0 && "Op not implemented yet.");
   }
   
   IPRegister += Decoded.Size;
   
#if SIM86_INTERNAL
   printf(" ip:0x%x->0x%x", OldIPRegister, IPRegister);
#endif
   
  }
  else
  {
   printf("Unrecognized instruction\n");
   break;
  }
  
#if SIM86_INTERNAL
  printf("\n");
#endif
  
 }
 
 printf("\nFinal registers:\n");
 for(u32 RegisterIndex = Register_a;
     RegisterIndex < Register_ds + 1;
     RegisterIndex++)
 {
  register_access Register = {};
  Register.Index = RegisterIndex;
  Register.Offset = 0;
  Register.Count = 2;
  
  u32 Value = Registers[RegisterIndex];
  if(Value > 0)
  {
   printf("     %s: 0x%0x (%d)\n", 
          Sim86_RegisterNameFromOperand(&Register),
          Value, Value);
  }
 }
 printf("     ip: 0x%04x (%d)\n", IPRegister, IPRegister);
 
 if(FlagsRegister)
 {
  char FlagsString[ArrayCount(flags_8086_strings)] = {};
  FlagsToString((u8 *)FlagsString, FlagsRegister);
  printf("  flags: %s\n", FlagsString);
 }
}

void PrintUsage(char *ExePath)
{
 printf("usage: %s [-exec] <assembly>\n", ExePath);
}

//~ Entrypoint
raddbg_entry_point(EntryPoint);
ENTRY_POINT(EntryPoint)
{
 if(LaneIndex() == 0)
 {
  u32 Version = Sim86_GetVersion();
  
#if SIM86_INTERNAL    
  printf("Sim86 Version: %u (expected %u)\n", Version, SIM86_VERSION);
#endif
  
  if(Version != SIM86_VERSION)
  {
   printf("ERROR: Header file version doesn't match DLL.\n");
   return 0;
  }
  
  instruction_table Table;
  Sim86_Get8086InstructionTable(&Table);
  
#if SIM86_INTERNAL    
  printf("8086 Instruction Instruction Encoding Count: %u\n", Table.EncodingCount);
#endif
  
  // print (default)
  // -exec (also execute the disassembled instructinos)
  
  b32 Execute = false;
  b32 Dump = false;
  char *FileName = 0;
  for(s32 ArgsIndex = 1;
      ArgsIndex < Params->ArgsCount;
      ArgsIndex++)
  {
   char *Command = Params->Args[ArgsIndex];
   if(0) {}
   else if(!strcmp(Command, "-exec"))
   {
    Execute = true;
   }
   else if(!strcmp(Command, "-dump"))
   {
    Dump = true;
   }
   else 
   {
    FileName = Command;
   }
  }
  
  if(FileName)
  {
   FILE *File = fopen(FileName, "rb");
   if(File)
   {
    umm BytesWritten = fread(GlobalMemory, 1, sizeof(GlobalMemory), File);
    fclose(File);
    
    if(Execute)
    {
     printf("--- %s execution ---\n", FileName);
     
     Run8086(BytesWritten, GlobalMemory);
     
     if(Dump)
     {
      // NOTE(luca): We have to add ".data" or Gimp will throw an error. 
      FILE *DumpFile = fopen("sim86_memory_0.data", "wb");
      fwrite(GlobalMemory, 1, sizeof(GlobalMemory), DumpFile);
      fclose(DumpFile);
     }
     
    }
    else
    {
     printf("ERROR: Disassembling not implemented yet.\n");
    }
    
   }
   else
   {
    printf("ERROR: Unable to open %s.\n", FileName);
    PrintUsage(Params->Args[0]);
   }
  }
 }
 
 return 0;
}
