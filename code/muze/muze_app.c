#define BASE_EXTERNAL_LIBS 1
#define BASE_NO_ENTRYPOINT 1
#include "base/base.h"
#include "base/base.c"

#if OS_WINDOWS 
#include <windows.h>
#include <mmsystem.h>
#endif


#include "muze/generated/everything.c"

#include "muze/muze_platform.h"
#include "muze/muze_libs.h"
#include "muze/muze_font.h"
#include "muze/muze_random.h"
#include "muze/muze_gl.h"
#include "muze/muze_renderer.h"
#include "muze/muze_ui.h"
#include "muze/muze_app.h"
#include "muze/muze_midi.h"

#include "muze/muze_renderer.c"
#include "muze/muze_ui.c"

//~ Globals
// TODO(luca): Metaprogram
typedef enum note_pitch note_pitch;
enum note_pitch
{
    Note_C = 0,
    Note_Cs,
    Note_D,
    Note_Ds,
    Note_E,
    Note_F,
    Note_Fs,
    Note_G,
    Note_Gs,
    Note_A,
    Note_As,
    Note_B,
    
    Note_Count
};

global_variable str8 NotePitchStrings[] =
{
    {(u8*)"C", 1},
    {(u8*)"C#", 2},
    {(u8*)"D", 1},
    {(u8*)"D#", 2},
    {(u8*)"E", 1},
    {(u8*)"F", 1},
    {(u8*)"F#", 2},
    {(u8*)"G", 1},
    {(u8*)"G#", 2},
    {(u8*)"A", 1},
    {(u8*)"A#", 2},
    {(u8*)"B", 1},
};
StaticAssert(ArrayCount(NotePitchStrings) == Note_Count, NotePitchStringsSizeCheck);

global_variable note_pitch NoteBasePitches[] =
{
    Note_C,
    Note_C,
    Note_D,
    Note_D,
    Note_E,
    Note_F,
    Note_F,
    Note_G,
    Note_G,
    Note_A,
    Note_A,
    Note_B
};
StaticAssert(ArrayCount(NoteBasePitches) == Note_Count, NoteBasePitchesSizeCheck);

global_variable b32 NotePianoColors[] =
{
    true, false, true, false, true, true, false, true, false, true, false, true 
};
StaticAssert(ArrayCount(NotePianoColors) == Note_Count, NotePianoColorsSizeCheck);

typedef enum base_note base_note;
enum base_note 
{
    BaseNote_A,
    BaseNote_B,
    BaseNote_C,
    BaseNote_D,
    BaseNote_E,
    BaseNote_F,
    BaseNote_G,
    BaseNote_Count
};

global_variable s32 NoteBasePitchToStep[] =
{
    0, 0,
    1, 1,
    2,
    3, 3,
    4, 4,
    5, 5,
    6
};
StaticAssert(ArrayCount(NoteBasePitchToStep) == Note_Count, NoteBasePitchToStepSizeCheck);

global_variable s32 Flags = (UI_BoxFlag_Clip |
                             UI_BoxFlag_DrawBorders |
                             UI_BoxFlag_DrawBackground |
                             UI_BoxFlag_DrawDisplayString |
                             UI_BoxFlag_CenterTextVertically |
                             UI_BoxFlag_CenterTextHorizontally);

//~ Helpers

internal f32
GetWallTime(void)
{
    f32 Result = (f32)OS_GetWallClock();
    return Result;
}

//~ Panels
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
TextMoveRight(app_text *Text)
{
    Text->Cursor += (Text->Cursor < Text->Count);
    Text->CursorAnimTime = 0.f;
}

