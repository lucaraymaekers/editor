//~ Defines
#define CLING_BUILD_PATH "../build/"

//~ Libaries
#define BASE_FORCE_THREADS_COUNT 1
#define BASE_CONSOLE_APPLICATION 1
#include "base/base.h"
#include "base/base.c"

NO_WARNINGS_BEGIN
#include "lib/metadesk/md.h"
#include "lib/metadesk/md.c"
NO_WARNINGS_END

#define CLING_CODE_PATH "../code/"
#define CLING_SOURCE_PATH CLING_CODE_PATH "cling/cling.c"
#include "cling/cling.h"
#include "cling/generated/cling.meta.c"

//~ Globals
global_variable MD_Arena *GlobalMDArena = 0;

//~ Functions
internal void
RunCommand(void)
{
 str8 BuildCommand = Cng_Str8ArrayJoin(' ');
 if(0)
 {
  Log("%S\n", BuildCommand);
 }
 Cng_RunCommand(BuildCommand);
}

internal void
WriteStreamToFile(MD_String8List Stream, char *FileName)
{
 MD_String8 Str = MD_S8ListJoin(GlobalMDArena, Stream, 0);
 OS_WriteEntireFile(FileName, S8Cast{.Data = Str.Data, .Size = Str.Size});
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
 Log("%S\n", Cng_GetBaseFileName(Source));
 
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
 
 str8 ClangCompilerFlags = S8("-fdiagnostics-absolute-paths -ftime-trace -ferror-limit=5000");
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
   OutputName = Str8Fmt("%S.o", OutputName);
  }
  else if(IsDLL)
  {
   Cng_Str8ArrayAppendTo(ExtraFlags, S8("-fPIC --shared"));
   OutputName = Str8Fmt("%S.so", OutputName);
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
   Cng_Str8ArrayAppendTo(ExtraFlags, Str8Fmt("-Fo" "%S.obj", OutputName));
  }
  else if(IsDLL)
  {
   Cng_Str8ArrayAppendTo(ExtraFlags, S8("/LD"));
   ExtraLinkerFlags = Str8Fmt("%S /DLL /OUT:%S.dll",
                              ExtraLinkerFlags, OutputName);
  }
  else
  {
   Cng_Str8ArrayAppendTo(ExtraFlags, Str8Fmt("-Fe%S.exe", OutputName));
  }
  
  ExtraLinkerFlags = Str8Fmt("/link -opt:ref -incremental:no %S ", ExtraLinkerFlags);
  
  Cng_Str8ArrayAppendTo(ExtraFlags, Str8Fmt("-Fm" "%S.map", OutputName));
  Cng_Str8ArrayAppendTo(ExtraFlags, ExtraSource);
  WindowsBuild(Source, ExtraFlags, ExtraLinkerFlags, Debug, Asan);
 }
}

//- Metadesk library helpers 

typedef struct MD_String8ListNode MD_String8ListNode;
struct MD_String8ListNode
{
 MD_String8List V;
 MD_String8ListNode *Prev;
 str8 FileName;
};

global_variable MD_String8ListNode *S8ListTop;

internal void
S8ListPush(MD_String8 String)
{
 MD_S8ListPush(GlobalMDArena, &S8ListTop->V, String);
}

internal void
PushStream(str8 FileName)
{
 MD_String8List *Result = 0;
 
 arena *Arena = GetScratch();
 MD_String8ListNode *List = PushArrayZero(Arena, MD_String8ListNode, 1);
 List->Prev = S8ListTop;
 
 List->FileName = FileName;
 
 S8ListTop = List;
}

internal void
PopStream(str8 NewDirPath)
{
 str8 OutPath = Str8Fmt("%S/%S", NewDirPath, S8ListTop->FileName);
 WriteStreamToFile(S8ListTop->V, (char *)OutPath.Data);
 
 S8ListTop = S8ListTop->Prev;
}

internal void
S8ListPushFmt(char *Format, ...)
{
 va_list Args;
 va_start(Args, Format);
 MD_String8 String = MD_S8FmtV(GlobalMDArena, Format, Args);
 va_end(Args);
 S8ListPush(String);
}

//- Custom build params
internal void 
LogBuildMode(str8 Name, b32 Debug)
{
 str8 ModeString = (Debug ? S8("debug") : S8("release"));
 Log("[%S (%S)]\n", Name, ModeString);
}

