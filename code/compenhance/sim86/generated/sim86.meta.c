enum flags_8086
{
Flag_Carry = (1 << 0),
 Flag_Parity = (1 << 1),
 Flag_AuxiliaryCarry = (1 << 2),
 Flag_Zero = (1 << 3),
 Flag_Sign = (1 << 4),
 Flag_Overflow = (1 << 5),
 Flag_Interrupt = (1 << 6),
 Flag_Direction = (1 << 7),
 Flag_Trap = (1 << 8),
 
};
typedef enum flags_8086 flags_8086;

enum sim86_enum
{
Sim86_Carry,
 Sim86_Parity,
 Sim86_AuxiliaryCarry,
 Sim86_Zero,
 Sim86_Sign,
 Sim86_Overflow,
 Sim86_Interrupt,
 Sim86_Direction,
 Sim86_Trap,
 Sim86_Count
};
typedef enum sim86_enum sim86_enum;

char * flags_8086_strings[] = {
"C","P","A","Z","S","O","I","D","T",
};

