#define BASE_EXTERNAL_LIBS 1
#define BASE_NO_ENTRYPOINT 1
#include "base/base.h"
#include "base/base.c"
#include "rl/generated/everything.c"
#include "rl/rl_platform.h"
#include "rl/rl_libs.h"
#include "rl/rl_font.h"
#include "rl/rl_random.h"
#include "rl/rl_gl.h"
#include "rl/rl_renderer.h"
#include "rl/rl_ui.h"
#include "editor/editor_app.h"
NO_WARNINGS_BEGIN
#include "editor/editor_lexer.h"
#include "editor/editor_parser.h"
#include "editor/editor_lexer.c"
#include "editor/editor_parser.c"
NO_WARNINGS_END
#include "rl/rl_renderer.c"
#include "rl/rl_ui.c"
#include "rl/rl_widgets.c"

//- Globals
global_variable panel *NilPanel = 0;

internal b32
IsNilPanel(panel *Panel)
{
    b32 Result = (!Panel || Panel == NilPanel);
    return Result;
}

global_variable arena *PanelArena = 0;
global_variable b32 PanelAppendToParent = false;
global_variable panel *PanelCurrent = 0;
global_variable app_state *PanelApp = 0;
global_variable app_input *PanelInput = 0;
global_variable panel *PanelDragging = 0;

global_variable s32 PanelDebugIndentation = 0;

global_variable axis2_stack_node *PanelAxisTop = 0; 

//~ Text editing 
internal void
DeleteChar(app_text *Text)
{
    if(Text->Cursor)
    {
        MemoryCopy(Text->Data + (Text->Cursor - 1),
                   Text->Data + Text->Cursor,
                   sizeof(rune)*((Text->Count - Text->Cursor) + 1));
        
        Text->Count -= 1;
        Text->Cursor -= 1;
    }
}

internal range_u64
GetSelection(app_text *Text)
{
    range_u64 Range = {0};
    
    u64 Start = Text->Cursor;
    u64 End = Text->Trail;
    if(Start > End) Swap(Start, End);
    
    Range.Min = Start;
    Range.Max = End;
    
    return Range;
};

internal void
SaveTextToFile(app_text *Text, str8 FileName)
{
    str8 Source = PushS8(FrameArena, Text->Count);
    for EachIndex(Idx, Source.Size)
    {
        Source.Data[Idx] = (u8)Text->Data[Idx];
    }
    char *Path = PathFromExe(FrameArena, FileName);
    OS_WriteEntireFile(Path, Source);
}

internal void
LoadFileToText(app_text *Text, str8 FileName)
{
    char *Path = PathFromExe(FrameArena, FileName);
    str8 Source = OS_ReadEntireFileIntoMemory(Path);
    if(Source.Size)
    {
        Text->PrevCursor = Text->Cursor = Text->Trail = 0;
        Text->Count = Source.Size;
        Text->CurRelLine = 0;
        for EachIndex(Idx, Source.Size)
        {
            Text->Data[Idx] = Source.Data[Idx];
        }
        OS_FreeFileMemory(Source);
    }
}

internal void
CopySelection(app_text *Text, app_input *Input)
{
    range_u64 Selection = GetSelection(Text);
    u64 Size = GetRangeU64Count(Selection);
    
    Input->PlatformSetClipboard = true;
    
    Input->PlatformClipboard.Size = Size;;
    
    for EachIndex(Idx, Size)
    {
        Input->PlatformClipboard.Data[Idx] = (u8)Text->Data[Selection.Min + Idx];
    }
    
}

internal void
UpdateCursorRelLine(app_text *Text)
{
    if(Text->PrevCursor != Text->Cursor)
    {    
        // TODO(luca): Merge for loops
        if(Text->PrevCursor < Text->Cursor)
        {        
            for(u64 Idx = Text->PrevCursor; Idx < Text->Cursor; Idx += 1)
            {
                if(Text->Data[Idx] == '\n')
                {
                    // TODO(luca): What when text->lines = 0
                    Text->CurRelLine += !!(Text->CurRelLine < Text->Lines - 1);
                }
            }
        }
        else
        {
            for(u64 Idx = Text->PrevCursor - 1; Idx >= Text->Cursor; Idx -= 1)
            {
                if(Text->Data[Idx] == '\n')
                {
                    Text->CurRelLine -= !!(Text->CurRelLine > 0);
                }
                if(Idx == 0)
                {
                    break;
                }
            }
        }
    }
    
    Text->PrevCursor = Text->Cursor;
}

internal void
DeleteSelection(app_text *Text)
{
    range_u64 Selection = GetSelection(Text);
    u64 SelectionSize = GetRangeU64Count(Selection);
    
    u64 CharsAfterEnd = Text->Count - Selection.Max;
    
    Text->Cursor = Selection.Min;
    
    // NOTE(luca): We need to update CurRelLine before the text is altered.
    UpdateCursorRelLine(Text);
    
    MemoryCopy(Text->Data + Selection.Min, Text->Data + Selection.Max,
               sizeof(rune)*(CharsAfterEnd));
    
    Text->Count -= SelectionSize;
    Text->CursorAnimTime = 0.f;
    
    Text->PrevCursor = Text->Cursor;
}

internal void

AppendChar(app_text *Text, rune Codepoint)
{
    MemoryCopy(Text->Data + (Text->Cursor + 1),
               Text->Data + (Text->Cursor),
               sizeof(rune)*((Text->Count - Text->Cursor) + 1));
    
    Text->Data[Text->Cursor] = Codepoint;
    
    Text->Count += 1;
    Text->Cursor += 1;
    Text->CursorAnimTime = 0.f;
    Text->Trail = Text->Cursor;
    
    Assert(Text->Count < Text->Capacity);
}

internal void
MoveRight(app_text *Text)
{
    Text->Cursor += (Text->Cursor < Text->Count);
    Text->CursorAnimTime = 0.f;
}

internal void
MoveLeft(app_text *Text)
{
    Text->Cursor -= (Text->Cursor > 0);
    Text->CursorAnimTime = 0.f;
}

internal void
MoveTrail(app_text *Text, b32 Shift)
{
    if(!Shift)
    {
        Text->Trail = Text->Cursor;
        Text->CursorAnimTime = 0.f;
    }
}

internal void
MoveDown(app_text *Text, b32 Shift)
{
    u64 Next = Text->Cursor + !!(Text->Cursor < Text->Count);
    while(Next < Text->Count && Text->Data[Next] != '\n') Next += 1;
    
    // NOTE(luca): If the text cursor was on the next new line we search before it
    u64 Begin = Text->Cursor - !!(Text->Cursor > 0 && Text->Cursor == Next);
    while(Begin > 0 && Text->Data[Begin] != '\n') Begin -= 1;
    
    u64 End = Next + !!(Next < Text->Count);
    while(End < Text->Count && Text->Data[End] != '\n') End += 1;
    
    u64 ColumnPos = (Text->Cursor - Begin);
    u64 NewPos = Next + ColumnPos;
    
    if(Text->Data[Begin] != '\n') NewPos += 1;
    
    if(Next == Text->Count)
    {
        NewPos = Text->Count;
    }
    
    Text->Cursor = Min(NewPos, End);
    
    Text->CursorAnimTime = 0.f;
    
    MoveTrail(Text, Shift);
}

