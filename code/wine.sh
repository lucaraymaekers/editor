#!/bin/sh

set -eu
ScriptDirectory="$(dirname "$(readlink -f "$0")")"
cd "$ScriptDirectory"

# Targets
editor=0
cling=0
for Arg in "$@"; do eval "$Arg=1" 2>/dev/null || :; done

Command=
[ "$editor" = 1 ] && Command="editor"
[ "$cling"  = 1 ] && Command="cling"

cat <<EOF | wine cmd.exe 2>/dev/null
call build.bat $Command
EOF
