#ifndef EDITOR_TREE_VISUALIZER_H
#define EDITOR_TREE_VISUALIZER_H

typedef struct v_node v_node;
struct v_node
{
  v2 Pos;

  v2 Size;

  str8 Label;

  v4      Color;
  v_node *Parent;
  v_node *NextVNode;
  v_node *First;
  v_node *Last;

  token  *Token;
};

typedef struct debug_tree debug_tree;
struct debug_tree
{
  v_node *Root;
  v_node *Current;
  // TODO(nasr): using the same struct everywhere find a way to simplify them
};

read_only global_variable v_node nil_v_node =
{
.Pos       = 0,
.Size      = 0,
.Label     = {NULL, 0},
.Color     = 0,
.Parent    = NULL,
.NextVNode = NULL,
.First     = NULL,
.Last      = NULL,

};

read_only global_variable debug_tree nil_debug_tree =
{
.Root    = NULL,
.Current = NULL,
};

#endif // EDITOR_TREE_VISUALIZER_H