internal void
MoveUp(app_text *Text, b32 Shift)
{
    u64 End = Text->Cursor - !!(Text->Cursor > 0);
    
    while(End > 0 && Text->Data[End] != '\n') End -= 1;
    
    u64 Begin = End - !!(End > 0);
    
    while(Begin > 0 && Text->Data[Begin] != '\n') Begin -= 1;
    
    u64 ColumnPos = (Text->Cursor - End);
    u64 NewPos = Begin + ColumnPos;
    
    if(Text->Data[Begin] != '\n') NewPos -= 1;
    
    // NOTE(luca): Special case, we go to the beginning of the line.
    if(End == 0) NewPos = 0;
    
    // NOTE(luca): If the cursor would end up after the newline clamp it to the end of it. 
    Text->Cursor = Min(NewPos, End);
    
    MoveTrail(Text, Shift);
}

internal void
DeleteWordLeft(app_text *Text)
{
    while(Text->Cursor > 0 && 
          IsWhiteSpace((u8)Text->Data[Text->Cursor - 1]))
    {
        DeleteChar(Text);
    }
    
    while(Text->Cursor > 0 && 
          !IsWhiteSpace((u8)Text->Data[Text->Cursor - 1]))
    {
        DeleteChar(Text);
    }
}

//~ Misc
typedef struct u64_array u64_array;
struct u64_array
{
    u64 Count;
    u64 *V;
};

internal u64_array
GetWrapPositions(arena *Arena, str8 Text, font_atlas *Atlas, f32 MaxWidth)
{
    u64_array Result = {0};
    
    // 1px per char
    u64 MaxChars = (u64)MaxWidth;
    
    Result.V = PushArray(Arena, u64, MaxChars);
    
    f32 X = 0.f;
    for EachIndex(Idx, Text.Size)
    {
        u8 Char = Text.Data[Idx];
        f32 CharWidth = Atlas->PackedChars[Char].xadvance;
        
        if(Char == '\n' || (X + CharWidth > MaxWidth))
        {
            Result.V[Result.Count] = Idx;
            Result.Count += 1;
            X = 0.f;
        }
        else
        {            
            X += CharWidth;
        }
    }
    
    return Result;
}

//~ Text rendering 

typedef struct custom_text_draw_params custom_text_draw_params;
struct custom_text_draw_params
{
    ui_box *Box;
    app_text *Text;
};

UI_CUSTOM_DRAW(TextComputeAndDraw)
{
    custom_text_draw_params *Params = (custom_text_draw_params *)CustomDrawData;
    ui_box *Box = Params->Box;
    app_text *Text = Params->Text;
    
    ui_box *Parent = Box->Parent;
    v4 Dest = Box->Rec;
    
    // TODO(luca): This box does not set the active/hot element and should.
    
    // TODO(luca): Account for padding instead
    v4 TextDim = RectShrink(Dest, Box->BorderThickness);
    v2 CursorPos = {0};
    f32 CharHeight = (UI_State->Atlas->HeightPx);
    v2 Pen = TextDim.Min;
    
    f32 TextRectHeight = (TextDim.Max.Y - TextDim.Min.Y);
    if(TextRectHeight > 0.f)
    {    
        u64 NewTextLines = (u64)floorf((TextDim.Max.Y - TextDim.Min.Y)/CharHeight);
        if(NewTextLines < Text->Lines)
        {
            // NOTE(luca): If we are on the last line and shrinking, scroll up
            if(Text->CurRelLine == Text->Lines - 1)
            {
                Text->CurRelLine -= !!(Text->CurRelLine > 0);
            }
        }
        else if(NewTextLines > Text->Lines)
        {
            // TODO(luca): What we actually want is that if the last line is visible *and* there is text before that line then  scroll down, i.e., auto scrolling
#if 0
            if(Text->Cursor == Text->Count)
            {
                Text->CurRelLine += 1;
            }
#endif
        }
        
        Text->Lines = NewTextLines;
    }
    
    u64 StartOffset = 0;
    u64 BeginningOfLineIdx = 0;
    {
        for(u64 Idx = Text->Cursor - !!(Text->Cursor > 0); Idx > 0; Idx -= 1)
        {
            if(Text->Data[Idx] == '\n')
            {
                BeginningOfLineIdx = Idx;
                break;
            }
        }
        
        u64 LineOffsetsCount = Text->CurRelLine + 1;
        u64 *LineOffsets = PushArrayZero(FrameArena, u64, LineOffsetsCount);
        
        u64 WriteHead = 0;
        
        u64 LineFromCursor = 0;
        
        for(u64 Idx = BeginningOfLineIdx; Idx > 0; Idx -= 1)
        {
            if(Text->Data[Idx] == '\n')
            {
                LineFromCursor += 1;
                
                // Update offsets
                {                
                    LineOffsets[WriteHead] = Idx + 1;
                    WriteHead += 1;
                    
                    if(LineFromCursor > Text->CurRelLine)
                    {
                        break;
                    }
                }
            }
        }
        
        StartOffset = LineOffsets[Text->CurRelLine];
        if(0)
        {
            Log("BOL: %lu, Line: %lu, Offset: %lu\n", BeginningOfLineIdx, LineFromCursor, StartOffset);
        }
    }
    
    Pen = TextDim.Min;
    for(u64 Idx = StartOffset; Idx < Text->Count; Idx += 1)
    {
        rune Char = Text->Data[Idx];
        f32 CharWidth = (UI_State->Atlas->PackedChars[Char].xadvance);
        
        v4 CharColor = Box->TextColor;
        rune DrawChar = Char;
        
        // TODO(luca): Proper wrapping on whitespace
        if(Pen.X + CharWidth > TextDim.Max.X)
        {
            Pen.X = TextDim.Min.X;
            Pen.Y += CharHeight;
        }
        
        v2 PenMax = V2AddV2(Pen, V2(CharWidth, CharHeight));
        if(IsInsideRectV2(Pen, TextDim) &&
           IsInsideRectV2(PenMax, TextDim))
        {
            b32 IsInSelection = false;
            
            v4 CharRect = RectFromSize(Pen, V2(CharWidth, Box->HeightPx));
            
            range_u64 Selection = GetSelection(Text);
            IsInSelection = (Idx >= Selection.Min && Idx < Selection.Max); 
            
            if(IsInSelection)
            {
                v4 SelectionColor = Color_Blue;
                SelectionColor.A = .5f;
                DrawRect(CharRect, SelectionColor, 0.f, 0.f, 0.f);
            }
            
            
            app_input *Input = UI_State->Input;
            if(!Input->Consumed)
            {                
                v2 MouseP = V2S32(Input->Mouse.X, Input->Mouse.Y);
                
                if(IsInsideRectV2(MouseP, CharRect))
                {
                    b32 OnRightSide = !!(((CharRect.Max.X - MouseP.X)/CharWidth) < .5f); 
                    
                    app_button_state LeftButton = Input->Mouse.Buttons[PlatformMouseButton_Left];
                    b32 Shift = (LeftButton.Modifiers & PlatformKeyModifier_Shift);
                    
                    if(LeftButton.EndedDown)
                    {
                        if(UI_IsActive(UI_NilBox))
                        {
                            Text->Trail = Idx + (u64)OnRightSide;
                            if(WasPressed(LeftButton))
                            {
                                if(!Shift)
                                {
                                    Text->Cursor = Text->Trail;
                                    Text->CursorAnimTime = 0.f;
                                }
                            }
                            
                            Input->Consumed = true;
                        }
                    }
                    
                    if(0)
                    {
                        DrawRect(CharRect, Color_Red, 0.f, 1.f, 0.f); 
                    }
                    
                    if(0 && Idx == BeginningOfLineIdx)
                    {
                        DrawRect(CharRect, Color_Green, 0.f, 1.f, 0.f);
                    }
                }
            }
            
            b32 VisualizeWhitespace = true;
            
            if(IsWhiteSpace((u8)Char) && VisualizeWhitespace)
            {
                if(Char == L' ')       DrawChar = L'\u00B7'; // · Middle Dot
                if(Char == L'\t')      DrawChar = L'\u2192'; // → Rightwards Arrow
                if(Char == L'\n')      DrawChar = L'\u0024'; // $ Dollar Sign
                CharColor.A *= 0.2f;
            }
            
            // TODO(luca): Assert that DrawChar is in Atlas
            DrawRectChar(UI_State->Atlas, Pen, DrawChar, CharColor);
            
            Pen.X += CharWidth;
        }
        
        if(Char == '\n')
        {
            Pen.X = TextDim.Min.X;
            Pen.Y += CharHeight;
        }
        
        if(Idx + 1 == Text->Cursor)
        {
            CursorPos = Pen;
            
            // NOTE(luca): When at the end of the line, draw cursor on the next.
            if(CursorPos.X + CharWidth > TextDim.Max.X)
            {
                CursorPos.X = TextDim.Min.X;
                CursorPos.Y += CharHeight;
            }
        }
        
    }
    
    // Draw the cursor
    {        
        if(Text->Trail == Text->Cursor)
        {            
            if(Text->Cursor == StartOffset)
            {
                CursorPos = TextDim.Min;
            }
            
            v4 CursorColor = Box->TextColor;
            
            f32 Ratio = 1000.f/1200.f;
            
            if(cos(Text->CursorAnimTime*Pi32*Ratio) < 0.f)
            {
                CursorColor.A = 0.f;
            }
            else
            {
                //DebugBreak();
            }
            
            v4 CurRec = RectFromSize(CursorPos, V2(1.f, CharHeight));
            
            CurRec = RectIntersect(CurRec, Parent->Rec);
            if(RectValid(CurRec)) 
            {
                DrawRect(CurRec, CursorColor, 0.f, 0.f, 0.f);
            }
        }
    }
}

