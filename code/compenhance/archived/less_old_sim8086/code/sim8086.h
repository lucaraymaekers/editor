/* date = May 13th 2025 11:06 pm */

#ifndef SIM8086_H
#define SIM8086_H

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int64_t  s64;
typedef int32_t  s32;
typedef int16_t  s16;
typedef int8_t   s8;
typedef s32      b32;
typedef float    r32;
typedef double   r64;
#define false    0
#define true     1

#define global_variable static
#define internal static
#define local_persist static

struct instruction_byte
{
    // NOTE(luca): All byte positions are 1-based so that 0 can mean that you do not have to test for this byte.
    u8 Op;
    u8 OpMask;
    u8 SubOpMask;
    u8 WBitAt;
    u8 SBitAt;
    u8 DBitAt;
    u8 VBitAt;
    u8 ModBitsAt;
    u8 RegBitsAt;
    u8 RMBitsAt;
};
struct sub_instruction
{
    u8 Op;
    char *Name;
};
struct instruction
{
    char *Name;
    char *Description;
    u32 OperandsCount;
    b32 HasSegmentRegister;
    b32 HasImmediate;
    b32 HasAddress;
    b32 IsByteJump;
    b32 ToAccumulator;
    b32 FlipOperands;
    
    u32 BytesCount;
    instruction_byte Bytes[8];
    
    // NOTE(luca): Sub instructions are instructions which do the same work but can have different names depending on a second opcode.
    u32 SubInstructionsCount;
    sub_instruction SubInstructions[8];
};

struct instruction_table
{
    instruction *Instructions;
    u32 Count;
    u32 Size;
};

#endif //SIM8086_H
