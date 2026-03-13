internal v4
CreateVNodeColor(syntax_node *Node)
{
    switch(Node->Type)
    {
        case(TokenIdentifier):
            return Color_Yellow;
        case(TokenIdentifierAssignmentValue):
            return Color_Cyan;
        case(TokenValue):
            return Color_White;
        case(TokenString):
            return Color_Green;
        case(TokenNumber):
            return Color_Magenta;
        case(TokenDoubleEqual):
            return Color_Orange;
        case(TokenGreaterEqual):
            return Color_Orange;
        case(TokenLesserEqual):
            return Color_Orange;
        case(TokenParam):
            return Color_Blue;
        case(TokenFunc):
            return Color_Cyan;
        case(TokenReturn):
            return Color_Green;
        case(TokenIf):
            return Color_Magenta;
        case(TokenElse):
            return Color_Magenta;
        case(TokenFor):
            return Color_Magenta;
        case(TokenWhile):
            return Color_Magenta;
        case(TokenBreak):
            return Color_Orange;
        case(TokenContinue):
            return Color_Orange;
        case(TokenExpression):
            return Color_White;
        case(TokenFuncBody):
            return Color_BackgroundSecond;
        case(TokenUnwantedChild):
            return Color_Gray;
        case(TokenNewLine):
            return Color_Background;
        case(TokenRightShift):
            return Color_Orange;
        case(TokenLeftShift):
            return Color_Orange;
        case(TokenUndefined):
        default:
            return Color_Red;
    }
}

internal v_node *
CreateVNode(arena *Arena, syntax_node *Node)
{
    v_node *VNode = PushStructZero(Arena, v_node);
    VNode->Color  = CreateVNodeColor(Node);
    VNode->Size   = {10.f};
    VNode->Pos    = {0};
    // VNode->Label     = Node->Token->Lexeme;
    VNode->Label     = {NULL, 0};
    VNode->Parent    = &nil_v_node;
    VNode->NextVNode = &nil_v_node;
    VNode->First     = &nil_v_node;
    VNode->Last      = &nil_v_node;
    return VNode;
}

internal void
BuildDebugTreeRecursive(debug_tree *DebugTree, v_node *ParentVNode, syntax_node *Node, arena *Arena)
{
    for(syntax_node *Sibling = Node; Sibling && Sibling != &nil_syntax_node; Sibling = Sibling->Next)
    {
        v_node *VNode = CreateVNode(Arena, Sibling);
        VNode->Parent = ParentVNode;

        if(ParentVNode->First == &nil_v_node)
        {
            ParentVNode->First = VNode;
            ParentVNode->Last  = VNode;
        }
        else
        {
            ParentVNode->Last->NextVNode = VNode;
            ParentVNode->Last            = VNode;
        }

        if(Sibling->First && Sibling->First != &nil_syntax_node)
        {
            BuildDebugTreeRecursive(DebugTree, VNode, Sibling->First, Arena);
        }
    }
}

internal debug_tree *
BuildDebugTree(concrete_syntax_tree *Tree, arena *Arena)
{
    debug_tree *DebugTree = PushStructZero(Arena, debug_tree);
    if(!Tree->Root || Tree->Root == &nil_syntax_node)
        return DebugTree;

    v_node *RootVNode = CreateVNode(Arena, Tree->Root);
    RootVNode->Parent = &nil_v_node;
    DebugTree->Root   = RootVNode;

    BuildDebugTreeRecursive(DebugTree, RootVNode, Tree->Root->First, Arena);
    return DebugTree;
}

internal void
DrawVNode(app_offscreen_buffer *Buffer, v_node *VNode, f32 *X, f32 *Y, f32 NodeSize)
{
    if(!VNode || VNode == &nil_v_node)
        return;

    for(v_node *Child = VNode->First; Child && Child != &nil_v_node; Child = Child->NextVNode)
    {
        rect Dest = RectFromSize(V2(*X, *Y), V2(NodeSize, NodeSize));
        DrawRect(Dest, VNode->Color, 8.f, 0.f, 1.f);
        *X += NodeSize + 5.f;
    }
    *Y += NodeSize + 5.f;

    for(v_node *Child = VNode->First; Child && Child != &nil_v_node; Child = Child->NextVNode)
    {
        DrawVNode(Buffer, Child, X, Y, NodeSize);
    }
}

internal void
DebugTree(app_offscreen_buffer *Buffer, debug_tree *DT)
{
    f32 Y        = 50.f;
    f32 X        = 50.f;
    f32 NodeSize = 40.f;
    DrawVNode(Buffer, DT->Root, &X, &Y, NodeSize);
}
