#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdint.h>
#include <signal.h>

#ifdef DEBUG
#define Assert(Expr) \
if (Expr) \
{ \
raise(SIGTRAP); \
}
#else
#define Assert(Expr) ;
#endif

void
print_binary(uint8_t Byte)
{
    printf(" ");
    int Count = 8;
    while (Count--)
    {
        printf("%d", Byte >> 7);
        Byte <<= 1;
    }
    printf("\n");
}

int main(int ArgC, char *Args[])
{
    if (ArgC < 2)
    {
        fprintf(stderr, "No argument provided.\n");
    }
    else
    {
        char *Filename = Args[1];
        int Ret = 0;
        int FD = 0;
        int Filesize = 0;
        struct stat StatBuffer = {0};
        uint8_t *Buffer = 0;
        
        FD = open(Filename, O_RDONLY);
        if (FD == -1)
        {
            fprintf(stderr, "Failed to open file '%s'.\n", Filename);
        }
        else
        {
            Ret = fstat(FD, &StatBuffer);
            if (Ret == -1)
            {
                fprintf(stderr, "Failed to get file stats.\n");
                return 1;
            }
            else
            {
                Filesize = StatBuffer.st_size;
                if (!Filesize)
                {
                    fprintf(stderr, "Cannot disassemble empty file.\n");
                    return 1;
                }
                
                Buffer = mmap(0, Filesize, PROT_READ, MAP_FILE | MAP_SHARED, FD, 0);
                if (Buffer == (void*)-1)
                {
                    fprintf(stderr, "Failed to allocate buffer for file.\n");
                    return 1;
                }
                else
                {
                    printf("; %s:\n", Filename);
                    printf("bits 16\n");
                    
#define MOV_RM_REG(Byte)   ((Byte & 0b11111100) == 0b10001000)
#define MOV_IM_REG(Byte)   ((Byte & 0b11110000) == 0b10110000)
#define MOV_IM_RM(Byte)    ((Byte & 0b11111110) == 0b11000110)
#define MOV_MEM_ACC(Byte)  ((Byte & 0b11111110) == 0b10100000)
#define MOV_ACC_MEM(Byte)  ((Byte & 0b11111110) == 0b10100010)
#define ARITH_RM_RM(Byte)  ((Byte & 0b11000100) == 0b00000000)
#define ARITH_IM_RM(Byte)  ((Byte & 0b11111100) == 0b10000000)
#define ARITH_IM_ACC(Byte) ((Byte & 0b11000100) == 0b00000100)
                    // NOTE(luca): Arithmetic operations add, sub and cmp have an octal field to
                    // differentiate between them.
                    // 00 000 0 dw -> ADD
                    // 00 101 0 dw -> SUB
                    // 00 111 0 dw -> CMP
                    
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
                    char *JumpOperations[] = {
                        [JE] = "je",
                        [JNZ] = "jnz",
                        [JL] = "jl",
                        [JLE] = "jle",
                        [JB] = "jb",
                        [JBE] = "jbe",
                        [JP] = "jp",
                        [JO] = "jo",
                        [JS] = "js",
                        [JNL] = "jnl",
                        [JG] = "jg",
                        [JNB] = "jnb",
                        [JA] = "ja",
                        [JNP] = "jnp",
                        [JNO] = "jno",
                        [JNS] = "jns",
                        [LOOP] = "loop",
                        [LOOPZ] = "loopz",
                        [LOOPNZ] = "loopnz",
                        [JCXZ] = "jcxz",
                    };
                    
                    // NOTE(luca): See 4-20 in the manual
                    // The following arrays are in order such that hey can be indexed with the r/m or reg field.
                    char *Registers[][2] = {
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
                    char *Displacements[] = {
                        [0b000] = "bx + si",
                        [0b001] = "bx + di",
                        [0b010] = "bp + si",
                        [0b011] = "bp + di",
                        [0b100] = "si",
                        [0b101] = "di",
                        [0b110] = "bp",
                        [0b111] = "bx"
                    };
                    
                    for (int At = 0; At < Filesize; At++)
                    {
                        uint8_t Byte1 = Buffer[At];
                        
                        if (MOV_RM_REG(Byte1))
                        {
                            // register/memory to/from register
                            // no displacement except when r/m is110
                            
#define MOD(Byte1, Mask) ((Byte1 & 0b11000000) == (Mask << 6))
                            
                            int DSet = Byte1 & (1 << 1);
                            // NOTE(Luca): Truncated to a boolean so that it can index the Registers array.
                            int WSet = Byte1 & (1 << 0) ? 1 : 0;
                            uint8_t Byte2 = Buffer[++At];
                            int RM = Byte2 & 0b111;
                            int REG = (Byte2 >> 3) & 0b111;
                            
                            if (MOD(Byte2, 0b00))
                            {
                                // memory mode no displacement
                                
                                if (RM == 0b110)
                                {
                                    // exception, 16 bit displacement
                                    uint8_t DispL = Buffer[++At];
                                    uint8_t DispH = Buffer[++At];
                                    int Displacement = (DispH << 8) + DispL;
                                    
                                    // NOTE(Luca): Since a 16 bit displacement follows, shouldn't W bit always be set?
                                    printf("mov %s, [%d]\n", Registers[REG][WSet], Displacement);
                                }
                                else
                                {
                                    if (DSet)
                                    {
                                        printf("mov %s, [%s]\n", Registers[REG][WSet], Displacements[RM]);
                                    }
                                    else
                                    {
                                        printf("mov [%s], %s\n", Displacements[RM], Registers[REG][WSet]);
                                    }
                                }
                                
                            }
                            else if (MOD(Byte2, 0b01))
                            {
                                // 8 bit displacement
                                uint8_t DispL = Buffer[++At];
                                int8_t Displacement = DispL;
                                char Sign = (Displacement < 0) ? '-' : '+';
                                Displacement = abs(Displacement);
                                
                                if (DSet)
                                {
                                    if (Displacement)
                                    {
                                        printf("mov %s, [%s %c %d]\n", Registers[REG][WSet], Displacements[RM], Sign, Displacement);
                                    }
                                    else
                                    {
                                        printf("mov %s, [%s]\n", Registers[REG][WSet], Displacements[RM]);
                                    }
                                }
                                else
                                {
                                    if (Displacement)
                                    {
                                        printf("mov [%s %c %d], %s\n", Displacements[RM], Sign, Displacement, Registers[REG][WSet]);
                                    }
                                    else
                                    {
                                        printf("mov [%s], %s\n", Displacements[RM], Registers[REG][WSet]);
                                    }
                                }
                            }
                            else if (MOD(Byte2, 0b10))
                            {
                                // 16 bit displacement
                                uint8_t DispL = Buffer[++At];
                                uint8_t DispH = Buffer[++At];
                                int16_t Displacement = DispL + (DispH << 8);
                                char Sign = (Displacement < 0) ? '-' : '+';
                                Displacement = abs(Displacement);
                                
                                if (DSet)
                                {
                                    if (Displacement)
                                    {
                                        printf("mov %s, [%s %c %d]\n", Registers[REG][WSet], Displacements[RM], Sign, Displacement);
                                    }
                                    else
                                    {
                                        printf("mov %s, [%s]\n", Registers[REG][WSet], Displacements[RM]);
                                    }
                                }
                                else
                                {
                                    if (Displacement)
                                    {
                                        printf("mov [%s %c %d], %s\n", Displacements[RM], Sign, Displacement, Registers[REG][WSet]);
                                    }
                                    else
                                    {
                                        printf("mov [%s], %s\n", Displacements[RM], Registers[REG][WSet]);
                                    }
                                }
                            }
                            else if (MOD(Byte2, 0b11))
                            {
                                // register mode
                                printf("mov %s, %s\n", Registers[RM][WSet], Registers[REG][WSet]);
                            }
                            
                        }
                        else if (MOV_IM_REG(Byte1))
                        {
                            // Immediate to register
                            int WSet = Byte1 & (1 << 3) ? 1 : 0;
                            int REG = Byte1 & 0b111;
                            uint8_t DataL = Buffer[++At];
                            
                            if (WSet)
                            {
                                uint8_t DataH = Buffer[++At];
                                uint16_t Immediate = (DataH << 8) + DataL;
                                printf("mov %s, %d\n", Registers[REG][WSet], Immediate);
                            }
                            else
                            {
                                uint16_t Immediate = DataL;
                                printf("mov %s, %d\n", Registers[REG][WSet], Immediate);
                            }
                            
                        }
                        else if (MOV_IM_RM(Byte1))
                        {
                            // Immediate to register/memory
                            int WSet = Byte1 & 1;
                            uint8_t Byte2 = Buffer[++At];
                            int MOD = (Byte2 & 0b10000000) >> 6;
                            int RM = Byte2 & 0b111;
                            Assert(0);
                            
                            if (MOD == 0b00)
                            {
                                // memory mode no displacement
                                uint8_t DataL = Buffer[++At];
                                char *OperationSize = 0;
                                uint8_t Immediate = DataL;
                                
                                if (WSet)
                                {
                                    uint8_t DataH = Buffer[++At];
                                    OperationSize = "word" ;
                                    Immediate += DataH << 8;
                                }
                                else
                                {
                                    OperationSize = "byte";
                                }
                                
                                printf("mov [%s], %s %d\n", Displacements[RM], OperationSize, Immediate);
                            }
                            else if (MOD == 0b01)
                            {
                                // memory mode 8 bit displacement
                                uint8_t DispL = Buffer[++At];
                                int8_t Displacement = DispL;
                                char Sign = (Displacement < 0) ? '-' : '+'; 
                                
                                if (WSet)
                                {
                                    uint8_t DataL = Buffer[++At];
                                    uint8_t DataH = Buffer[++At];
                                    uint16_t Immediate = DataL + (DataH << 8);
                                    printf("mov [%s %c %d], %d", Displacements[RM], Sign, Displacement, Immediate);
                                }
                                else
                                {
                                    uint8_t Immediate = Buffer[++At];
                                    printf("mov [%s %c %d], %d", Displacements[RM], Sign, Displacement, Immediate);
                                }
                            }
                            else if (MOD == 0b10)
                            {
                                // memory mode 16 bit displacement
                                
                                uint8_t Byte3 = Buffer[++At];
                                uint8_t Byte4 = Buffer[++At];
                                
                                int16_t Displacement = Byte3 + (Byte4 << 8);
                                char Sign = (Displacement < 0) ? '-' : '+';
                                
                                if (WSet)
                                {
                                    uint8_t Byte5 = Buffer[++At];
                                    uint8_t Byte6 = Buffer[++At];
                                    uint16_t Immediate = Byte5 + (Byte6 << 8);
                                    printf("mov [%s %c %d], word %d\n", Displacements[RM], Sign, abs(Displacement), Immediate);
                                }
                                else
                                {
                                    uint8_t Immediate = Buffer[++At];
                                    printf("mov [%s %c %d], byte %d\n", Displacements[RM], Sign, abs(Displacement), Immediate);
                                }
                            }
                            else if (MOD == 0b11)
                            {
                                printf("; register mode no displacement\n");
                            }
                            
                        }
                        else if (MOV_MEM_ACC(Byte1))
                        {
                            // memory to accumulator
                            int WSet = Byte1 & (1 << 0);
                            uint8_t DataL = Buffer[++At];
                            int Immediate = DataL;
                            
                            if (WSet)
                            {
                                uint8_t DataH = Buffer[++At];
                                Immediate += DataH << 8;
                            }
                            printf("mov ax, [%d]\n", Immediate);
                            
                        }
                        else if (MOV_ACC_MEM(Byte1))
                        {
                            // accumulator to memory
                            int WSet = Byte1 & (1 << 0);
                            uint8_t DataL = Buffer[++At];
                            int Immediate = DataL;
                            
                            if (WSet)
                            {
                                uint8_t DataH = Buffer[++At];
                                Immediate += DataH << 8;
                            }
                            printf("mov [%d], ax\n", Immediate);
                        }
                        else if (ARITH_RM_RM(Byte1))
                        {
                            int WSet = Byte1 & (1 << 0);
                            int DSet = Byte1 & (1 << 1);
                            char *Operation = 0;
                            int Displacement = 0;
                            int OP = (Byte1 & 0b00111000) >> 3;
                            uint8_t Byte2 = Buffer[++At];
                            int MOD = (Byte2 & 0b11000000) >> 6;
                            int REG = (Byte2 & 0b00111000) >> 3;
                            int RM  = (Byte2 & 0b00000111);
                            
                            switch (OP)
                            {
                                case 0b000: Operation = "add"; break;
                                case 0b101: Operation = "sub"; break;
                                case 0b111: Operation = "cmp"; break;
                            }
                            
                            if (MOD == 0b01)
                            {
                                uint8_t DispL = Buffer[++At];
                                Displacement = DispL;
                            }
                            else if (MOD == 0b10)
                            {
                                uint8_t DispL = Buffer[++At];
                                uint8_t DispH = Buffer[++At];
                                Displacement = DispL + (DispH << 8);
                            }
                            
                            if (MOD == 0b11)
                            {
                                char *Src  = 0; 
                                char *Dest = 0;
                                if (DSet)
                                {
                                    Src  = Registers[REG][WSet];
                                    Dest = Registers[RM][WSet];
                                }
                                else
                                {
                                    Dest = Registers[REG][WSet];
                                    Src  = Registers[RM][WSet];
                                }
                                printf("%s %s, %s\n", Operation, Src, Dest);
                            }
                            else
                            {
                                char *Src = 0;
                                char *Dest = 0;
                                if (DSet)
                                {
                                    printf("%s %s, [%s + %d]\n", Operation, Registers[REG][WSet], Displacements[RM], Displacement);
                                }
                                else
                                {
                                    printf("%s [%s + %d], %s\n", Operation, Displacements[RM], Displacement, Registers[REG][WSet]);
                                }
                            }
                            
                        }
                        else if (ARITH_IM_RM(Byte1))
                        {
                            int WSet = Byte1 & (1 << 0);
                            int SSet = Byte1 & (1 << 1);
                            int Displacement = 0;
                            int Immediate = 0;
                            char *Operation = 0;
                            char *OperationSize = WSet ? "word" : "byte";
                            uint8_t Byte2 = Buffer[++At];
                            int MOD = (Byte2 & 0b11000000) >> 6;
                            int RM  = (Byte2 & 0b00000111);
                            int OP  = (Byte2 & 0b00111000) >> 3;
                            
                            // TODO: sign extension
                            
                            switch (OP)
                            {
                                case 0b000: Operation = "add"; break;
                                case 0b101: Operation = "sub"; break;
                                case 0b111: Operation = "cmp"; break;
                            }
                            
                            if (MOD == 0b01)
                            {
                                uint8_t DispL = Buffer[++At];
                                Displacement = DispL;
                            }
                            else if (MOD == 0b10)
                            {
                                uint8_t DispL = Buffer[++At];
                                uint8_t DispH = Buffer[++At];
                                Displacement = DispL + (DispH << 8);
                            }
                            
                            uint8_t DataL = Buffer[++At];
                            Immediate = DataL;
                            if (WSet)
                            {
                                uint8_t DataH = Buffer[++At];
                                Immediate += DataH << 8;
                            }
                            
                            if (MOD == 0b00 && RM == 0b110)
                            {
                                int Memory = Immediate;
                                Immediate = Buffer[++At];
                                printf("%s %s [%d], %d\n", Operation, OperationSize, Memory, Immediate);
                            }
                            else if (MOD == 0b11)
                            {
                                printf("%s %s, %d\n", Operation, Registers[RM][WSet], Immediate);
                            }
                            else
                            {
                                // TODO(luca): Support signed displacements like in MOV
                                printf("%s %s [%s + %d], %d\n",
                                       Operation,
                                       OperationSize,
                                       Displacements[RM],
                                       Displacement,
                                       Immediate);
                            }
                        }
                        else if (ARITH_IM_ACC(Byte1))
                        {
                            int WSet = Byte1 & (1 << 0);
                            uint8_t DataL = Buffer[++At];
                            int Immediate = DataL;
                            char *Operation = 0;
                            
                            int OP = (Byte1 & 0b00111000) >> 3;
                            switch (OP)
                            {
                                case 0b000: Operation = "add"; break;
                                case 0b101: Operation = "sub"; break;
                                case 0b111: Operation = "cmp"; break;
                            }
                            
                            if (WSet)
                            {
                                uint8_t DataH = Buffer[++At];
                                Immediate += DataH << 8;
                            }
                            
                            printf("%s %s, %d\n", Operation, Registers[ACCUMULATOR][WSet], Immediate);
                        }
                        else
                        {
                            // Handles all jump instructions
                            // TODO: labels
                            switch (Byte1)
                            {
                                case JE:
                                case JL:
                                case JLE:
                                case JB:
                                case JBE:
                                case JP:
                                case JO:
                                case JS:
                                case JNL:
                                case JG:
                                case JNB:
                                case JA:
                                case JNP:
                                case JNO:
                                case JNS:
                                case JNZ:
                                case LOOP:
                                case LOOPZ:
                                case LOOPNZ:
                                case JCXZ:
                                {
                                    uint8_t INC = Buffer[++At];
                                    printf("%s %d\n", JumpOperations[Byte1], INC);
                                } break;
                                default:
                                {
                                    fprintf(stderr, "Unrecognized Operation from byte:\n");
                                    print_binary(Byte1);
                                    return 1;
                                } break;
                            }
                        }
                    }
                }
            }
        }
    }
    
    return 0;
}
