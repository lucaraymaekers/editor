#!/bin/sh

set -eu

ScriptDirectory="$(dirname "$(readlink -f "$0")")"
cd "$ScriptDirectory"

#- Main
DidWork=0
Build="../build"

clang=1
gcc=0
debug=1
release=0
personal=0
slow=0
no_asan=1
clean=0

editor=0
windows=0

# Default
[ "$#" = 0 ] && editor=1

for Arg in "$@"; do eval "$Arg=1" 2>/dev/null || :; done
# Exclusive flags
[ "$release" = 1 ] && debug=0
[ "$gcc"     = 1 ] && clang=0
mkdir -p "$Build"

[ -f "./base/base_build.h" ] && personal=1

Compile()
{
 SourceFiles="$1"
 Out="$2"

 Flags="${3:-}"

 # NOTE(luca): _GNU_SOURCE is only for C source files since it is enabled by default in c++.
 CommonCompilerFlags="-nostdinc++ -fno-threadsafe-statics -I$ScriptDirectory -D_GNU_SOURCE=1"
 CommonWarningFlags="-Wall -Wextra -Wconversion -Wdouble-promotion -Wno-sign-conversion -Wno-sign-compare -Wno-double-promotion -Wno-unused-but-set-variable -Wno-unused-variable -Wno-write-strings -Wno-pointer-arith -Wno-unused-parameter -Wno-unused-function -Wno-missing-field-initializers -Wno-switch"
 LinkerFlags="-lm -ldl -lpthread"

 DebugFlags="-g -ggdb -g3"
 ReleaseFlags="-O3"

 ClangFlags="-fno-omit-frame-pointer -fdiagnostics-absolute-paths -fsanitize-undefined-trap-on-error -ftime-trace
-Wno-null-dereference -Wno-missing-braces -Wno-vla-extension -Wno-writable-strings   -Wno-address-of-temporary -Wno-int-to-void-pointer-cast -Wno-reorder-init-list -Wno-c99-designator"

 [ "$no_asan" = 0 ] && ClangFlags="$ClangFlags -fsanitize-trap -fsanitize=address"

 GCCFlags="-Wno-cast-function-type -Wno-missing-field-initializers -Wno-int-to-pointer-cast"

 Flags="$CommonCompilerFlags $Flags"
 [ "$debug"   = 1 ] && Flags="$Flags $DebugFlags -DEDITOR_INTERNAL=1"
 [ "$release" = 1 ] && Flags="$Flags $ReleaseFlags"
 [ "$personal" = 1 ] && Flags="$Flags -DEDITOR_PERSONAL=1"
 [ "$slow"     = 1 ] && Flags="$Flags -DEDITOR_SLOW_COMPILE=1"
 Flags="$Flags $CommonWarningFlags"
 [ "$clang" = 1 ] && Flags="$Flags $ClangFlags"
 [ "$gcc"   = 1 ] && Flags="$Flags $GCCFlags"
 Flags="$Flags $LinkerFlags"

 printf '%s\n' "$SourceFiles"

 Source=
 for File in $SourceFiles
 do Source="$Source $(readlink -f "$File")"
 done
 
 $Compiler $Flags $Source -o "$Build"/"$Out"

 DidWork=1
}

#- Targets
if [ "$clean"  = 1 ]
then
 rm -rf ../build/*
 DidWork=1
fi

[ "$debug"   = 1 ] && printf '[debug mode]\n'
[ "$release" = 1 ] && printf '[release mode]\n'
[ "$gcc"   = 1 ] && Compiler="g++"
[ "$clang" = 1 ] && Compiler="clang"
printf '[%s compile]\n' "$Compiler"

if [ "$editor" = 1 ]
then
 AppFlags="-fPIC --shared" 

 LibsFile="../build/editor_libs.o"

 # Faster compilation times by compiling all libraries in a separate translation unit.
 if [ "$slow" = 0 ]
 then
  # If libsfile does not exist or
  # If compiling with asan but libsfile does not contain asan
  # If compiling without asan but libsfile contains asan
  if { { [ "$no_asan" = 1 ] && nm "$LibsFile" 2>/dev/null | grep 'asan' > /dev/null; } ||
     { [ "$no_asan" = 0 ] && ! nm "$LibsFile" 2>/dev/null | grep 'asan' > /dev/null; } ||
     [ ! -f "$LibsFile" ]; }
  then
   Compile ./editor/editor_libs.h "$LibsFile" "-fPIC -x c++ -c -DEDITOR_SLOW_COMPILE=1 -Wno-unused-command-line-argument"
  fi
  AppFlags="$AppFlags $LibsFile"
 fi

 Compile "./editor/editor_app.cpp" editor_app.so "$AppFlags"
 Compile "./editor/editor_platform.cpp" editor "-lX11 -lGL -lGLX"
fi

if [ "$windows" = 1 ]
then
 printf 'call C:\BuildTools\devcmd.bat\ncall build.bat\n' | wine cmd.exe 2>/dev/null
 DidWork=1
fi

#- End

if [ "$DidWork" = 0 ]
then
 printf 'ERROR: No valid build target provided.\n'
 printf 'Usage: %s <editor [clean/no_asan/debug/release/gcc/clang/slow]>\n' "$0"
else
 printf 'Done.\n' # 4coder bug
fi
