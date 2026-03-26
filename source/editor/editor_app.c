#define BASE_EXTERNAL_LIBS 1
#define BASE_NO_ENTRYPOINT 1
#include "base/base.h"
#include "base/base.c"
#include "editor/generated/everything.c"
#include "editor/editor_platform.h"
#include "editor/editor_font.h"
#include "editor/editor_random.h"
#include "editor/editor_libs.h"
#include "editor/editor_gl.h"
#include "editor/editor_app.h"
#include "editor/editor_ui.h"
#include "editor/editor_lexer.h"
#include "editor/editor_parser.h"
#include "editor/editor_lexer.c"
#include "editor/editor_parser.c"

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
global_variable panel *PanelRoot = 0;
global_variable app_state *PanelApp = 0;
global_variable app_input *PanelInput = 0;
global_variable panel *PanelDragging = 0;

global_variable s32 PanelDebugIndentation = 0;
global_variable s32 PanelDebugLevel = 0;

global_variable axis2_stack_node *PanelAxisTop = 0; 

//- 
internal inline b32
InRange(f32 P, f32 Min, f32 Max, f32 Size)
{
    b32 Result = ((P >= Min        && P < Min + Size) ||
                  (P >= Max - Size && P < Max));
    
    return Result;
}

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
SaveTextToFile(app_text *Text, app_memory *Memory, str8 FileName)
{
    str8 Source = PushS8(FrameArena, Text->Count);
    for EachIndex(Idx, Source.Size)
    {
        Source.Data[Idx] = (u8)Text->Data[Idx];
    }
    char *Path = PathFromExe(FrameArena, Memory->ExeDirPath, FileName);
    OS_WriteEntireFile(Path, Source);
}

internal void
LoadFileToText(app_text *Text, app_memory *Memory, str8 FileName)
{
    char *Path = PathFromExe(FrameArena, Memory->ExeDirPath, FileName);
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
          IsWhiteSpace(Text->Data[Text->Cursor - 1]))
    {
        DeleteChar(Text);
    }
    
    while(Text->Cursor > 0 && 
          !IsWhiteSpace(Text->Data[Text->Cursor - 1]))
    {
        DeleteChar(Text);
    }
}

//~ Misc

internal inline b32
EqualsWithEpsilon(f32 A, f32 B, f32 Epsilon)
{
    b32 Result = (A < (B + Epsilon) && 
                  A > (B - Epsilon));
    return Result;
}

internal rect_instance *
DrawRect(v4 Dest, v4 Color, f32 CornerRadius, f32 BorderThickness, f32 Softness)
{
    rect_instance *Result = GlobalRectsInstances + GlobalRectsCount;
    
    GlobalRectsCount += 1;
    
    MemoryZero(Result);
    
    Result->Dest = Dest;
    Result->Color0 = Color;
    Result->Color1 = Color;
    Result->Color2 = Color;
    Result->Color3 = Color;
    Result->CornerRadii.e[0] = CornerRadius;
    Result->CornerRadii.e[1] = CornerRadius;
    Result->CornerRadii.e[2] = CornerRadius;
    Result->CornerRadii.e[3] = CornerRadius;
    Result->Border = BorderThickness;
    Result->Softness = Softness;
    
    return Result;
}

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


