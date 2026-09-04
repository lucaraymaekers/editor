/*
	8086 asm disassembler
    For now this assumes that instructions are two bytes long.
*/

#include <stdio.h>
#include <string.h>

#define     INSTRUCTION_MASK   0b11111100
#define     D_MASK             (1 << 1)
#define     W_MASK             1
#define     REG_MASK           0b00111000
#define     MOD_MASK           0b00000111
#define     INSTRUCTION_MOV    0b10001000

void print_reg(unsigned char reg, int wide_flag);

int main(int argc, char **argv)
{
	FILE *file;
	char *filename = argv[1];

	if (argc < 2)
	{
		printf("No argument provided.\n");
		return 1;
	}
	
	file = fopen(filename ,"rb");
	
	if (file == NULL)
	{
		printf("File, %s not found.\n", filename);
		return 1;
	}
	
	char file_byte;
	unsigned char reg, mod, c;

	/* flags */
	int is_reg_destination = 0;
	int is_wide_operation = 0;
	char instruction[4];
	
	/* print useful header */
	printf("; %s disassembly:\n", filename);
	printf("bits 16\n");
	

	while ((file_byte = fgetc(file)) != EOF)
	{
		/* get the six first instruction bits */
		c = file_byte & INSTRUCTION_MASK;
		if (c == INSTRUCTION_MOV)
			strcpy(instruction, "mov\0");
		else
			strcpy(instruction, "ERR\0");

		/* check D bit */
		/* NOTE: This shift could be defined to show that this is the D-mask */
		c = file_byte & D_MASK;
		is_reg_destination = c ? 1 : 0;

		/* check W bit */
		c = file_byte & 1;
		is_wide_operation = c ? 1 : 0;

		file_byte = fgetc(file);

		/* get REG */
		c = file_byte & REG_MASK;
		reg = c >> 3;

		/* get R/M */
		c = file_byte & MOD_MASK;
		mod = c;

		/* print the decoded instructions */
		printf("%s ", instruction);

		/* print operands */
		if (is_reg_destination)
			print_reg(reg, is_wide_operation);
		else
			print_reg(mod, is_wide_operation);
		printf(", ");
		if (is_reg_destination)
			print_reg(mod, is_wide_operation);
		else
			print_reg(reg, is_wide_operation);

		printf("\n");
	}

	fclose(file);
	
	return 0;
}

void print_reg(unsigned char reg, int wide_flag)
{
	switch (reg)
	{
		case 0b000:
			if (wide_flag)
				printf("ax");
			else
				printf("al");
			break;
		case 0b001:
			if (wide_flag)
				printf("cx");
			else
				printf("cl");
			break;
		case 0b010:
			if (wide_flag)
				printf("dx");
			else
				printf("dl");
			break;
		case 0b011:
			if (wide_flag)
				printf("bx");
			else
				printf("bl");
			break;
		case 0b100:
			if (wide_flag)
				printf("sp");
			else
				printf("ah");
			break;
		case 0b101:
			if (wide_flag)
				printf("bp");
			else
				printf("ch");
			break;
		case 0b110:
			if (wide_flag)
				printf("si");
			else
				printf("dh");
			break;
		case 0b111:
			if (wide_flag)
				printf("di");
			else
				printf("dh");
			break;
		default:
			/* unknown register */
			printf("ER");
			break;
	}
}