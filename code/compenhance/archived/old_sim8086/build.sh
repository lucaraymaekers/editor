#!/bin/sh
set -ex

gcc -ggdb -Wall 8086disassembler.c -o 8086disassembler
nasm -o listing_0037_single_register_mov listing_0037_single_register_mov.asm
nasm -o listing_0038_many_register_mov listing_0038_many_register_mov.asm
