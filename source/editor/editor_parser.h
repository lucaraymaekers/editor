#ifndef EDITOR_PARSER_H
#define EDITOR_PARSER_H

enum syntax_node_type
{
    SyntaxNodeLiteral,
    SyntaxNodeIdentifier,
    SyntaxNodeBinary,

    SyntaxNodeAssignment,
    SyntaxNodeReturn,
    SyntaxNodeFunction
};

typedef struct syntax_node syntax_node;
struct syntax_node
{
    syntax_node *First;
    syntax_node *Last;
    syntax_node *Parent;
    syntax_node *Next;

    token *Token;

    syntax_node_type Type;
};

typedef struct concrete_syntax_tree concrete_syntax_tree;
struct concrete_syntax_tree
{
    syntax_node *Root;
    syntax_node *Current;
};

// TODO(nasr): implement this later together with file handling
read_only global_variable
syntax_node nil_syntax_node =
{
.First  = &nil_syntax_node,
.Last   = &nil_syntax_node,
.Parent = &nil_syntax_node,
.Next   = &nil_syntax_node,
.Token  = &nil_token,
};

read_only global_variable
concrete_syntax_tree nil_concrete_syntax_tree =
{
.Root    = &nil_syntax_node,
.Current = &nil_syntax_node,
};

#define PeekForward(Node, Type, NilNode) \
    for(; (Node) && (Node) != &(NilNode); (Node) = (Node)->Next) \
        if((Node)->Token->Type == (Type)) \
            break;

#endif // EDITOR_PARSER_H
