
enum instruction_clocks_operand_type
{
    InstructionClocksOperand_None = 0,
    InstructionClocksOperand_Memory,
    InstructionClocksOperand_Immediate,
    InstructionClocksOperand_Accumulator,
    InstructionClocksOperand_Register,
    InstructionClocksOperand_Count
};

struct instruction_clocks
{
    operation_type Op;
    instruction_clocks_operand_type Operands[2];
    u32 Clocks;
    u32 Transfers;
    b32 EffectiveAddress;
};

#define Memory InstructionClocksOperand_Memory
#define Immediate InstructionClocksOperand_Immediate
#define Accumulator InstructionClocksOperand_Accumulator
#define Register InstructionClocksOperand_Register
#define None InstructionClocksOperand_None

// NOTE(luca): Instructions containing accumulator should be put first so they have precedence
// on instructions with registers (accumulator is a register).
instruction_clocks ClocksTable[] =
{
    { Op_mov, { Memory, Accumulator }, 10, 1, false },
    { Op_mov, { Accumulator, Memory }, 10, 1, false },
    { Op_mov, { Register, Register },   2, 0, false },
    { Op_mov, { Register, Memory },     8, 1, true  },
    { Op_mov, { Memory, Register },     9, 1, true  },
    { Op_mov, { Register, Immediate },  4, 0, false },
    { Op_mov, { Memory, Immediate },   10, 1, true  },
    
    { Op_add, { Accumulator, Immediate },  4, 0, false },
    { Op_add, { Register, Register },      3, 0, false },
    { Op_add, { Register, Memory },        9, 1, true  },
    { Op_add, { Memory, Register },       16, 2, true  },
    { Op_add, { Register, Immediate },     4, 0, false },
    { Op_add, { Memory, Immediate },      17, 2, true  },
};

#undef Memory
#undef Register
#undef None
#undef Accumulator
#undef Immediate