/* date = February 23rd 2026 6:09 pm */

#ifndef MUZE_UI_H
#define MUZE_UI_H

//~ Types
enum axis2
{
    Axis2_X,
    Axis2_Y,
    Axis2_Count
};
typedef enum axis2 axis2;

enum ui_size_kind
{
    UI_SizeKind_Null,
    UI_SizeKind_Pixels,
    UI_SizeKind_TextContent,
    UI_SizeKind_PercentOfParent,
    UI_SizeKind_ChildrenSum,
};
typedef enum ui_size_kind ui_size_kind;

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
    ui_key Key;
    u64 LastTouchedFrameIdx;
    
    // TODO(luca): Metaprogram
    s32 Flags;
    ui_size SemanticSize[Axis2_Count];
    str8 DisplayString;
    f32 BorderThickness;
    f32 Softness;
    v4 BackgroundColor;
    v4 BorderColor;
    v4 TextColor;
    v4 CornerRadii;
    axis2 LayoutAxis;
    ui_custom_draw *CustomDraw; 
    void *CustomDrawData;
    f32 HeightPx;
    font_kind FontKind;
    
    f32 tHot;
    f32 tActive;
    
    // Produced from layout resolving
    v2 FixedPosition;
    v2 FixedSize;
    
    // Computed per build
    b32 Clicked;
    v4 Rec;
};
#define UI_EachBox(Node, First) \
(ui_box *Node = First; !UI_IsNilBox(Node); Node = Node->Next)  
#define UI_EachHashBox(Node, First) \
(ui_box *Node = First; !UI_IsNilBox(Node); Node = Node->HashNext)

//- Stack nodes 
// TODO(luca): Metaprogram
typedef struct b32_stack_node b32_stack_node;
struct b32_stack_node
{
    b32_stack_node *Prev;
    b32 Value;
};

typedef struct f32_stack_node f32_stack_node;
struct f32_stack_node
{
    f32_stack_node *Prev;
    f32 Value;
};

typedef struct ui_size_stack_node ui_size_stack_node;
struct ui_size_stack_node
{
    ui_size_stack_node *Prev;
    ui_size Value;
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

typedef struct font_kind_stack_node font_kind_stack_node;
struct font_kind_stack_node
{
    font_kind_stack_node *Prev;
    font_kind Value;
};

// TODO(luca): Metaprogram
#define UI_StateStacks \
v4_stack_node *BackgroundColorTop; \
v4_stack_node *BorderColorTop; \
v4_stack_node *TextColorTop; \
f32_stack_node *SoftnessTop; \
f32_stack_node *BorderThicknessTop; \
v4_stack_node *CornerRadiiTop; \
axis2_stack_node *LayoutAxisTop; \
ui_size_stack_node *SemanticHeightTop; \
ui_size_stack_node *SemanticWidthTop; \
f32_stack_node *HeightPxTop; \
font_kind_stack_node *FontKindTop;

//-
typedef struct ui_state ui_state;
struct ui_state
{
    arena *Arena;
    u64 BoxTableSize;
    ui_box *BoxTable;
    
    ui_key Active;
    ui_key Hot;
    
    // Per build information
    app_input *Input;
    font_atlas *Atlas;
    u64 FrameIdx;
    
    b32 AppendToParent;
    ui_box *Current;
    ui_box *Root;
    struct
    {
        UI_StateStacks
    };
};

//~ Globals
global_variable ui_state *UI_State = 0;
global_variable arena *UI_FrameArena = 0;

global_variable ui_box *UI_NilBox = 0;

global_variable v4 Color_Background = {U32ToV4Arg(0xff4c566a)};
global_variable v4 Color_Foreground = {U32ToV4Arg(0xffeceff4)};
global_variable v4 Color_ButtonBorder = {U32ToV4Arg(0xff3b4252)};
global_variable v4 Color_ButtonBackground = {U32ToV4Arg(0xFF81A1C1)};
global_variable v4 Color_ButtonText = {U32ToV4Arg(0xff000000)};

#define UI_SizePx(Value, Strictness) UI_Size(UI_SizeKind_Pixels, Value, Strictness)
#define UI_SizeText(Value, Strictness) UI_Size(UI_SizeKind_TextContent, Value, Strictness)
#define UI_SizeEm(_Value, Strictness) UI_Size(UI_SizeKind_Pixels, ((_Value)*UI_State->HeightPxTop->Value), Strictness)
#define UI_SizeParent(Value, Strictness) UI_Size(UI_SizeKind_PercentOfParent, Value, Strictness)
#define UI_SizeChildren(Strictness) UI_Size(UI_SizeKind_ChildrenSum, 0.f, Strictness)

// TODO(luca): Freelist?
#define StackPush(Arena, t, PushValue, Top) \
t *Push = PushStructZero((Arena), t); \
Push->Value = (PushValue); \
Push->Prev = (Top); \
Top = Push;

// NOTE(luca): This only works if there is a possible nil value 
#define StackPop(Top) \
((Top)->Value, (Top = Top->Prev)->Value)

#define UI_StackPush(t, Name) StackPush(UI_FrameArena, t##_stack_node, Name, UI_State->Name##Top)
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

internal void UI_PushHeightPx(f32 HeightPx) { UI_StackPush(f32, HeightPx); }
internal void UI_PopHeightPx()              { UI_StackPop(HeightPx); }

internal void UI_PushFontKind(font_kind FontKind) { UI_StackPush(font_kind, FontKind); }
internal void UI_PopFontKind()                    { UI_StackPop(FontKind); }

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

#define UI_FontKind(Value) DeferLoop(UI_PushFontKind(Value), UI_PopFontKind())

#define UI_SemanticFull() \
UI_SemanticHeight(UI_SizeParent(1.f, 1.f)) \
UI_SemanticWidth(UI_SizeParent(1.f, 1.f))

#define UI_

#endif //MUZE_UI_H