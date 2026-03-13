internal editor_type
PushFamilyBranch()
{
}

internal editor_type
PopFamilyBranch()
{
}

// NOTE(nasr): used for when a lexeme has been identified and we want to check if the lexeme
// is a "type" in the known translation unit or not

// TODO(nasr): this wil only work for some basic types
internal type_kind
LookupTypeKind(str8 Lexeme)

{
    for(s32 index = 0;
    index < sizeof(PrimitiveTypes);
    ++index)
    {
        if(S8Match(Lexeme), S8(PrimitveTypes[index]))
        {
        }
    }
}