internal rect_instance *
DrawRectChar(font_atlas *Atlas, v2 Pos, rune Codepoint, v4 Color)
{
    rect_instance *Result = 0;
    v4 Dest = {0};
    
    // NOTE(luca): Evereything happens in pixel coordinates in here.
    
    b32 Supported = (Codepoint >= Atlas->FirstCodepoint && 
                     Codepoint < Atlas->CodepointsCount - Atlas->FirstCodepoint);
    if(1 || Supported)
{        
    rune CharIdx = Codepoint - Atlas->FirstCodepoint;
    stbtt_packedchar *PackedChar = &Atlas->PackedChars[CharIdx];
    stbtt_aligned_quad *Quad = &Atlas->AlignedQuads[CharIdx];
    f32 Width = (PackedChar->x1 - PackedChar->x0);
    f32 Height = (PackedChar->y1 - PackedChar->y0);
    {    
        v2 Min = Pos;
        // TODO(luca): Looks off, maybe rounding is incorrect, investigate...
        Min.X += (PackedChar->xoff);
        f32 Baseline = GetBaseline(Atlas->Font, Atlas->FontScale);
        f32 Descent = (f32)Atlas->Font->Descent*Atlas->FontScale;
        Min.Y += (PackedChar->yoff) + Baseline + Descent;
        
        v2 Max = V2(Min.X + Width, Min.Y + Height);
        
        Result = DrawRect(Dest, Color, 0.f, 0.f, 0.f);
        
        Result->Dest = Rect(Min.X, Min.Y, Max.X, Max.Y);
        Result->TexSrc = Rect(Quad->s0, Quad->t0, Quad->s1, Quad->t1);
    }
    
    Result->HasTexture = true;
    
#if 0
    DrawRect(Result->Dest, V4(0.f, 0.f, 1.f, 1.f), 0.f, 1.f, 0.f);
#endif
    }

    return Result;
}

internal rect_instance *
DrawRectIcon(font_atlas *Atlas, v2 Pos, rune Codepoint, v4 Color)
{
    rune Shifted = (Atlas->FirstCodepoint + Atlas->CodepointsCount + 
                      Codepoint - Atlas->IconsFirstCodepoint);
    rect_instance *Result = DrawRectChar(Atlas, Pos, Shifted, Color); 
    return Result;
}