//~ Panels 

internal inline f32 
SizeOnAxis(v4 Rec, s32 Axis)
{
    f32 Result = (Rec.Max.e[Axis] - Rec.Min.e[Axis]);
    return Result;
}

internal panel *
PanelAlloc(arena *Arena)
{
    panel *New = PushStructZero(Arena, panel);
    New->First = New->Last = New->Next = New->Prev = New->Parent = NilPanel;
    return New;
}

internal void PanelPush() { PanelAppendToParent = true; }
internal void PanelPop(void) { PanelCurrent = PanelCurrent->Parent; }
internal void PanelPushAxis(axis2 Axis) { StackPush(PanelArena, axis2_stack_node, Axis, PanelAxisTop); }
internal void PanelPopAxis() 
{
    if(PanelAxisTop) PanelAxisTop = PanelAxisTop->Prev;
}

internal panel *
PanelAdd_(arena *Arena, axis2 Axis, panel *Current, b32 AppendToParent, f32 ParentPct)
{
    panel *New = PanelAlloc(Arena);
    
    New->First = New->Last = New->Next = New->Prev = New->Parent = NilPanel;
    New->Root = UI_NilBox;
    
    New->ParentPct = ParentPct;
    New->Axis = (s32)Axis;
    
    if(!IsNilPanel(Current))
    {            
        if(AppendToParent)
        {
            Current->First = New;
            New->Parent = Current;
        }
        else
        {
            Current->Next = New;
            New->Prev = Current;
            New->Parent = Current->Parent;
        }
        
        New->Parent->Last = New;
    }
    
    PanelCurrent = New;
    PanelAppendToParent = false;
    
    return New;
}

#define PanelGroup() DeferLoop(PanelPush(), PanelPop())
#define PanelAxis(Axis) DeferLoop(PanelPushAxis(Axis), PanelPopAxis())
#define PanelAdd(ParentPct) PanelAdd_(PanelArena, PanelAxisTop->Value, PanelCurrent, PanelAppendToParent, ParentPct)

internal inline b32 
IsLeafPanel(panel *Panel)
{
    b32 Result = IsNilPanel(Panel->First);
    return Result;
}

internal panel *
SplitPanel(arena *Arena, panel *To, s32 Axis, b32 Backwards)
{
    panel *Result = To;
    
    panel *New = PanelAlloc(Arena);
    panel *Parent = To->Parent;
    
    if(!IsNilPanel(To))
    {
        // NOTE(luca): Must be a leaf node.
        Assert(IsLeafPanel(To));
        
        if(Axis == Parent->Axis)
        {
            //The new child's ParentPct becomes 1/n where n is the children count
            //Other children's ParentPct *= (1-1/n) 
            {        
                s32 ChildrenCount = 0;
                for EachChildPanel(Child, Parent)
                {
                    ChildrenCount += 1;
                }
                
                Assert(ChildrenCount > 0);
                // The new child
                ChildrenCount += 1;
                
                New->ParentPct = 1.f/(f32)ChildrenCount;;
                
                for EachChildPanel(Child, Parent)
                {
                    Child->ParentPct *= (1.f - New->ParentPct);
                }
            }
            
            New->Parent = Parent;
            
            if(!Backwards)
            {
                if(To == Parent->Last)
                {
                    Parent->Last = New;
                }
            }
            else
            {
                if(To == Parent->First)
                {
                    Parent->First = New;
                }
            }
            
            if(!Backwards)
            {    
                New->Next = To->Next;
                
                if(!IsNilPanel(To->Next)) To->Next->Prev = New;
                
                To->Next = New;
                New->Prev = To;
            }
            else
            {
                New->Next = To;
                
                if(!IsNilPanel(To->Prev)) To->Prev->Next = New;
                
                New->Prev = To->Prev;
                To->Prev = New;
            }
            
            Result = New;
        }
        else
        {
            //1. Create NewParent replacing Parent
            //  - update To siblings and Parent child links
            //2. To becomes child of NewParent
            //3. Split To
            
            panel *NewParent = New;
            NewParent->Axis = Axis;
            NewParent->ParentPct = To->ParentPct;
            NewParent->Parent = Parent;
            
            //1.
            if(To == Parent->First)
            {
                Parent->First = NewParent;
            }
            
            if(To == Parent->Last)
            {
                Parent->Last = NewParent;
            }
            
            if(!IsNilPanel(To->Prev))
            {
                To->Prev->Next = NewParent;
                NewParent->Prev = To->Prev;
            }
            
            if(!IsNilPanel(To->Next))
            {
                To->Next->Prev = NewParent;
                NewParent->Next = To->Next;
            }
            
            //2. 
            To->Parent = NewParent;
            NewParent->First = To;
            NewParent->Last = To;
            To->Next = To->Prev = NilPanel;
            
            //3. 
            To->ParentPct = 1.f;
            Result = SplitPanel(Arena, To, Axis, Backwards); 
        }
    }
    
    return Result;
}