internal void
TextMoveLeft(app_text *Text)
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
        
        Panel->Kind = PanelKind_Free;
        
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
    v2 PanelSize = {0};
    
    f32 ParentSize = (Parent->Region.Max.e[Axis] - Parent->Region.Min.e[Axis]);
    f32 OtherSize = (FreeRegion.Max.e[OtherAxis] - FreeRegion.Min.e[OtherAxis]);
    
    PanelSize.e[Axis] = (!IsNilPanel(Parent) ?
                         (Panel->ParentPct*ParentSize) :
                         (FreeRegion.Max.e[Axis] - FreeRegion.Min.e[Axis]));
    PanelSize.e[OtherAxis] = OtherSize;
    
    
    if(!IsNilPanel(Panel))
    {       
        Panel->Region = RectFromSize(Pos, PanelSize);
        
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
        v2 MouseP = MousePosFromInput(Input);
        
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

internal panel_rec
PanelRecDepthFirstPreOrder(panel *Panel)
{
    panel_rec Rec = {0};
    
    if(Panel->First)
    {
        Rec.Next = Panel->First;
        Rec.PushCount = 1;
    }
    else for(panel *P = Panel; !IsNilPanel(P); P = P->Parent)
    {
        if(!IsNilPanel(P->Next))
        {
            Rec.Next = P->Next;
            break;
        }
        Rec.PopCount += 1;
    }
    
    return Rec;
}

//~ Muze

internal void
ProcessMIDINotes(app_memory *Memory, app_state *App, app_midi_note *MIDINotes, u64 Count)
{
    for EachIndex(Idx, Count)
    {
        app_midi_note *MIDINote = MIDINotes + Idx;
        f32 Timestamp = MIDINote->Timestamp;
        
#if MUZE_INTERNAL
        // NOTE(luca): This is a hack to get loop editing to work properly.
        // TODO(luca): "AppTime" concept.
        Timestamp = GetWallTime();
#endif
        
        midi_message Message = {MIDINote->Message};
        
        u8 Status  = Message.U8[0];
        u8 Data1   = Message.U8[1];
        u8 Data2   = Message.U8[2];
        u8 Type    = Status & 0xF0;
        u8 Channel = Status & 0x0F;
        
        if(App->IsRecording)
        {
            if(0) {} 
            else if(Type == 0x90 && Data2 > 0) 
            {
                // Note On
                note *Note = App->Notes + App->NotesCount;
                MemoryZero(Note);
                App->NotesCount += 1;
                
                Note->Timestamp = Timestamp - App->RecordStart;
                Note->Pitch = Data1;
                Note->Velocity = Data2;
                
                App->MaxPitch = Max(Note->Pitch, App->MaxPitch);
                App->MinPitch = Min(Note->Pitch, App->MinPitch);
                
                {
                    midi_message OutMessage = {0};
                    
                    u8 Pitch = Data1;
                    u8 Velocity = Data2;
                    
                    OutMessage.U8[0] = MIDIEventType_NoteOn;
                    OutMessage.U8[1] = Pitch;
                    OutMessage.U8[2] = Velocity;
                    OutMessage.U8[3] = 0;
                    
                    Memory->PlatformMIDISend(App->Out, OutMessage.U32[0]);
                    
                }
            }
            else if(Type == MIDIEventType_NoteOff || 
                    (Type == MIDIEventType_NoteOn && Data2 == 0))
            {
                // Note Off, set the duration of the last note with same pitch
                u8 Pitch = Data1;
                u8 Velocity = Data2;
                
                // NOTE(luca): NotesCount can be 0 if we start recording when a note is still playing.
                if(App->NotesCount > 0)
                {
                    for(s64 Idx = App->NotesCount - 1; Idx >= 0; Idx -= 1)
                    {
                        note *Note = App->Notes + Idx;
                        if(Note->Pitch == Pitch)
                        {
                            Note->Duration = ((Timestamp - App->RecordStart)- Note->Timestamp);
                            break;
                        }
                    }
                    
                    // Send this note out to the output device
                    {                
                        midi_message OutMessage = {0};
                        u8 OutChannel = 0;
                        
                        Assert(OutChannel < 16);
                        OutMessage.U8[0] = MIDIEventType_NoteOff | OutChannel;
                        OutMessage.U8[1] = Pitch;
                        OutMessage.U8[2] = 0;
                        OutMessage.U8[3] = 0;
                        
                        Memory->PlatformMIDISend(App->Out, OutMessage.U32[0]);
                    }
                }
            }
            else if(Status == 0xB0) 
            {
                // Control Change
                
                //Log("CC       - Channel: %d, Controller: %d, Value: %d\n", Channel, Data1, Data2);
            }
        }
    }
    
}



internal void
MuzeInit(app_state *App)
{
    App->NotesCount = 0;
    App->MaxPitch = 75;
    App->MinPitch = 40;
    App->RecordStart = 0.f;
    App->RecordEnd = 0.f;
    App->PlayPos = 0.f;
    App->BPM = 100.f;
    App->TimeSig = 3;
    App->NoteSel = 0;
}

internal void
StartRecording(app_state *App)
{
    MuzeInit(App);
    App->RecordStart = GetWallTime();
    App->PlayPos = 0.f;
    App->IsRecording = true;
    App->IsPlaying = false;
}

internal b32
IsNotePlaying(note *Note, app_state *App, f32 dtForFrame)
{
    b32 Result = false;
    
    if(App->IsPlaying)
    {
        f32 NoteStart = (Note->Timestamp);
        f32 NoteEnd = (NoteStart + Note->Duration);
        
        Result = (App->PlayPos >= NoteStart - dtForFrame &&
                  App->PlayPos < NoteEnd + dtForFrame);
    }
    
    if(App->IsRecording)
    {
        Result = (Note->Duration == 0.f);
    }
    
    return Result;
}

internal void
StopAllPlayingNotes(app_memory *Memory, app_state *App, f32 dtForFrame)
{
    for EachNote(Note, App->Notes, App->NotesCount)
    {
        b32 NoteIsPlaying = IsNotePlaying(Note, App, dtForFrame);
        if(NoteIsPlaying)
        {
            if(App->IsRecording)
            {
                f32 RecordLength = (App->RecordEnd - App->RecordStart);
                f32 NoteMaxDuration = (RecordLength - Note->Timestamp);
                f32 Now = (GetWallTime() - App->RecordStart);
                Note->Duration = ClampTop(Now-  Note->Timestamp, NoteMaxDuration);
            }
            
            union { u32 U32[1]; u8 U8[4]; } OutMessage;
            u8 OutChannel = 0;
            
            OutMessage.U8[0] = 0x80 | OutChannel;
            OutMessage.U8[1] = Note->Pitch;
            OutMessage.U8[2] = 0;
            OutMessage.U8[3] = 0;
            
            Memory->PlatformMIDISend(App->Out, OutMessage.U32[0]);
        }
    }
}

internal void
StopRecording(app_memory *Memory, app_state *App, f32 dtForFrame)
{
    StopAllPlayingNotes(Memory, App, dtForFrame);
    App->IsRecording = false;
}

//~ UI 
#define UI_YSpacer() UI_SemanticHeight(UI_SizeEm(.3f, 1.f)) UI_AddBox(S8(""), UI_BoxFlag_Clip);

internal void
UI_PushContainer(axis2 Axis, str8 Name)
{
    UI_LayoutAxis(Axis) 
        UI_SemanticWidth(UI_SizeChildren(1.f)) 
        UI_SemanticHeight(UI_SizeChildren(1.f)) 
        UI_AddBox(Name, UI_BoxFlag_Clip);
    UI_PushBox();
    
    UI_AddBox(Name, Flags);
    UI_YSpacer();
}

internal void
UI_PopContainer(void)
{
    UI_PopBox();
    
    // TODO(luca): Check for the widest child of the container and make all children its size
    ui_box *Parent = UI_State->Current;
    axis2 Axis = 1 - Parent->LayoutAxis;
    
    f32 MaxSize = 0.f;
    for UI_EachBox(Box, Parent->First)
    {
        f32 Size = UI_MeasureTextWidth(Box->DisplayString, Box->FontKind);
        MaxSize = Max(MaxSize, Size);
    }
    
    f32 TextPadding = 4.f;
    
    for UI_EachBox(Box, Parent->First)
    {
        Box->SemanticSize[Axis].Kind = UI_SizeKind_Pixels;
        Box->SemanticSize[Axis].Value = MaxSize + 2.f*TextPadding;
        Box->SemanticSize[Axis].Strictness = 1.f;
    }
}

internal b32
UI_Button(str8 Label)
{
    b32 Result = false;
    UI_BackgroundColor(Color_ButtonBackground)
    {
        Result = (UI_AddBox(Label, Flags | UI_BoxFlag_MouseClickability)->Clicked);
    }
    return Result;
}

#define UI_List(Axis, Name) DeferLoop(UI_PushContainer(Axis, Name), UI_PopContainer())

typedef struct muze_box_data muze_box_data;
struct muze_box_data
{
    ui_box *Box;
    app_state *App;
    
    f32 ScrollX;
};

UI_CUSTOM_DRAW(CustomDrawSheetMusic)
{
    
    muze_box_data *Data = (muze_box_data *)CustomDrawData;
    ui_box *Box = Data->Box;
    app_state *App = Data->App;
    f32 ScrollX = Data->ScrollX;
    app_input *Input = UI_State->Input;
    
    v4 BackgroundColor = Box->BackgroundColor;
    v4 ForegroundColor = Color_Black;
    
    DrawRect(Box->Rec, BackgroundColor, 0.f, 0.f, 0.f);
    
    f32 NoteSize = 11.f;
    v2 NoteDim = V2(NoteSize, NoteSize);
    v2 TailDim = V2(2.f, 24.f);
    
    // TODO(luca): Keep consistent with RecordMarkerX
    f32 SheetMusicWidth = (Box->FixedSize.X);
    
    v4 NoteColor = ForegroundColor;
    v4 BarColor = ForegroundColor;
    
    v2 BoxPos = V2(Box->FixedPosition.X - ScrollX, Box->FixedPosition.Y);
    v2 BoxSize = Box->FixedSize;
    
    f32 RecordLength = (App->RecordEnd - App->RecordStart);
    f32 BPS = (App->BPM/60.f);
    
    // Staff Lines
    v2 StaffPos = BoxPos;
    s32 StaffLinesCount = 5;
    f32 StaffLineWidth = 2.f;
    f32 StaffHeight = (f32)(StaffLinesCount-1)*NoteSize + StaffLineWidth;
    
    // Bars
    f32 WholeBarWidth = 200.f;;
    f32 BarDuration = 1.f/BPS;
    f32 BarsCount = floorf(RecordLength/(BarDuration*(f32)App->TimeSig)) + 1.f;
    v2 BarDim = V2(2.f, StaffHeight);
    
    // Draw staff
    {    
        StaffPos.Y += .5f*(BoxSize.Y - StaffHeight);
        {   
            for EachIndex(Idx, 5)
            {
                f32 X = StaffPos.X;
                f32 Y = StaffPos.Y + (f32)Idx*(NoteSize);
                
                f32 Width = (BarsCount)*WholeBarWidth;
                
                v4 Dest = RectFromSize(V2(X, Y), V2(Width, StaffLineWidth));
                Dest = RectIntersect(Dest, Box->Rec);
                
                DrawRect(Dest, ForegroundColor, 0.f, 0.f, 0.f); 
            }
        }
    }
    
    // Draw bars
    {    
        for EachIndex(Idx, (s32)BarsCount)
        {
            v2 BarPos = V2(BoxPos.X + ((f32)(Idx + 1) * WholeBarWidth),
                           StaffPos.Y);
            DrawRect(RectFromSize(BarPos, BarDim), BarColor, 0.f, 0.f, 0.f); 
        }
    }
    
    for EachNote(Note, App->Notes, App->NotesCount)
    {        
        NoteColor = Box->TextColor;
        for EachNode(Node, note_node, App->NoteSel)
        {
            if(Node->Value == Note)
            {
                NoteColor = Color_Blue;
            }
        }
        
        b32 NoteIsPlaying = IsNotePlaying(Note, App, Input->dtForFrame);
        if(NoteIsPlaying)
        {
            NoteColor = Color_Yellow;
        }
        
        Assert(!(App->IsPlaying && Note->Duration == 0.f));
        
        u8 PitchClass = (Note->Pitch%Note_Count);
        
        s32 Length = 1;
        
        f32 NoteBorderSize = 0.f;
        
        f32 Duration = Note->Duration;
        if(Duration == 0.f)
        {
            Assert(App->IsRecording);
            f32 Now = (GetWallTime() - App->RecordStart);
            Duration = (Now - Note->Timestamp);
        }
        
        f32 NoteLength = (Duration*BPS);
        f32 NoteStart = (Note->Timestamp);
        // NOTE(luca): Get pos relative of time inside bar 
        
        f32 NoteX = (NoteStart*BPS/(f32)App->TimeSig)*WholeBarWidth;
        
        s32 FirstNoteOctave = 6;
        s32 FirstNoteSteps = NoteBasePitchToStep[Note_F];
        
        s32 NoteSteps = NoteBasePitchToStep[PitchClass];
        s32 NoteOctave = Note->Pitch/Note_Count;
        
        s32 Steps = ((FirstNoteOctave*BaseNote_Count + FirstNoteSteps) - (NoteOctave*BaseNote_Count + NoteSteps));
        f32 StepHeight = .5f*NoteSize;
        
        f32 NoteYOffset = ((f32)(Steps - 1)*StepHeight);
        v2 NotePos = V2(BoxPos.X + NoteX, StaffPos.Y + NoteYOffset + .5f*StaffLineWidth);
        
        if(NoteLength < 4.f)
        {
            f32 X = NotePos.X + NoteSize - NoteBorderSize - 2.f;
            f32 Y = NotePos.Y + .5f*NoteSize;
            
            v4 Dest = RectFromSize(V2(X, Y - TailDim.Y),
                                   TailDim);
            rect_instance *Inst = DrawRect(Dest, NoteColor, 0.f, 0.f, 0.f);
            Inst->CornerRadii.e[2] = .5f;
            
            // NOTE(luca): Will be negative for notes longer than 1
            s32 NumOfSideTails = (s32)roundf(-log2f(NoteLength));
            for EachIndex(TailIdx, NumOfSideTails)
            {
                f32 SideTailWidth = (TailDim.X);
                f32 SideTailY = ((Y + (2.f*SideTailWidth)*(f32)TailIdx) - TailDim.Y);
                v4 TailDest = RectFromSize(V2(X, SideTailY), V2(8.f, SideTailWidth));
                
                DrawRect(TailDest, NoteColor, 0.f, 0.f, 0.f);
            }
            
        }
        
        if(roundf(NoteLength) >= 2.f)
        {
            NoteBorderSize = 2.f;
        }
        
        {
            v4 Dest = RectFromSize(NotePos, NoteDim);
            DrawRect(Dest, NoteColor, .5f*NoteSize, NoteBorderSize, .5);
        }
        
        {
#if 0
            str8 NoteString = NotePitchStrings[PitchClass];
#else
            b32 IsBlack = !NotePianoColors[PitchClass];
            str8 NoteString = (IsBlack ? S8("#") : S8("") );
#endif
            
            font_atlas *Atlas = UI_State->Atlas;
            
            rune Shift = UI_GetShiftForFont(Box->FontKind);
            
            v2 TextCur = V2(NotePos.X + NoteSize, NotePos.Y - 4.f);
            for EachIndex(CharIdx, NoteString.Size)
            {
                rune Char = (rune)(NoteString.Data[CharIdx]) + Shift;
                f32 CharWidth = (Atlas->PackedChars[Char - Atlas->FirstCodepoint].xadvance);
                f32 CharHeight = (Atlas->HeightPx);
                
                v2 TextCurMax = V2AddV2(TextCur, V2(CharWidth, CharHeight));
                if(IsInsideRectV2(TextCur, Box->Rec) &&
                   IsInsideRectV2(TextCurMax, Box->Rec))
                {
                    DrawRectChar(Atlas, TextCur, Char, Box->TextColor);
                    
                    TextCur.X += CharWidth;
                }
            }
        }
    }
    
}

UI_CUSTOM_DRAW(CustomDrawPianoRoll)
{
    muze_box_data *Data = (muze_box_data *)CustomDrawData;
    ui_box *Box = Data->Box;
    app_state *App = Data->App;
    
    app_input *Input = UI_State->Input;
    
    b32 MouseLeftDown = false;
    b32 MouseRightDown = false;
    v2 MouseP = {0};
    v2 MouseStartP = {0};
    v4 SelDest = {0};
    
    f32 BPS = App->BPM/60.f;
    
    f32 Zoom = (BPS/(f32)App->TimeSig*200.f);
    
    // Get Input
    {    
        if(!Input->Consumed)
        {
            MouseLeftDown = Input->Mouse.Buttons[PlatformMouseButton_Left].EndedDown;
            MouseRightDown = Input->Mouse.Buttons[PlatformMouseButton_Right].EndedDown;
        }
        MouseP = MousePosFromInput(Input);
        MouseStartP = V2S32(Input->Mouse.StartX, Input->Mouse.StartY);
        
        SelDest = (v4){.Min = MouseP, .Max = MouseStartP};
        
        if(SelDest.Min.X > SelDest.Max.X)
        {
            Swap(SelDest.Min.X, SelDest.Max.X);
        }
        if(SelDest.Min.Y > SelDest.Max.Y)
        {
            Swap(SelDest.Min.Y, SelDest.Max.Y);
        }
        
        SelDest = RectIntersect(Box->Rec, SelDest);
        if(MouseLeftDown || MouseRightDown)
        {
            b32 Adding = (MouseLeftDown && !MouseRightDown);
            DrawRect(SelDest, V4(V3Arg(Adding ? Color_Blue : Color_Snow2), .3f), 1.f, 0.f, 0.f);
            DrawRect(SelDest, V4(V3Arg(Adding ? Color_Orange : Color_Black), 1.f), 1.f, 1.f, 0.f);
        }
    }
    
    
    // Piano roll
    {    
        u8 Range = (App->MaxPitch - App->MinPitch);
        
        f32 NoteHeight = (Box->FixedSize.Y/(f32)(Range + 1));
        f32 NoteWidth = 1.5f*NoteHeight;
        f32 PianoKeyGap = 1.f;
        
        f32 RollX = (Box->FixedPosition.X - Data->ScrollX);
        f32 RollY = (Box->FixedPosition.Y + (Box->FixedSize.Y - Box->FixedSize.Y));
        f32 RollHeight = Box->FixedSize.Y;
        f32 RollWidth = Box->FixedSize.X;
        
        // Draw piano overlay
        {
            f32 PianoWidth = 1.5f*NoteWidth;
            v2 PianoPos = Box->FixedPosition;
            
            for(u8 Idx = App->MinPitch; 
                Idx < App->MaxPitch + 1; 
                Idx += 1)
            {
                f32 YOffset = NoteHeight*(f32)(Range - (Idx - App->MinPitch));
                
                f32 X = (PianoPos.X);
                f32 Y = (PianoPos.Y + YOffset);
                
                u8 PitchClass = (Idx%Note_Count);
                b32 White = NotePianoColors[PitchClass];
                v4 KeyColor = (White ? Color_Snow0 : Color_Black);
                KeyColor.A = .2f; 
                v4 Dest = RectIntersect(RectFromSize(V2(X, Y), V2(PianoWidth, NoteHeight - PianoKeyGap)), Box->Rec);
                DrawRect(Dest, KeyColor, 0.f, 0.f, 0.f);
            }
        }
        
        // Draw the roll and markers
        {        
            // NOTE(luca): This is also the current time when recording.
            f32 RecordMarkerX;
            {
                f32 Timestamp = Max(App->RecordEnd - App->RecordStart, 0.f);
                f32 ZoomedTimestamp = Timestamp*Zoom;
                RecordMarkerX = RollX + roundf(ZoomedTimestamp);
            }
            
            f32 PlayMarkerX;
            {
                f32 Timestamp = App->PlayPos;
                f32 ZoomedTimestamp = Timestamp*Zoom;
                PlayMarkerX = RollX + roundf(ZoomedTimestamp); 
            }
            
            for EachNote(Note, App->Notes, App->NotesCount)
            {
                b32 NoteIsRecording = (Note->Duration == 0.f);
                
                // TODO(luca): Visualize
                u8 Velocity = Note->Velocity;
                
                f32 StartX = roundf(Note->Timestamp*Zoom);
                // NOTE(luca): We have to flip the Y since we have TopDown coordinates for UI.
                f32 StartY = NoteHeight*(f32)(Range - (Note->Pitch - App->MinPitch));
                
                f32 Width;
                {                
                    f32 EndX = StartX + NoteWidth;
                    if(NoteIsRecording)
                    {
                        if(RecordMarkerX > StartX)
                        {
                            EndX = RecordMarkerX;
                        }
                    }
                    else
                    {
                        // NOTE(luca): We might want to shrink this
                        EndX = roundf((Note->Timestamp + Note->Duration)*Zoom);
                    }
                    
                    Width = (EndX - StartX);
                }
                
                v2 NotePos = V2((RollX + StartX),
                                (RollY + StartY));
                
                u8 PitchClass = (Note->Pitch%Note_Count);
                b32 White = NotePianoColors[PitchClass];
                
                // NOTE(luca): Find selection node for note
                note_node *Sel = 0;
                for EachNode(Node, note_node, App->NoteSel)
                {
                    if(Node->Value == Note)
                    {
                        Sel = Node;
                        break;
                    }
                }
                
                v4 NoteColor = (IsNotePlaying(Note, App, Input->dtForFrame) ? Color_Yellow : 
                                Sel ? Color_Blue :  (White ? Color_Snow0 : Color_Black));
                v4 NoteDest = RectFromSize(NotePos, V2(Width, NoteHeight)); 
                v4 Dest = RectIntersect(NoteDest, Box->Rec);
                // Draw Note
                {
                    rect_instance *Instance = DrawRect(Dest, NoteColor, 5.f, 0.f, .5f);
                    Instance->Color0.A = .2f;
                    Instance->Color2.A = .2f;
                }
                
                // Draw selection rectangle
                if(MouseLeftDown || MouseRightDown)
                {
                    b32 Adding = (MouseLeftDown && !MouseRightDown);
                    
                    // NOTE(luca): @Command
                    if(RectOverlap(Dest, SelDest))
                    {
                        if(Adding && !Sel)
                        {
                            note_node *Node = PushStruct(App->NoteNodesArena, note_node);
                            Node->Value = Note;
                            
                            if(App->NoteSel) 
                            {
                                App->NoteSel->Prev = Node;
                            }
                            Node->Next = App->NoteSel;
                            
                            App->NoteSel = Node;
                        }
                        else if(!Adding && Sel)
                        {
                            // TODO(luca): PUt on the free list
                            if(Sel->Prev)
                            {
                                Sel->Prev->Next = Sel->Next;
                            }
                            
                            if(Sel->Next)
                            {
                                Sel->Next->Prev = Sel->Prev;
                            }
                            
                            if(Sel == App->NoteSel)
                            {
                                App->NoteSel = (Sel->Next ? Sel->Next : Sel->Prev);
                            }
                        }
                    }
                }
                
                if(IsInsideRectV2(MouseP, Dest))
                {
                    // Draw border around whene hovered
                    if(Sel) White = !White;
                    v4 BorderColor = (!White ? Color_Snow0 : Color_Black);
                    DrawRect(Dest, BorderColor, 5.f, 2.f, 0.5f);
                }
            }
            
            // Time markers
            {    
                DrawRect(RectFromSize(V2(RecordMarkerX, RollY), 
                                      V2(2.f, RollHeight)),
                         Color_Red, 0.f, 0.f, 0.f);
                
                DrawRect(RectFromSize(V2(PlayMarkerX, RollY), 
                                      V2(2.f, RollHeight)),
                         Color_Green, 0.f, 0.f, 0.f);
            }
        }
    }
    
}


//~ EntryPoint
C_LINKAGE
UPDATE_AND_RENDER(UpdateAndRender)
{
    b32 ShouldQuit = false;
    
#if MUZE_INTERNAL
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
        
        App->Notes = PushArray(PermanentArena, note, KB(128));
        App->NoteNodesArena = PushArena(PermanentArena, KB(64), false);
        
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
        MuzeInit(App);
        
        // NOTE(luca): Hardcoded for my windows setup
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
        
        // UI
{
                UI_State = PushStructZero(App->UIArena, ui_state);
                UI_State->Arena = App->UIArena;
                UI_State->BoxTableSize = 4096;
                UI_State->BoxTable = PushArray(UI_State->Arena, ui_box, UI_State->BoxTableSize);
            App->TrackerForUI_State = UI_State;
        }
        
        // Panels
        { 
            // TODO(luca): Invert axis on group.
            // TODO(luca): We should be able to only add one panel here...
            PanelAxis(Axis2_X) PanelGroup()
            {
                App->FirstPanel = PanelAdd(1.f);
                App->FirstPanel->Kind = PanelKind_Muze;
                
                PanelAxis(Axis2_Y) PanelGroup()
                {
                    PanelAdd(1.f);
                }
            }
            
            App->SelectedPanel = PanelNextLeaf(App->FirstPanel, false);
        }
        
        OS_ProfileAndPrint("Memory Init");
    }
    
    UI_NilBox = App->TrackerForUI_NilBox;
    UI_State = App->TrackerForUI_State;
    NilPanel = App->TrackerForNilPanel;
    
    StringsScratch = FrameArena;
    
    PanelArena = App->PanelArena;
    PanelInput = Input;
    PanelApp = App;
    
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
            if(!Control)
            {
                switch(Key.Codepoint)
                {
                    case 'p':
                    {
                        StopRecording(Memory, App, Input->dtForFrame);
                        
                        if(App->IsPlaying)
                        {
                            App->IsPlaying = false;
                        }
                        else
                        {
                            StopAllPlayingNotes(Memory, App, Input->dtForFrame);
                            App->PlayPos = 0.f;
                            App->IsPlaying = true;
                        }
                    } break;
                    
                    case ' ':
                    {
                        if(App->IsRecording)
                        {
                            StopRecording(Memory, App, Input->dtForFrame);
                        }
                        else
                        {
                            StopAllPlayingNotes(Memory, App, Input->dtForFrame);
                            StartRecording(App);
                        }
                    } break;
                    default: break;
                }
                
            }
            else
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
                            App->SelectedPanel = ClosePanel(App, App->SelectedPanel);
                        }
                    } break;
                    
                    case 'm':
                    {
                        if(!IsNilPanel(App->SelectedPanel))
                        {
                            App->SelectedPanel->Kind = PanelKind_Muze;
                        }
                    } break;
                    
