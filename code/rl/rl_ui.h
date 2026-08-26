/* date = February 23rd 2026 6:09 pm */

#pragma once

//~ Types
enum axis2
{
 Axis2_X,
 Axis2_Y,
 Axis2_Count
};
typedef enum axis2 axis2;
#define UI_EachAxis(Idx) (axis2 Idx = 0; Idx < Axis2_Count; Idx += 1)

enum ui_size_kind
{
 UI_SizeKind_Null,
 UI_SizeKind_Pixels,
 UI_SizeKind_TextContent,
 UI_SizeKind_ParentPct,
 UI_SizeKind_ChildrenSum,
};
typedef enum ui_size_kind ui_size_kind;
#if 0
raddbg_type_view(ui_size, rows($, 
                               Kind == 0 ? "Null" : 
                               Kind == UI_SizeKind_Pixels ? "Pixels" : 
                               Kind == UI_SizeKind_TextContent ? "Text" : 
                               Kind == UI_SizeKind_ParentPct ? "ParentPct" : 
                               Kind == UI_SizeKind_ChildrenSum ? "Children" : 
                               "???", 
                               Value, Strictness));
#endif

enum font_kind
{
 FontKind_Text,
 FontKind_Icon
};
typedef enum font_kind font_kind;

typedef struct ui_size ui_size;
struct ui_size
{
 ui_size_kind Kind;
 f32 Value;
 f32 Strictness;
};

typedef struct ui_key ui_key; 
struct ui_key
{
 u64 U64[1];
}; 

#define UI_CUSTOM_DRAW(Name) void Name(void *CustomDrawData)
typedef UI_CUSTOM_DRAW(ui_custom_draw);

typedef struct ui_box ui_box;
struct ui_box
{
 // Tree links
 ui_box *First;
 ui_box *Last;
 ui_box *Prev;
 ui_box *Next;
 ui_box *Parent;
 
 // Hash links
 ui_box *HashNext;
 ui_box *HashPrev;
 
 // Key and generation info
 str8 String;
 ui_key Key;
 u64 LastTouchedFrameIdx;
 
 s32 Flags;
 // TODO(luca): Metaprogram
 v4 BackgroundColor;
 v4 TextColor;
 v4 BorderColor;
 f32 BorderThickness;
 f32 Softness;
 v4 CornerRadii;
 axis2 LayoutAxis;
 f32 HeightPx;
 font_kind FontKind;
 
 str8 DisplayString;
 ui_custom_draw *CustomDraw; 
 void *CustomDrawData;
 ui_size SemanticSize[Axis2_Count];
 
 // Produced from layout resolving
 v2 FixedPos;
 v2 FixedSize;
 v4 Rec;
 v2 AnimatedPos;
 
 // Produced from input
 b32 Clicked;
 b32 WasClicked;
 b32 Hovered;
 b32 Pressed;
 
 v2 Scroll;
 
 f32 tHot;
 f32 tActive;
};
#define UI_EachBox(Node, First) \
(ui_box *Node = First; !UI_IsNilBox(Node); Node = Node->Next)  
#define UI_EachHashBox(Node, First) \
(ui_box *Node = First; !UI_IsNilBox(Node); Node = Node->HashNext)
raddbg_type_view(ui_box, 
                 rows($, 
                      &$.First == UI_NilBox || &$.First == 0, 
                      String, SemanticSize, 
                      FixedPos, 
                      AnimatedPos, 
                      FixedSize,
                      Rec, 
                      list($, Next), 
                      omit($, String)));

typedef struct ui_box_rec ui_box_rec;
struct ui_box_rec
{
 ui_box *Next;
 s64 PushCount;
 s64 PopCount;
};

typedef struct ui_box_node ui_box_node;
struct ui_box_node
{
 ui_box *Box;
 ui_box_node *Next;
};

//- Stack nodes 
UI_LayoutStacks

//-
typedef struct ui_state ui_state;
struct ui_state
{
 arena *Arena;
 u64 BoxTableSize;
 ui_box *BoxTable;
 
 // Constants
 f32 AnimSpeed;
 
 ui_key Active;
 ui_key Hot;
 
 // Per build information
 app_input *Input;
 ui_box *InputConsumerBox;
 font_atlas *Atlas;
 u64 FrameIdx;
 arena *StyleArena;
 
 arena *FrameArenaFront;
 arena *FrameArenaBack;
 
 b32 AppendToParent;
 ui_box *Current;
 ui_box *Root;
 ui_box_node *FirstDebugBox;
 struct
 {
  UI_StateStacks
 };
 
 b32 RectDebugMode;
};

//~ Globals
global_variable ui_state *UI_State = 0;

global_variable ui_box *UI_NilBox = 0;