internal void
PanelDebugPrint(panel *Panel)
{
    Log("%*s %.2f %u:\n"
        "%*s  (%.0f,%.0f)\n"
        "%*s  (%.0f,%.0f)\n",
        PanelDebugIndentation, "", Panel->ParentPct, Panel->Axis,
        PanelDebugIndentation, "", V2Arg(Panel->Region.Min),
        PanelDebugIndentation, "", V2Arg(Panel->Region.Max));
    
    if(!IsNilPanel(Panel->First))
    {
        PanelDebugIndentation += 1;
        PanelDebugPrint(Panel->First);
        PanelDebugIndentation -= 1;
    }
    
    if(!IsNilPanel(Panel->Next))
    {
        PanelDebugPrint(Panel->Next);
    }
}

internal panel *
PanelNextLeaf(panel *Start, b32 Backwards)
{
    panel *Result = Start;
    // NOTE(luca): If nothing was found we will return the starting point
    
    b32 IsLeaf = false;
    
    panel *Search = Start;
    
    while(!IsLeaf && !IsNilPanel(Search))
    {
        panel *Next = (Backwards ? Search->Prev : Search->Next);
        
        if(!IsNilPanel(Next) || IsNilPanel(Search->Parent))
        {
            if(!IsNilPanel(Next))
            {            
                Search = Next;
                
                IsLeaf = IsLeafPanel(Search);
            }
            
            while(!IsLeaf)
            {
                Search = (Backwards ? Search->Last : Search->First);
                
                IsLeaf = IsLeafPanel(Search);
            }
        }
        else
        {
            Search = Search->Parent;
        }
    }
    
    if(IsLeaf)
    { 
        if(IsNilPanel(Search))
        {
            Result = Start;
        }
        else
        {
            Result = Search;
        }
    }
    
    return Result;
}

// NOTE(luca): Returns the panel that absorbed its size
internal panel *
ClosePanel(app_state *App, panel *Panel)
{
    // NOTE(luca): The panel which will take over the side of the deleted one.
    panel *Collapse = NilPanel;
    
    if(!Panel->CannotClose)
    {    
        panel *Parent = Panel->Parent;
        
        if(!IsNilPanel(Panel))
        {        
            if(Parent->First == Panel)
            {
                Parent->First = Panel->Next;
            }
            
            if(Parent->Last == Panel)
            {
                Parent->Last = Panel->Prev;
            }
            
            if(!IsNilPanel(Panel->Next)) 
            {
                Panel->Next->Prev = Panel->Prev;
                
                Collapse = Panel->Next;
            }
            
            if(!IsNilPanel(Panel->Prev)) 
            {
                Panel->Prev->Next = Panel->Next;
                
                Collapse = Panel->Prev;
            }
            
            if(!IsNilPanel(Collapse))
            {
                Collapse->ParentPct += Panel->ParentPct;
                
                // TODO(luca): If this collapsed into 100%, we should collapse it with the parent and overwrite the parent's Axis with ours, this will keep the tree compact.
            }
            else
            {
                // NOTE(luca): Last node of its parent, parent should get deleted.  End of the bloodline.
                Collapse = ClosePanel(App, Panel->Parent);
            }
            
            if(Panel == App->FirstPanel)
            {
                App->FirstPanel = NilPanel;
            }
            // TODO(luca): Push onto the free list
        }
        
        if(!IsLeafPanel(Collapse))
        {
            Collapse = PanelNextLeaf(Collapse, false);
        }
    }
    else
    {
        Collapse = Panel;
    }
    
    return Collapse;
}

internal void
PanelGetRegionAndInput(panel *Panel, v4 FreeRegion)
{
    panel *Parent = Panel->Parent;
    s32 Axis = Panel->Parent->Axis;
    s32 OtherAxis = 1 - Axis;
    f32 GapSize = 1.f;
    f32 PanelBorderSize = 2.f;
    
    v2 Pos = FreeRegion.Min;
    
    f32 ParentSize = (Parent->Region.Max.e[Axis] - Parent->Region.Min.e[Axis]);
    f32 OtherSize = (FreeRegion.Max.e[OtherAxis] - FreeRegion.Min.e[OtherAxis]);
    
    
    if(!IsNilPanel(Panel))
        {       
{
        v2 Size = {0};
        Size.e[Axis] = (!IsNilPanel(Parent) ?
                        (Panel->ParentPct*ParentSize) :
                        (FreeRegion.Max.e[Axis] - FreeRegion.Min.e[Axis]));
        Size.e[OtherAxis] = OtherSize;
        
        Panel->Region = RectFromSize(Pos, Size);
        }

        if(!IsNilPanel(Panel->First))
        {
            PanelGetRegionAndInput(Panel->First, Panel->Region);
        }
        
        if(!IsNilPanel(Panel->Next))
        {
            FreeRegion.Min.e[Axis] = Panel->Region.Max.e[Axis];
            PanelGetRegionAndInput(Panel->Next, FreeRegion);
        }
        
        app_input *Input = PanelInput;
        v2 MouseP = V2S32(Input->Mouse.X, Input->Mouse.Y);
        
        b32 MouseIsDown = false;
        b32 MouseWasPressed = false;
        b32 IsInsidePanel = false;
        
        if(!Input->Consumed)
        {    
            MouseIsDown = Input->Mouse.Buttons[PlatformMouseButton_Left].EndedDown;
            MouseWasPressed = WasPressed(Input->Mouse.Buttons[PlatformMouseButton_Left]);
            IsInsidePanel = IsInsideRectV2(MouseP, Panel->Region);
        }
        
        b32 HasResizeBorder = !IsNilPanel(Panel->Next); 
        
        v4 PanelBorderColor = Color_Blue;
        
        if(Panel == PanelApp->SelectedPanel)
        {
            PanelBorderColor = Color_Yellow;
        }
        
        // NOTE(luca): This happens post-order so that the borders are overlaid correctly.
        b32 HasContents = IsNilPanel(Panel->First);
        
        // draw panel rectangle
        if(HasContents)
        {
            if(IsInsidePanel && MouseWasPressed)
            {
                PanelApp->SelectedPanel = Panel;
            }
            
            Panel->Region = RectShrink(Panel->Region, GapSize);
            DrawRect(Panel->Region, PanelBorderColor, 0.f, PanelBorderSize, 0.f);
            
            Panel->Region = RectShrink(Panel->Region, PanelBorderSize);
        }
        
        if(HasResizeBorder)
        {
            v4 BorderColor = Color_Red;
            f32 BorderSize = 8.f;
            
            v4 Border = Panel->Region;
            
            Border.Max.e[Axis] += PanelBorderSize;
            
            Border.Min.e[Axis] = Border.Max.e[Axis];
            
            Border.Min.e[Axis] -= BorderSize; 
            Border.Max.e[Axis] += BorderSize;
            
            b32 Down = false;
            b32 Pressed = false;
            b32 IsInsideBorder = false;
            if(!Input->Consumed && (IsNilPanel(PanelDragging) || Panel == PanelDragging))
            {
                Pressed = WasPressed(Input->Mouse.Buttons[PlatformMouseButton_Left]);
                Down = Input->Mouse.Buttons[PlatformMouseButton_Left].EndedDown;
                IsInsideBorder = IsInsideRectV2(MouseP, Border);
                
                if(PanelDragging == Panel && !Down) 
                {
                    PanelDragging = NilPanel;
                }
            }
            
            if(IsInsideBorder)
            {
                Input->Consumed = true;
                Input->PlatformCursor = (PlatformCursorShape_ResizeHorizontal + Axis);
                
                BorderColor = Color_Blue;
                
                if(IsNilPanel(PanelDragging) && Pressed)
                {                
                    PanelDragging = Panel; 
                }
            }
            
            b32 IsDragging = (PanelDragging == Panel && Down); 
            if(IsDragging)
            {
                Input->Consumed = true;
                Input->PlatformCursor = (PlatformCursorShape_ResizeHorizontal + Axis);
                
                BorderColor = Color_Yellow;
                
                // Resize
                {
                    panel *Next = Panel->Next;
                    AssertMsg(!IsNilPanel(Next), "Panel should have a next panel since it has a resize border");
                    
                    f32 Size = SizeOnAxis(PanelApp->FirstPanel->Region, Axis);
                    f32 Pct = MouseP.e[Axis]/Size;
                    
                    // NOTE(luca): Will the calculations be wrong because we don't account for gap size?
                    // no it might create the illusion that we should not be resizing though, but I guess that's fine
                    
                    //1. Position of mouse in the region of the panel
                    //2. Subtra ct all Pcts before 
                    
                    //Dec->ParentPct -= Value;
                    
                    //
                    
                    f32 dPPct = Pct;
                    for(panel *Sibling = Panel; !IsNilPanel(Sibling); Sibling = Sibling->Prev)
                    {
                        dPPct -= Sibling->ParentPct;
                    }
                    
                    f32 AbsMin = .05f;
                    
                    panel *Inc = Panel;
                    panel *Dec = Next;
                    if(dPPct < 0.f)
                    {
                        Swap(Inc, Dec);
                        dPPct = -dPPct;
                    }
                    
                    if(Dec->ParentPct - dPPct >= AbsMin)
                    {                    
                        Inc->ParentPct += dPPct;
                        Dec->ParentPct -= dPPct;
                    }
                    
                }
                
            }
            
#if 0
            DrawRect(Border, BorderColor, 0.f, 0.f, 0.f);
#endif
        }
    }
}

