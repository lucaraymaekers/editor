@echo off

REM ----------------------------------------------------------

call C:\msvc\setup_x64.bat

cd %~dp0

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

REM TODO: fix this hack by having cling.exe be up-to-date with cling_temp.exe or running cling_temp.exe if it is newer.

IF NOT EXIST ..\build\cling.exe (
  cl -MTd -Gm- -nologo -GR- -EHa- -Oi -FC -Zi -WX -W4 -wd4459 -wd4456 -wd4201 -wd4100 -wd4101 -wd4189 -wd4063 -wd4505 -wd4996 -wd4389 -wd4244 -wd4702 -wd5287 -I..\source -Zc:strictStrings- -I..\source -Fecling.exe -Fmcling.map ..\source\cling\cling.c /link -opt:ref -incremental:no
 ..\build\cling.exe
) ELSE (
 IF NOT EXIST ..\build\cling_temp.exe (
 ..\build\cling.exe
 ) ELSE (
 ..\build\cling_temp.exe norebuild
 )
)

REM ----------------------------------------------------------
