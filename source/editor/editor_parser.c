internal syntax_node *
CreateNode(concrete_syntax_tree *Tree, arena *Arena, token *Token)
{
    syntax_node *SyntaxNode = PushStruct(Arena, syntax_node);
    SyntaxNode->Token       = Token;
    SyntaxNode->First       = &nil_syntax_node;
    SyntaxNode->Last        = &nil_syntax_node;
    SyntaxNode->Next        = &nil_syntax_node;
    SyntaxNode->Parent      = &nil_syntax_node;
    return SyntaxNode;
}

/*
   a negative PeekOffset navigates to the offset of the parent 
   a negative PeekOffset navigates to the offset of the child 
   */
internal syntax_node *
PeekToOffset(syntax_node *Node, s32 PeekOffset, b32 findChild)
{
    if(PeekOffset < 0)
	{
	    for(s32 PeekCount = 0; PeekCount < PeekOffset; PeekCount++)
		{
		    if(!Node || !Node->Next) return &nil_syntax_node;
		    Node = Node->Next;
		}
	}
	else if(PeekOffset > 0)
    {
        for(s32 PeekCount = 0; PeekCount < PeekOffset; PeekCount++)
        {
	        if(!Node || !Node->First) return &nil_syntax_node;
	        Node = Node->First;
        }
    }
    else if(PeekOffset == 0)
    {
        return Node;
    }

    return &nil_syntax_node;
}

internal void
NodePushChild(concrete_syntax_tree *Tree, syntax_node *Node)
{
    Node->Parent = Tree->Current;

    if(Tree->Current->First == &nil_syntax_node)
    {
	    Tree->Current->First = Node;
	    Tree->Current->Last  = Node;
    }
    else
    {
	    Tree->Current->Last->Next = Node;
	    Tree->Current->Last       = Node;
    }
}

internal void
NodePushNext(concrete_syntax_tree *Tree, syntax_node *Node)
{
    Node->Parent = Tree->Current;

    if(Tree->Current->Next == &nil_syntax_node) Tree->Current->Next = Node;
    else Tree->Current->Last->Next = Node;
}

internal inline b32
IsNilSyntaxNode(syntax_node *Node)
{
    return Node == &nil_syntax_node || Node == NULL;
}

internal void
DisownNode(concrete_syntax_tree *Tree, syntax_node *Node)
{
    Node->First = &nil_syntax_node;
    Node->Last= &nil_syntax_node;;
    Node->Parent= &nil_syntax_node;
    Node->Next= &nil_syntax_node;
    Node->Token= &nil_token;
    Node->Type = SyntaxNodeUnwanted;
}

internal void
AdoptNode(concrete_syntax_tree *Tree, syntax_node *ParentNode, syntax_node *ChildNode)
{
    // TODO(nasr)
}

internal inline void
Ground(token *Token)
{
    Token->Flags = (token_flags)(Token->Flags | FlagDirty);
}

