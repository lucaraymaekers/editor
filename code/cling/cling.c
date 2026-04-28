//~ Defines
#define CLING_BUILD_PATH ".." SLASH "build" SLASH

//~ Libaries
#define BASE_FORCE_THREADS_COUNT 1
#define BASE_CONSOLE_APPLICATION 1
#include "base/base.h"
#include "base/base.c"

#define CLING_CODE_PATH ".." SLASH "code" SLASH
#define CLING_SOURCE_PATH CLING_CODE_PATH "cling" SLASH "cling.c"
#include "cling/cling.h"

NO_WARNINGS_BEGIN
#include "lib/metadesk/md.h"
#include "lib/metadesk/md.c"
NO_WARNINGS_END

//~ Globals
global_variable MD_Arena *GlobalMDArena = 0;
global_variable b32 OSWindows = false;
global_variable b32 OSLinux = false;

//~ Functions
internal void
RunCommand(void)
{
    str8 BuildCommand = Str8ArrayJoin(' ');
    Cng_RunCommand(BuildCommand);
}

internal void
WriteStreamToFile(MD_String8List Stream, char *FileName)
{
    MD_String8 Str = MD_S8ListJoin(GlobalMDArena, Stream, 0);
    OS_WriteEntireFile(FileName, S8Cast{.Data = Str.str, .Size = Str.size});
}

internal u8
HexToByte(u8 Char)
{
    u8 Byte = 0;
    if(Char >= 'a' && Char <= 'f') Byte = ((Char - 'a') + 10);
    if(Char >= 'A' && Char <= 'F') Byte = ((Char - 'A') + 10);
    if(Char >= '0' && Char <= '9') Byte = ((Char - '0') + 0);
    return Byte;
}

internal void 
WindowsBuild(str8 Source, str8_array *ExtraCompilerFlags, str8 ExtraLinkerFlags)
{
    arena *Arena = GlobalClingArena;
    str8 Output = PushS8(Arena, KB(2)); 
    
    str8_array *Command = PushStr8Array(256);
    SetSelectedArray(Command);
    
    Str8ArrayAppend(S8("cmd /c \"C:\\msvc\\setup_x64.bat"));
    Str8ArrayAppend(S8("&&"));
    
    Str8ArrayAppend(S8("cl"));
    Str8ArrayAppend(S8("-MTd -Gm- -nologo -GR- -EHa- -Oi -FC -Z7"));
    Str8ArrayAppend(S8("-Zc:strictStrings-"));
    Str8ArrayAppend(S8("-WX -W4 -wd4459 -wd4201 -wd4100 -wd4101 -wd4189 -wd4505 -wd4996 -wd4389 -wd4244 -wd5287 -wd4063 -wd4127"));
    
    Str8ArrayAppend(Str8ArrayJoinFrom(ExtraCompilerFlags, ' '));
    Str8ArrayAppend(Source);
    Str8ArrayAppend(ExtraLinkerFlags);
    
    RunCommand();
}

internal void
LinuxBuildCommand(str8 Source, 
                      str8 OutputName, 
                      b32 GCC, b32 Clang, b32 Asan, b32 Debug,
                      str8_array *ExtraFlags,
                      b32 Run)
{
    Log(S8Fmt "\n", S8Arg(Cng_GetBaseFileName(Source)));
    
    str8_array *Command = PushStr8Array(256);
    SetSelectedArray(Command);
    
    // NOTE(luca): These are almost all c++ flags.
    str8 CommonCompilerFlags = S8("-fno-threadsafe-statics -nostdinc++ -D_GNU_SOURCE=1 -fno-exceptions -fno-rtti");
    // TODO(luca): nasr should fix his enums, so we can enable -Wswitch again.
    str8 CommonWarningFlags = S8("-Wall -Wextra -Wconversion -Wno-double-promotion -Wno-unused-but-set-variable -Wno-write-strings -Wno-missing-field-initializers -Wno-pointer-arith -Wno-switch "
                                 "-Wno-unused-parameter "
                                 "-Wno-unused-variable "
                                 "-Wno-unused-function "
                                 "-Wno-unused-command-line-argument ");
    
    str8 LinkerFlags = S8("-lm");
    str8 Compiler = {0};
    if(0) {}
    else if(Clang) Compiler = S8("clang");
    else if(GCC) Compiler = S8("gcc");
    
    str8 ModeFlags = (Debug ? 
                      S8("-g -ggdb -g3 -fno-omit-frame-pointer") :
                      S8("-O3"));
    
    str8 ClangCompilerFlags = S8("-fdiagnostics-absolute-paths -ftime-trace");
    str8 ClangWarningFlags = S8("-Wno-null-dereference -Wno-missing-braces -Wno-vla-cxx-extension -Wno-writable-strings -Wno-missing-designated-field-initializers -Wno-address-of-temporary -Wno-int-to-void-pointer-cast");
    str8 AsanFlags = S8("-fsanitize=undefined,address");
    
    str8 GCCWarningFlags = S8("-Wno-cast-function-type -Wno-missing-field-initializers -Wno-int-to-pointer-cast");
    
    Str8ArrayAppendMultiple(Compiler,
                            ModeFlags,
                            CommonCompilerFlags,
                            CommonWarningFlags);
    if(Clang)
    {
        Str8ArrayAppendMultiple(ClangCompilerFlags, ClangWarningFlags);
    }
    if(GCC)
    {
        Str8ArrayAppend(GCCWarningFlags);
    }
    if(Asan)
    {
        Str8ArrayAppend(AsanFlags);
    }
    
    Str8ArrayAppendMultiple(Str8ArrayJoinFrom(ExtraFlags, ' '),
                            S8("-o"), OutputName,
                            LinkerFlags,
                            Source);
    
    if(Run)
    {
        RunCommand();
    }
}

