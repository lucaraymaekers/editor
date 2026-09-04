#!/bin/sh

ThisDir="$(dirname "$(readlink -f "$0")")"
cd "$ThisDir"

CompilerFlags="
-ggdb 
-DSIM8086_INTERNAL
-nostdinc++
"

WarningFlags="
-Wall
-Wextra
-Wno-unused-label
-Wno-unused-variable
-Wno-unused-but-set-variable
-Wno-missing-field-initializers
-Wno-write-strings
"

printf 'sim8086.c\n'
g++ $CompilerFlags $WarningFlags -o ../build/sim8086 sim8086.cpp

# printf 'print_binary.c\n'
# gcc -ggdb -Wall -Wno-unused-variable -o ../build/print_binary print_binary.c