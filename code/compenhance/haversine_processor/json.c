
//~ Types

typedef enum json_token_type json_token_type;
enum json_token_type
{
 JSON_TokenType_None = 0,
 
 JSON_TokenType_Object,
 JSON_TokenType_Array,
 JSON_TokenType_Number,
 JSON_TokenType_String,
 JSON_TokenType_Colon,
 JSON_TokenType_Comma,
 // keywords
 JSON_TokenType_True,
 JSON_TokenType_False,
 JSON_TokenType_Null,
 
 JSON_TokenType_Error,
 
 JSON_TokenType_Count,
};

typedef struct json_token json_token;
struct json_token
{
 json_token_type Type;
 str8 Value;
 f64 Number;
 
 u64 ErrorAt;
 
 json_token *Child;
 json_token *Next;
};

//~ Globals

global_variable json_token JSON_NilToken;

//~ Functions

internal b32 
MatchString(str8 Buffer, str8 Match, u64 Offset)
{
 b32 Result = true;
 Assert(Offset < Buffer.Size);
 Buffer.Data += Offset;
 Buffer.Size -= Offset;
 
 if(Buffer.Size < Match.Size)
 {
  return false;
 }
 
 for(u64 At = 0;
     At < Match.Size;
     At += 1)
 {
  if(Buffer.Data[At] != Match.Data[At])
  {
   Result = false;
   break;
  }
 }
 
 return Result;
}

internal f64 
JSON_StringToFloat(str8 In)
{
 b32 Negative = false;
 f64 Value = 0;
 
 u64 At = 0;
 
 if(In.Data[At] == '-')
 {
  At += 1;
  Negative = true;
 }
 
 // Integer part
 while(At < In.Size && (In.Data[At] >= '0' && In.Data[At] <= '9')) 
 {
  Value = 10.0*Value + (f64)(In.Data[At] - '0');
  At += 1;
 }
 Assert(In.Data[At] == '.');
 At += 1;
 
 // Fractional part
 f64 Divider = 10;
 while(At < In.Size && (In.Data[At] >= '0' && In.Data[At] <= '9'))
 {
  f64 Digit = (f64)(In.Data[At] - '0');
  
  Value += (Digit / Divider);
  Divider *= 10;
  
  At += 1;
 }
 
 if(Negative)
 {
  Value = -Value;
 }
 
 return Value;
}


json_token *JSON_ParseElement(str8 Buffer, u64 *Pos);

void JSON_ParseList(str8 Buffer, json_token *Token, json_token_type Type, u64 *Pos, u8 EndChar)
{
 Token->Type = Type;
 *Pos += 1;
 json_token *Child = 0;
 while(*Pos < Buffer.Size && Buffer.Data[*Pos] != EndChar)
 {
  if(!Token->Child)
  {
   Child = JSON_ParseElement(Buffer, Pos);
   Token->Child = Child;
  }
  else
  {
   Child->Next = JSON_ParseElement(Buffer, Pos);
   Child = Child->Next;
  }
  while(*Pos < Buffer.Size && IsWhiteSpace(Buffer.Data[*Pos])) *Pos += 1;
 }
}