internal void
MuzeBuildCommand(str8 Source, 
                 str8 OutputName, 
                 b32 GCC, b32 Clang, b32 Asan, b32 Debug,
                 str8_array *ExtraFlags,
                 str8 ExtraLinkerFlags,
                 str8 ExtraSource,
                 b32 IsDLL, b32 IsObject)
{
    if(0) {}
    else if(OSLinux)
    {
        if(0) {}
        else if(IsObject)
        {
            Str8ArrayAppendTo(ExtraFlags, S8("-fPIC -x c++ -c"));
            OutputName = Str8Fmt(S8Fmt ".o", S8Arg(OutputName));
        }
        else if(IsDLL)
        {
            Str8ArrayAppendTo(ExtraFlags, S8("-fPIC --shared"));
            OutputName = Str8Fmt(S8Fmt ".so", S8Arg(OutputName));
        }
        
        Str8ArrayAppendTo(ExtraFlags, ExtraLinkerFlags);
        LinuxBuildCommand(Source, OutputName, GCC, Clang, Asan, Debug, ExtraFlags, false);
        Str8ArrayAppend(ExtraSource);
        RunCommand();
    }
    else if(OSWindows)
    {
        if(0) {}
        else if(IsObject)
        {
            Str8ArrayAppendTo(ExtraFlags, S8("-c /TP"));
            Str8ArrayAppendTo(ExtraFlags, Str8Fmt("-Fo" S8Fmt ".obj", S8Arg(OutputName)));
        }
        else if(IsDLL)
        {
            Str8ArrayAppendTo(ExtraFlags, S8("/LD"));
            ExtraLinkerFlags = Str8Fmt(S8Fmt " /DLL /OUT:" S8Fmt ".dll",
                                       S8Arg(ExtraLinkerFlags), S8Arg(OutputName));
        }
        else
        {
            Str8ArrayAppendTo(ExtraFlags, Str8Fmt("-Fe" S8Fmt ".exe", S8Arg(OutputName)));
        }
        
        ExtraLinkerFlags = Str8Fmt("/link -opt:ref -incremental:no " S8Fmt, S8Arg(ExtraLinkerFlags));
        
        Str8ArrayAppendTo(ExtraFlags, Str8Fmt("-Fm" S8Fmt ".map", S8Arg(OutputName)));
        Str8ArrayAppendTo(ExtraFlags, ExtraSource);
        WindowsBuild(Source, ExtraFlags, ExtraLinkerFlags);
    }
}

internal void 
LogBuildMode(str8 Name, b32 Debug)
{
    Log("[" S8Fmt " (" S8Fmt ")]\n", 
        S8Arg(Name), S8Arg(Debug ? S8("debug") : S8("release")));
}