#if 0                    
                    case 'n':
                    {
                        panel *Panel = App->SelectedPanel;
                        
                        b32 IsFreePanel = (!IsNilPanel(Panel) && 
                                           Panel->Kind == PanelKind_Free &&
                                           !Panel->CannotClose);
                        if(IsFreePanel)
                        {                        
                            panel_node *New = PushStructZero(PanelArena, panel_node);
                            
                            New->Next = App->TextPanels;
                            App->TextPanels = New;
                            
                            New->Value = Panel;
                            
                            Panel->Text = PushStructZero(App->TextArena, app_text);
                            Panel->Text->Capacity = KB(64);
                            Panel->Text->Data = PushArray(App->TextArena, rune, Panel->Text->Capacity);
                            Panel->Kind = PanelKind_Text;
                        }
                        
                    } break;
#endif
                    
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
                case PlatformKey_Escape:
                {
                    ShouldQuit = true;
                } break;
            }
        }
    }
    local_persist f32 ScrollX    = 0.f;
    local_persist f32 ScrollVelX = 0.f;
    
    // Scrolling
    {    
        f32 dt = Input->dtForFrame;
        
        f32 MaxSpeed = 2000.f;
        f32 Accel = 8000.f;
        f32 Damping = 12.f;
        
        f32 InputDir = 0.f;
        if(Input->ActionLeft.EndedDown)  InputDir -= 1.f;
        if(Input->ActionRight.EndedDown) InputDir += 1.f;
        
        if(InputDir != 0.f)
        {
            ScrollVelX += InputDir * Accel * dt;
            ScrollVelX  = Clamp(ScrollVelX, -MaxSpeed, MaxSpeed);
        }
        else
        {
            // Only damp when no input, so full speed is actually reachable
            ScrollVelX *= expf(-Damping * dt);
        }
        
        ScrollX  += ScrollVelX * dt;
        ScrollX   = Max(0.f, ScrollX);
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
        UI_State->Atlas = &App->FontAtlas;
        UI_State->FrameIdx = App->FrameIdx;
        UI_State->Input = Input;
        if(UI_IsActive(UI_NilBox))
        {
            UI_State->Hot = UI_KeyNull();
        }
        
        UI_FrameArena = FrameArena;
    }
    
    
    OS_ProfileAndPrint("UI setup");
    
    // UI Controls
    {
        local_persist ui_box *Root = 0;
        if(UI_IsNilBox(Root))
        {
            Root = UI_BoxAlloc(App->UIArena);
        }
        
        // Init root
        {    
            v2 Pos = V2(0.f, 0.f);
            v2 Size = BufferDim;
            Pos = V2AddF32(Pos, WindowBorderSize);
            Size = V2SubF32(Size, 2.f*WindowBorderSize);
            
            Root->FixedPosition = Pos;
            Root->FixedSize = Size;
            Root->Rec = RectFromSize(Root->FixedPosition, Root->FixedSize);
            Root->Key.U64[0] = U64HashFromSeedStr8((u64)Root, S8("muze panel root"));
        }
        
        UI_DefaultState(Root, App->HeightPx);
        
        s32 ButtonFlags = (Flags | UI_BoxFlag_MouseClickability);
        
        platform_midi_get_devices_result DevicesArray = Memory->PlatformMIDIGetDevices();
        
        if(!Memory->Initialized)
        {
            b32 OutFound = false;
            b32 InFound = false;
            for EachIndex(Idx, DevicesArray.Count)
            {
                platform_midi_device *Device = DevicesArray.Devices + Idx;
                if(Device->IsOutput)
                {
                    OutFound = true;
                    App->Out = *Device;
                }
                else
                {
                    InFound = true;
                    App->In = *Device;
                }
                if(InFound && OutFound) break;
            }
        }
        
        UI_LayoutAxis(Axis2_Y)
            UI_AddBox(S8(""), UI_BoxFlag_Clip);
        
        UI_Push()
        {        
            UI_LayoutAxis(Axis2_X)
                UI_SemanticWidth(UI_SizeChildren(1.f))
                UI_SemanticHeight(UI_SizeChildren(1.f))
                UI_AddBox(S8(""), UI_BoxFlag_Clip);
            
            UI_Push()
                UI_SemanticWidth(UI_SizeText(4.f, 1.f))
                UI_SemanticHeight(UI_SizeText(2.f, 1.f))
                // Top
            {
                UI_List(Axis2_Y, S8("Input Devices"))
                {
                    
                    b32 DeviceChanged = !Memory->Initialized;
                    
                    for EachIndex(Idx, DevicesArray.Count)
                    {
                        platform_midi_device *Device = DevicesArray.Devices + Idx;
                        if(!Device->IsOutput)
                        {
                            UI_BackgroundColor(((Device->Id == App->In.Id) ? Color_Yellow : Color_ButtonBackground))
                                if(UI_AddBox(Device->Name, ButtonFlags)->Clicked)
                            {
                                DeviceChanged = (App->In.Id != Device->Id);
                                App->In = *Device;
                            }
                        }
                    }
                    
                    if(DeviceChanged)
                    {
                        Memory->PlatformMIDIListen(App->In);
                    }
                }
                
                UI_List(Axis2_Y, S8("Output Devices"))
                {
                    for EachIndex(Idx, DevicesArray.Count)
                    {         
                        platform_midi_device *Device = DevicesArray.Devices + Idx;
                        if(Device->IsOutput)
                        {                            
                            
                            UI_BackgroundColor(((Device->Id == App->Out.Id) ? Color_Yellow : Color_ButtonBackground))
                                if(UI_AddBox(Device->Name, ButtonFlags)->Clicked)
                            {
                                if(App->Out.Id != Device->Id)
                                {
                                    StopAllPlayingNotes(Memory, App, Input->dtForFrame);
                                    App->Out = *Device;
                                }
                            }
                        }
                    }
                }
                
                UI_List(Axis2_Y, S8("Controls"))
                {
                    if(UI_Button(S8("Reset")))
                    {
                        MuzeInit(App);
                        App->IsRecording = false;
                        App->IsPlaying = false;
                        StopAllPlayingNotes(Memory, App, Input->dtForFrame);
                    }
                    
                    UI_BackgroundColor((!App->IsRecording ? Color_ButtonBackground : Color_Red))
                        if(UI_AddBox(S8("Record"), ButtonFlags)->Clicked)
                    {
                        StopAllPlayingNotes(Memory, App, Input->dtForFrame);
                        if(!App->IsRecording)
                        {
                            StartRecording(App);
                        }
                        else
                        {
                            StopRecording(Memory, App, Input->dtForFrame);
                        }
                        
                        App->IsPlaying = false;
                    }
                    
                    UI_BackgroundColor(Color_ButtonBackground)
                    {
                        
                    }
                    
                    UI_BackgroundColor((!App->IsPlaying ? Color_ButtonBackground : Color_Green))
                        if(UI_AddBox(S8("Play"), ButtonFlags)->Clicked)
                    {
                        StopRecording(Memory, App, Input->dtForFrame);
                        
                        if(App->IsPlaying)
                        {
                            App->IsPlaying = false;
                        }
                        else
                        {
                            App->PlayPos = 0.f;
                            App->IsPlaying = true;
                        }
                    }
                }
                
                UI_List(Axis2_Y,  S8("Edits"))
                {
                    if(UI_Button(S8("Trim")))
                    {
                        if(App->NotesCount)
                        {
                            StopAllPlayingNotes(Memory, App, Input->dtForFrame);
                            note *LastNote = App->Notes + (App->NotesCount - 1);
                            
                            f32 LastNoteEnd = (LastNote->Timestamp + LastNote->Duration); 
                            for EachNote(Note, App->Notes, App->NotesCount)
                            {
                                LastNoteEnd = Max(LastNoteEnd, (Note->Timestamp + Note->Duration));
                            }
                            
                            App->RecordEnd = App->RecordStart + LastNoteEnd;
                            
                            note *FirstNote = App->Notes;
                            f32 StartSilence = FirstNote->Timestamp;
                            
                            for EachNote(Note, App->Notes, App->NotesCount)
                            {
                                Note->Timestamp -= StartSilence;
                            }
                            
                            App->RecordEnd -= StartSilence;
                            App->PlayPos = 0.f;
                        }
                    }
                    
                    if(UI_Button(S8("Round")))
                    {
                        f32 BPS = App->BPM/60.f;
                        
                        for EachNoteNode(Note, App->NoteSel)
                        {
                            f32 NoteLength = Note->Duration*BPS;
                            f32 DenomOfFraction = roundf(-log2f(NoteLength));
                            f32 NoteValue = (DenomOfFraction > 0.f ? 
                                             (1.f/2.f*DenomOfFraction) : 
                                             roundf(NoteLength));
                            f32 NewDuration = NoteValue/BPS;
                            Note->Duration = NewDuration;
                        }
                    }
                    
                    if(UI_Button(S8("Sync")))
                    {
                        f32 BPS = App->BPM/60.f;
                        for EachNoteNode(Note, App->NoteSel)
                        {
                            f32 NoteLength = Note->Duration*BPS;
                            f32 DenomOfFraction = roundf(-log2f(NoteLength));
                            f32 NoteValue = (DenomOfFraction > 0.f ? 
                                             (1.f/2.f*DenomOfFraction) : 
                                             roundf(NoteLength));
                            
                            f32 Timestamp = BPS*Note->Timestamp;
                            
                            f32 NewTimestamp = Note->Timestamp;
                            NewTimestamp = (NoteValue >= 1.f ? 
                                            roundf(Timestamp) : 
                                            (roundf(Timestamp/NoteValue)*NoteValue));
                            
                            f32 RealNewTimestamp = NewTimestamp/BPS;
                            Note->Timestamp = RealNewTimestamp;
                            
                        }
                    }
                    
                    if(UI_Button(S8("Clear")))
                    {
                        App->NoteSel = 0;
                    }
                    
                    if(UI_Button(S8("Pattern")))
                    {
                        // TODO(luca): 
                        // 1. Check position in beat
                        // 2. Travel by bar-length
                        // 3. Find note at matching position.
                    }
                }
                
                UI_List(Axis2_Y, S8("Recording"))
                {                    
                    UI_AddBox(Str8Fmt("Length: %.2f###RecordLength", (App->RecordEnd - App->RecordStart)), Flags);
                }
                
                UI_List(Axis2_Y, S8("Playing"))
                {                    
                    UI_AddBox(Str8Fmt("Pos: %.2f###RecordEnd", App->PlayPos), Flags);
                }
                
                if(App->NoteSel)
                {
                    UI_List(Axis2_Y, S8("Note"))
                    {                    
                        note *Sel = App->NoteSel->Value;
                        f32 BPS = App->BPM/60.f;
                        UI_AddBox(Str8Fmt("Duration: %.2f/%.2f###Duration", Sel->Duration, BPS*Sel->Duration), Flags);
                        UI_AddBox(Str8Fmt("Start: %.2f/%.2f###Start", Sel->Timestamp, BPS*Sel->Timestamp), Flags);
                    }
                }
                
                UI_List(Axis2_Y, S8("Music"))
                {                    
                    UI_AddBox(Str8Fmt("BPM: %.2f/%.2f###BPM", App->BPM, App->BPM/60.f), Flags);
                    UI_AddBox(Str8Fmt("Sig: %d/4###TimeSig", App->TimeSig), Flags);
                }
                
                UI_List(Axis2_Y, S8("UI"))
                {                    
                    UI_AddBox(Str8Fmt("ScrollX: %.2f###ScrollX", ScrollX), Flags);
                }
            }
            
            // UI Panels
            
            for(panel *Panel = App->FirstPanel; 
                !IsNilPanel(Panel); 
                Panel = PanelRecDepthFirstPreOrder(Panel).Next)
            {
                if(0) {}
                else if(Panel->Kind == PanelKind_Muze)
                {        
                    UI_BackgroundColor(Color_Night3)
                        UI_TextColor(Color_Snow2)
                        UI_SemanticWidth(UI_SizeParent(1.f, 1.f))
                        UI_SemanticHeight(UI_SizePx(300.f, 1.f))
                    {
                        ui_box *Box = UI_AddBox(S8("MuzeSheetMusic"), UI_BoxFlag_Clip|UI_BoxFlag_DrawBorders);
                        
                        muze_box_data *Data = PushStruct(FrameArena, muze_box_data);
                        Data->Box = Box;
                        Data->App = App;;
                        Data->ScrollX = ScrollX;
                        
                        Box->CustomDraw = CustomDrawSheetMusic;
                        Box->CustomDrawData = Data;
                    }
                    
                    UI_SemanticWidth(UI_SizeParent(1.f, 1.f))
                        UI_SemanticHeight(UI_SizeParent(1.f, 0.f))
                    {
                        ui_box *Box = UI_AddBox(S8("MuzePianoRoll"), UI_BoxFlag_Clip|UI_BoxFlag_DrawBorders);
                        
                        muze_box_data *Data = PushStruct(FrameArena, muze_box_data);
                        Data->Box = Box;
                        Data->App = App;
                        Data->ScrollX = ScrollX;
                        
                        Box->CustomDraw = CustomDrawPianoRoll;
                        Box->CustomDrawData = Data;
                    }
                }
            }
        }
        
        UI_ResolveLayout(Root->First);
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
    
    f32 MaxRecordingLength = 3600.f;
    
    if(App->IsPlaying)
    {
        for EachNote(Note, App->Notes, App->NotesCount)
        {
            f32 NoteStart = Note->Timestamp;
            f32 NoteEnd = NoteStart + Note->Duration;
            
            // TODO(luca): If a note is really short it could skip it.  But how else would you do this.
            // NOTE(luca): We need to  use midiStream instead?
            
            if(NoteStart <= App->PlayPos &&
               NoteStart > (App->PlayPos - Input->dtForFrame))
            {
                midi_message Message = {0};
                
                u8 Channel = 0;
                
                Message.U8[0] = MIDIEventType_NoteOn | Channel;
                Message.U8[1] = Note->Pitch;
                Message.U8[2] = Note->Velocity;
                Message.U8[3] = 0;
                
                Memory->PlatformMIDISend(App->Out, Message.U32[0]);
            }
            
            if(NoteEnd <= App->PlayPos &&
               NoteEnd > (App->PlayPos - Input->dtForFrame))
            {
                midi_message Message = {0};
                
                u8 Channel = 0;
                
                Message.U8[0] = MIDIEventType_NoteOn | Channel;
                Message.U8[1] = Note->Pitch;
                Message.U8[2] = 0;
                Message.U8[3] = 0;
                
                Memory->PlatformMIDISend(App->Out, Message.U32[0]);
            }
        }
        
        App->PlayPos += Input->dtForFrame;
        if(App->PlayPos >= (App->RecordEnd - App->RecordStart))
        {
            StopAllPlayingNotes(Memory, App, Input->dtForFrame);
            
            App->PlayPos = 0.f;
        }
    }
    
    if(App->IsRecording)
    {
        App->RecordEnd = GetWallTime();
        
        if(App->RecordEnd - App->RecordStart >= MaxRecordingLength)
        {
            StopRecording(Memory, App, Input->dtForFrame);
        }
    }
    
    // MIDI Processing
    {    
        // Virtual MIDI keyboard (piano)
    {
        u64 MaxNotesPerFrame = 128; 
        app_midi_note *Notes = PushArray(FrameArena, app_midi_note, MaxNotesPerFrame);
        u64 NotesCount = 0;
        
        u8 StartPitch = 60;
        for EachElement(Idx, Input->MIDI.Buttons)
        {
            app_button_state *Key = Input->MIDI.Buttons + Idx;
            
            b32 IsOn = (Key->EndedDown && Key->HalfTransitionCount == 1);
            b32 IsOff = (!Key->EndedDown && Key->HalfTransitionCount & 1);
            Assert(!(IsOn && IsOff));
            
            if(IsOn || IsOff)
            {                            
                Assert(Idx < 127);
                u8 Pitch = StartPitch + (u8)Idx;
                u8 Velocity = 64;
                
                app_midi_note *Note = Notes + NotesCount;
                NotesCount += 1;
                Note->Timestamp = (f32)OS_GetWallClock();
                
                midi_message Message = {0};
                
                if(IsOn)
                {
                    Message.NoteOn.Type = MIDIEventType_NoteOn;
                    Message.NoteOn.Pitch = Pitch;
                    Message.NoteOn.Velocity = Velocity;
                }
                else if(IsOff)
                {
                    Message.NoteOff.Type = MIDIEventType_NoteOff;
                    Message.NoteOff.Pitch = Pitch;
                }
                
                Note->Message = Message.U32[0];
            }
        }
        
        ProcessMIDINotes(Memory, App, Notes, NotesCount);
    }
    
    ProcessMIDINotes(Memory, App, Input->MIDI.Notes, Input->MIDI.Count);
    }

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
    Memory->Reloaded = false;
    
#if 0 || MUZE_STARTUP_PROFILE
    // NOTE(luca): Useful for profiling startup times.
    if(App->FrameIdx == 3)
        ShouldQuit = true;
#endif
    
    return ShouldQuit;
}

GET_AUDIO_SAMPLES(GetAudioSamples)
{
    s16 *Samples = (s16 *)Buffer;
    
#if 0
    uint SampleRate = 48000;
    
    f32 nfreq = (Pi32 * 2.f) / (f32)SampleRate;
    f32 Volume   = (1 << 15) * .5f;
    f32 Pitch  = 440.f;
    local_persist f32 ctr = 0.0;
    
    for EachIndex(Idx, FramesCount)
        {
            s16 SampleValue = (s16)(Volume * sinf(Pitch * nfreq * ctr));
            ctr += 1.f;
        Samples[2*Idx + 0] = SampleValue;
            Samples[2*Idx + 1] = SampleValue;
        }
#endif
}