#include "editor_ui.c"


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
            // TODO(luca): What we actually want is that if the last line is visible *and* there is text before that line then  scroll down
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
            
            range_u64 Selection = GetSelection(Text);
            IsInSelection = (Idx >= Selection.Min && Idx < Selection.Max); 
            
            v4 CharRect = RectFromSize(Pen, V2(CharWidth, Box->HeightPx));
            
            if(IsInSelection)
            {
                v4 SelectionColor = Color_Blue;
                SelectionColor.A = .5f;
                DrawRect(CharRect, SelectionColor, 0.f, 0.f, 0.f);
            }
            
            // Mouse text input
            app_input *Input = UI_State->Input;
            if(!Input->Consumed)
            {                
                v2 MouseP = V2S32(Input->Mouse.X, Input->Mouse.Y);
                
                if(IsInsideRectV2(MouseP, CharRect))
                {
                    b32 OnRightSide = !!(((CharRect.Max.X - MouseP.X)/CharWidth) < .5f); 
                    
                    app_button_state *LeftButton = &Input->Mouse.Buttons[PlatformMouseButton_Left];
                    b32 Shift = (LeftButton->Modifiers & PlatformKeyModifier_Shift);
                    
                    if(LeftButton->EndedDown)
                    {
                        Text->Trail = Idx + (u64)OnRightSide;
                        if(WasPressed(*LeftButton))
                        {
                            if(!Shift)
                            {
                                Text->Cursor = Text->Trail;
                                Text->CursorAnimTime = 0.f;
                            }
                            
                            Input->Consumed = true;
                        }
                    }
                    
                    if(0)
                    {
                        DrawRect(CharRect, Color_Red, 0.f, 1.f, 0.f); 
                    }
                }
                
                
                if(0 && Idx == BeginningOfLineIdx)
                {
                    DrawRect(CharRect, Color_Green, 0.f, 1.f, 0.f);
                }
                
            }
            
            b32 VisualizeWhitespace = true;
            
            if(IsWhiteSpace(Char) && VisualizeWhitespace)
            {
                if (Char == L' ')       DrawChar = L'\u00B7'; // · Middle Dot
                if (Char == L'\t')      DrawChar = L'\u2192'; // → Rightwards Arrow
                if (Char == L'\n')      DrawChar = L'\u0024'; // $ Dollar Sign
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
                if(0)
                {
                    CursorColor.A = 0.f;
                }
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

//~ Opengl

internal void
gl_InitState(gl_render_state *Render)
{
    glGenVertexArrays(ArrayCount(Render->VAOs), &Render->VAOs[0]);
    glGenTextures(ArrayCount(Render->Textures), &Render->Textures[0]);
    glGenBuffers(ArrayCount(Render->VBOs), &Render->VBOs[0]);
    glBindVertexArray(Render->VAOs[0]);
    glBindBuffer(GL_ARRAY_BUFFER, Render->VBOs[0]);
    
}

internal void
gl_CleanupState(gl_render_state *Render)
{
    glDeleteVertexArrays(ArrayCount(Render->VAOs), &Render->VAOs[0]);
    glDeleteTextures(ArrayCount(Render->Textures), &Render->Textures[0]);
    glDeleteBuffers(ArrayCount(Render->VBOs), &Render->VBOs[0]);
    glDeleteProgram(Render->RectShader);
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

internal panel *
SplitPanel(arena *Arena, panel *To, s32 Axis, b32 Backwards)
{
    panel *Result = To;
    
    panel *New = PanelAlloc(Arena);
    panel *Parent = To->Parent;
    
    // NOTE(luca): Must be a leaf node.
    Assert(IsNilPanel(To->First));
    
    if(Axis == Parent->Axis)
    {
        //The new child's ParentPct becomes 1/n where n is the children count
        //Other children's ParentPct *= (1-1/n) 
        {        
            s32 ChildrenCount = 0;
            for EachPanel(Child, Parent)
            {
                ChildrenCount += 1;
            }
            
            Assert(ChildrenCount > 0);
            // The new child
            ChildrenCount += 1;
            
            New->ParentPct = 1.f/(f32)ChildrenCount;;
            
            for EachPanel(Child, Parent)
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

internal inline b32 
IsLeafPanel(panel *Panel)
{
    b32 Result = IsNilPanel(Panel->First);
    return Result;
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
        Assert(!IsNilPanel(Search) || Search == Start);
        Result = Search;
    }
    
    return Result;
}

// NOTE(luca): Returns the panel that absorbed its size
internal panel *
ClosePanel(panel *Panel)
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
            
            // TODO(luca): When this becomes the first and last child, should we collapse it together with the parent?
            
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
            }
            else
            {
                // Last node of its parent, parent should get deleted.  End of the bloodline.
                Collapse = ClosePanel(Panel->Parent);
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
    v2 Size = {0};
    
    f32 ParentSize = (Parent->Region.Max.e[Axis] - Parent->Region.Min.e[Axis]);
    f32 OtherSize = (FreeRegion.Max.e[OtherAxis] - FreeRegion.Min.e[OtherAxis]);
    
    Size.e[Axis] = (!IsNilPanel(Parent) ?
                    (Panel->ParentPct*ParentSize) :
                    (FreeRegion.Max.e[Axis] - FreeRegion.Min.e[Axis]));
    Size.e[OtherAxis] = OtherSize;
    
    Panel->Region = RectFromSize(Pos, Size);
    
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
    
    if(1)
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
        
        v2 MouseP = V2S32(PanelInput->Mouse.X, PanelInput->Mouse.Y);
        
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
            
            BorderColor = Color_Blue;
            Input->PlatformCursor = (PlatformCursorShape_ResizeHorizontal + Axis);
            
            if(IsNilPanel(PanelDragging) && Pressed)
            {                
                PanelDragging = Panel; 
            }
        }
        
        if(PanelDragging == Panel && Down)
        {
            Input->Consumed = true;
            
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

//-

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
    
    Input->Consumed = false;
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
        
        App->Text.Capacity = KB(64);
        App->Text.Data = PushArray(PermanentArena, rune, App->Text.Capacity);
        App->FontAtlasArena = PushArena(PermanentArena, MB(150), false);
        
        App->UIBoxTableSize = 4096;
        App->UIBoxTable = PushArray(PermanentArena, ui_box, App->UIBoxTableSize);
        App->UIBoxArena = PushArena(PermanentArena, (256*sizeof(ui_box)), false);
        
        PanelArena = App->PanelArena = PushArena(PermanentArena, ArenaAllocDefaultSize, false);
    }
    
    font_atlas *Atlas = &App->FontAtlas;
    
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
    v2 MouseP = V2S32(Input->Mouse.X, Input->Mouse.Y);
    
    if(!Memory->Initialized)
    {
        char *FontPath = PathFromExe(FrameArena, Memory->ExeDirPath, S8("../data/font_regular.ttf"));
        InitFont(&App->Font, FontPath);
        App->PreviousHeightPx = DefaultHeightPx + 1.0f;
        App->HeightPx = DefaultHeightPx;
        
        // Nil read only structs 
        {        
            arena *Arena = ArenaAlloc(.Size = MB(1), .Offset = TB(3));
            
            ui_box *Box = PushStruct(Arena, ui_box);
            *Box = (ui_box){Box, Box, Box, Box, Box, Box, Box};
            
            panel *Panel = PushStruct(Arena, panel);
            *Panel = (panel){Panel, Panel, Panel, Panel, Panel};
            Panel->Root = Box;
            
#if OS_WINDOWS
            DWORD OldProtection = {0};
            if(VirtualProtect(Arena->Base, Arena->Size, PAGE_READONLY, &OldProtection) == 0)
            {
                Win32LogIfError();
            }
#elif OS_LINUX
            s32 Ret = mprotect(Arena->Base, Arena->Size, PROT_READ);
            AssertErrno(Ret == 0);
#endif
            
            NilPanel  = App->TrackerForNilPanel  = Panel;
            UI_NilBox = App->TrackerForUI_NilBox = Box;
        }
        
        gl_InitState(&App->Render);
        
        // Panels
        {        
            PanelAxis(Axis2_Y) PanelGroup()
            {
                panel *RootPanel = PanelAdd(1.f);
                
                PanelAxis(Axis2_X) PanelGroup()
                {
                    panel *AppPanel = PanelAdd(1.f);
                    
                    PanelAxis(Axis2_Y) PanelGroup()
                    {
                        panel *LeftPanel = PanelAdd(.4f);
                        PanelGroup()
                        {            
                            panel *TopLeftPanel = PanelAdd(.33f);
                            panel *MiddleLeftPanel = PanelAdd(.33f);
                            panel *BottomLeftPanel = PanelAdd(.34f);
                        }
                        panel *RightPanel = PanelAdd(.6f);
                        PanelGroup()
                        {            
                            panel *DebugPanel = PanelAdd(.6f);
                            App->DebugPanel = DebugPanel;
                            App->DebugPanel->Root = UI_BoxAlloc(App->PanelArena);
                            App->SelectedPanel = App->DebugPanel;
                            panel *BottomRightPanel = PanelAdd(.4f);
                        }
                    }
                }
                App->FirstPanel = RootPanel;
                App->LastPanel = RootPanel;
            }
        }
        
        OS_ProfileAndPrint("Memory Init");
    }
    
    UI_NilBox = App->TrackerForUI_NilBox;
    NilPanel = App->TrackerForNilPanel;
    
    StringsScratch = FrameArena;
    
    PanelArena = App->PanelArena;
    PanelInput = Input;
    PanelApp = App;
    
    app_text *Text = &App->Text;
    
    Text->PrevCursor = Text->Cursor;
    
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
                        if(App->SelectedPanel == App->DebugPanel)
                        {
                            App->DebugPanel = NilPanel;
                        }
                        App->SelectedPanel = (!Shift ?
                                              SplitPanel(PanelArena, App->SelectedPanel, Axis2_X, false) :
                                              ClosePanel(App->SelectedPanel));
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
                        App->SelectedPanel = (!Shift ?
                                              PanelNextLeaf(App->SelectedPanel, false) :
                                              PanelNextLeaf(App->SelectedPanel, true));
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
        if(App->SelectedPanel == App->DebugPanel)
        {
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
                            CopySelection(&App->Text, Input);
                        } break;
                        
                        case 'x':
                        {
                            CopySelection(&App->Text, Input);
                            DeleteSelection(&App->Text);
                        } break; 
                        
                        case 'v':
                        {
                            str8 Clip = Input->PlatformClipboard;
                            DeleteSelection(&App->Text);
                            
                            u64 Cursor = Text->Cursor;
                            
                            for EachIndex(Idx, Input->PlatformClipboard.Size)
                            {
                                AppendChar(&App->Text, (rune)Clip.Data[Idx]);
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
                            SaveTextToFile(Text, Memory, S8("./hello.c"));
                        } break;
                        
                        case 'o':
                        {
                            LoadFileToText(Text, Memory, S8("./hello.c"));
                        } break;
                    }
                }
            }
            else
            {
                switch(Key.Symbol)
                {
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
                                  IsWhiteSpace(Text->Data[Text->Cursor]))
                            {
                                MoveRight(Text);
                            }
                            
                            while(Text->Cursor < Text->Count && 
                                  !IsWhiteSpace(Text->Data[(Text->Cursor)]))
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
                                  IsWhiteSpace(Text->Data[Text->Cursor - 1]))
                            {
                                MoveLeft(Text);
                            }
                            
                            while(Text->Cursor > 0 && 
                                  !IsWhiteSpace(Text->Data[Text->Cursor - 1]))
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
                        for EachIndex(Idx, Text->Lines) 
                        {
                            MoveUp(Text, Shift);
                        }
                    } break;
                    
                    case PlatformKey_PageDown:
                    {
                        for EachIndex(Idx, Text->Lines)
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
                                      IsWhiteSpace(Text->Data[Text->Cursor - 1]))
                                {
                                    MoveLeft(Text);
                                }
                                
                                while(Text->Cursor > 0 && 
                                      !IsWhiteSpace(Text->Data[Text->Cursor - 1]))
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
                                      IsWhiteSpace(Text->Data[Text->Cursor]))
                                {
                                    MoveRight(Text);
                                }
                                
                                while(Text->Cursor < Text->Count && 
                                      !IsWhiteSpace(Text->Data[(Text->Cursor)]))
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
    
    // NOTE(luca): Covers all movement, deletion is handled in place.
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
    
    OS_ProfileAndPrint("Input");
    
    glViewport(0, 0, Buffer->Width, Buffer->Height);
    OS_ProfileAndPrint("GL setup");
    
    v4 WindowBorderColor;
    if(0) {}
    else if(Input->PlatformIsRecording) WindowBorderColor = Color_Red;
    else if(Input->PlatformIsPlaying) WindowBorderColor = Color_Green;
    else if(Input->PlatformWindowIsFocused) WindowBorderColor = Color_Snow2;
    else WindowBorderColor = Color_Black;
    
    //- Prepare rects rendering 
    u64 MaxRectsCount = 1024;
    GlobalRectsCount = 0;
    GlobalRectsInstances = PushArray(FrameArena, rect_instance, MaxRectsCount);
    
    OS_ProfileAndPrint("Misc setup");
    
    //- Draw text
        if(App->PreviousHeightPx != App->HeightPx)
        {
            App->PreviousHeightPx = App->HeightPx;
        
        // Init atlas
        { 
            font_atlas *Atlas = &App->FontAtlas;
            app_font *Font = &App->Font;
            f32 HeightPx = App->HeightPx;
            arena *Arena = App->FontAtlasArena;
            
            // TODO(luca): Display icons.
            // There are three options:
            
            // 1) Add icons to the font and render from there.
            //    - If we use PUA that means we need to create a very big atlas?
            //      - We could create the cached glyph table version of our atlas for this instead.
            //    + This is good because we will eventually need this.
            
            // 2) Use multiple fonts
            
            //    2.1) Use a megatexture that is all atlases concatenated, have atlas store an offset?
            //         - This sounds hard, because this has to play nice with stbtt for creating correct Alignedquads
            //         - NOTE(luca): this can end up being easier if we can pack multiple fonts into the same atlas with stbtt_PackFontRange() 
            
            //    2.2) Have an option to specify a texture per list of items, grouping the draw calls by texture.
            //         - This sounds like the easiest.
            //         + A similar system will probably be needed in the future i.e., specifying different rendering properties for an array of elements to draw whilst having the API have a single path for adding elements.
            
            //    2.3) Specify a second texture
            //         - this makes the shader more complex
            
            local_persist app_font IconsFont = {0};
            if(!IconsFont.Initialized)
            {
                char *FontPath = PathFromExe(FrameArena, Memory->ExeDirPath, S8("../data/icons.ttf"));
                InitFont(&IconsFont, FontPath);
            }
            
            if(Font->Initialized)
            {
                Atlas->FirstCodepoint = ' ';
            // NOTE(luca): 2 bytes =>small alphabets.
            Atlas->CodepointsCount = ((0xFFFF - 1) - Atlas->FirstCodepoint);
                Atlas->IconsFirstCodepoint = 'a';
                Atlas->IconsCodepointsCount = 2;
                
                Atlas->Width = KB(1);
                Atlas->Height = KB(2);
            u64 Size = (u64)(Atlas->Width*Atlas->Height);
            
            Arena->Pos = 0;
                Atlas->Data = PushArray(Arena, u8, Size);
                s32 CodepointsCount = (Atlas->CodepointsCount + Atlas->IconsCodepointsCount);
                Atlas->PackedChars = PushArray(Arena, stbtt_packedchar, (u64)CodepointsCount);
                Atlas->AlignedQuads = PushArray(Arena, stbtt_aligned_quad, (u64)CodepointsCount);
            
            Atlas->HeightPx = HeightPx;
            Atlas->FontScale = stbtt_ScaleForPixelHeight(&Font->Info, HeightPx);
            Atlas->Font = Font;
            
            stbtt_pack_context Ctx;
            {
                stbtt_PackBegin(&Ctx, 
                                Atlas->Data,
                                Atlas->Width, Atlas->Height,
                                0, 1, 0);
                
                stbtt_PackFontRange(&Ctx, Font->Info.data, 0, HeightPx, 
                                    Atlas->FirstCodepoint, Atlas->CodepointsCount, 
                                    Atlas->PackedChars);
                    
                    // Icons
{                    
                    stbtt_PackFontRange(&Ctx, IconsFont.Info.data, 0, HeightPx, 
                                        Atlas->IconsFirstCodepoint, Atlas->IconsCodepointsCount, 
                                        Atlas->PackedChars + Atlas->CodepointsCount);
                    }

                    stbtt_PackEnd(&Ctx);
            }
            
                for EachIndex(Idx, CodepointsCount)
                {
                float UnusedX, UnusedY;
                stbtt_GetPackedQuad(Atlas->PackedChars, Atlas->Width, Atlas->Height, 
                                    Idx,
                                    &UnusedX, &UnusedY,
                                        &Atlas->AlignedQuads[Idx], 0);
            }
            }
}
        }
    
    OS_ProfileAndPrint("Atlas");
    
    // Draw rectangles 
    {
        // Window borders
        {
            v4 Dest = RectFromSize(V2(0.f, 0.f), BufferDim);
            rect_instance *Inst = DrawRect(Dest, WindowBorderColor, 0.f, WindowBorderSize, 0.f);
            Inst->Color0 = WindowBorderColor;
            Inst->Color1.W = 0.2f;
            Inst->Color2.W = 0.2f;
            Inst->Color3 = WindowBorderColor;
        }
    }
    
    s32 Flags = (UI_BoxFlag_DrawBorders | UI_BoxFlag_DrawBackground |
                 UI_BoxFlag_DrawDisplayString |
                 UI_BoxFlag_CenterTextHorizontally | UI_BoxFlag_CenterTextVertically |
                 UI_BoxFlag_Clip);
    s32 ButtonFlags = (Flags | UI_BoxFlag_MouseClickability);
    s32 TitlebarFlags = (Flags ^ 
                         UI_BoxFlag_DrawDisplayString ^ 
                         UI_BoxFlag_DrawBackground);
    
    OS_ProfileAndPrint("UI setup");
    
    v4 FreeRegion;
{    
    v2 Pos = V2(0.f, 0.f);
    v2 Size = BufferDim;
    Pos = V2AddF32(Pos, WindowBorderSize);
    Size = V2SubF32(Size, 2.f*WindowBorderSize);
    FreeRegion = RectFromSize(Pos, Size);
    }

    // UI Panels
    
    {
        PanelGetRegionAndInput(App->FirstPanel, FreeRegion);
        
        if(!IsNilPanel(App->DebugPanel))
        {
            panel *Panel = App->DebugPanel;
            ui_box *Root = Panel->Root;
            
            Root->FixedPosition = Panel->Region.Min;
            Root->FixedSize = SizeFromRect(Panel->Region);
            Root->Rec = RectFromSize(Root->FixedPosition, Root->FixedSize);
            
            UI_State = PushStructZero(FrameArena, ui_state);
            UI_InitState(App->DebugPanel->Root, Input, App);
            
            s32 Flags = (UI_BoxFlag_Clip |
                         UI_BoxFlag_DrawBorders |
                         UI_BoxFlag_DrawBackground |
                         UI_BoxFlag_DrawDisplayString |
                         UI_BoxFlag_CenterTextVertically |
                         UI_BoxFlag_CenterTextHorizontally);
            
            // NOTE(luca): Adding an extra parent like this makes it easy to override defaults
            UI_LayoutAxis(Axis2_Y) 
                UI_SemanticWidth(UI_SizeParent(1.f, 1.f))
                UI_AddBox(S8(""), UI_BoxFlag_Clip);
            
            UI_BorderThickness(1.f)
                UI_CornerRadii(V4(0.f, 0.f, 0.f, 0.f))
                UI_Push()
            {
                UI_SemanticWidth(UI_SizeParent(1.f, 1.f)) 
                    UI_SemanticHeight(UI_SizeText(2.f, 1.f))
                {
                    local_persist f64 StartTime; 
                    DoOnce StartTime = OS_GetWallClock();
                    
                    UI_SemanticHeight(UI_SizeChildren(1.f))
                        UI_AddBox(S8(""), UI_BoxFlag_Clip);
                    
                    UI_Push()
                    {
                        UI_SemanticWidth(UI_SizeParent(1.f/5.f, 1.f)) 
                            UI_SemanticHeight(UI_SizeText(2.f, 1.f))
                            UI_LayoutAxis(Axis2_X)
                        {
                            // Background
                            UI_SemanticWidth(UI_SizeParent(1.f, 1.f))
                                UI_AddBox(S8(""), (UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorders|
                                                   UI_BoxFlag_FloatingX|UI_BoxFlag_Clip));
                            
                            if(UI_AddBox(S8("Open"), ButtonFlags)->Clicked)
                            {
                                LoadFileToText(Text, Memory, S8("./hello.c"));
                            }
                            
                            if(UI_AddBox(S8("Clear"), ButtonFlags)->Clicked)
                            {
                                Text->Count = 0;
                                Text->Cursor = 0;
                                Text->Trail = 0;
                                Text->CurRelLine = 0;
                            }
                            
                            if(UI_AddBox(S8("Save"), ButtonFlags)->Clicked)
                            {
                                SaveTextToFile(Text, Memory, S8("./hello.c"));
                            }
                            
                            UI_SemanticWidth(UI_SizeParent(1.f, 0.f))
                                UI_AddBox(S8(""), UI_BoxFlag_Clip);
                            
                            if(UI_AddBox(S8("Close"), ButtonFlags)->Clicked)
                            {
                                App->SelectedPanel = ClosePanel(App->DebugPanel);
                                App->DebugPanel = NilPanel;
                            }
                        }
                    }
                    
                    UI_SemanticHeight(UI_SizeChildren(1.f)) 
                        UI_AddBox(S8(""), UI_BoxFlag_Clip);
                    UI_Push() UI_SemanticWidth(UI_SizeText(4.f, 1.f)) UI_BorderThickness(1.f)
                    {
                        UI_AddBox(Str8Fmt("Rel:%lu/%lu###Line pos", Text->CurRelLine, Text->Lines - !!(Text->Lines > 0)), Flags);
                        UI_AddBox(Str8Fmt("#%lu###Text count", Text->Count), Flags);
                        UI_AddBox(Str8Fmt("Cur:%lu/%lu###Text cursor pos", Text->Cursor, Text->Trail), Flags);
                        
                        UI_SemanticWidth(UI_SizeParent(1.f, 0.f)) UI_CornerRadii(V4(0.f, 0.f, 0.f, 0.f))
                            UI_AddBox(S8(""), UI_BoxFlag_Clip|UI_BoxFlag_DrawBackground|UI_BoxFlag_DrawBorders);
                    }
                    
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
            
            DrawRectIcon(&App->FontAtlas, V2(845.f, 8.f), 'b', Color_ButtonText); 
        }
    }
    
    // Prune unused boxes
    for EachIndex(Idx, App->UIBoxTableSize)
    {
        ui_box *First = App->UIBoxTable + Idx;
        for(ui_box *Node = First; !UI_IsNilBox(Node); Node = Node->HashNext)
        {
            if(App->FrameIndex > Node->LastTouchedFrameIndex)
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
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    
    glClearColor(V4Arg(Color_Background));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    OS_ProfileAndPrint("R Clear");
    
    // Render rectangles
    {
        if(!App->Render.ShadersCompiled)
        {
            gl_uint RectShader = App->Render.RectShader;
            glDeleteProgram(RectShader);
            
            RectShader = GL_ProgramFromShaders(FrameArena, Memory->ExeDirPath,
                                               S8("../source/editor/generated/rect_vert.glsl"),
                                               S8("../source/editor/generated/rect_frag.glsl"));
            glUseProgram(RectShader);
            
            GL_LoadTextureFromImage(App->Render.Textures[0], Atlas->Width, Atlas->Height, Atlas->Data, GL_RED, RectShader, "Texture");
            // Uniforms
            {
                gl_int UViewport = glGetUniformLocation(RectShader, "Viewport");
                glUniform2f(UViewport, V2Arg(BufferDim));
            }
            
            App->Render.RectShader = RectShader;
            App->Render.ShadersCompiled = true;
        }
        
#if EDITOR_HOT_RELOAD_SHADERS
        App->Render.ShadersCompiled = false;
#endif
        
        Assert(GlobalRectsCount < MaxRectsCount);
        u64 Size = GlobalRectsCount*sizeof(rect_instance);
        glBufferData(GL_ARRAY_BUFFER, (s32)Size, GlobalRectsInstances, GL_STATIC_DRAW);
        
        u64 Offset = 0;
        
        s32 TotalSize = 0;
        for EachElement(Idx, RectVSAttribOffsets)
        {            
            GL_SetQuadAttribute((gl_uint)Idx, (u64)RectVSAttribOffsets[Idx], &Offset);
            TotalSize += RectVSAttribOffsets[Idx];
        }
        Assert(TotalSize == (sizeof(rect_instance)/sizeof(f32)));
        
        glEnable(GL_MULTISAMPLE);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (gl_int)GlobalRectsCount);
    }
    
    OS_ProfileAndPrint("R rects");
    
    App->FrameIndex += 1;
    
    // NOTE(luca): This is so that we can split our initialization code if we want.
    Memory->Initialized = true;
    
    return ShouldQuit;
}