ENTRY_POINT(EntryPoint)
{
    if(LaneIndex() == 0)
    {
        Cng_InitAndRebuildSelf(Params->ArgsCount, Params->Args, Params->Env);
        
        {        
            str8 Path = Cng_PathFromExe(Params->Args[0], S8(CLING_BUILD_PATH));
            OS_ChangeDirectory((char *)Path.Data);
        }
        
        // Targets
        b32 Editor = false;
        b32 HaversineProcessor = false;
        b32 HaversineGenerator = false;
        b32 Sim86 = false;
        b32 Muze = true;
        
#if OS_WINDOWS
        OSWindows = true;
#elif OS_LINUX
        OSLinux = true;
#endif
        
        //- Targets 
        b32 Asan = false;
        b32 Debug = true;
        //- TODO(luca): 
        b32 Clean = false;
        b32 Clang = true;
        b32 GCC = false;
        b32 Slow = false;
        b32 Wine = false;
        //-
        
        //~ Computer enhance
        
        if(HaversineGenerator)
        {
            LogBuildMode(S8("Haversine generator"), Debug);
            
            // Haversine generator metaprogram
            {
                Log("Generating code...\n");
                
                GlobalMDArena = MD_ArenaAlloc();
                MD_String8 FileName = MD_S8Lit("../code/computerenhance/haversine_generator/haversine.mdesk");
                MD_ParseResult Parse = MD_ParseWholeFile(GlobalMDArena, FileName);
                
                // print metadesk errors
                for(MD_Message *Message = Parse.errors.first;
                    Message != 0;
                    Message = Message->next)
                {
                    MD_CodeLoc code_loc = MD_CodeLocFromNode(Message->node);
                    MD_PrintMessage(stdout, code_loc, Message->kind, Message->string);
                }
                
                if(Parse.errors.max_message_kind < MD_MessageKind_Error)
                {
                    MD_Node *Root = Parse.node->first_child;
                    
                    MD_String8List Stream = {0};
                    for(MD_EachNode(Node, Root))
                    {
                        if(MD_NodeHasTag(Node, MD_S8Lit("table_gen_flags"), 0))
                        {
                            MD_String8 TableName = MD_NodeAtIndex(Node->first_tag->first_child, 0)->string;
                            MD_String8 MemberName = MD_NodeAtIndex(Node->first_tag->first_child, 1)->string;
                            
                            MD_Node *TableNode = MD_FirstNodeWithString(Root, TableName, 0);
                            MD_Node *TableTag = MD_TagFromString(TableNode, MD_S8Lit("table"), 0);
                            
                            MD_Node *MemberNode = MD_FirstNodeWithString(TableTag->first_child, MemberName, 0);
                            int MemberIndex = MD_IndexFromNode(MemberNode);
                            
                            DeferLoop(MD_S8ListPushFmt(GlobalMDArena, &Stream, "enum %S\n{\n", Node->string),
                                      MD_S8ListPushFmt(GlobalMDArena, &Stream, "%S", MD_S8Lit("};\n\n")))
                            {                            
                                int MemberCount = 0;
                                for(MD_EachNode(Member, TableNode->first_child))
                                {
                                    MD_Node *MemberNode = MD_NodeAtIndex(Member->first_child, MemberIndex);
                                    
                                    MD_S8ListPushFmt(GlobalMDArena, &Stream, " Flag_%S = (1 << %d),\n", 
                                                     MemberNode->string, MemberCount);
                                    MemberCount++;
                                }
                            }
                            
                            
                        }
                        
                        if(MD_NodeHasTag(Node, MD_S8Lit("table_gen_data"), 0))
                        {
                            MD_String8 TableName = MD_NodeAtIndex(Node->first_tag->first_child, 0)->string;
                            MD_String8 Type = MD_NodeAtIndex(Node->first_tag->first_child, 1)->string;
                            MD_String8 MemberName = MD_NodeAtIndex(Node->first_tag->first_child, 2)->string;
                            
                            MD_Node *Table = MD_FirstNodeWithString(Root, TableName, 0);
                            
                            MD_Node *TableTag = MD_TagFromString(Table, MD_S8Lit("table"), 0);
                            MD_Node *MemberNode = MD_FirstNodeWithString(TableTag->first_child, MemberName, 0);
                            int MemberIndex = MD_IndexFromNode(MemberNode);
                            
                            MD_u64 MemberCount = MD_ChildCountFromNode(Table);
                            
                            MD_S8ListPushFmt(GlobalMDArena, &Stream, "int %SCount = %d;\n", Node->string, MemberCount);
                            
                            DeferLoop(MD_S8ListPushFmt(GlobalMDArena, &Stream, "%S%S[] =\n{\n", Type, Node->string),
                                      MD_S8ListPushFmt(GlobalMDArena, &Stream, "%S", MD_S8Lit("};\n\n")))
                            {                            
                                for(MD_EachNode(Member, Table->first_child))
                                {
                                    MD_Node *ValueNode  = MD_NodeAtIndex(Member->first_child, MemberIndex);
                                    MD_S8ListPushFmt(GlobalMDArena, &Stream, " %S,\n", ValueNode->raw_string);
                                }
                            }
                            
                        }
                        
                        if(MD_NodeHasTag(Node, MD_S8Lit("table_gen_enum"), 0))
                        {
                            MD_String8 TableName = MD_NodeAtIndex(Node->first_tag->first_child, 0)->string;
                            MD_String8 Prefix = MD_NodeAtIndex(Node->first_tag->first_child, 1)->string;
                            MD_String8 MemberName = MD_NodeAtIndex(Node->first_tag->first_child, 2)->string;
                            
                            MD_Node *Table = MD_FirstNodeWithString(Root, TableName, 0);
                            
                            MD_Node *TableTag = MD_TagFromString(Table, MD_S8Lit("table"), 0);
                            MD_Node *MemberNode = MD_FirstNodeWithString(TableTag->first_child, MemberName, 0);
                            int MemberIndex = MD_IndexFromNode(MemberNode);
                            
                            
                            DeferLoop(MD_S8ListPushFmt(GlobalMDArena, &Stream, "enum %S\n{\n", Node->raw_string),
                                      MD_S8ListPushFmt(GlobalMDArena, &Stream, "};\n"))
                            {                                      
                                for(MD_EachNode(Member, Table->first_child))
                                {
                                    MD_Node *ValueNode  = MD_NodeAtIndex(Member->first_child, MemberIndex);
                                    if(Member != Table->first_child)
                                    {
                                        MD_S8ListPushFmt(GlobalMDArena, &Stream, " %S%S,\n", Prefix, ValueNode->raw_string);
                                    }
                                    else
                                    {
                                        MD_S8ListPushFmt(GlobalMDArena, &Stream, " %S%S = 0,\n", Prefix, ValueNode->raw_string);
                                    }
                                }
                            }
                            
                            
                        }
                    }
                    
                    WriteStreamToFile(Stream, "../code/computerenhance/haversine_generator/generated/types.h");
                }
            }
            
            // Haversine generator build
            {
                str8_array *ExtraFlags = PushStr8Array(256);
                Str8ArrayAppendTo(ExtraFlags, S8("-I" CLING_CODE_PATH));
                
                LinuxBuildCommand(S8("../code/computerenhance/haversine_generator/haversine_generator.c"), 
                                      S8("haversine_generator"),
                                      GCC, Clang, Asan, Debug,
                                      ExtraFlags,
                                      false);
                
                RunCommand();
                
            }
        }
        
        if(HaversineProcessor)
        {
            LogBuildMode(S8("Haversine processor"), Debug);
            
            if(OSLinux)
            {            
                str8_array *ExtraFlags = PushStr8Array(256);
                Str8ArrayAppendTo(ExtraFlags, S8("-I" CLING_CODE_PATH
                                                 " -std=c++11"));
                
                LinuxBuildCommand(S8("../code/computerenhance/haversine_processor/haversine_processor.c"), 
                                      S8("haversine_processor"),
                                      GCC, Clang, Asan, Debug,
                                      ExtraFlags,
                                      false);
                
                RunCommand();
            }
        }
        
        if(Sim86)
        {
            LogBuildMode(S8("sim86"), Debug);
            
            if(OSLinux)
            {            
                str8_array *ExtraFlags = PushStr8Array(256);
                Str8ArrayAppendTo(ExtraFlags, S8("-I" CLING_CODE_PATH));
                
                LinuxBuildCommand(S8("../code/computerenhance/sim86/sim86.cpp"), 
                                      S8("sim86"),
                                      GCC, Clang, Asan, Debug,
                                      ExtraFlags,
                                      false);
                
                RunCommand();
            }
        }
        
        if(Editor)
        {            
            LogBuildMode(S8("editor"), Debug);
            
            // Metaprogram
            {
                Log("Generating code...\n");
                
                GlobalMDArena = MD_ArenaAlloc();
                MD_String8 FileName = MD_S8Lit("../code/editor/tables.mdesk");
                MD_ParseResult Parse = MD_ParseWholeFile(GlobalMDArena, FileName);
                
                // Print metadesk errors
                for(MD_Message *Message = Parse.errors.first;
                    Message != 0;
                    Message = Message->next)
                {
                    MD_CodeLoc code_loc = MD_CodeLocFromNode(Message->node);
                    MD_PrintMessage(stdout, code_loc, Message->kind, Message->string);
                }
                if(Parse.errors.max_message_kind < MD_MessageKind_Error)
                {
                    MD_Node *Root = Parse.node->first_child;
                    
                    MD_String8List VSStream = {0};
                    MD_String8List PSStream = {0};
                    MD_String8List CStream = {0};
                    
                    // Colors
                    {
                        MD_Node *ColorsTable = MD_FirstNodeWithString(Root, MD_S8Lit("Colors"), 0);
                        
                        DeferLoop(MD_S8ListPush(GlobalMDArena, &CStream, MD_S8Lit("//- Colors begin\n")),
                                  MD_S8ListPush(GlobalMDArena, &CStream, MD_S8Lit("//- Colors end\n")))
                        {                            
                            for(MD_EachNode(Node, ColorsTable->first_child))
                            {
                                MD_Node *ColorName = MD_NodeAtIndex(Node->first_child, 0);
                                MD_Node *ColorValue = MD_NodeAtIndex(Node->first_child, 1);
                                MD_S8ListPushFmt(GlobalMDArena, &CStream, "const u32 ColorU32_%S = %S;\n", ColorName->string, ColorValue->string);
                            }
                            
                            for(MD_EachNode(Node, ColorsTable->first_child))
                            {
                                MD_Node *ColorName = MD_NodeAtIndex(Node->first_child, 0);
                                MD_Node *ColorValue = MD_NodeAtIndex(Node->first_child, 1);
                                MD_S8ListPushFmt(GlobalMDArena, &CStream, "v4 Color_%S = {U32ToV4Arg(%S)};\n", ColorName->string, ColorValue->string);
                                
                            }
                        }
                    }
                    
                    // Rect shader attributes
                    {                
                        MD_Node *Table = MD_FirstNodeWithString(Root, MD_S8Lit("RectVSAttributes"), 0);
                        
                        MD_Node *ShaderPreTable = MD_FirstNodeWithString(Root, MD_S8Lit("RectVSPreCode"), 0);
                        MD_Node *ShaderPostTable = MD_FirstNodeWithString(Root, MD_S8Lit("RectVSPostCode"), 0);
                        MD_S8ListPush(GlobalMDArena, &VSStream, ShaderPreTable->first_child->string);
                        
                        DeferLoop(MD_S8ListPushFmt(GlobalMDArena, &VSStream, "\n//- Generated code start\n"),
                                  MD_S8ListPushFmt(GlobalMDArena, &VSStream, "\n//- Generated code end\n"))
                        {                        
                            MD_Node *RectPSCodeTable = MD_FirstNodeWithString(Root, MD_S8Lit("RectPSCode"), 0);
                            MD_S8ListPush(GlobalMDArena, &PSStream, RectPSCodeTable->first_child->string);
                            
                            DeferLoop(MD_S8ListPushFmt(GlobalMDArena, &CStream,
                                                       "s32 RectVSAttribOffsets[] =\n{\n"),
                                      MD_S8ListPushFmt(GlobalMDArena, &CStream, "};\n"))
                            {                        
                                s32 Index = 0;
                                for(MD_EachNode(Node, Table->first_child))
                                {
                                    MD_Node *Name = MD_NodeAtIndex(Node->first_child, 0);
                                    MD_Node *Size = MD_NodeAtIndex(Node->first_child, 1);
                                    
                                    MD_String8 TypeName = MD_S8Lit("null");
                                    
                                    if(0) {}
                                    else if(MD_S8Match(Size->string, MD_S8Lit("1"), 0)) TypeName = MD_S8Lit("f32");
                                    else if(MD_S8Match(Size->string, MD_S8Lit("4"), 0)) TypeName = MD_S8Lit("v4");
                                    else InvalidPath();
                                    
                                    MD_S8ListPushFmt(GlobalMDArena, &VSStream, 
                                                     "layout (location = %2d) in %3S %S;\n", 
                                                     Index, TypeName, Name->string);
                                    MD_S8ListPushFmt(GlobalMDArena, &CStream,
                                                     "%S,\n", Size->string);
                                    
                                    Index += 1;
                                }
                            }
                        }
                        
                        MD_S8ListPush(GlobalMDArena, &VSStream, ShaderPostTable->first_child->string);
                    }
                    
                    // UI Box flags
                    {
                        MD_Node *Table = MD_FirstNodeWithString(Root, MD_S8Lit("UI_BoxFlags"), 0);
                        
                        DeferLoop(MD_S8ListPushFmt(GlobalMDArena, &CStream, 
                                                   "enum ui_box_flag\n{\n"),
                                  MD_S8ListPushFmt(GlobalMDArena, &CStream, 
                                                   "};\n"
                                                   "typedef enum ui_box_flag ui_box_flag;\n"))
                        {                        
                            u64 MaxWidth = 0;
                            for(MD_EachNode(Node, Table->first_child))
                            {
                                MaxWidth = Max(Node->string.size, MaxWidth);
                            }
                            
                            s32 Index = 0;
                            s32 Value = 0;
                            
                            for(MD_EachNode(Node, Table->first_child))
                            {
                                if(Index > 0) Value = 1;
                                MD_S8ListPushFmt(GlobalMDArena, &CStream, 
                                                 "UI_BoxFlag_%-*S = (%d << %d),\n",
                                                 MaxWidth, Node->string, Value, Index);
                                
                                Index += 1;
                            }
                        }
                    }
                    
                    WriteStreamToFile(CStream, "../code/editor/generated/everything.c");
                    WriteStreamToFile(VSStream, "../code/editor/generated/rect_vert.glsl");
                    WriteStreamToFile(PSStream, "../code/editor/generated/rect_frag.glsl");
                }
            }
            
            // Editor build
            {
                str8_array *CommonMuzeFlags = PushStr8Array(256);
                Str8ArrayAppendTo(CommonMuzeFlags, S8("-I" CLING_CODE_PATH));
                
                if(OS_FileExists(CLING_CODE_PATH "base/base_build.h"))
                {
                    Str8ArrayAppendTo(CommonMuzeFlags, S8("-DBASE_PERSONAL=1"));
                }
                
                char *LibsFileName = (OSWindows ? 
                                      CLING_BUILD_PATH "editor_libs.obj" :
                                      (OSLinux ?
                                       CLING_BUILD_PATH "editor_libs.o" :
                                       0));
                
                if(!OS_FileExists(LibsFileName))
                {
                    str8 ExtraLinkerFlags = {0};
                    Str8ArrayPushCount(CommonMuzeFlags)
                    {
                        Str8ArrayAppendTo(CommonMuzeFlags, S8("-DEDITOR_SLOW_COMPILE=1"));
                        
                        MuzeBuildCommand(S8("../code/editor/editor_libs.h"), 
                                         S8("editor_libs"), 
                                         GCC, Clang, Asan, Debug,
                                         CommonMuzeFlags, ExtraLinkerFlags,
                                         S8(""),
                                         false, true);
                    }
                }
                
                Str8ArrayPushCount(CommonMuzeFlags)
                {
                    str8 ExtraLinkerFlags  = {0};
                    
                    if(OSWindows)
                    {
                        
                        ExtraLinkerFlags = S8("/EXPORT:UpdateAndRender");
                    }
                    
                    Str8ArrayAppendTo(CommonMuzeFlags, S8("-DEDITOR_SLOW_COMPILE=0"));
                    
                    MuzeBuildCommand(S8("../code/editor/editor_app.c"), 
                                     S8("editor_app"),
                                     GCC, Clang, Asan, Debug,
                                     CommonMuzeFlags, ExtraLinkerFlags,
                                     S8FromCString(LibsFileName),
                                     true, false);
                }
                
                Str8ArrayPushCount(CommonMuzeFlags)
                {
                    str8 ExtraLinkerFlags = {0};
                    
                    Str8ArrayAppendTo(CommonMuzeFlags, S8("-DEDITOR_SLOW_COMPILE=0"));
                    
                    if(0) {}
                    else if(OSLinux)
                    {
                        ExtraLinkerFlags = S8("-lasound -lX11 -lGL -lGLX");
                    }
                    else if(OSWindows)
                    {
                        ExtraLinkerFlags = S8("user32.lib Gdi32.lib winmm.lib Opengl32.lib");
                    }
                    
                    MuzeBuildCommand(S8("../code/editor/editor_platform.c"),
                                     S8("editor"),
                                     GCC, Clang, Asan, Debug,
                                     CommonMuzeFlags, ExtraLinkerFlags,
                                     S8FromCString(LibsFileName), 
                                     false, false);
                }
            }
        }
        
        if(Muze)
        {
            LogBuildMode(S8("muze"), Debug);
            
            // Metaprogram
            {
                Log("Generating code...\n");
                
                GlobalMDArena = MD_ArenaAlloc();
                MD_String8 FileName = MD_S8Lit("../code/muze/tables.mdesk");
                MD_ParseResult Parse = MD_ParseWholeFile(GlobalMDArena, FileName);
                
                // Print metadesk errors
                for(MD_Message *Message = Parse.errors.first;
                    Message != 0;
                    Message = Message->next)
                {
                    MD_CodeLoc code_loc = MD_CodeLocFromNode(Message->node);
                    MD_PrintMessage(stdout, code_loc, Message->kind, Message->string);
                }
                if(Parse.errors.max_message_kind < MD_MessageKind_Error)
                {
                    MD_Node *Root = Parse.node->first_child;
                    
                    MD_String8List VSStream = {0};
                    MD_String8List PSStream = {0};
                    MD_String8List CStream = {0};
                    
                    // Colors
                    {
                        MD_Node *ColorsTable = MD_FirstNodeWithString(Root, MD_S8Lit("Colors"), 0);
                        
                        DeferLoop(MD_S8ListPush(GlobalMDArena, &CStream, MD_S8Lit("//- Colors begin\n")),
                                  MD_S8ListPush(GlobalMDArena, &CStream, MD_S8Lit("//- Colors end\n")))
                        {                            
                            for(MD_EachNode(Node, ColorsTable->first_child))
                            {
                                MD_Node *ColorName = MD_NodeAtIndex(Node->first_child, 0);
                                MD_Node *ColorValue = MD_NodeAtIndex(Node->first_child, 1);
                                MD_S8ListPushFmt(GlobalMDArena, &CStream, "const u32 ColorU32_%S = %S;\n", ColorName->string, ColorValue->string);
                            }
                            
                            for(MD_EachNode(Node, ColorsTable->first_child))
                            {
                                MD_Node *ColorName = MD_NodeAtIndex(Node->first_child, 0);
                                MD_Node *ColorValue = MD_NodeAtIndex(Node->first_child, 1);
                                MD_S8ListPushFmt(GlobalMDArena, &CStream, "v4 Color_%S = {U32ToV4Arg(%S)};\n", ColorName->string, ColorValue->string);
                                
                            }
                        }
                    }
                    
                    // Rect shader attributes
                    {                
                        MD_Node *Table = MD_FirstNodeWithString(Root, MD_S8Lit("RectVSAttributes"), 0);
                        
                        MD_Node *ShaderPreTable = MD_FirstNodeWithString(Root, MD_S8Lit("RectVSPreCode"), 0);
                        MD_Node *ShaderPostTable = MD_FirstNodeWithString(Root, MD_S8Lit("RectVSPostCode"), 0);
                        MD_S8ListPush(GlobalMDArena, &VSStream, ShaderPreTable->first_child->string);
                        
                        DeferLoop(MD_S8ListPushFmt(GlobalMDArena, &VSStream, "\n//- Generated code start\n"),
                                  MD_S8ListPushFmt(GlobalMDArena, &VSStream, "\n//- Generated code end\n"))
                        {                        
                            MD_Node *RectPSCodeTable = MD_FirstNodeWithString(Root, MD_S8Lit("RectPSCode"), 0);
                            MD_S8ListPush(GlobalMDArena, &PSStream, RectPSCodeTable->first_child->string);
                            
                            DeferLoop(MD_S8ListPushFmt(GlobalMDArena, &CStream,
                                                       "s32 RectVSAttribOffsets[] =\n{\n"),
                                      MD_S8ListPushFmt(GlobalMDArena, &CStream, "};\n"))
                            {                        
                                s32 Index = 0;
                                for(MD_EachNode(Node, Table->first_child))
                                {
                                    MD_Node *Name = MD_NodeAtIndex(Node->first_child, 0);
                                    MD_Node *Size = MD_NodeAtIndex(Node->first_child, 1);
                                    
                                    MD_String8 TypeName = MD_S8Lit("null");
                                    
                                    if(0) {}
                                    else if(MD_S8Match(Size->string, MD_S8Lit("1"), 0)) TypeName = MD_S8Lit("f32");
                                    else if(MD_S8Match(Size->string, MD_S8Lit("4"), 0)) TypeName = MD_S8Lit("v4");
                                    else InvalidPath();
                                    
                                    MD_S8ListPushFmt(GlobalMDArena, &VSStream, 
                                                     "layout (location = %2d) in %3S %S;\n", 
                                                     Index, TypeName, Name->string);
                                    MD_S8ListPushFmt(GlobalMDArena, &CStream,
                                                     "%S,\n", Size->string);
                                    
                                    Index += 1;
                                }
                            }
                        }
                        
                        MD_S8ListPush(GlobalMDArena, &VSStream, ShaderPostTable->first_child->string);
                    }
                    
                    // UI Box flags
                    {
                        MD_Node *Table = MD_FirstNodeWithString(Root, MD_S8Lit("UI_BoxFlags"), 0);
                        
                        DeferLoop(MD_S8ListPushFmt(GlobalMDArena, &CStream, 
                                                   "enum ui_box_flag\n{\n"),
                                  MD_S8ListPushFmt(GlobalMDArena, &CStream, 
                                                   "};\n"
                                                   "typedef enum ui_box_flag ui_box_flag;\n"))
                        {                        
                            u64 MaxWidth = 0;
                            for(MD_EachNode(Node, Table->first_child))
                            {
                                MaxWidth = Max(Node->string.size, MaxWidth);
                            }
                            
                            s32 Index = 0;
                            s32 Value = 0;
                            
                            for(MD_EachNode(Node, Table->first_child))
                            {
                                if(Index > 0) Value = 1;
                                MD_S8ListPushFmt(GlobalMDArena, &CStream, 
                                                 "UI_BoxFlag_%-*S = (%d << %d),\n",
                                                 MaxWidth, Node->string, Value, Index);
                                
                                Index += 1;
                            }
                        }
                    }
                    
                    WriteStreamToFile(CStream, "../code/muze/generated/everything.c");
                    WriteStreamToFile(VSStream, "../code/muze/generated/rect_vert.glsl");
                    WriteStreamToFile(PSStream, "../code/muze/generated/rect_frag.glsl");
                }
            }
            
            // Compile
            {            
                str8_array *CommonMuzeFlags = PushStr8Array(256);
                Str8ArrayAppendTo(CommonMuzeFlags, S8("-I" CLING_CODE_PATH));
                
                // Alsa sample
                if(1)
                {                
                MuzeBuildCommand(S8("../code/muze/lnx_alsa.c"),
                                 S8("lnx_alsa"),
                                 GCC, Clang, Asan, Debug,
                                 CommonMuzeFlags, S8("-lasound"), S8(""), false, false);
                }

                if(OS_FileExists(CLING_CODE_PATH "base/base_build.h"))
                {
                    Str8ArrayAppendTo(CommonMuzeFlags, S8("-DBASE_PERSONAL=1"));
                }
                
                char *LibsFileName = (OSWindows ? 
                                      CLING_BUILD_PATH "muze_libs.obj" :
                                      (OSLinux ?
                                       CLING_BUILD_PATH "muze_libs.o" :
                                       0));
                
                if(!OS_FileExists(LibsFileName))
                {
                    Str8ArrayPushCount(CommonMuzeFlags)
                    {
                        Str8ArrayAppendTo(CommonMuzeFlags, S8("-DMUZE_SLOW_COMPILE=1"));
                        
                        MuzeBuildCommand(S8("../code/muze/muze_libs.h"), 
                                         S8("muze_libs"), 
                                         GCC, Clang, Asan, Debug,
                                         CommonMuzeFlags, S8(""), S8(""),
                                         false, true);
                    }
                }
                
                Str8ArrayPushCount(CommonMuzeFlags)
                {
                    str8 ExtraLinkerFlags  = {0};
                    
                    if(OSWindows)
                    {
                        ExtraLinkerFlags = S8("/EXPORT:UpdateAndRender winmm.lib");
                    }
                    
                    Str8ArrayAppendTo(CommonMuzeFlags, S8("-DMUZE_SLOW_COMPILE=0"));
                    
                    MuzeBuildCommand(S8("../code/muze/muze_app.c"), 
                                     S8("muze_app"),
                                     GCC, Clang, Asan, Debug,
                                     CommonMuzeFlags, ExtraLinkerFlags, S8FromCString(LibsFileName),
                                     true, false);
                }
                
                Str8ArrayPushCount(CommonMuzeFlags)
                {
                    str8 ExtraLinkerFlags = {0};
                    
                    Str8ArrayAppendTo(CommonMuzeFlags, S8("-DMUZE_SLOW_COMPILE=0"));
                    
                    if(0) {}
                    else if(OSLinux)
                    {
                        ExtraLinkerFlags = S8("-lasound -lX11 -lGL -lGLX");
                    }
                    else if(OSWindows)
                    {
                        ExtraLinkerFlags = S8("user32.lib Gdi32.lib winmm.lib Opengl32.lib");
                    }
                    
                    MuzeBuildCommand(S8("../code/muze/muze_platform.c"),
                                     S8("muze"),
                                     GCC, Clang, Asan, Debug,
                                     CommonMuzeFlags, ExtraLinkerFlags, S8FromCString(LibsFileName), 
                                     false, false);
                }
                
            }
        }
        
        Log("Done.\n");
    }
    
    return 0;
}
