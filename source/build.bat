@echo off

REM ----------------------------------------------------------

call C:\msvc\setup_x64.bat

cd %~dp0

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

IF NOT EXIST ..\build\cling.exe (
 cl -MTd -Gm- -nologo -GR- -EHa- -Oi -FC -Zi -WX -W4 -wd4459 -wd4456 -wd4201 -wd4100 -wd4101 -wd4189 -wd4063 -wd4505 -wd4996 -wd4389 -wd4244 -wd4702 -wd5287 -I..\source -Zc:strictStrings- -I..\source -Fecling.exe -Fmcling.map ..\source\cling\cling.c /link -opt:ref -incremental:no
)

..\build\cling.exe

REM ----------------------------------------------------------
