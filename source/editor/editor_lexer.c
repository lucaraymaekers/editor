internal inline b32
IsAlpha(rune Character)
{
    return ((Character >= 'a' && Character <= 'z') ||
            (Character >= 'A' && Character <= 'Z') ||
            (Character == '_'));
}

internal inline b32
IsDigit(rune Character)
{
    return (Character >= '0' && Character <= '9');
}

internal b32
IsDelimiter(rune Character)
{
    for(s32 Index = 0; Index < ArrayCount(Delimiters); ++Index)
    {
        if(Delimiters[Index] == Character)
        {
            return 1;
        }
    }
    return 0;
}

internal inline b32
IsNilTokenNode(token_node *TokenNode)
{
    return TokenNode == &nil_token_node || TokenNode == NULL;
}

internal inline b32
IsNilToken(token *Token)
{
    return Token == &nil_token || Token == NULL;
}

internal inline b32
IsWhiteSpace(rune Character)
{
    return (Character == '\n' || Character == '\r' ||
            Character == ' ' || Character == '\t');
}

internal inline void
ParseCStyleComment(rune Buffer[])
{
    // TODO(nasr): handle c style comments
    // couuld be usefull for function information visualiszation
    // so think of a way to link themn to functions and variables?
    // some sort of meta data per thing?
    // and then we can do a visualization if the str8.count of the metadata thing is bigger then 0
    // we should a visualization thing for the thing
    // if the thing is less then 0, we dont do anything?

    // TODO(nasr): while doingn this we could also add in some editor specific anotations ?
}

internal inline void
ParseCPPStyleComment(rune Buffer[])
{
    // TODO(nasr):
}

internal inline b32
Is_TokenBreak(rune Character)
{
    return (IsWhiteSpace(Character) || IsDelimiter(Character));
}

internal token_list *
Lex(app_state *App, arena *Arena, token_list *List)
{
    b32 Initialized = 0;
    s32 Line        = 1;
    s32 Column      = 1;

    for(s32 TextIndex = 0; TextIndex < App->TextCount; TextIndex++)
    {
        rune Character = App->Text[TextIndex];

        if(Character == '\r' || Character == '\n')
        {
            if(Character == '\r' &&
               (TextIndex + 1 < App->TextCount) &&
               App->Text[TextIndex + 1] == '\n')
            {
                TextIndex++;
            }

            ++TextIndex;
            ++Line;

            // NOTE(nasr): reset the column to the beginning of the line
            Column = 1;
            continue;
        }

        if(IsWhiteSpace(Character))
        {
            ++Column;
            continue;
        }

        token_node *TokenNode = PushStructZero(Arena, token_node);
        token      *Token     = PushStructZero(Arena, token);
        TokenNode->Next       = &nil_token_node;
        TokenNode->Previous   = &nil_token_node;
        TokenNode->Token      = Token;
        Token->Line           = Line;
        Token->Column         = Column;
        Token->ByteOffset     = (u64)TextIndex;
        Token->Flags          = FlagNone;

        s32 TokenStart = TextIndex;
        s32 TokenEnd   = TextIndex;

        if(Character > 126)
        {
            Token->Type = TokenUnwantedChild;
            TokenEnd    = TextIndex + 1;
        }
        else if(IsAlpha(Character))
        {
            {
                while((TextIndex + 1 < App->TextCount) &&
                      (IsAlpha(App->Text[TextIndex + 1]) || IsDigit(App->Text[TextIndex + 1])))
                {
                    ++TextIndex;
                }

                TokenEnd = TextIndex + 1;
            }
            {
                str8 Lexeme = S8FromTo(((str8){.Data = (u8 *)App->Text, .Size = (u64)App->TextCount}), (u64)TokenStart, (u64)TokenEnd);

                // TODO(nasr): handle functions
                if(S8Match(Lexeme, S8("if"), 0))
                    Token->Type = TokenIf;
                else if(S8Match(Lexeme, S8("else"), 0))
                    Token->Type = TokenElse;
                else if(S8Match(Lexeme, S8("return"), 0))
                    Token->Type = TokenReturn;
                else if(S8Match(Lexeme, S8("while"), 0))
                    Token->Type = TokenWhile;
                else if(S8Match(Lexeme, S8("for"), 0))
                    Token->Type = TokenFor;
                else if(S8Match(Lexeme, S8("break"), 0))
                    Token->Type = TokenBreak;
                else if(S8Match(Lexeme, S8("continue"), 0))
                    Token->Type = TokenContinue;
                else
                    Token->Type = TokenIdentifier;
            }
        }
        else if(IsDigit(Character))
        {
            while((TextIndex + 1 < App->TextCount) &&
                  IsDigit(App->Text[TextIndex + 1]))
            {
                ++TextIndex;
            }

            TokenEnd    = TextIndex + 1;
            Token->Type = TokenNumber;
        }

        else
        {
            rune Next = (TextIndex + 1 < App->TextCount) ? App->Text[TextIndex + 1] : 0;

            switch(Character)
            {
                case '=':
                    {
                        if(Next == '=')
                        {
                            Token->Type = TokenDoubleEqual;
                            TextIndex++;
                        }
                        else
                        {
                            Token->Type = (token_type)'=';
                        }
                    }
                    break;

                case '>':
                    {
                        if(Next == '=')
                        {
                            Token->Type = TokenGreaterEqual;
                            TextIndex++;
                        }
                        else if(Next == '>')
                        {
                            Token->Type = TokenRightShift;
                            TextIndex++;
                        }
                        else
                        {
                            Token->Type = (token_type)'>';
                        }
                    }
                    break;

                case '<':
                    {
                        if(Next == '=')
                        {
                            Token->Type = TokenLesserEqual;
                            TextIndex++;
                        }
                        else if(Next == '<')
                        {
                            Token->Type = TokenLeftShift;
                            TextIndex++;
                        }
                        else
                        {
                            Token->Type = (token_type)'<';
                        }
                    }
                    break;

                case '"':
                    {
                        while(App->Text[TextIndex + 1] != '"' && App->Text[TextIndex + 1] != '\0')
                        {
                            ++TextIndex;
                            if(App->Text[TextIndex + 1] == '\\')

                                ++TextIndex;  
                        }

                        TokenStart += 1;
                        Token->Type = TokenString;
                    }
                    break;

                default:
                    {
                        Token->Type = (token_type)Character;
                    }
                    break;
            }
        }

        TokenEnd = TextIndex + 1;

        Token->Lexeme.Data = (u8 *)&App->Text[TokenStart];
        Token->Lexeme.Size = (u64)(TokenEnd - TokenStart);
        Column += (s32)Token->Lexeme.Size;

        //Log("Token: \t%.lu*s\n", Token->Lexeme.Size, Token->Lexeme.Data);

        if(!Initialized)
        {
            Initialized   = 1;
            List->Root    = TokenNode;
            List->Current = TokenNode;
        }
        else
        {
            TokenNode->Previous = List->Current;
            List->Current->Next = TokenNode;
            List->Current       = TokenNode;
        }
    }

    return List;
}