internal panel_node *
IsTextPanel(app_state *App, panel *Panel)
{
    panel_node *Result = 0;
    for EachPanel(Node, App->TextPanels)
    {
        if(Node->Value == Panel)
        {
            Result = Node;
            break;
        }
    }
    
    return Result;
}

internal void
RemoveTextPanel(app_state *App, panel_node *Node)
{
    if(Node)
    {
        panel_node *PreviousNode = 0;
        for EachPanel(Previous, App->TextPanels)
        {
            if(Previous->Next == Node)
            {
                PreviousNode = Previous;
                break;
            }
        }
        
        if(PreviousNode)
        {
            PreviousNode->Next = Node->Next;
        }
        else
        {
            Assert(Node == App->TextPanels);
            App->TextPanels = App->TextPanels->Next;
        }
    }
}

//~

C_LINKAGE
UPDATE_AND_RENDER(UpdateAndRender)
{
    b32 ShouldQuit = false;
    
#if EDITOR_INTERNAL
    GlobalDebuggerIsAttached = Memory->IsDebuggerAttached;
#endif
#if OS_WINDOWS
    GlobalPerfCountFrequency = Memory->PerfCountFrequency;
#endif
    GlobalIsProfiling = Memory->IsProfiling;
    ThreadContext = Memory->ThreadCtx;
    ExeDirPath = Memory->ExeDirPath;
    
    Input->PlatformCursor = PlatformCursorShape_Arrow;
    
    OS_ProfileInit(" G");
    
    arena *PermanentArena;
    app_state *App;
    {
        // NOTE(luca): Pushes on the permanentarena may not zero the memory since that would clear the memory that was there before..
        
        PermanentArena = (arena *)Memory->Memory;
        PermanentArena->Size = Memory->MemorySize - sizeof(arena);
        PermanentArena->Base = (u8 *)Memory->Memory + sizeof(arena);
        PermanentArena->Pos = 0;
        AsanPoisonMemoryRegion(PermanentArena->Base, PermanentArena->Size);
        
        FrameArena = PushArena(PermanentArena, Memory->MemorySize/2, true);
        
        App = PushStruct(PermanentArena, app_state);
        
        App->TextArena = PushArena(PermanentArena, MB(64), false);
        App->FontAtlasArena = PushArena(PermanentArena, MB(150), false);
        
        App->UIArena = PushArena(PermanentArena, MB(64), false);
        
        PanelArena = App->PanelArena = PushArena(PermanentArena, ArenaAllocDefaultSize, false);
    }
    
    // NOTE(luca): Will be rerun when reloaded.
    local_persist s32 GLADVersion = 0;
    if(!Memory->Initialized || Memory->Reloaded)
    {
        GLADVersion = gladLoaderLoadGL();
        OS_ProfileAndPrint("glad Init");
    }
    
    OS_ProfileAndPrint("Init");
    
    f32 WindowBorderSize = 2.f;
    v2 BufferDim = V2S32(Buffer->Width, Buffer->Height);
    
    if(!Memory->Initialized)
    {
        char *FontPath = PathFromExe(FrameArena, S8("../data/font_regular.ttf"));
        InitFont(&App->Font, FontPath);
        App->PreviousHeightPx = DefaultHeightPx + 1.0f;
        App->HeightPx = DefaultHeightPx;
        App->FrameIdx = 0;
        
        // Nil read only structs 
        {        
            arena *Arena = ArenaAlloc(.Size = MB(1), .Offset = TB(3));
            
            ui_box *Box = PushStruct(Arena, ui_box);
            *Box = (ui_box){Box, Box, Box, Box, Box, Box, Box};
            
            panel *Panel = PushStruct(Arena, panel);
            *Panel = (panel){Panel, Panel, Panel, Panel, Panel};
            Panel->Root = Box;
            
            OS_MarkReadonly(Arena->Base, Arena->Size);
            
            NilPanel  = App->TrackerForNilPanel  = Panel;
            UI_NilBox = App->TrackerForUI_NilBox = Box;
        }
        
        RenderInit(&App->Render);
        
        // Panels
        { 
            // TODO(luca): Invert axis on group.
            PanelAxis(Axis2_X) PanelGroup()
            {
                App->FirstPanel = PanelAdd(1.f);
                
                PanelAxis(Axis2_Y) PanelGroup()
                {
                    PanelAdd(.4f);
                    PanelAdd(.6f);
                    if(IsEditorBuildInternal)
                    {
                        PanelAxis(Axis2_X) PanelGroup()
                        {
                            PanelAdd(.5f);
                            App->DebugPanel = PanelAdd(.5f);
                            App->DebugPanel->CannotClose = true;
                        }
                    }
                }
            }
        }
        
        App->SelectedPanel = PanelNextLeaf(App->FirstPanel, false);
        
        OS_ProfileAndPrint("Memory Init");
    }
    
    UI_NilBox = App->TrackerForUI_NilBox;
    NilPanel = App->TrackerForNilPanel;
    
    StringsScratch = FrameArena;
    
    PanelArena = App->PanelArena;
    PanelInput = Input;
    PanelApp = App;
    
    panel_node *TextPanel = IsTextPanel(App, App->SelectedPanel);
    if(TextPanel)
    {
        TextPanel->Text->PrevCursor = TextPanel->Text->Cursor;
    }
    
    for EachIndex(Idx, Input->Text.Count)
    {
        app_text_button Key = Input->Text.Buffer[Idx];
        
        b32 Control = Key.Modifiers & PlatformKeyModifier_Control;
        b32 Shift = Key.Modifiers & PlatformKeyModifier_Shift;
        b32 Alt = Key.Modifiers & PlatformKeyModifier_Alt;
        s32 ModifiersShiftIgnored = (Key.Modifiers & ~PlatformKeyModifier_Shift);
        b32 None = (Key.Modifiers == PlatformKeyModifier_None);
        
        if(!Key.IsSymbol)
        {
            if(Control)
            {
                switch(Key.Codepoint)
                {
                    case 'b': DebugBreak();  break;
                    
                    //- Panels 
                    case 'p':
                    {
                        if(!Shift)
                        {
                            App->SelectedPanel = SplitPanel(PanelArena, App->SelectedPanel, Axis2_X, false);
                        }
                        else
                        {
                            RemoveTextPanel(App, TextPanel);
                            
                            App->SelectedPanel = ClosePanel(App, App->SelectedPanel);
                        }
                    } break;
                    
                    case 'n':
                    {
                        panel *Panel = App->SelectedPanel;
                        
                        b32 IsFreePanel = (!IsNilPanel(Panel) && 
                                           !IsTextPanel(App, Panel) &&
                                           !Panel->CannotClose);
                        if(IsFreePanel)
                        {                        
                            panel_node *New = PushStructZero(PanelArena, panel_node);
                            
                            New->Text = PushStructZero(App->TextArena, app_text);
                            New->Text->Capacity = KB(64);
                            New->Text->Data = PushArray(App->TextArena, rune, New->Text->Capacity);
                            
                            New->Next = App->TextPanels;
                            App->TextPanels = New;
                            
                            New->Value = Panel;
                        }
                        
                    } break;
                    
                    case '-':
                    {
                        if(!Shift)
                        {                        
                            App->SelectedPanel = SplitPanel(PanelArena, App->SelectedPanel, Axis2_Y, false);
                        }
                    } break;
                    
                    case ',':
                    {
                        App->SelectedPanel = (IsNilPanel(App->SelectedPanel) ?
                                              PanelNextLeaf(App->FirstPanel, true) :
                                              (!Shift ?
                                               PanelNextLeaf(App->SelectedPanel, false) :
                                               PanelNextLeaf(App->SelectedPanel, true)));
                    } break;
                }
            }
        }
        else
        {
            switch(Key.Codepoint)
            {
                case PlatformKey_Escape: ShouldQuit = true; break;
            }
        }
        
        //- Text Input
        if(TextPanel)
        {
            app_text *Text = TextPanel->Text;
            
            if(!Key.IsSymbol)
            {
                if(ModifiersShiftIgnored == PlatformKeyModifier_None)
                {
                    DeleteSelection(Text);
                    AppendChar(Text, Key.Codepoint);
                }
                else
                {
                    switch(Key.Codepoint)
                    {
                        case 'a':
                        {
                            Text->Trail = 0;
                            Text->Cursor = Text->Count;
                        } break; 
                        
                        case 'c':
                        {
                            CopySelection(Text, Input);
                        } break;
                        
                        case 'x':
                        {
                            CopySelection(Text, Input);
                            DeleteSelection(Text);
                        } break; 
                        
                        case 'v':
                        {
                            str8 Clip = Input->PlatformClipboard;
                            DeleteSelection(Text);
                            
                            u64 Cursor = Text->Cursor;
                            
                            for EachIndex(CharIdx, Input->PlatformClipboard.Size)
                            {
                                AppendChar(Text, (rune)Clip.Data[CharIdx]);
                            }
                            
                            Text->Cursor = Text->Trail = Cursor + Input->PlatformClipboard.Size;
                        } break;
                        
                        case 'z':
                        {
                            NotImplemented();
                        } break;
                        
                        case 'y':
                        {
                            NotImplemented();
                        } break;
                        
                        case 's':
                        {
                            SaveTextToFile(Text, S8("./hello.c"));
                        } break;
                        
                        case 'o':
                        {
                            LoadFileToText(Text, S8("./hello.c"));
                        } break;
                    }
                }
            }
            else
            {
                switch(Key.Symbol)
                {
                    default: break;
                    
                    case PlatformKey_Return: 
                    {
                        DeleteSelection(Text);
                        AppendChar(Text, L'\n'); 
                    } break;
                    
                    case PlatformKey_Right: 
                    {
                        if(!Control)
                        {
                            if(!Shift && Text->Trail != Text->Cursor)
                            {                                
                                u64 End = ((Text->Trail < Text->Cursor) ?
                                           Text->Cursor :
                                           Text->Trail);
                                Text->Cursor = End ;
                            }
                            else
                            {
                                MoveRight(Text);
                            }
                        }
                        else
                        {
                            while(Text->Cursor < Text->Count &&
                                  IsWhiteSpace((u8)Text->Data[Text->Cursor]))
                            {
                                MoveRight(Text);
                            }
                            
                            while(Text->Cursor < Text->Count && 
                                  !IsWhiteSpace((u8)Text->Data[(Text->Cursor)]))
                            {
                                MoveRight(Text);
                            }
                        }
                        
                        MoveTrail(Text, Shift);
                    } break;
                    
                    case PlatformKey_Left: 
                    {
                        if(!Control)
                        {
                            if(!Shift && Text->Trail != Text->Cursor)
                            {
                                // Get on the left of the selection
                                u64 Begin = ((Text->Trail > Text->Cursor) ?
                                             Text->Cursor :
                                             Text->Trail);
                                Text->Cursor = Begin;
                            }
                            else
                            {
                                MoveLeft(Text);
                            }
                        }
                        else
                        {
                            while(Text->Cursor > 0 && 
                                  IsWhiteSpace((u8)Text->Data[Text->Cursor - 1]))
                            {
                                MoveLeft(Text);
                            }
                            
                            while(Text->Cursor > 0 && 
                                  !IsWhiteSpace((u8)Text->Data[Text->Cursor - 1]))
                            {
                                MoveLeft(Text);
                            }
                        }
                        
                        MoveTrail(Text, Shift);
                    } break;
                    
                    // Move up by line
                    case PlatformKey_Up:
                    {
                        MoveUp(Text, Shift);
                    } break;
                    
                    // Move down by line
                    case PlatformKey_Down:
                    {
                        MoveDown(Text, Shift);
                    } break;
                    
                    case PlatformKey_Home:
                    {
                        u64 Cursor = Text->Cursor;
                        
                        if(!Control)
                        {
                            while(Cursor > 0 && Text->Data[Cursor - 1] != '\n') Cursor -= 1;
                        }
                        else
                        {
                            Cursor = 0;
                        }
                        
                        Text->Cursor = Cursor;
                        
                        MoveTrail(Text, Shift);
                    } break;
                    
                    case PlatformKey_End:
                    {
                        u64 Cursor = Text->Cursor;
                        
                        if(!Control)
                        {
                            while(Cursor < Text->Count && Text->Data[Cursor] != '\n') Cursor += 1;
                        }
                        else
                        {
                            Cursor = Text->Count;
                        }
                        
                        Text->Cursor = Cursor;
                        
                        MoveTrail(Text, Shift);
                    } break;
                    
                    case PlatformKey_PageUp:
                    {
                        for EachCount(Text->Lines) 
                        {
                            MoveUp(Text, Shift);
                        }
                    } break;
                    
                    case PlatformKey_PageDown:
                    {
                        for EachCount(Text->Lines)
                        {
                            MoveDown(Text, Shift);
                        }
                    } break;
                    
                    case PlatformKey_BackSpace: 
                    {
                        if(Text->Trail == Text->Cursor)
                        {
                            if(None)
                            {
                                MoveLeft(Text); 
                            }
                            else if(Control)
                            {
                                while(Text->Cursor > 0 && 
                                      IsWhiteSpace((u8)Text->Data[Text->Cursor - 1]))
                                {
                                    MoveLeft(Text);
                                }
                                
                                while(Text->Cursor > 0 && 
                                      !IsWhiteSpace((u8)Text->Data[Text->Cursor - 1]))
                                {
                                    MoveLeft(Text);
                                }
                            }
                        }
                        
                        DeleteSelection(Text);
                        
                        MoveTrail(Text, false);
                    } break;
                    
                    case PlatformKey_Delete:
                    {
                        if(Text->Cursor == Text->Trail)
                        {
                            if(None)
                            {
                                if(Text->Cursor < Text->Count)
                                {
                                    MoveRight(Text);
                                }
                            }
                            else if(Control)
                            {
                                while(Text->Cursor < Text->Count &&
                                      IsWhiteSpace((u8)Text->Data[Text->Cursor]))
                                {
                                    MoveRight(Text);
                                }
                                
                                while(Text->Cursor < Text->Count && 
                                      !IsWhiteSpace((u8)Text->Data[(Text->Cursor)]))
                                {
                                    MoveRight(Text);
                                }
                            }
                        }
                        
                        DeleteSelection(Text);
                        
                        MoveTrail(Text, false);
                    } break;
                    
                }
            }
        }
    }
    
    // NOTE(luca): Covers scrolling in case movement brought us on a different line.
    if(TextPanel)
    {    
        app_text *Text = TextPanel->Text;
        
        UpdateCursorRelLine(Text);
        
        if(Text->Lines != 0)
        {        
            if(Text->CurRelLine != 0)
            {
                Text->CurRelLine %= Text->Lines;
            }
        }
        else
        {
            Text->CurRelLine = 0;
        }
    }
    
    OS_ProfileAndPrint("Input");
    
    RenderBeginFrame(FrameArena, Buffer->Width, Buffer->Height);
    
    OS_ProfileAndPrint("Misc setup");
    
    App->HeightPx = DefaultHeightPx;
    
    if(App->PreviousHeightPx != App->HeightPx)
    {
        App->PreviousHeightPx = App->HeightPx;
        
        local_persist font IconsFont = {0};
        if(!IconsFont.Initialized)
        {
            char *FontPath = PathFromExe(FrameArena, S8("../data/icons.ttf"));
            InitFont(&IconsFont, FontPath);
        }
        
        RenderBuildAtlas(App->FontAtlasArena, &App->FontAtlas, &App->Font, &IconsFont, 
                         App->HeightPx);
    }
    
    OS_ProfileAndPrint("Atlas");
    
    // UI Setup
    {    
        UI_State = PushStruct(PermanentArena, ui_state);
        if(!Memory->Initialized)
        {
            MemoryZero(UI_State);
            UI_State->Arena = App->UIArena;
            UI_State->BoxTableSize = 4096;
            UI_State->BoxTable = PushArray(UI_State->Arena, ui_box, UI_State->BoxTableSize);
        }
        
        UI_State->Atlas = &App->FontAtlas;
        UI_State->FrameIdx = App->FrameIdx;
        UI_State->Input = Input;
        if(UI_IsActive(UI_NilBox))
        {
            UI_State->Hot = UI_KeyNull();
        }
        
        UI_FrameArena = FrameArena;
    }
    
    // Draw rectangles 
    {
        // Window borders
        {
            v4 WindowBorderColor;
            if(0) {}
            else if(Input->PlatformIsRecording) WindowBorderColor = Color_Red;
            else if(Input->PlatformIsStepping) WindowBorderColor = Color_Yellow;
            else WindowBorderColor = Color_Black;
            
            v4 Dest = RectFromSize(V2(0.f, 0.f), BufferDim);
            rect_instance *Inst = DrawRect(Dest, WindowBorderColor, 0.f, WindowBorderSize, 0.f);
            Inst->Color0 = WindowBorderColor;
            Inst->Color1 = (Input->PlatformWindowIsFocused ? Color_Snow2 : V4(V3Arg(Color_Snow2), 0.2f));
            Inst->Color2 = (Input->PlatformWindowIsFocused ? Color_Snow2 : V4(V3Arg(Color_Snow2), 0.2f));
            Inst->Color3 = WindowBorderColor;
        }
    }
    
    
    OS_ProfileAndPrint("UI setup");
    
    // UI Panels
    {
        v4 FreeRegion;
        {    
            v2 Pos = V2(0.f, 0.f);
            v2 Size = BufferDim;
            Pos = V2AddF32(Pos, WindowBorderSize);
            Size = V2SubF32(Size, 2.f*WindowBorderSize);
            FreeRegion = RectFromSize(Pos, Size);
        }
        
        PanelGetRegionAndInput(App->FirstPanel, FreeRegion);
        
        for EachNode(Node, panel_node, App->TextPanels)
        {
            panel *Panel = Node->Value;
            app_text *Text = Node->Text;
            
            if(Panel != App->SelectedPanel)
            {
                Text->CursorAnimTime = 0.f;
            }
            
            ui_box *Root = 0;
            if(UI_IsNilBox(Panel->Root))
            {
                Panel->Root = UI_BoxAlloc(App->UIArena);
            }
            Root = Panel->Root;
            
            Root->FixedPosition = Panel->Region.Min;
            Root->FixedSize = SizeFromRect(Panel->Region);
            Root->Rec = RectFromSize(Root->FixedPosition, Root->FixedSize);
            
            UI_DefaultState(Root, App->HeightPx);
            
            s32 Flags = (UI_BoxFlag_Clip |
                         UI_BoxFlag_DrawBorders |
                         UI_BoxFlag_DrawBackground |
                         UI_BoxFlag_DrawDisplayString |
                         UI_BoxFlag_CenterTextVertically |
                         UI_BoxFlag_CenterTextHorizontally);
            s32 ButtonFlags = (Flags | UI_BoxFlag_MouseClickable);
            
            // NOTE(luca): Adding an extra parent like this makes it easy to override defaults
            UI_LayoutAxis(Axis2_Y) 
                UI_SemanticFull()
                UI_AddBox(S8(""), UI_BoxFlag_Clip);
            
            UI_BorderThickness(1.f)
                UI_Push()
            {
                UI_SemanticWidth(UI_SizeParent(1.f, 1.f)) 
                    UI_SemanticHeight(UI_SizeText(2.f, 1.f))
                {
                    UI_SemanticHeight(UI_SizeChildren(1.f))
                        UI_AddBox(Str8Fmt("%p", Root), UI_BoxFlag_Clip);
                    
                    UI_Push()
                    {
                        UI_SemanticWidth(UI_SizeParent(1.f/5.f, 1.f)) 
                            UI_SemanticHeight(UI_SizeText(2.f, 1.f))
                            UI_LayoutAxis(Axis2_X)
                        {
                            // Background
                            UI_SemanticWidth(UI_SizeParent(1.f, 1.f))
                                UI_AddBox(S8("background"), (UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorders|
                                                             UI_BoxFlag_FloatingX|UI_BoxFlag_Clip));
                            
                            UI_BackgroundColor(Color_ButtonBackground)
                            {
                                if(UI_Button(S8("Open")))
                                {
                                    LoadFileToText(Text, S8("./hello.c"));
                                }
                                
                                if(UI_Button(S8("Clear")))
                                {
                                    Text->Count = 0;
                                    Text->Cursor = 0;
                                    Text->Trail = 0;
                                    Text->CurRelLine = 0;
                                }
                                
                                if(UI_Button(S8("Save")))
                                {
                                    SaveTextToFile(Text, S8("./hello.c"));
                                }
                            }
                            
                            UI_SemanticWidth(UI_SizeParent(1.f, 0.f))
                                UI_AddBox(S8("spacer"), UI_BoxFlag_Clip);
                            
                            // Close panel button
                            {                            
                                b32 ShouldClosePanel = false;
                                
                                UI_SemanticWidth(UI_SizeText(2.f, 1.f))
                                    UI_TextColor(Color_Black)
                                    UI_FontKind(FontKind_Icon) 
                                    if(UI_AddBox(S8("b##close"), ButtonFlags)->Clicked)
                                {
                                    ShouldClosePanel = true;
                                }
                                
                                if(ShouldClosePanel)
                                {
                                    App->SelectedPanel = ClosePanel(App, Panel);
                                    RemoveTextPanel(App, Node);
                                }
                            }
                        }
                    }
                    
#if EDITOR_INTERNAL                    
                    UI_SemanticHeight(UI_SizeChildren(1.f)) 
                        UI_AddBox(S8("debug info"), UI_BoxFlag_Clip);
                    UI_Push() UI_SemanticWidth(UI_SizeText(4.f, 1.f)) UI_BorderThickness(1.f)
                    {
                        UI_AddBox(Str8Fmt("Rel:%lu/%lu###Line pos", Text->CurRelLine, Text->Lines - !!(Text->Lines > 0)), Flags);
                        UI_AddBox(Str8Fmt("#%lu###Text count", Text->Count), Flags);
                        UI_AddBox(Str8Fmt("Cur:%lu/%lu###Text cursor pos", Text->Cursor, Text->Trail), Flags);
                        
                        UI_SemanticWidth(UI_SizeParent(1.f, 0.f))
                            UI_AddBox(S8("remaining spacer"), UI_BoxFlag_Clip|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorders);
                    }
#endif
                    
                    // Text
                    {                
                        UI_SemanticHeight(UI_SizeParent(1.f, 0.f))
                            UI_TextColor(Color_Snow0)
                        {
                            ui_box *TextBox = UI_AddBox(S8("app text"), UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorders | UI_BoxFlag_Clip);
                            custom_text_draw_params *Params = PushStructZero(FrameArena, custom_text_draw_params);
                            Params->Box = TextBox;
                            Params->Text = Text;
                            TextBox->CustomDraw = TextComputeAndDraw;
                            TextBox->CustomDrawData = Params;
                            
                            Text->CursorAnimTime += Input->dtForFrame;
                        }
                    }
                }
            }
            UI_ResolveLayout(Root->First);
        }
        
        // Debug Panel
        if(IsEditorBuildInternal)
        {
            panel *Panel = App->DebugPanel;
            ui_box *Root = 0;
            if(UI_IsNilBox(Panel->Root))
            {
                Panel->Root = UI_BoxAlloc(App->UIArena);
            }
            Root = Panel->Root;
            
            Root->FixedPosition = Panel->Region.Min;
            Root->FixedSize = SizeFromRect(Panel->Region);
            Root->Rec = RectFromSize(Root->FixedPosition, Root->FixedSize);
            
            UI_DefaultState(Root, App->HeightPx);
            
            ui_box *Hot = UI_BoxFromKey(UI_State->Hot);
            ui_box *Active = UI_BoxFromKey(UI_State->Active);
            str8 ActiveDisplayString = Active->DisplayString;
            str8 HotDisplayString = Hot->DisplayString;
            if(ActiveDisplayString.Size == 0) ActiveDisplayString = S8("null");
            if(HotDisplayString.Size == 0) HotDisplayString = S8("null");
            
            s32 Flags = (UI_BoxFlag_Clip |
                         UI_BoxFlag_DrawBackground |
                         UI_BoxFlag_DrawDisplayString |
                         UI_BoxFlag_CenterTextVertically);
            s32 ButtonFlags = (Flags | UI_BoxFlag_MouseClickable);
            
            UI_LayoutAxis(Axis2_Y)
                UI_SemanticFull()
                UI_AddBox(S8("Debug panel parent"), UI_BoxFlag_Clip);
            UI_Push() 
                UI_SemanticWidth(UI_SizeParent(1.f, 1.f))
                UI_SemanticHeight(UI_SizeText(2.f, 1.f))
            {
                UI_AddBox(Str8Fmt("Active: " S8Fmt, S8Arg(ActiveDisplayString)), Flags);
                UI_AddBox(Str8Fmt("Hot: " S8Fmt, S8Arg(HotDisplayString)), Flags);
                
                UI_AddBox(Str8Fmt("Active->tHot = %.2f", Active->tHot), Flags);
                UI_AddBox(Str8Fmt("Active->tActive = %.2f", Active->tActive), Flags);
                UI_AddBox(Str8Fmt("Hot->tHot = %.2f", Hot->tHot), Flags);
                UI_AddBox(Str8Fmt("Hot->tActive = %.2f", Hot->tActive), Flags);
            }
            
            UI_ResolveLayout(Root->First);
        }
    }
    
    // Prune unused boxes
    for EachIndex(Idx, UI_State->BoxTableSize)
    {
        ui_box *First = UI_State->BoxTable + Idx;
        
        for UI_EachHashBox(Node, First)
        {
            if(App->FrameIdx > Node->LastTouchedFrameIdx)
            {
                if(!UI_KeyMatch(Node->Key, UI_KeyNull()))
                {
                    Node->Key = UI_KeyNull();
                }
            }
        }
    }
    
    OS_ProfileAndPrint("UI");
    
    //- Rendering 
    
    // Render rectangles
    if(!Input->SkipRendering)
    {
        RenderClear();
        RenderDrawAllRectangles(&App->Render, BufferDim, &App->FontAtlas);
    }
    
    OS_ProfileAndPrint("R rects");
    
    App->FrameIdx += 1;
    
    // NOTE(luca): This is so that we can split our initialization code if we want.
    Memory->Initialized = true;
    
#if 0 || EDITOR_STARTUP_PROFILE
    // NOTE(luca): Useful for profiling startup times.
    if(App->FrameIdx == 2)
        ShouldQuit = true;
#endif
    
    if(IsNilPanel(App->FirstPanel))
    {
        ShouldQuit = true;
    }
    
    return ShouldQuit;
}
