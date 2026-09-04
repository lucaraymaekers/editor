/*

Instruction have 6 bytes that define what type of instruction it is.

 Each instruction has zero, one or two operands.
 The operands can be switched depending on the D bit.

 The MOD field indicates if a displacement follows. 
Except when R/M is 110.

The D and W bits are always in the first byte.

Data is always required.

Reg can happen in the first byte.

Some expressions have a second 3bit field that stores the "sub-kind" of instruction.
*/

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "sim8086.h"

#define Assert(Expression) \
if(!(Expression)) \
{ \
*(char *)0 = 0; \
}

#if SIM8086_INTERNAL
#define DebugLabel(Name) Name:
#else
#define DebugLabel(Name)
#endif

#define Swap(Type, First, Second) \
{ \
Type Temp; \
Temp = First; \
First = Second; \
Second = Temp; \
}

#define JE      0b01110100
#define JL      0b01111100
#define JLE     0b01111110
#define JB      0b01110010
#define JBE     0b01110110
#define JP      0b01111010
#define JO      0b01110000
#define JS      0b01111000
#define JNL     0b01111101
#define JG      0b01111111
#define JNB     0b01110011
#define JA      0b01110111
#define JNP     0b01111011
#define JNO     0b01110001
#define JNS     0b01111001
#define JNZ     0b01110101
#define LOOP    0b11100010
#define LOOPZ   0b11100001
#define LOOPNZ  0b11100000
#define JCXZ    0b11100011

#define ADD 0b000
#define SUB 0b101
#define CMP 0b111

// Can be indexed by R/M or reg field.
// Also if the W bit is set it can be used to index and get the wide name.
global_variable char *RegistersNames[][2] = 
{
    [0b000] = { "al", "ax" },
    [0b001] = { "cl", "cx" },
    [0b010] = { "dl", "dx" },
    [0b011] = { "bl", "bx" },
    [0b100] = { "ah", "sp" },
    [0b101] = { "ch", "bp" },
    [0b110] = { "dh", "si" },
    [0b111] = { "bh", "di" }
};
#define ACCUMULATOR 0b000

global_variable char *DisplacementsNames[] = 
{
    [0b000] = "bx + si",
    [0b001] = "bx + di",
    [0b010] = "bp + si",
    [0b011] = "bp + di",
    [0b100] = "si",
    [0b101] = "di",
    [0b110] = "bp",
    [0b111] = "bx"
};

global_variable char *SegmentRegistersNames[] =
{
    [0b000] = "ss",
    [0b001] = "cs",
    [0b010] = "ds",
    [0b011] = "es",
};

global_variable char GlobalBinaryTempString[9];

inline
void MemoryCopy(char *To, char *From, size_t Count)
{
    while(Count--) *To++ = *From++;
}

inline
void CStringCopy(char *To, char *From)
{
    while((*To++ = *From++));
}

inline
u32 CStringLength(char *Src)
{
    u32 Result = 0;
    while(Src[Result]) Result++;
    return Result;
}

char *PrefixString(char *Dest, char *Prefix)
{
    u32 PrefixLength = CStringLength(Prefix);
    u32 DestLen = CStringLength(Dest);
    
    // Move the string over by the prefix length
    for(s32 CharAt = DestLen;
        CharAt >= 0;
        CharAt--)
    {
        Dest[PrefixLength + CharAt] = Dest[CharAt];
    }
    
    // Copy the prefix over
    MemoryCopy(Dest, Prefix, 5);
    
    return Dest;
}

int ReadBytesFromFile(int File, u8 **Dest, size_t Count)
{
    int BytesRead = read(File, *Dest, Count);
    *Dest += BytesRead;
    Assert(BytesRead == (int)Count || BytesRead == 0);
    return BytesRead;
}

char *ByteToBinaryString(u8 Byte)
{
    char *Result = GlobalBinaryTempString;
    
    int Count = 8;
    while(Count--)
    {
        *Result++ = '0' + (Byte >> 7);
        Byte <<= 1;
    }
    *Result = 0;
    
    return GlobalBinaryTempString;
}

void SetBitFromBitAt(b32 *Bit, u8 Byte, u32 BitAt)
{
    if(BitAt)
    {
        *Bit = ((Byte & (1 << (BitAt - 1))) != 0);
    }
}

