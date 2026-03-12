#!/bin/sh

# NOTE(luca): This script bootsraps cling and runs it

set -eu
ScriptDirectory="$(dirname "$(readlink -f "$0")")"
cd "$ScriptDirectory"

mkdir -p ../build

[ -x "../build/cling" ] || cc -Wno-write-strings -D_GNU_SOURCE=1 -I. -o ../build/cling ./cling/cling.c
../build/cling "$@"