global_variable v4 Color_Background = {U32ToV4Arg(0xff4c566a)};
global_variable v4 Color_Foreground = {U32ToV4Arg(0xffeceff4)};
global_variable v4 Color_ButtonBorder = {U32ToV4Arg(0xff3b4252)};
global_variable v4 Color_ButtonBackground = {U32ToV4Arg(0xFF81A1C1)};
global_variable v4 Color_ButtonText = {U32ToV4Arg(0xff000000)};

#define UI_SizePx(Value, Strictness) UI_Size(UI_SizeKind_Pixels, Value, Strictness)
#define UI_SizeText(Value, Strictness) UI_Size(UI_SizeKind_TextContent, Value, Strictness)
#define UI_SizeEm(_Value, Strictness) UI_Size(UI_SizeKind_Pixels, ((_Value)*UI_State->HeightPxTop->Value), Strictness)
#define UI_SizeParent(Value, Strictness) UI_Size(UI_SizeKind_ParentPct, Value, Strictness)
#define UI_SizeChildren(Strictness) UI_Size(UI_SizeKind_ChildrenSum, 0.f, Strictness)
#define UI_SizeFull() UI_SizeParent(1.f, 0.f)
#define UI_SizeFullStrict() UI_SizeParent(1.f, 1.f)

#define UI_FillWidth() UI_SemanticWidth(UI_SizeFull())  
#define UI_FillHeight() UI_SemanticHeight(UI_SizeFull())
#define UI_FillAll() UI_FillHeight() UI_FillWidth()
#define UI_FillAllStrict() \
UI_SemanticWidth(UI_SizeFullStrict()) UI_SemanticHeight(UI_SizeFullStrict())

// TODO(luca): Freelist?
#define StackPush(Arena, t, PushValue, Top) \
t *Push = PushArrayZero((Arena), t, 1); \
Push->Value = (PushValue); \
Push->Prev = (Top); \
Top = Push;

// NOTE(luca): This only works if there is a possible nil value 
#define StackPop(Top) \
((Top)->Value, (Top = Top->Prev)->Value)

#define UI_StackPush(t, Name) StackPush(UI_State->StyleArena, t##_stack_node, Name, UI_State->Name##Top)
#define UI_StackPop(Name) StackPop(UI_State->Name##Top)

//- Generated stack functions 
UI_StackFunctions

internal v4 UI_BorderColorTop() { return UI_State->BorderColorTop->Value; }
internal f32 UI_BorderThicknessTop() { return UI_State->BorderThicknessTop->Value; }

internal void
UI_PushSemanticSizeOnAxis(axis2 Axis, ui_size Size)
{
 if(0) {}
 else if(Axis == Axis2_X)
 {
  UI_PushSemanticWidth(Size);
 }
 else if(Axis == Axis2_Y)
 {
  UI_PushSemanticHeight(Size);
 }
}

internal void
UI_PopSemanticSizeOnAxis(axis2 Axis)
{
 if(0) {}
 else if(Axis == Axis2_X)
 {
  UI_PopSemanticWidth();
 }
 else if(Axis == Axis2_Y)
 {
  UI_PopSemanticHeight();
 }
}

#define UI_BackgroundColor(Value) DeferLoop(UI_PushBackgroundColor(Value), UI_PopBackgroundColor())
#define UI_TextColor(Value) DeferLoop(UI_PushTextColor(Value), UI_PopTextColor())
#define UI_BorderColor(Value) DeferLoop(UI_PushBorderColor(Value), UI_PopBorderColor())
#define UI_BorderThickness(Value) DeferLoop(UI_PushBorderThickness(Value), UI_PopBorderThickness())
#define UI_Softness(Value) DeferLoop(UI_PushSoftness(Value), UI_PopSoftness())
#define UI_CornerRadii(Value) DeferLoop(UI_PushCornerRadii(Value), UI_PopCornerRadii())
#define UI_LayoutAxis(Value) DeferLoop(UI_PushLayoutAxis(Value), UI_PopLayoutAxis())
#define UI_SemanticWidth(Value) DeferLoop(UI_PushSemanticWidth(Value), UI_PopSemanticWidth())
#define UI_SemanticHeight(Value) DeferLoop(UI_PushSemanticHeight(Value), UI_PopSemanticHeight())
#define UI_HeightPx(Value) DeferLoop(UI_PushHeightPx(Value), UI_PopHeightPx())

#define UI_SemanticSizeOnAxis(Axis, Size) \
DeferLoop(UI_PushSemanticSizeOnAxis(Axis, Size), UI_PopSemanticSizeOnAxis(Axis))

#define UI_FontKind(Value) DeferLoop(UI_PushFontKind(Value), UI_PopFontKind())

#define UI_SemanticFull() \
UI_SemanticHeight(UI_SizeParent(1.f, 1.f)) \
UI_SemanticWidth(UI_SizeParent(1.f, 1.f))

#define UI_FillOnAxis(Axis) UI_SemanticSizeOnAxis(Axis, UI_SizeParent(1.f, 0.f))