u8 ExtractBitsFromByte(u8 Byte, u8 Bits, u8 BitAt)
{
    u8 Result = 0;
    
    u8 ShiftValue = BitAt - 1;
    u8 Mask = (Bits << ShiftValue);
    Result = ((Byte & Mask) >> ShiftValue);
    
    return Result;
}

int main(int ArgCount, char *Args[])
{
    u8 Bytes[255] = {};
    instruction InstructionsBuffer[256] = {};
    instruction_table Table = {};
    Table.Instructions = InstructionsBuffer;
    Table.Size = (sizeof(InstructionsBuffer)/sizeof(InstructionsBuffer[0])) - 1;
    
    Table.Instructions[Table.Count++] = 
    {
        .Name = "mov",
        .Description = "Register/memory to/from register",
        .OperandsCount = 2,
        .BytesCount = 2,
        .Bytes =
        {
            {
                .Op = 0b10001000,
                .OpMask = 0b11111100,
                .WBitAt = 1,
                .DBitAt = 2,
            },
            {
                .ModBitsAt = 7,
                .RegBitsAt = 4,
                .RMBitsAt = 1
            }
        }
    };
    
    Table.Instructions[Table.Count++] =
    {
        .Name  = "mov", 
        .Description = "Immediate to register/memory",
        .OperandsCount = 2,
        .HasImmediate = true,
        .BytesCount = 1,
        .Bytes =
        {
            {
                .Op = 0b10000110,
                .OpMask = 0b11111110,
                .WBitAt = 1,
                .ModBitsAt = 7,
                .RMBitsAt = 1
            }
        }
    };
    
    Table.Instructions[Table.Count++] =
    {
        .Name = "mov",
        .Description = "Immediate to register",
        .OperandsCount = 2,
        .HasImmediate = true,
        .BytesCount = 1,
        .Bytes = {
            {
                .Op = 0b10110000,
                .OpMask = 0b11110000,
                .WBitAt = 4,
                .RegBitsAt = 1,
            }
        }
    };
    
    Table.Instructions[Table.Count++] = 
    {
        .Name = "mov",
        .Description = "Immediate to register/memory",
        .OperandsCount = 2,
        .HasImmediate = true,
        .BytesCount = 2,
        .Bytes =
        {
            {
                .Op = 0b11000110,
                .OpMask = 0b11111110,
                .WBitAt = 1,
            },
            {
                .ModBitsAt = 7, 
                .RMBitsAt = 1,
            }
        }
    };
    
    Table.Instructions[Table.Count++] =
    {
        .Name = "mov",
        .Description = "Memory to accumulator",
        .OperandsCount = 2,
        .HasAddress = true,
        .ToAccumulator = true,
        .BytesCount = 1,
        .Bytes = 
        {
            {
                .Op = 0b10100000,
                .OpMask = 0b11111110,
                .WBitAt = 1
            }
        }
    };
    
    // NOTE(luca): This is the same instruction as the previous one with the D bit always set to true...
    Table.Instructions[Table.Count++] =
    {
        .Name = "mov",
        .Description = "Accumulator to memory",
        .OperandsCount = 2,
        .HasAddress = true,
        .ToAccumulator = true,
        .FlipOperands = true,
        .BytesCount = 1,
        .Bytes = 
        {
            {
                .Op = 0b10100010,
                .OpMask = 0b11111110,
                .WBitAt = 1
            }
        }
    };
    
    Table.Instructions[Table.Count++] =
    {
        .Name = "nil",
        .Description = "Reg/Memory with register to either",
        .OperandsCount = 2,
        .BytesCount = 2,
        .Bytes =
        {
            {
                .Op = 0b00000000,
                .OpMask = 0b11000100,
                .SubOpMask = 0b111 << 3,
                .WBitAt = 1,
                .DBitAt = 2,
            },
            {
                .ModBitsAt = 7,
                .RegBitsAt = 4,
                .RMBitsAt = 1,
            }
        },
        .SubInstructionsCount = 5,
        .SubInstructions =
        {
            { 0b000 << 3, "add" },
            { 0b010 << 3, "adc" },
            { 0b101 << 3, "sub" },
            { 0b011 << 3, "sbb" },
            { 0b111 << 3, "cmp" },
        }
    };
    
    Table.Instructions[Table.Count++] = 
    {
        .Name = "nil",
        .Description = "Immediate to register/memory",
        .OperandsCount = 2,
        .HasImmediate = true,
        .BytesCount = 2,
        .Bytes =
        {
            {
                .Op = 0b10000000,
                .OpMask = 0b11000100,
                .WBitAt = 1,
                .SBitAt = 2,
            },
            {
                .SubOpMask = 0b00111000,
                .ModBitsAt = 7, 
                .RMBitsAt = 1,
            }
        },
        .SubInstructionsCount = 5,
        .SubInstructions =
        {
            { 0b000 << 3, "add" },
            { 0b010 << 3, "adc" },
            { 0b101 << 3, "sub" },
            { 0b011 << 3, "sbb" },
            { 0b111 << 3, "cmp" },
        }
    };
    
    Table.Instructions[Table.Count++] =
    {
        .Name = "nil",
        .Description = "Immediate to accumulator",
        .OperandsCount = 2,
        .HasImmediate = true,
        .ToAccumulator = true,
        .BytesCount = 1,
        .Bytes =
        {
            {
                .Op = 0b00000100,
                .OpMask = 0b11000110,
                .SubOpMask = 0b111 << 3,
                .WBitAt = 1,
            },
        },
        .SubInstructionsCount = 5,
        .SubInstructions =
        {
            { 0b000 << 3, "add" },
            { 0b010 << 3, "adc" },
            { 0b101 << 3, "sub" },
            { 0b011 << 3, "sbb" },
            { 0b111 << 3, "cmp" },
        }
    };
    
#define ByteJump(InsName, InsDescription, InsOp) \
{ \
.Name = InsName, \
.Description = InsDescription, \
.OperandsCount = 1, \
.IsByteJump = true, \
.BytesCount = 1, \
.Bytes = \
{ \
{ \
.Op = InsOp, \
.OpMask = 0b11111111, \
} \
} \
}
    Table.Instructions[Table.Count++] = ByteJump("jz", "Jump on zero", 0b01110100);
    Table.Instructions[Table.Count++] = ByteJump("jl", "Jump on less", 0b01111100);
    Table.Instructions[Table.Count++] = ByteJump("jle", "Jump on less or equal", 0b01111110);
    Table.Instructions[Table.Count++] = ByteJump("jb", "Jump on below", 0b01110010);
    Table.Instructions[Table.Count++] = ByteJump("jbe", "Jump on below or equal", 0b01110110);
    Table.Instructions[Table.Count++] = ByteJump("jp", "Jump on parity", 0b01111010);
    Table.Instructions[Table.Count++] = ByteJump("jo", "Jump on overflow", 0b01110000);
    Table.Instructions[Table.Count++] = ByteJump("js", "Jump on sign", 0b01111000);
    Table.Instructions[Table.Count++] = ByteJump("jnz", "Jump on not zero", 0b01110101);
    Table.Instructions[Table.Count++] = ByteJump("jnl", "Jump on not less", 0b01111101);
    Table.Instructions[Table.Count++] = ByteJump("jnle", "Jump on not less or equal", 0b01111111);
    Table.Instructions[Table.Count++] = ByteJump("jnb", "Jump on not below", 0b01110011);
    Table.Instructions[Table.Count++] = ByteJump("jnbe", "Jump on not below or equal", 0b01110111);
    Table.Instructions[Table.Count++] = ByteJump("jnp", "Jump on not par", 0b01111011);
    Table.Instructions[Table.Count++] = ByteJump("jno", "Jump on not overflow", 0b01110001);
    Table.Instructions[Table.Count++] = ByteJump("jns", "Jump on not sign", 0b01111001);
    Table.Instructions[Table.Count++] = ByteJump("loop", "Loop CX times", 0b11100010);
    Table.Instructions[Table.Count++] = ByteJump("loopz", "Loop while zero", 0b11100001);
    Table.Instructions[Table.Count++] = ByteJump("loopnz", "Loop while not zero", 0b11100000);
    Table.Instructions[Table.Count++] = ByteJump("jcxz", "Jump on CX zero", 0b11100011);
#undef ByteJump
    
    Table.Instructions[Table.Count++] =
    {
        .Name = "push",
        .Description = "Register/memory",
        .OperandsCount = 1,
        .BytesCount = 2,
        .Bytes =
        {
            {
                .Op = 0b11111111,
                .OpMask = 0b11111111,
            },
            {
                .Op = 0b00110000,
                .OpMask = 0b00111000,
                .ModBitsAt = 7,
                .RMBitsAt = 1,
            }
        }
    };
    
    Table.Instructions[Table.Count++] =
    {
        .Name = "push",
        .Description = "Register",
        .OperandsCount = 1,
        .BytesCount = 1,
        .Bytes =
        {
            {
                .Op = 0b01010000,
                .OpMask = 0b11111000,
                .RegBitsAt = 1,
            },
        }
    };
    
    Table.Instructions[Table.Count++] =
    {
        .Name = "push",
        .Description = "Segment register",
        .OperandsCount = 1,
        .HasSegmentRegister = true,
        .BytesCount = 1,
        .Bytes =
        {
            {
                .Op = 0b00000110,
                .OpMask = 0b11000111,
                .RegBitsAt = 4,
            },
        }
    };
    
    if(ArgCount > 1)
    {
        int File = open(Args[1], O_RDONLY);
        if(File != -1)
        {
            int BytesRead = 0;
            
            printf("bits 16\n\n");
            
            int KeepReading = true;
            while(KeepReading)
            {
                u8 *ByteAt = Bytes;
                BytesRead = ReadBytesFromFile(File, &ByteAt, 1);
                
                if(BytesRead == 1)
                {
                    for(u32 InstructionIndex = 0;
                        InstructionIndex < Table.Count;
                        InstructionIndex++)
                    {
                        instruction InsAt = Table.Instructions[InstructionIndex];
                        
                        if((Bytes[0] & InsAt.Bytes[0].OpMask) == InsAt.Bytes[0].Op)
                        {
                            DebugLabel(ByteFound);
                            BytesRead = ReadBytesFromFile(File, &ByteAt, InsAt.BytesCount - BytesRead);
                            
                            b32 HasMode = false;
                            b32 HasDisplacement = false;
                            b32 HasRegister = false;
                            b32 MemoryMode = false;
                            b32 RegisterMode = false;
                            b32 EffectiveAddressMode = false;
                            b32 Displacement8Bit = false;
                            b32 Displacement16Bit = false;
                            b32 DBitSet = false;
                            b32 SBitSet = false;
                            b32 VBitSet = false;
                            b32 WBitSet = false;
                            u8 RegBits = 0;
                            u8 ModBits = 0;
                            u8 RMBits = 0;
                            s16 DisplacementValue = 0;
                            s16 DataValue = 0;
                            s8 IncrementValue = 0;
                            
                            char *InstructionName = InsAt.Name;
#define OP_DEST 0
#define OP_SOURCE 1
                            char Operands[2][255] = {};
                            char *SourceOperand = Operands[1];
                            char *DestOperand = Operands[0];
                            u32 NextOperand = OP_DEST;
                            
                            if(InsAt.ToAccumulator)
                            {
                                NextOperand = OP_SOURCE;
                            }
                            
                            for(u32 BytesIndex = 0;
                                BytesIndex < InsAt.BytesCount;
                                BytesIndex++)
                            {
                                u8 CurrentByte = Bytes[BytesIndex];
                                instruction_byte CurrentInsByte = InsAt.Bytes[BytesIndex];
                                
                                if(CurrentInsByte.OpMask)
                                {
                                    Assert((CurrentByte & CurrentInsByte.OpMask) == CurrentInsByte.Op);
                                } 
                                
                                SetBitFromBitAt(&DBitSet, CurrentByte, CurrentInsByte.DBitAt);
                                SetBitFromBitAt(&WBitSet, CurrentByte, CurrentInsByte.WBitAt);
                                SetBitFromBitAt(&SBitSet, CurrentByte, CurrentInsByte.SBitAt);
                                SetBitFromBitAt(&VBitSet, CurrentByte, CurrentInsByte.VBitAt);
                                
                                if(CurrentInsByte.SubOpMask)
                                {
                                    Assert(InsAt.SubInstructionsCount);
                                    
                                    u8 SubInsBits = (CurrentByte & CurrentInsByte.SubOpMask);
                                    
                                    b32 FoundSubInstruction = false;
                                    sub_instruction ScanSubIns = {};
                                    for(u32 SubInstructionsIndex = 0;
                                        (SubInstructionsIndex < InsAt.SubInstructionsCount) && !FoundSubInstruction;
                                        SubInstructionsIndex++)
                                    {
                                        ScanSubIns = InsAt.SubInstructions[SubInstructionsIndex];
                                        FoundSubInstruction = (ScanSubIns.Op == SubInsBits);
                                    }
                                    Assert(FoundSubInstruction);
                                    
                                    InstructionName = ScanSubIns.Name;
                                }
                                
                                if(CurrentInsByte.ModBitsAt)
                                {
                                    HasMode = true;
                                    ModBits = ExtractBitsFromByte(CurrentByte, 0b11, CurrentInsByte.ModBitsAt);
                                    
                                    if(ModBits == 0b00)
                                    {
                                        MemoryMode = true;
                                    }
                                    else if(ModBits == 0b01)
                                    {
                                        HasDisplacement = true;
                                        Displacement8Bit = true;
                                    }
                                    else if(ModBits == 0b10)
                                    {
                                        HasDisplacement = true;
                                        Displacement16Bit = true;
                                    }
                                    else if(ModBits == 0b11)
                                    {
                                        RegisterMode = true;
                                    }
                                }
                                
                                if(CurrentInsByte.RegBitsAt)
                                {
                                    RegBits = ExtractBitsFromByte(CurrentByte, 0b111, CurrentInsByte.RegBitsAt);
                                    HasRegister = true;
                                }
                                
                                if(CurrentInsByte.RMBitsAt)
                                {
                                    RMBits = ExtractBitsFromByte(CurrentByte, 0b111, CurrentInsByte.RMBitsAt);
                                    
                                    if(MemoryMode && RMBits == 0b110)
                                    {
                                        MemoryMode = false;
                                        EffectiveAddressMode = true;
                                    }
                                }
                                
                            } // for
                            
                            DebugLabel(Parsed);
                            
                            if(InsAt.HasAddress)
                            {
                                HasMode = true;
                                WBitSet = true;
                                EffectiveAddressMode = true;
                            }
                            
                            if(HasRegister)
                            {
                                b32 IsWide = (WBitSet || InsAt.OperandsCount == 1);
                                u32 TargetOperand = OP_DEST;
                                char *OperandName = 0;
                                
                                if(!InsAt.HasSegmentRegister)
                                {
                                    OperandName = RegistersNames[RegBits][IsWide];
                                }
                                else
                                {
                                    OperandName = SegmentRegistersNames[RegBits];
                                }
                                
                                b32 OperandIsDest = (DBitSet ||
                                                     InsAt.HasImmediate ||
                                                     (InsAt.OperandsCount == 1));
                                TargetOperand = (OperandIsDest) ? OP_DEST : OP_SOURCE;
                                NextOperand = (OperandIsDest)   ? OP_SOURCE : OP_DEST;
                                
                                CStringCopy(Operands[TargetOperand], OperandName);
                            }
                            
                            
                            if(HasMode)
                            {
                                if(MemoryMode)
                                {
                                    if(InsAt.OperandsCount == 1)
                                    {
                                        sprintf(Operands[NextOperand++], "%s [%s]", WBitSet ? "byte" : "word", DisplacementsNames[RMBits]);
                                    }
                                    else
                                    {
                                        sprintf(Operands[NextOperand++], "[%s]", DisplacementsNames[RMBits]);
                                    }
                                }
                                else if(RegisterMode)
                                {
                                    CStringCopy(Operands[NextOperand++], RegistersNames[RMBits][WBitSet]);
                                }
                                else if(HasDisplacement)
                                {
                                    if(Displacement8Bit)
                                    {
                                        ReadBytesFromFile(File, &ByteAt, 1);
                                        DisplacementValue = (s8)ByteAt[-1];
                                    }
                                    else if(Displacement16Bit)
                                    {
                                        ReadBytesFromFile(File, &ByteAt, 2);
                                        DisplacementValue = ((s16)ByteAt[-1] << 8) | (s16)ByteAt[-2];
                                    }
                                    else
                                    {
                                        Assert(0);
                                    }
                                    
                                    if(DisplacementValue != 0)
                                    {
                                        if(InsAt.OperandsCount == 1)
                                        {
                                            b32 IsNegative = (DisplacementValue < 0);
                                            sprintf(Operands[NextOperand++], "word [%s %c %d]",
                                                    DisplacementsNames[RMBits], 
                                                    IsNegative ? '-' : '+',
                                                    IsNegative ? DisplacementValue * -1 : DisplacementValue);
                                        }
                                        else
                                        {
                                            b32 IsNegative = (DisplacementValue < 0);
                                            sprintf(Operands[NextOperand++], "[%s %c %d]", 
                                                    DisplacementsNames[RMBits], 
                                                    IsNegative ? '-' : '+',
                                                    IsNegative ? DisplacementValue * -1 : DisplacementValue);
                                        }
                                    }
                                    else
                                    {
                                        if(InsAt.OperandsCount == 1)
                                        {
                                            sprintf(Operands[OP_DEST], "%s [%s]", WBitSet ? "word" : "byte", DisplacementsNames[RMBits]);
                                        }
                                        else
                                        {
                                            sprintf(Operands[NextOperand++], "[%s]", DisplacementsNames[RMBits]);
                                        }
                                    }
                                    
                                }
                                else if(EffectiveAddressMode)
                                {
                                    // NOTE(luca): Data is always 16-bit displacement, read 2 bytes.
                                    ReadBytesFromFile(File, &ByteAt, 2);
                                    DataValue = ((s16)ByteAt[-1] << 8) | (s16)ByteAt[-2];
                                    
                                    if(InsAt.HasImmediate || InsAt.OperandsCount == 1)
                                    {
                                        sprintf(Operands[OP_DEST], "word [%d]", DataValue);
                                    }
                                    else
                                    {
                                        sprintf(Operands[NextOperand++], "[%d]", DataValue);
                                    }
                                    
                                }
                                else
                                {
                                    Assert(0);
                                }
                            }
                            
                            if(InsAt.HasImmediate)
                            {
                                if(WBitSet && !SBitSet)
                                {
                                    ReadBytesFromFile(File, &ByteAt, 2);
                                    DataValue = (ByteAt[-1] << 8) | ByteAt[-2];
                                }
                                else
                                {
                                    ReadBytesFromFile(File, &ByteAt, 1);
                                    DataValue = ByteAt[-1];
                                }
                                
                                if(MemoryMode || HasDisplacement)
                                {
                                    sprintf(Operands[OP_SOURCE], "%s %d", WBitSet ? "word" : "byte", DataValue);
                                }
                                else
                                {
                                    sprintf(Operands[OP_SOURCE], "%d", DataValue);
                                }
                            }
                            
                            if(InsAt.ToAccumulator)
                            {
                                sprintf(Operands[OP_DEST], "%s", RegistersNames[ACCUMULATOR][WBitSet]);
                            }
                            
                            if(InsAt.IsByteJump)
                            {
                                ReadBytesFromFile(File, &ByteAt, 1);
                                IncrementValue = (s8)ByteAt[-1];
                                IncrementValue += InsAt.BytesCount + 1;
                                b32 IsNegative = (IncrementValue < 0);
                                sprintf(Operands[NextOperand++], "$%c%d", 
                                        IsNegative ? '-' : '+',
                                        IsNegative ? IncrementValue *= -1 : IncrementValue);
                            }
                            
                            if(InsAt.FlipOperands)
                            {
                                Assert(InsAt.OperandsCount == 2);
                                Swap(char *, SourceOperand, DestOperand);
                            }
                            
                            if(InsAt.OperandsCount == 2)
                            {
                                printf("%s %s, %s\n", InstructionName, DestOperand, SourceOperand);
                            }
                            else if(InsAt.OperandsCount == 1)
                            {
                                printf("%s %s\n", InstructionName, DestOperand);
                            }
                            else if(InsAt.OperandsCount == 0)
                            {
                                printf("%s\n", InstructionName);
                            }
                            else
                            {
                                Assert(0);
                            }
                            
                            break;
                        } // if op
                    } // for ins
                } // if bytesread
                else if(BytesRead == -1)
                {
                    fprintf(stderr, "Error while reading.\n");
                    KeepReading = false;
                }
                else if(BytesRead == 0)
                {
                    KeepReading = false;
                }
                else
                {
                    fprintf(stderr, "Read too many bytes.\n");
                    KeepReading = false;
                }
            } // while
        } // if file
        else
        {
            fprintf(stderr, "Could not open file.\n");
        }
    } // if argcount
    else
    {
        fprintf(stderr, "Missing argument.\n"
                "Usage: %s <assembly>\n", Args[0]);
    }
    
    return 0;
}