internal concrete_syntax_tree *
Parse(arena *Arena, token_list *List, concrete_syntax_tree *Tree)
{
    for(token_node *TokenNode = List->Root;
	    TokenNode != &nil_token_node && TokenNode != NULL;
	    TokenNode = TokenNode->Next)
    {
	    token       *Token      = TokenNode->Token;
	    syntax_node *SyntaxNode = CreateNode(Tree, Arena, Token);

	    if(IsNilSyntaxNode(Tree->Root))
        {
		    Tree->Root    = SyntaxNode;
		    Tree->Current = SyntaxNode;
        }

	    switch((token_type)Token->Type)
        {
		    case TokenIdentifier:
		        {
			        if(!IsNilTokenNode(TokenNode) && TokenNode->Next->Token->Type == (token_type)'=')
			        {
				        token_node *ValueNode = TokenNode->Next->Next;

				        if(ValueNode->Token->Type != TokenIdentifierAssignmentValue &&
				           ValueNode->Token->Type != TokenValue)
				        {
					        ValueNode->Token->Type = TokenIdentifierAssignmentValue;
				        }
			        }

			        if(Tree->Current != SyntaxNode)
			        {
				        NodePushChild(Tree, SyntaxNode);
			        }
		        }
		        break;

		    case TokenIdentifierAssignmentValue:
		        {
			        NodePushChild(Tree, SyntaxNode);
		        }
		        break;

		    case TokenNumber:
		    case TokenString:
		        {
			        if(Tree->Current && Tree->Current->Token->Type == (token_type)'=')
			        {
				        Token->Type = TokenIdentifierAssignmentValue;

				        if(Tree->Current->Parent && Tree->Current->Parent->Token->Type != TokenIdentifier)
				        {
					        Ground(Token);
				        }
			        }

			        NodePushChild(Tree, SyntaxNode);
		        }
		        break;

		    case TokenDoubleEqual:
		        {
			        NodePushChild(Tree, SyntaxNode);

			        if(!IsNilSyntaxNode(Tree->Current->Parent))
			        {
				        Tree->Current->First  = Tree->Current->Parent;
				        Tree->Current->Last   = Tree->Current->Next;
				        Tree->Current->Parent = Tree->Current;
			        }
		        }
		        break;

		    case TokenGreaterEqual:
		    case TokenLesserEqual:
		    case(token_type)'<':
		    case(token_type)'>':
		        {
			        NodePushChild(Tree, SyntaxNode);
			        Tree->Current = SyntaxNode;
		        }
		        break;

		    case(token_type)'(':
		        {
			        syntax_node *Current = Tree->Current;
			        Tree->Current->First = PeekToOffset(Tree->Current, 1, 0);

			        while(Tree->Current->Token->Type != (token_type)')' && !IsNilSyntaxNode(Tree->Current->Next))
			        {
				        Current = Current->Next;
			        }
			        if(Current == &nil_syntax_node)
			        {
				        Log("Forgot to close paran");
			        }
		        }
		        break;

		    case(token_type)')':
		        {
			        while(Tree->Current &&
				          Tree->Current != &nil_syntax_node &&
				          Tree->Current->Token->Type != (token_type)'(')
			        {
				        Tree->Current = Tree->Current->Parent;
			        }

			        if(Tree->Current && Tree->Current->Parent)
			        {
				        Tree->Current = Tree->Current->Parent;
			        }
		        }
		        break;

		    case(token_type)'{':
		        {
		            syntax_node *Node = &nil_syntax_node;

		            for(s32 index = 0; PeekToOffset(Tree->Current, index, 0); ++index)
		            {
		                Tree->Current->First = Tree->Current->Next;
		                // TODO(nasr): was doing something here 
		            }
		        }
		        break;

		    case(token_type)'}':
		        {
			        if(Tree->Current && Tree->Current->Parent)
			        {
				        Tree->Current = Tree->Current->Parent;
			        }
		        }
		        break;

		    case(token_type)';':
		        {
			        Tree->Current->Last = Tree->Current;
			        Tree->Current       = Tree->Current->Parent;
		        }
		        break;

		    case TokenFunc:
		        {
			        // TODO(nasr): define the function body
			        NodePushChild(Tree, SyntaxNode);
			        Tree->Current = SyntaxNode;
		        }
		        break;

		    case TokenReturn:
		        {
			        if(Tree->Current &&
			           Tree->Current->Parent &&
			           Tree->Current->Parent->Token->Type != TokenFunc)
			        {
				        Ground(Token);
			        }

			        NodePushChild(Tree, SyntaxNode);
			        Tree->Current = SyntaxNode;
		        }
		        break;

		    case TokenIf:
		        {
			        NodePushChild(Tree, SyntaxNode);
			        Tree->Current = SyntaxNode;
		        }
		        break;

		    case TokenElse:
		        {
			        // TODO(nasr): handle no body
			        NodePushChild(Tree, SyntaxNode);
		        }
		        break;

		    case TokenWhile:
		    case TokenFor:
		        {
			        NodePushChild(Tree, SyntaxNode);
			        Tree->Current = SyntaxNode;
		        }
		        break;

		    case TokenBreak:
		        {
			        token_type Type = Tree->Current->Parent->Token->Type;

			        if(Type != TokenFor && Type != TokenWhile)
			        {
				        Ground(Token);
				        Log("Break statement not allowed here");
			        }
			        NodePushChild(Tree, SyntaxNode);
		        }
		        break;

		    case TokenContinue:
		        {
			        NodePushChild(Tree, SyntaxNode);
		        }
		        break;

		    case TokenExpression:
		    case TokenParam:
		        {
			        NodePushChild(Tree, SyntaxNode);
			        Tree->Current = SyntaxNode;
		        }
		        break;

		    case TokenStar:
		        {
			        // TODO(nasr): once we get to better visualizations i think
			        NodePushChild(Tree, SyntaxNode);
		        }
		        break;

		    case TokenUndefined:
		        {
			        Ground(Token);
			        NodePushChild(Tree, SyntaxNode);
		        }
		        break;

		    default:
		        {
			        Ground(Token);
			        NodePushChild(Tree, SyntaxNode);
		        }
		        break;
        }
    }

    return Tree;
}