internal void
ApplicationBuild(str8 ExeName, str8 AppName, str8 AppSource, str8 WindowName,
                 b32 GCC, b32 Clang, b32 Asan, b32 Debug, b32 Slow)
{
 LogBuildMode(ExeName,  Debug);
 
 // Compile
 {            
  str8_array *CommonFlags = Cng_PushStr8Array(256);
  Cng_Str8ArrayAppendTo(CommonFlags, 
                        Str8Fmt("-I" CLING_CODE_PATH " "
                                "-DRL_PLATFORM_APP_NAME=%S "
                                "-DRL_PLATFORM_WINDOW_NAME=%S",
                                AppName, WindowName));
  
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
               MD_S8Match(Value, S8("1"), 0));
 return Result;
}

raddbg_entry_point(EntryPoint);
ENTRY_POINT(EntryPoint)
{
 if(LaneIndex() == 0)
 {
  Cng_InitAndRebuildSelf(Params->ArgsCount, Params->Args, Params->Env);
  
  // Change to build directory
  {        
   str8 Path = Cng_PathFromExe(S8FromCString(Params->Args[0]), S8(CLING_BUILD_PATH));
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
  
  MD_String8 FileName = S8("../code/cling/cling.mdesk");
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
    else if(ConfigMatch(Name, S8("Asan"),  Value)) Asan = 1;
    else if(ConfigMatch(Name, S8("Debug"), Value)) Debug = 1;
    else if(ConfigMatch(Name, S8("Clean"), Value)) Clean = 1;
    else if(ConfigMatch(Name, S8("Clang"), Value)) Clang = 1;
    else if(ConfigMatch(Name, S8("GCC"),   Value)) GCC = 1;
    else if(ConfigMatch(Name, S8("Slow"),  Value)) Slow = 1;
    else if(ConfigMatch(Name, S8("Wine"),  Value)) Wine = 1;
    
    else if(ConfigMatch(Name, S8("HaversineProcessor"), Value)) HaversineProcessor = 1;
    else if(ConfigMatch(Name, S8("HaversineGenerator"), Value)) HaversineGenerator = 1;
    else if(ConfigMatch(Name, S8("Sim86"),  Value)) Sim86 = 1;
    else if(ConfigMatch(Name, S8("Editor"), Value)) Editor = 1;
    else if(ConfigMatch(Name, S8("Muze"),   Value)) Muze = 1;
    
   }
  }
  
  Slow = Slow || Asan;
  
  //~ Metaprogram
  b32 Metaprogram = true;
  if(Metaprogram)
  {
   Log("Generating code...\n");
   
   arena *Arena = GlobalClingArena;
   
   u64 HashArraySize = KB(4);
   table_hash_node *TableHashArray = PushArrayZero(Arena, table_hash_node, HashArraySize);
   field_hash_node *FieldHashArray = PushArrayZero(Arena, field_hash_node, HashArraySize);
   
   for(os_dir *Dir = OS_WalkDirPush(S8("../code"), 0);
       Dir != 0;
       )
   {
    os_dir_entry *Entry = OS_WalkDirGetNext(Dir);
    
    if(Entry != 0)
    {
     str8 Name = Entry->Name;
     
     b32 InvalidDir = (S8Match(Entry->Name, S8("."), false) || 
                       S8Match(Entry->Name, S8(".."), false));
     if(!InvalidDir)
     {
      if(0) {}
      else if(Entry->Kind == OS_DirEntryKind_Directory)
      {
       Dir = OS_WalkDirPush(Entry->Name, Dir);
      }
      else if(Entry->Kind == OS_DirEntryKind_File)
      {
       str8 Ext = S8(".mdesk");
       str8 Scan = S8From(Name, Name.Size - Ext.Size);
       
       b32 Match = (Name.Size > Ext.Size &&
                    S8Match(Scan, Ext, false));
       
       if(Match)
       {
        str8 NewDirPath = Str8Fmt("%S/generated", Dir->Path);
        OS_MakeDirectory((char *)NewDirPath.Data);
        
        str8 BaseName = S8To(Name, Name.Size - Ext.Size);
        str8 OutFile = Str8Fmt("%S.meta.c", BaseName); 
        
        MD_String8 FilePath = Str8Fmt("%S/%S", Dir->Path, Name);
        MD_ParseResult Parse = MD_ParseWholeFile(GlobalMDArena, FilePath);
        
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
         
         PushStream(OutFile);
         
         for(MD_EachNode(Node, Root))
         {
          //- NOTE(luca): Header "file" tag  
          b32 FilePushed = false;
          str8 FileName = {0};
          {
           MD_Node *FileTag = MD_TagFromString(Node, S8("file"), 0);
           if(!MD_NodeIsNil(FileTag))
           {
            FileName = FileTag->first_child->string;
            PushStream(FileName);
            FilePushed = true;
           }
          }
          
          //- NOTE(luca): Add tables
          {
           MD_Node *TableTag = MD_TagFromString(Node, S8("table"), 0);
           if(!MD_NodeIsNil(TableTag))
           {
            str8 TableName = Node->string;
            table_hash_node *TableHash = AddTableHashNode(Arena, TableName, 0, 
                                                          HashArraySize, TableHashArray);
            TableHash->Table = Node;
            
            u64 FieldIdx = 0;
            for(MD_EachNode(Field, TableTag->first_child))
            {
             AddFieldHashNode(Arena, Field->string, TableHash->Key, 
                              HashArraySize, FieldHashArray)->Idx = FieldIdx;
             
             FieldIdx += 1;
            }
           }
          }
          
          //- NOTE(luca): Gen data
          {
           b32 ToLower = MD_NodeHasTag(Node, S8("to_lower"), 0);
           b32 GenData = MD_NodeHasTag(Node, S8("data"), 0);
           b32 NoPadding = MD_NodeHasTag(Node, S8("no_padding"), 0);
           MD_Node *GenDataTable = MD_TagFromString(Node, S8("data_table"), 0);
           MD_Node *GenDataStrings = MD_TagFromString(Node, S8("data_strings"), 0);
           MD_Node *GenDataEnum = MD_TagFromString(Node, S8("data_enum"), 0);
           MD_Node *GenDataStruct = MD_TagFromString(Node, S8("data_struct"), 0);
           str8 TypeName = {0};
           
           GenData |= (!MD_NodeIsNil(GenDataTable));
           GenData |= (!MD_NodeIsNil(GenDataStrings));
           GenData |= (!MD_NodeIsNil(GenDataEnum));
           GenData |= (!MD_NodeIsNil(GenDataStruct));
           
           if(GenData)
           {
            // Add headers
            {                                                
             if(NoPadding)
             {
              S8ListPush(S8("NO_STRUCT_PADDING_BEGIN\n"));
             }
             
             if(!MD_NodeIsNil(GenDataTable))
             {
              TypeName = GenDataTable->first_child->string;
              S8ListPush(Str8Fmt("%S %S[] = {\n", TypeName, Node->string)); 
             }
             
             if(!MD_NodeIsNil(GenDataStruct))
             {
              TypeName = GenDataStruct->first_child->string;
              S8ListPush(Str8Fmt("typedef struct %S %S;\n"
                                 "struct %S\n"
                                 "{\n", TypeName, TypeName, TypeName)); 
             }
             
             if(!MD_NodeIsNil(GenDataEnum))
             {
              TypeName = GenDataEnum->first_child->string;
              S8ListPush(Str8Fmt("enum %S\n"
                                 "{\n", TypeName, TypeName, TypeName)); 
              
             }
             
             if(!MD_NodeIsNil(GenDataStrings))
             {
              str8 TableName = MD_NodeAtIndex(GenDataStrings->first_child, 0)->string;
              str8 FieldName = MD_NodeAtIndex(GenDataStrings->first_child, 1)->string;
              
              table_hash_node *TableHash = GetTableHashNode(TableName, 0, HashArraySize, TableHashArray);
              Assert(TableHash);
              field_hash_node *FieldHash = GetFieldHashNode(FieldName, TableHash->Key, HashArraySize, FieldHashArray);
              
              if(FieldHash)
              {                                                                
               DeferLoop(S8ListPushFmt("str8 %S[] =\n"  "{\n", Node->string),  S8ListPushFmt("};\n"))
                for(MD_EachNode(Row, TableHash->Table->first_child))
               {                                                   
                MD_Node *Child = MD_NodeAtIndex(Row->first_child, (int)FieldHash->Idx);
                if(!MD_NodeIsNil(GenDataStrings))
                {
                 S8ListPushFmt("{(u8 *)\"%S\", %llu},\n", Child->string, Child->string.Size);
                }
               }
              }
              else
              {
               // TODO(luca): Report error
               NotImplemented();
              }
             }
             
            }
            
            for(MD_EachNode(Row, Node->first_child))
            {
             
             MD_Node *ExpandTag = MD_TagFromString(Row, S8("expand"), 0);
             if(!MD_NodeIsNil(ExpandTag))
             {
              str8 TableName = ExpandTag->first_child->string;
              
              table_hash_node *TableHash = GetTableHashNode(TableName, 0, HashArraySize, TableHashArray);
              Assert(TableHash);
              
              u64 RowCount = 0;
              for(MD_EachNode(TableRow, TableHash->Table->first_child))
              {
               u64 MaxSize = KB(4);
               str8 String = PushS8(Arena, MaxSize);
               u64 StringIdx = 0;
               
               str8 RowString = Row->string;
               
               for EachIndex(Idx, Row->string.Size)
               {
                if(RowString.Data[Idx] == '$')
                {
                 Idx += 1;
                 
                 if(RowString.Data[Idx] == '(')
                 {
                  Idx += 1;
                  
                  u64 Start = Idx;
                  
                  while(RowString.Data[Idx] != ')') Idx += 1;
                  
                  str8 FieldName = S8FromTo(RowString, Start, Idx); 
                  str8 OutString = {0};
                  
                  if(S8Match(FieldName, S8("_Idx"), false))
                  {
                   OutString = Str8Fmt("%llu", RowCount);
                  }
                  else
                  {
                   field_hash_node *FieldHash = GetFieldHashNode(FieldName, TableHash->Key, HashArraySize, FieldHashArray);
                   MD_Node *Field = MD_NodeAtIndex(TableRow->first_child, (int)FieldHash->Idx);
                   OutString = Field->string;
                  }
                  
                  // Output that string
                  {                 
                   for EachIndex(CharIdx, OutString.Size)
                   {
                    u8 Char = OutString.Data[CharIdx];
                    if(ToLower)
                    {
                     Char = ToLowercase(Char);
                    }
                    String.Data[StringIdx] = Char;
                    StringIdx += 1;
                   }
                  }
                  
                  if(OutString.Size == 0)
                  {
                   Log("ERROR: field \"%s\" not found", FieldName);
                  }
                  
                 }
                }
                else
                {
                 String.Data[StringIdx] = RowString.Data[Idx];
                 StringIdx += 1;
                }
               }
               
               String.Size = StringIdx;
               
               ArenaFree(Arena, MaxSize - String.Size);
               
               S8ListPush(String);
               
               RowCount += 1;
              }
              
             }
             else
             {
              S8ListPush(Row->string);
             }
            }
            
            // Add footers 
            {           
             if(!MD_NodeIsNil(GenDataTable) ||
                !MD_NodeIsNil(GenDataStruct))
             {
              S8ListPush(S8("\n};\n")); 
             }
             
             if(!MD_NodeIsNil(GenDataEnum))
             {
              S8ListPush(Str8Fmt("\n};\n"
                                 "typedef enum %S %S;\n", TypeName, TypeName));
             }
             
             if(NoPadding)
             {
              S8ListPush(S8("NO_STRUCT_PADDING_END\n"));
             }
             
            }
            
            // Pretty
            S8ListPush(S8("\n"));
           }
          }
          
          //- NOTE(luca): Footer "file" tag  
          if(FilePushed)
          {
           PopStream(NewDirPath);
          }
          //- 
         }
         
         PopStream(NewDirPath);
        }
       }
      }
     }
    }
    else
    {
     Dir = OS_WalkDirPop(Dir);
    }
   }
  }
  
  //~ Haversine Generator
  if(HaversineGenerator)
  {
   LogBuildMode(S8("Haversine generator"), Debug);
   
   Cng_SetSelectedArray(Cng_PushStr8Array(256));
   
   Cng_CommonBuildCommand(GCC, Clang, Debug, Asan);
   if(Cng_IsOs(CngOS_Linux))
   {
    Cng_Str8ArrayAppend(S8("-I" CLING_CODE_PATH));
   }
   
   Cng_Str8ArrayAppend(S8("../code/compenhance/haversine_generator/haversine_generator.c"));
   
   RunCommand();
   
  }
  
  //~ Haversine Processor
  if(HaversineProcessor)
  {
   LogBuildMode(S8("Haversine processor"), Debug);
   
   Cng_SetSelectedArray(Cng_PushStr8Array(256));
   
   Cng_CommonBuildCommand(GCC, Clang, Debug, Asan);
   if(Cng_IsOs(CngOS_Linux))
   {
    Cng_Str8ArrayAppend(S8("-I" CLING_CODE_PATH));
   }
   
   Cng_Str8ArrayAppend(S8("../code/compenhance/haversine_processor/haversine_processor.c"));
   
   RunCommand();
  }
  
  //~ Sim86
  if(Sim86)
  {
   LogBuildMode(S8("sim86"), Debug);
   
   if(Cng_IsOs(CngOS_Linux))
   {            
    str8_array *ExtraFlags = Cng_PushStr8Array(256);
    Cng_Str8ArrayAppendTo(ExtraFlags, S8("-I" CLING_CODE_PATH));
    Cng_Str8ArrayAppendTo(ExtraFlags, S8("-Wno-sign-conversion"));
    
    LinuxBuildCommand(S8("../code/compenhance/sim86/sim86.cpp"), 
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
