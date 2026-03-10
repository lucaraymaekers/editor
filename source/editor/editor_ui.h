/* date = February 23rd 2026 6:09 pm */

#ifndef EDITOR_UI_H
#define EDITOR_UI_H
//~ Types

enum axis2
{
    Axis2_X,
    Axis2_Y,
    Axis2_Count
};
typedef enum axis2 axis2;

// TODO(luca): Metaprogram
enum ui_box_flag
{
    UI_BoxFlag_None                   = 0,
    UI_BoxFlag_DrawBackground         = (1 << 0),
    UI_BoxFlag_DrawBorders            = (1 << 1),
    UI_BoxFlag_DrawDebugBorder        = (1 << 2),
    UI_BoxFlag_DrawShadow             = (1 << 3),
    UI_BoxFlag_DrawDisplayString      = (1 << 4),
    UI_BoxFlag_CenterTextHorizontally = (1 << 5),
    UI_BoxFlag_CenterTextVertically   = (1 << 6),
    UI_BoxFlag_MouseClickability      = (1 << 7),
    UI_BoxFlag_Clip                   = (1 << 8),
    UI_BoxFlag_TextWrap               = (1 << 9),
    UI_BoxFlag_DrawCursor             = (1 << 10),
};
typedef enum ui_box_flag ui_box_flag;

enum ui_size_kind
{
    UI_SizeKind_Null,
    UI_SizeKind_Pixels,
    UI_SizeKind_TextContent,
    UI_SizeKind_PercentOfParent,
    UI_SizeKind_ChildrenSum,
};
typedef enum ui_size_kind ui_size_kind;

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
    ui_key Key;
    u64 LastTouchedFrameIndex;
    
    u32 Flags;
    ui_size SemanticSize[Axis2_Count];
    
    v2 FixedPosition;
    v2 FixedSize;
    
    str8 DisplayString;
    
    b32 Hovered;
    b32 Clicked;
    b32 Pressed;
    rect Rec;
    
    f32 BorderThickness;
    f32 Softness;
    v4 BackgroundColor;
    v4 BorderColor;
    v4 TextColor;
    v4 CornerRadii;
    axis2 LayoutAxis;
};

#define UI_EachBox(Node, First) (ui_box *Node = First; !UI_IsNilBox(Node); Node = Node->Next)  

//- Stack nodes 
typedef struct b32_stack_node b32_stack_node;
struct b32_stack_node
{
    b32_stack_node *Prev;
    b32 Value;
};

typedef struct ui_size_stack_node ui_size_stack_node;
struct ui_size_stack_node
{
    ui_size_stack_node *Prev;
    ui_size Value;
};

typedef struct f32_stack_node f32_stack_node;
struct f32_stack_node
{
    f32_stack_node *Prev;
    f32 Value;
};

typedef struct v4_stack_node v4_stack_node;
struct v4_stack_node
{
    v4_stack_node *Prev;
    v4 Value;
};

typedef struct axis2_stack_node axis2_stack_node;
struct axis2_stack_node
{
    axis2_stack_node *Prev;
    axis2 Value;
};

//-

typedef struct ui_state ui_state;
struct ui_state
{
    ui_box *Root;
    ui_box *Current;
    b32 AppendToParent;
    
    app_input *Input;
    font_atlas *Atlas;
    u64 FrameIndex;
    
    v4_stack_node *BackgroundColorTop;
    v4_stack_node *BorderColorTop;
    v4_stack_node *TextColorTop;
    f32_stack_node *SoftnessTop;
    f32_stack_node *BorderThicknessTop;
    v4_stack_node *CornerRadiiTop;
    axis2_stack_node *LayoutAxisTop;
    ui_size_stack_node *SemanticHeightTop;
    ui_size_stack_node *SemanticWidthTop;
    
    b32 RectDebugMode;
};

//~ Globals
global_variable arena *UI_BoxArena = 0;
global_variable ui_box *UI_BoxTable = 0;
global_variable u64 UI_BoxTableSize = 0;

global_variable ui_state *UI_State = 0;

