#include "../shared_libraries/lr/lr.h"

PUSH_WARNINGS
#include "md.h"
#include "md.c"
POP_WARNINGS

#define Assert(Expression) if(!(Expression)) { __asm__ volatile("int3"); }

static MD_Arena *Arena = 0;

int main(int ArgsCount, char *Args[])
{
    if(ArgsCount > 1)
    {
        Arena = MD_ArenaAlloc();
        MD_String8 FileName = MD_S8CString(Args[1]);
        MD_ParseResult Parse = MD_ParseWholeFile(Arena, FileName);
        
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
                
                // 1. If we find a table_gen_flags tag
                if(MD_NodeHasTag(Node, MD_S8Lit("table_gen_flags"), 0))
                {
                    MD_String8 TableName = MD_NodeAtIndex(Node->first_tag->first_child, 0)->string;
                    MD_String8 MemberName = MD_NodeAtIndex(Node->first_tag->first_child, 1)->string;
                    
                    // 2. Find the table in the first argument 
                    // TODO(luca): Check for not nil otherwise print error
                    MD_Node *TableNode = MD_FirstNodeWithString(Root, TableName, 0);
                    MD_Node *TableTag = MD_TagFromString(TableNode, MD_S8Lit("table"), 0);
                    
                    // TODO(luca): Check for not nil otherwise print error
                    MD_Node *MemberNode = MD_FirstNodeWithString(TableTag->first_child, MemberName, 0);
                    int MemberIndex = MD_IndexFromNode(MemberNode);
                    
                    // Header
                    MD_S8ListPushFmt(Arena, &Stream, "enum %S\n{\n", Node->string);
                    
                    int MemberCount = 0;
                    for(MD_EachNode(Member, TableNode->first_child))
                    {
                        // 3. Use the member of it in the second argument
                        MD_Node *MemberNode = MD_NodeAtIndex(Member->first_child, MemberIndex);
                        
                        // 4. For each member in the table
                        //    1. Create an enum where you create flags in the form 
                        //       Flag_None 0 << 1
                        //       Flag_First 1 << 1
                        //       ...
                        MD_S8ListPushFmt(Arena, &Stream, " Flag_%S = (1 << %d),\n", 
                                         MemberNode->string, MemberCount);
                        MemberCount++;
                    }
                    
                    MD_S8ListPush(Arena, &Stream, MD_S8Lit("};\n\n"));
                }
                
                // 1. If we find a table_gen_data tag
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
                    
                    MD_S8ListPushFmt(Arena, &Stream, "int %SCount = %d;\n", Node->string, MemberCount);
                    // Header
                    MD_S8ListPushFmt(Arena, &Stream, "%S%S[] =\n{\n", Type, Node->string);
                    
                    for(MD_EachNode(Member, Table->first_child))
                    {
                        MD_Node *ValueNode  = MD_NodeAtIndex(Member->first_child, MemberIndex);
                        MD_S8ListPushFmt(Arena, &Stream, " %S,\n", ValueNode->raw_string);
                    }
                    
                    MD_S8ListPush(Arena, &Stream, MD_S8Lit("};\n\n"));
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
                    
                    
                    MD_S8ListPushFmt(Arena, &Stream, "enum %S\n{\n", Node->raw_string);
                    
                    for(MD_EachNode(Member, Table->first_child))
                    {
                        MD_Node *ValueNode  = MD_NodeAtIndex(Member->first_child, MemberIndex);
                        if(Member != Table->first_child)
                        {
                            MD_S8ListPushFmt(Arena, &Stream, " %S%S,\n", Prefix, ValueNode->raw_string);
                        }
                        else
                        {
                            MD_S8ListPushFmt(Arena, &Stream, " %S%S = 0,\n", Prefix, ValueNode->raw_string);
                        }
                        
                    }
                    MD_S8ListPushFmt(Arena, &Stream, "};\n");
                    
                    
                }
                
                
#if 0                
                if(MD_NodeHasTag(Node, MD_S8Lit("table_gen_enum"), 0))
                {
                    // Header
                    MD_S8ListPushFmt(Arena, &Stream, "enum %S\n{\n", Node->string);
                    
                    for(MD_EachNode(Member, Node->first_child))
                    {
                        if(MD_NodeHasTag(Member, MD_S8Lit("expand"), 0))
                        {
                            MD_Node *Tag = MD_TagFromString(Member, MD_S8Lit("expand"), 0);
                            MD_String8 TableName = MD_NodeAtIndex(Tag->first_child, 0)->string;
                            MD_Node *Table = MD_FirstNodeWithString(Root, TableName, 0);
                            
                            // 2. For each node in the table
                            for(MD_EachNode(TableMember, Table->first_child))
                            {
                                // 3. Evaluate the expression
                                //    1. Search Member->string for a '$' sign
                                //    2. If it's followed by a dot and a name, parse that name.
                                //    3. Store the name and use it to lookup the members
                                // TODO(luca): 
                                
                                
                                Member->string;
                            }
                            
                        }
                        else
                        {
                            MD_S8ListPush(Arena, &Stream, Member->string);
                            MD_S8ListPush(Arena, &Stream, MD_S8Lit("\n"));
                        }
                        
                    }
                    
                    MD_S8ListPush(Arena, &Stream, MD_S8Lit("};\n"));
                }
#endif
                
                
                // MD_PrintDebugDumpFromNode(stderr, Node, MD_GenerateFlags_All);
            }
            
            MD_String8 Str = MD_S8ListJoin(Arena, Stream, 0);
            fwrite(Str.str, 1, Str.size, stdout);
        }
        
    }
    return 0;
}
