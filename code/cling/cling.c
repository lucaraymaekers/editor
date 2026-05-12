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

raddbg_entry_point(EntryPoint);

//~ Globals
global_variable MD_Arena *GlobalMDArena = 0;

//~ Functions
internal void
RunCommand(void)
{
 str8 BuildCommand = Cng_Str8ArrayJoin(' ');
 if(0)
 {
  Log(S8Fmt "\n", S8Arg(BuildCommand));
 }
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
WindowsBuild(str8 Source, str8_array *ExtraCompilerFlags, str8 ExtraLinkerFlags, 
             b32 Debug, b32 Asan)
{
 str8_array *Command = Cng_PushStr8Array(256);
 Cng_SetSelectedArray(Command);
 
 Cng_Str8ArrayAppend(S8("cmd /c \"C:\\msvc\\setup_x64.bat"));
 Cng_Str8ArrayAppend(S8("&&"));
 
 Cng_Str8ArrayAppend(S8("cl"));
 Cng_Str8ArrayAppend(S8("-MTd -Gm- -nologo -GR- -EHa- -Oi -FC -Z7"));
 
 if(Asan)
 {
  Cng_Str8ArrayAppend(S8("/fsanitize=address"));
 }
 
 if(!Debug)
 {
  Cng_Str8ArrayAppend(S8("-O2"));
 }
 
 Cng_Str8ArrayAppend(S8("-Zc:strictStrings-"));
 Cng_Str8ArrayAppend(S8("-WX -W4 -wd4459 -wd4201 -wd4100 -wd4101 -wd4189 -wd4505 -wd4996 -wd4389 -wd4244 -wd5287 -wd4063 -wd4127"));
 
 Cng_Str8ArrayAppend(Cng_Str8ArrayJoinFrom(ExtraCompilerFlags, ' '));
 Cng_Str8ArrayAppend(Source);
 Cng_Str8ArrayAppend(ExtraLinkerFlags);
 
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
 
 str8_array *Command = Cng_PushStr8Array(256);
 Cng_SetSelectedArray(Command);
 
 // NOTE(luca): These are almost all c++ flags.
 str8 CommonCompilerFlags = S8("-fno-threadsafe-statics -nostdinc++ -D_GNU_SOURCE=1 -fno-exceptions -fno-rtti");
 // TODO(luca): nasr should fix his enums, so we can enable -Wswitch again.
 str8 CommonWarningFlags = S8("-Wall -Wextra -Wconversion -Wswitch -Wshadow " 
                              "-Wno-double-promotion -Wno-unused-but-set-variable -Wno-write-strings -Wno-pointer-arith "
                              "-Wno-missing-field-initializers "
                              "-Wno-initializer-overrides "
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
 
 Cng_Str8ArrayAppendMultiple(Compiler,
                             ModeFlags,
                             CommonCompilerFlags,
                             CommonWarningFlags);
 if(Clang)
 {
  Cng_Str8ArrayAppendMultiple(ClangCompilerFlags, ClangWarningFlags);
 }
 if(GCC)
 {
  Cng_Str8ArrayAppend(GCCWarningFlags);
 }
 if(Asan)
 {
  Cng_Str8ArrayAppend(AsanFlags);
 }
 
 Cng_Str8ArrayAppendMultiple(Cng_Str8ArrayJoinFrom(ExtraFlags, ' '),
                             S8("-o"), OutputName,
                             LinkerFlags,
                             Source);
 
 if(Run)
 {
  RunCommand();
 }
}

internal void
BuildAndRun(str8 Source, 
            str8 OutputName, 
            b32 GCC, b32 Clang, b32 Asan, b32 Debug,
            str8_array *ExtraFlags,
            str8 ExtraLinkerFlags,
            str8 ExtraSource,
            b32 IsDLL, b32 IsObject)
{
 if(0) {}
 else if(Cng_IsOs(CngOS_Linux))
 {
  if(0) {}
  else if(IsObject)
  {
   Cng_Str8ArrayAppendTo(ExtraFlags, S8("-fPIC -x c++ -c"));
   OutputName = Str8Fmt(S8Fmt ".o", S8Arg(OutputName));
  }
  else if(IsDLL)
  {
   Cng_Str8ArrayAppendTo(ExtraFlags, S8("-fPIC --shared"));
   OutputName = Str8Fmt(S8Fmt ".so", S8Arg(OutputName));
  }
  
  Cng_Str8ArrayAppendTo(ExtraFlags, ExtraLinkerFlags);
  LinuxBuildCommand(Source, OutputName, GCC, Clang, Asan, Debug, ExtraFlags, false);
  Cng_Str8ArrayAppend(ExtraSource);
  RunCommand();
 }
 else if(Cng_IsOs(CngOS_Windows))
 {
  if(0) {}
  else if(IsObject)
  {
   Cng_Str8ArrayAppendTo(ExtraFlags, S8("-c /TP"));
   Cng_Str8ArrayAppendTo(ExtraFlags, Str8Fmt("-Fo" S8Fmt ".obj", S8Arg(OutputName)));
  }
  else if(IsDLL)
  {
   Cng_Str8ArrayAppendTo(ExtraFlags, S8("/LD"));
   ExtraLinkerFlags = Str8Fmt(S8Fmt " /DLL /OUT:" S8Fmt ".dll",
                              S8Arg(ExtraLinkerFlags), S8Arg(OutputName));
  }
  else
  {
   Cng_Str8ArrayAppendTo(ExtraFlags, Str8Fmt("-Fe" S8Fmt ".exe", S8Arg(OutputName)));
  }
  
  ExtraLinkerFlags = Str8Fmt("/link -opt:ref -incremental:no " S8Fmt, S8Arg(ExtraLinkerFlags));
  
  Cng_Str8ArrayAppendTo(ExtraFlags, Str8Fmt("-Fm" S8Fmt ".map", S8Arg(OutputName)));
  Cng_Str8ArrayAppendTo(ExtraFlags, ExtraSource);
  WindowsBuild(Source, ExtraFlags, ExtraLinkerFlags, Debug, Asan);
 }
}

//- Metadesk library helpers 
global_variable MD_String8List *GlobalMDStringList;

internal void
S8ListPush(MD_String8 String)
{
 MD_S8ListPush(GlobalMDArena, GlobalMDStringList, String);
}

internal void
S8ListPushFmt(char *Format, ...)
{
 va_list Args;
 va_start(Args, Format);
 MD_String8 String= MD_S8FmtV(GlobalMDArena, Format, Args);
 va_end(Args);
 S8ListPush(String);
}

//- Custom build params

internal void 
LogBuildMode(str8 Name, b32 Debug)
{
 Log("[" S8Fmt " (" S8Fmt ")]\n", 
     S8Arg(Name), S8Arg(Debug ? S8("debug") : S8("release")));
}


internal void
ApplicationBuild(str8 ExeName, str8 AppName, str8 AppSource, str8 WindowName,
                 b32 GCC, b32 Clang, b32 Asan, b32 Debug, b32 Slow)
{
 LogBuildMode(ExeName,  Debug);
 
 // Metaprogram
 {
  Log("Generating code...\n");
  
  MD_String8 FileName = MD_S8Lit("../code/rl/rl_tables.mdesk");
  MD_ParseResult Parse = MD_ParseWholeFile(GlobalMDArena, FileName);
  
  // Print metadesk errors
  for(MD_Message *Message = Parse.errors.first;
      Message != 0;
      Message = Message->next)
  {
   MD_CodeLoc code_loc = MD_CodeLocFromNode(Message->node);
   MD_PrintMessage(stderr, code_loc, Message->kind, Message->string);
  }
  if(Parse.errors.max_message_kind < MD_MessageKind_Error)
  {
   MD_Node *Root = Parse.node->first_child;
   
   MD_String8List VSStream = {0};
   MD_String8List PSStream = {0};
   MD_String8List CStream = {0};
   
   // Enums and string tables
   {
    GlobalMDStringList = &CStream;
    
    for(MD_EachNode(Node, Root))
    {
     MD_Node *Tag;
     
     Tag = MD_TagFromString(Node, MD_S8Lit("table_gen_enum"), 0); 
     if(!MD_NodeIsNil(Tag))
     {
      MD_String8 TypeName = MD_NodeAtIndex(Tag->first_child, 0)->string;
      MD_String8 Prefix = MD_NodeAtIndex(Tag->first_child, 1)->string;
      
      DeferLoop(S8ListPushFmt("typedef enum %S %S;\n"
                              "enum %S\n"
                              "{\n", TypeName, TypeName, TypeName), 
                S8ListPushFmt("};\n"))
      {
       for(MD_EachNode(Child, Node->first_child))
       {
        S8ListPushFmt(" %S_%S,\n", Prefix, Child->string);
       }
       S8ListPushFmt(" %S_Count,\n", Prefix);
      }
     }
     
     Tag = MD_TagFromString(Node, MD_S8Lit("table_gen_strings"), 0); 
     if(!MD_NodeIsNil(Tag))
     {
      MD_String8 Name = MD_NodeAtIndex(Tag->first_child, 0)->string;
      
      DeferLoop(S8ListPushFmt("global_variable str8 %S[] =\n" 
                              "{\n", Name), 
                S8ListPushFmt("};\n"))
      {
       for(MD_EachNode(Child, Node->first_child))
       {
        S8ListPushFmt("{(u8 *)\"%S\", %llu},\n", Child->string, Child->string.size);
       }
      }
     }
     
     
    }
   }
   
   // Colors
   {
    MD_Node *ColorsTable = MD_FirstNodeWithString(Root, MD_S8Lit("Colors"), 0);
    
    DeferLoop(S8ListPush(MD_S8Lit("//- Colors begin\n")),
              S8ListPush(MD_S8Lit("//- Colors end\n")))
    {                            
     for(MD_EachNode(Node, ColorsTable->first_child))
     {
      MD_Node *ColorName = MD_NodeAtIndex(Node->first_child, 0);
      MD_Node *ColorValue = MD_NodeAtIndex(Node->first_child, 1);
      S8ListPushFmt("const u32 ColorU32_%S = %S;\n", ColorName->string, ColorValue->string);
     }
     
     for(MD_EachNode(Node, ColorsTable->first_child))
     {
      MD_Node *ColorName = MD_NodeAtIndex(Node->first_child, 0);
      MD_Node *ColorValue = MD_NodeAtIndex(Node->first_child, 1);
      S8ListPushFmt("v4 Color_%S = {U32ToV4Arg(%S)};\n", ColorName->string, ColorValue->string);
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
     
     DeferLoop(S8ListPushFmt("s32 RectVSAttribOffsets[] =\n{\n"),
               S8ListPushFmt("};\n"))
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
       S8ListPushFmt("%S,\n", Size->string);
       
       Index += 1;
      }
     }
    }
    
    MD_S8ListPush(GlobalMDArena, &VSStream, ShaderPostTable->first_child->string);
   }
   
   // UI Box flags
   {
    MD_Node *Table = MD_FirstNodeWithString(Root, MD_S8Lit("UI_BoxFlags"), 0);
    
    DeferLoop(S8ListPushFmt("enum ui_box_flag\n{\n"),
              S8ListPushFmt("};\n"
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
      S8ListPushFmt("UI_BoxFlag_%-*S = (%d << %d),\n",
                    MaxWidth, Node->string, Value, Index);
      
      Index += 1;
     }
    }
   }
   
   WriteStreamToFile(CStream, "../code/rl/generated/everything.c");
   WriteStreamToFile(VSStream, "../code/rl/generated/rect_vert.glsl");
   WriteStreamToFile(PSStream, "../code/rl/generated/rect_frag.glsl");
  }
 }
 
 // Compile
 {            
  str8_array *CommonFlags = Cng_PushStr8Array(256);
  Cng_Str8ArrayAppendTo(CommonFlags, 
                        Str8Fmt("-I" CLING_CODE_PATH " "
                                "-DRL_PLATFORM_APP_NAME=" S8Fmt " "
                                "-DRL_PLATFORM_WINDOW_NAME=" S8Fmt,
                                S8Arg(AppName), S8Arg(WindowName)));
  
  char *LibsFileName = (Cng_IsOs(CngOS_Windows) ? 
                        CLING_BUILD_PATH "rl_libs.obj" :
                        (Cng_IsOs(CngOS_Linux) ?
                         CLING_BUILD_PATH "rl_libs.o" :
                         0));
  
  if(!Slow && !OS_FileExists(LibsFileName))
  {
   Cng_Str8ArrayPushCount(CommonFlags)
   {
    Cng_Str8ArrayAppendTo(CommonFlags, S8("-DRL_LIBS_IMPLEMENTATION=1"));
    
    BuildAndRun(S8("../code/rl/rl_libs.h"), 
                S8("rl_libs"), 
                GCC, Clang, Asan, Debug,
                CommonFlags, S8(""), S8(""),
                false, true);
   }
  }
  
  str8 LibsName = Slow ?  S8("") : S8FromCString(LibsFileName);
  
  Cng_Str8ArrayPushCount(CommonFlags)
  {
   str8 ExtraLinkerFlags = {0};
   
   Cng_Str8ArrayAppendTo(CommonFlags, (Slow ? 
                                       S8("-DRL_LIBS_IMPLEMENTATION=1") :
                                       S8("-DRL_LIBS_IMPLEMENTATION=0")));
   
   BuildAndRun(AppSource, 
               AppName,
               GCC, Clang, Asan, Debug,
               CommonFlags, ExtraLinkerFlags, LibsName,
               true, false);
  }
  
  Cng_Str8ArrayPushCount(CommonFlags)
  {
   str8 ExtraLinkerFlags = {0};
   Cng_Str8ArrayAppendTo(CommonFlags, (Slow ? 
                                       S8("-DRL_LIBS_IMPLEMENTATION=1") :
                                       S8("-DRL_LIBS_IMPLEMENTATION=0")));
   
   if(0) {}
   else if(Cng_IsOs(CngOS_Linux))
   {
    ExtraLinkerFlags = S8("-lasound -lX11 -lGL -lGLX");
   }
   
   BuildAndRun(S8("../code/rl/rl_platform.c"),
               ExeName,
               GCC, Clang, Asan, Debug,
               CommonFlags, ExtraLinkerFlags, LibsName, 
               false, false);
  }
  
 }
}

internal b32
ConfigMatch(MD_String8 Name, MD_String8 Match, MD_String8 Value)
{
 b32 Result = (MD_S8Match(Name, Match, 0) && 
               MD_S8Match(Value, MD_S8Lit("1"), 0));
 return Result;
}

ENTRY_POINT(EntryPoint)
{
 if(LaneIndex() == 0)
 {
  Cng_InitAndRebuildSelf(Params->ArgsCount, Params->Args, Params->Env);
  
  // Change to build directory
  {        
   str8 Path = Cng_PathFromExe(Params->Args[0], S8(CLING_BUILD_PATH));
   OS_ChangeDirectory((char *)Path.Data);
  }
  
  // Globals
  {        
   GlobalMDArena = MD_ArenaAlloc();
  }
  
  //~ Config 
  
  //- Targets 
  b32 Editor = false;
  b32 HaversineProcessor = false;
  b32 HaversineGenerator = false;
  b32 Sim86 = false;
  b32 Muze = false;
  
  //- Build parameters 
  b32 Asan = false;
  b32 Debug = false;
  b32 Clean = false;
  b32 Clang = false;
  b32 GCC = false;
  b32 Wine = false;
  b32 Slow = false;
  
  MD_String8 FileName = MD_S8Lit("../code/cling/config.mdesk");
  MD_ParseResult Parse = MD_ParseWholeFile(GlobalMDArena, FileName);
  
  for (MD_Message *Message = Parse.errors.first; Message; Message = Message->next)
  {
   MD_CodeLoc Loc = MD_CodeLocFromNode(Message->node);
   MD_PrintMessage(stderr, Loc, Message->kind, Message->string);
  }
  
  if(Parse.errors.max_message_kind < MD_MessageKind_Error)
  {
   MD_Node *Root = Parse.node->first_child;
   for(MD_EachNode(Node, Root))
   {
    MD_String8 Name = Node->string;
    MD_String8 Value = Node->first_child->string;
    
    if(0) {}
    else if(ConfigMatch(Name, MD_S8Lit("Asan"),  Value)) Asan = 1;
    else if(ConfigMatch(Name, MD_S8Lit("Debug"), Value)) Debug = 1;
    else if(ConfigMatch(Name, MD_S8Lit("Clean"), Value)) Clean = 1;
    else if(ConfigMatch(Name, MD_S8Lit("Clang"), Value)) Clang = 1;
    else if(ConfigMatch(Name, MD_S8Lit("GCC"),   Value)) GCC = 1;
    else if(ConfigMatch(Name, MD_S8Lit("Slow"),  Value)) Slow = 1;
    else if(ConfigMatch(Name, MD_S8Lit("Wine"),  Value)) Wine = 1;
    
    else if(ConfigMatch(Name, MD_S8Lit("HaversineProcessor"), Value)) HaversineProcessor = 1;
    else if(ConfigMatch(Name, MD_S8Lit("HaversineGenerator"), Value)) HaversineGenerator = 1;
    else if(ConfigMatch(Name, MD_S8Lit("Sim86"),  Value)) Sim86 = 1;
    else if(ConfigMatch(Name, MD_S8Lit("Editor"), Value)) Editor = 1;
    else if(ConfigMatch(Name, MD_S8Lit("Muze"),   Value)) Muze = 1;
    
   }
  }
  
  Slow = Slow || Asan;
  
  //~ Haversine Generator
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
     MD_PrintMessage(stderr, code_loc, Message->kind, Message->string);
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
         MemberCount += 1;
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
    str8_array *ExtraFlags = Cng_PushStr8Array(256);
    Cng_Str8ArrayAppendTo(ExtraFlags, S8("-I" CLING_CODE_PATH));
    
    LinuxBuildCommand(S8("../code/computerenhance/haversine_generator/haversine_generator.c"), 
                      S8("haversine_generator"),
                      GCC, Clang, Asan, Debug,
                      ExtraFlags,
                      false);
    
    RunCommand();
    
   }
  }
  
  //~ Haversine Processor
  if(HaversineProcessor)
  {
   LogBuildMode(S8("Haversine processor"), Debug);
   
   if(Cng_IsOs(CngOS_Linux))
   {            
    str8_array *ExtraFlags = Cng_PushStr8Array(256);
    Cng_Str8ArrayAppendTo(ExtraFlags, S8("-I" CLING_CODE_PATH
                                         " -std=c++11"));
    
    LinuxBuildCommand(S8("../code/computerenhance/haversine_processor/haversine_processor.c"), 
                      S8("haversine_processor"),
                      GCC, Clang, Asan, Debug,
                      ExtraFlags,
                      false);
    
    RunCommand();
   }
  }
  
  //~ Sim86
  if(Sim86)
  {
   LogBuildMode(S8("sim86"), Debug);
   
   if(Cng_IsOs(CngOS_Linux))
   {            
    str8_array *ExtraFlags = Cng_PushStr8Array(256);
    Cng_Str8ArrayAppendTo(ExtraFlags, S8("-I" CLING_CODE_PATH));
    
    LinuxBuildCommand(S8("../code/computerenhance/sim86/sim86.cpp"), 
                      S8("sim86"),
                      GCC, Clang, Asan, Debug,
                      ExtraFlags,
                      false);
    
    RunCommand();
   }
  }
  
  //~ Editor
  if(Editor)
  {
   ApplicationBuild(S8("editor"), S8("editor_app"), 
                    S8("../code/editor/editor_app.c"), S8("Editor"),
                    GCC, Clang, Asan, Debug, Slow);
  }
  
  //~ Muze
  if(Muze)
  {
   ApplicationBuild(S8("muze"), S8("muze_app"), 
                    S8("../code/muze/muze_app.c"), S8("Muze"),
                    GCC, Clang, Asan, Debug, Slow);
  }
  
  Log("Done.\n");
 }
 
 return 0;
}