read_only global_variable ui_box _UI_NilBox =
{
    &_UI_NilBox, 
    &_UI_NilBox,
    &_UI_NilBox,
    &_UI_NilBox,
    &_UI_NilBox,
    &_UI_NilBox,
    &_UI_NilBox,
};

global_variable ui_box *UI_NilBox = &_UI_NilBox;

// TODO(luca): Freelist?
#define StackPush(Arena, t, PushValue, Top) \
t *Push = PushStruct((Arena), t); \
Push->Value = (PushValue); \
Push->Prev = (Top); \
Top = Push;

#define StackPop(Top) \
((Top)->Value, (Top = Top->Prev)->Value)

#define UI_StackPush(t, Name) StackPush(FrameArena, t##_stack_node, Name, UI_State->Name##Top)
#define UI_StackPop(Name) StackPop(UI_State->Name##Top)

internal void UI_PushBackgroundColor(v4 BackgroundColor)  { UI_StackPush(v4, BackgroundColor); }
internal void UI_PopBackgroundColor()                     { UI_StackPop(BackgroundColor); }

internal void UI_PushTextColor(v4 TextColor)              { UI_StackPush(v4, TextColor); }
internal void UI_PopTextColor()                           { UI_StackPop(TextColor); }

internal void UI_PushBorderColor(v4 BorderColor)          { UI_StackPush(v4, BorderColor); }
internal void UI_PopBorderColor()                         { UI_StackPop(BorderColor); }

internal void UI_PushBorderThickness(f32 BorderThickness) { UI_StackPush(f32, BorderThickness); }
internal void UI_PopBorderThickness()                     { UI_StackPop(BorderThickness); }

internal void UI_PushSoftness(f32 Softness)               { UI_StackPush(f32, Softness); }
internal void UI_PopSoftness()                            { UI_StackPop(Softness); }

internal void UI_PushCornerRadii(v4 CornerRadii)          { UI_StackPush(v4, CornerRadii); }
internal void UI_PopCornerRadii()                         { UI_StackPop(CornerRadii); }

internal void UI_PushLayoutAxis(axis2 LayoutAxis)         { UI_StackPush(axis2, LayoutAxis); }
internal void UI_PopLayoutAxis()                          { UI_StackPop(LayoutAxis); }

internal void UI_PushSemanticWidth(ui_size SemanticWidth) { UI_StackPush(ui_size, SemanticWidth); }
internal void UI_PopSemanticWidth() { UI_StackPop(SemanticWidth); }

internal void UI_PushSemanticHeight(ui_size SemanticHeight) { UI_StackPush(ui_size, SemanticHeight); }
internal void UI_PopSemanticHeight()                        { UI_StackPop(SemanticHeight); }

#define UI_BackgroundColor(Value) DeferLoop(UI_PushBackgroundColor(Value), UI_PopBackgroundColor())
#define UI_BackgroundColor(Value) DeferLoop(UI_PushBackgroundColor(Value), UI_PopBackgroundColor())
#define UI_TextColor(Value) DeferLoop(UI_PushTextColor(Value), UI_PopTextColor())
#define UI_BorderColor(Value) DeferLoop(UI_PushBorderColor(Value), UI_PopBorderColor())
#define UI_BorderThickness(Value) DeferLoop(UI_PushBorderThickness(Value), UI_PopBorderThickness())
#define UI_Softness(Value) DeferLoop(UI_PushSoftness(Value), UI_PopSoftness())
#define UI_CornerRadii(Value) DeferLoop(UI_PushCornerRadii(Value), UI_PopCornerRadii())
#define UI_LayoutAxis(Value) DeferLoop(UI_PushLayoutAxis(Value), UI_PopLayoutAxis())
#define UI_Softness(Value) DeferLoop(UI_PushSoftness(Value), UI_PopSoftness())
#define UI_SemanticHeight(Value) DeferLoop(UI_PushSemanticHeight(Value), UI_PopSemanticHeight())
#define UI_SemanticWidth(Value) DeferLoop(UI_PushSemanticWidth(Value), UI_PopSemanticWidth())

#endif //EDITOR_UI_H