json_token *JSON_ParseElement(str8 Buffer, u64 *Pos)
{
 json_token *Token = (json_token *)malloc(sizeof(json_token));
 Assert(Token);
 
 MemoryZero(Token);
 
 if(*Pos < Buffer.Size)
 {
  while(*Pos < Buffer.Size && IsWhiteSpace(Buffer.Data[*Pos])) *Pos += 1;
  Assert(*Pos < Buffer.Size);
  
  u64 TokenStart = *Pos;
  
  str8 Keyword = {};
  
  switch(Buffer.Data[*Pos])
  {
   case '{': JSON_ParseList(Buffer, Token, JSON_TokenType_Object, Pos, '}'); break;
   case '[': JSON_ParseList(Buffer, Token, JSON_TokenType_Array,  Pos, ']'); break;
   
   case ':': Token->Type = JSON_TokenType_Colon; break;
   case ',': Token->Type = JSON_TokenType_Comma; break;
   
   case 't': Keyword = S8("true");  Token->Type = JSON_TokenType_True;  break;
   case 'f': Keyword = S8("false"); Token->Type = JSON_TokenType_False; break;
   case 'n': Keyword = S8("null");  Token->Type = JSON_TokenType_Null;  break;
   
   case '"':
   {
    Token->Type = JSON_TokenType_String;
    
    u64 At = *Pos;
    Token->Value.Data = Buffer.Data + At;
    while(*Pos < Buffer.Size)
    {
     *Pos += 1;
     if(Buffer.Data[*Pos] == '\\') continue;
     if(Buffer.Data[*Pos] == '"')  break;
    }
    
    if(*Pos >= Buffer.Size)
    {
     Token->Type = JSON_TokenType_Error;
     Token->ErrorAt = At;
    }
   } break;
   
   default:
   {
    if(Buffer.Data[*Pos] == '-' ||
       (Buffer.Data[*Pos] >= '0' && Buffer.Data[*Pos] <= '9'))
    {
     Token->Type = JSON_TokenType_Number;
     
     f64 Number = 0;
     
     if(Buffer.Data[*Pos] == '-')
     {
      *Pos += 1;
     }
     
     while(*Pos < Buffer.Size)
     {
      if(0) {}
      else if(IsDigit(Buffer.Data[*Pos]))
      {
      }
      else if(Buffer.Data[*Pos] == '.')
      {
       *Pos += 1;
       while(*Pos < Buffer.Size && IsDigit(Buffer.Data[*Pos])) *Pos += 1;
       break;
      }
      
      *Pos += 1;
     }
     
     // TODO(luca): 
     if(Buffer.Data[*Pos] == 'e' || Buffer.Data[*Pos] == 'E') *Pos += 1;
     if(Buffer.Data[*Pos] == '+' || Buffer.Data[*Pos] == '-') *Pos += 1;
     while(*Pos < Buffer.Size && IsDigit(Buffer.Data[*Pos]))  *Pos += 1;
     
     *Pos -= 1;
     
    }
   } break;
  }
  
  
  if(Keyword.Size)
  {
   if(MatchString(Buffer, Keyword, *Pos))
   {
    *Pos += Keyword.Size - 1;
   }
   else
   {
    Token->Type = JSON_TokenType_Error;
    Token->ErrorAt = *Pos;
   }
  }
  
  *Pos += 1;
  
  Token->Value.Data = Buffer.Data + TokenStart;
  Token->Value.Size = *Pos - TokenStart;
 }
 
 return Token;
}

json_token *JSON_LookupIdentifierValue(json_token *Token, str8 Identifier)
{
 json_token *Found = &JSON_NilToken;
 
 for(json_token *Search = Token->Child;
     Search;
     Search = Search->Next)
 {
  if(MatchString(Search->Value, Identifier, 0))
  {
   Found = Search;
   break;
  }
  
  Assert(Search->Next->Type == JSON_TokenType_Colon);
  Assert(Search->Next->Next->Type == JSON_TokenType_Number);
  Search = Search->Next->Next;
  
  if(Search->Next)
  {
   Assert(Search->Next->Type == JSON_TokenType_Comma);
   Search = Search->Next;
  }
 }
 
 if(Found != &JSON_NilToken)
 {
  Assert(Found->Next->Type == JSON_TokenType_Colon);
  Found = Found->Next->Next;
 }
 
 return Found;
}


#if 0
struct json_token_stack_node
{
 json_token *Token;
 json_token_stack_node *Prev;
};

struct json_token_stack
{
 json_token_stack_node *Last;
};

json_token *Pop(json_token_stack *Stack)
{
 json_token *Result = Stack->Last->Token;
 
 json_token_stack_node *FreeNode = Stack->Last;
 Stack->Last = Stack->Last->Prev;
 free(FreeNode);
 
 return Result;
}

void Push(json_token_stack *Stack, json_token *Token)
{
 json_token_stack_node *New = (json_token_stack_node *)malloc(sizeof(*New));
 *New = {};
 New->Token = Token;
 if(Stack->Last)
 {
  New->Prev = Stack->Last;
 }
 Stack->Last = New;
}

b32 IsEmpty(json_token_stack Stack)
{
 b32 Result = (Stack.Last == 0);
 return Result;
}

void JSON_FreeTokensNoRecursion(json_token *Token)
{
 json_token_stack Stack = {};
 
 Push(&Stack, Token);
 
 do
 {
  Token = Pop(&Stack);
  while(Token)
  {
   if(Token->Child)
   {
    Push(&Stack, Token->Child);
   }
   
   json_token *FreeElement = Token;
   Token = Token->Next;
   free(FreeElement);
  }
  
 }
 while(!IsEmpty(Stack));
}
#endif

void JSON_FreeTokens(json_token *Token)
{
 while(Token)
 {
  json_token *FreeElement = Token;
  
  JSON_FreeTokens(Token->Child);
  Token = Token->Next; 
  
  free(FreeElement);
 }
 
}