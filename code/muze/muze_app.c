#define BASE_EXTERNAL_LIBS 1
#define BASE_NO_ENTRYPOINT 1
#include "base/base.h"
#include "base/base.c"

#define TSF_IMPLEMENTATION
NO_WARNINGS_BEGIN
#include "lib/tsf.h"
NO_WARNINGS_END

#include "rl/generated/everything.c"
#include "rl/rl_platform.h"
#include "rl/rl_libs.h"
#include "rl/rl_font.h"
#include "rl/rl_random.h"
#include "rl/rl_gl.h"
#include "rl/rl_renderer.h"
#include "rl/rl_ui.h"
#include "rl/rl_midi.h"

#include "muze/muze_app.h"

#include "rl/rl_renderer.c"
#include "rl/rl_ui.c"
#include "rl/rl_widgets.c"

#if OS_WINDOWS
# pragma comment(linker, "/export:GetAudioSamples")
# pragma comment(linker, "/export:UpdateAndRender")
#endif

//~ Globals
global_variable tsf *GlobalTSF;
global_variable tsf *NilTSF;
global_variable panel *NilPanel = 0;

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

//~ Helpers

internal f32
GetWallTime(void)
{
    f32 Result = (f32)OS_GetWallClock();
    return Result;
}

internal void
InitReadOnlyGlobals(arena *Arena)
{
    ArenaSetPos(Arena, 0);
    UI_NilBox = PushArray(Arena, ui_box, 1);
    NilPanel = PushArray(Arena, panel, 1);
    NilNote = PushArray(Arena, note, 1);
}

//~ Panels

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

internal void
ZeroPanel(panel *Panel)
{
    MemoryZero(Panel);
    Panel->First = Panel->Last = Panel->Next = Panel->Prev = Panel->Parent = NilPanel;
}

internal panel *
PanelAlloc(void)
{
    panel *Result = NilPanel;
    
    if(!IsNilPanel(PanelApp->FreePanel))
    {
        Result = PanelApp->FreePanel;
        PanelApp->FreePanel = PanelApp->FreePanel->Next;
    }
    else
    {
        Result = PushArray(PanelArena, panel, 1);
    }
    
    ZeroPanel(Result);
    return Result;
}

internal void PanelPushAxis(axis2 Axis) { StackPush(PanelArena, axis2_stack_node, Axis, PanelAxisTop); }
internal void PanelPopAxis() { PanelAxisTop = PanelAxisTop->Prev; }

internal void PanelPush() 
{
    PanelAppendToParent = true;
    
    axis2 Axis = 1 - PanelAxisTop->Value;
    PanelPushAxis(Axis);
}
internal void PanelPop(void)  { PanelCurrent = PanelCurrent->Parent; }

internal panel *
PanelAdd_(panel *Current, b32 AppendToParent, f32 ParentPct)
{
    panel *New = PanelAlloc();
    
    New->First = New->Last = New->Next = New->Prev = New->Parent = NilPanel;
    New->Root = UI_NilBox;
    
    New->ParentPct = ParentPct;
    
    if(!IsNilPanel(Current))
    {            
        if(AppendToParent)
        {
            Current->First = New;
            New->Parent = Current;
            New->Axis = 1 - Current->Axis;
            
        }
        else
        {
            Current->Next = New;
            New->Prev = Current;
            New->Parent = Current->Parent;
            New->Axis = Current->Axis;
        }
        
        New->Parent->Last = New;
    }
    
    PanelCurrent = New;
    PanelAppendToParent = false;
    
    return New;
}

#define PanelGroup() DeferLoop(PanelPush(), PanelPop())
#define PanelAxis(Axis) DeferLoop(PanelPushAxis(Axis), PanelPopAxis())
#define PanelAdd(ParentPct) PanelAdd_(PanelCurrent, PanelAppendToParent, ParentPct)

internal inline b32 
IsLeafPanel(panel *Panel)
{
    b32 Result = IsNilPanel(Panel->First);
    return Result;
}

internal panel *
SplitPanel(panel *To, s32 Axis, b32 Backwards)
{
    panel *Result = To;
    
    panel *New = PanelAlloc();
    panel *Parent = To->Parent;
    
    if(!IsNilPanel(To))
    {
        // NOTE(luca): Must be a leaf node.
        Assert(IsLeafPanel(To));
        
        if(IsNilPanel(Parent))
        {
            MemoryCopyStruct(New, To);
            ZeroPanel(To);
            
            To->ParentPct = 1.f;
            To->Axis = Axis;
            
            To->First = New;
            To->Last = New;
            New->Parent = To;
            
            New->Axis = 1 - Axis;
            New->ParentPct = 1.f;
            
            Result = SplitPanel(New, Axis, Backwards);
        }
        else if(Axis == Parent->Axis)
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
            //- Create NewParent replacing Parent
            panel *NewParent = New;
            {
                NewParent->Axis = Axis;
                NewParent->ParentPct = To->ParentPct;
                NewParent->Parent = Parent;
            }
            
            //- Update To and Parent links
            {
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
                
                To->Parent = NewParent;
                NewParent->First = To;
                NewParent->Last = To;
                To->Next = To->Prev = NilPanel;
            }
            
            //- Split To along the same axis 
            {
                To->ParentPct = 1.f;
                Result = SplitPanel(To, Axis, Backwards); 
            }
            
            //- Move kind from parent over to new leaf child 
            {
                Result->Kind = Result->Parent->Kind;
                Result->Parent->Kind = PanelKind_Empty;
            }
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
            
            // NOTE(luca): Put on the free list.
            {
                Panel->Next = App->FreePanel;
                App->FreePanel = Panel;
            }
            
            // NOTE(luca): We should be the last and only child, so we can be merged with the parent.
            if(EqualsWithEpsilon(Collapse->ParentPct, 1.f, 0.001f))
            { 
                // NOTE(luca): Put on the free list.
                {
                    Collapse->Next = App->FreePanel;
                    App->FreePanel = Collapse;
                }
                
                Collapse = Parent;
                Collapse->First = NilPanel;
                Collapse->Last = NilPanel;
                
                // NOTE(luca): Copy the application data into merged parent.
                Collapse->Voice = Panel->Voice;
                Collapse->Kind = Panel->Kind;
            }
            
        }
        else
        {
            // NOTE(luca): This should be the last panel, we just make it an empty one.
            Collapse = Panel;
            Panel->Kind = PanelKind_Empty;
        }
        
    }
    
    if(!IsLeafPanel(Collapse))
    {
        Collapse = PanelNextLeaf(Collapse, false);
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
                    PanelApp->SelectedPanel = Panel;
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
                    
                    panel *RootPanel = PanelApp->FirstPanel;
                    
                    f32 Size = SizeOnAxis(RootPanel->Region, Axis);
                    
                    f32 Pct = (MouseP.e[Axis] - Panel->Parent->Region.Min.e[Axis])/Size;
                    
#if 0                    
                    Log("%.2f\n", Pct);
#endif
                    
                    f32 dPPct = Pct;
                    for(panel *Sibling = Panel; !IsNilPanel(Sibling); Sibling = Sibling->Prev)
                    {
                        dPPct -= Sibling->ParentPct;
                    }
                    
                    // Take from this panel and give to next
                    {                    
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
                
            }
            
            if(0)
            {
                DrawRect(Border, BorderColor, 0.f, 0.f, 0.f);
            }
        }
    }
}

internal panel_rec
PanelRecDepthFirstPreOrder(panel *Panel)
{
    panel_rec Rec = {0};
    
    if(!IsNilPanel(Panel->First))
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

//~ Muze - Notes 
internal b32
IsNilNote(note *Note)
{
    b32 Result = (Note == NilNote || Note == 0);
    return Result;
}

//~ Muze - Voices 
internal void
VoiceReset(voice *Voice)
{
    Voice->NoteCount = 0;
    Voice->MaxPitch = 0;
    Voice->MinPitch = (u8)-1;
    
    Voice->RecordStart = 0.f;
    Voice->RecordLength = 0.f;
    Voice->PlayPos = 0.f;
    
    Voice->FirstNote = NilNote;
    Voice->LastNote = NilNote;
    Voice->NoteSel = 0;
    
    ArenaSetPos(Voice->Arena, 0);
}

internal voice *
VoiceAdd(app_state *App)
{
    voice *Voice = App->Muze.Voices + App->Muze.VoiceCount;
    MemoryZero(Voice);
    Voice->Arena = PushArena(App->Muze.Arena, KB(64), false);
    
    VoiceReset(Voice);
    Voice->Channel = (int)App->Muze.VoiceCount;
    tsf_channel_set_presetindex(GlobalTSF, Voice->Channel, Voice->PresetIdx);
    
    App->Muze.VoiceCount += 1;
    
    return Voice;
}

internal note *
NoteAdd(voice *Voice)
{
    note *Note = NilNote;
    
    // Push on to the back
    {                
        Note = PushArrayZero(Voice->Arena, note, 1);
        
        if(IsNilNote(Voice->FirstNote))
        { 
            Voice->FirstNote = Note;
        }
        
        if(!IsNilNote(Voice->LastNote))
        {
            Voice->LastNote->Next = Note;
        }
        
        Note->Prev = Voice->LastNote;
        Voice->LastNote = Note;
        
        Voice->NoteCount += 1;
    }
    
    return Note;
}

//~ Muze - Record and replay

internal void
PlayNote(app_memory *Memory, app_state *App, voice *Voice, note *Note)
{
    if(App->Muze.IsOutputSynth)
    {
        if(0) {}
        else if(Note->Kind == NoteKind_Note)
        {
            if(Note->Velocity > 0)
            {
                tsf_channel_note_on(GlobalTSF, Voice->Channel, Note->Pitch, (f32)Note->Velocity/127.f);
            }
            else
            {
                tsf_channel_note_off(GlobalTSF, Voice->Channel, Note->Pitch);
            }
        }
        else if(Note->Kind == NoteKind_Pedal)
        {
            tsf_channel_midi_control(GlobalTSF, Voice->Channel, Note->Controller, Note->Velocity);
        }
    }
    else
    {
        // TODO(luca): Channel ?
        // 1. Equip parameters to the Voice struct and use them to create messages here.
        
        midi_message Message = {0};
        switch(Note->Kind)
        {
            case NoteKind_Note:
            {
                Message.U8[0] = MIDIEventType_NoteOn;
                Message.U8[1] = Note->Pitch;
                Message.U8[2] = Note->Velocity;
            } break;
            
            case NoteKind_Pedal:
            {
                Message.U8[0] = MIDIEventType_Control;
                Message.U8[1] = Note->Controller;
                Message.U8[2] = Note->Velocity;
            } break;
        }
        
        Memory->PlatformMIDISend(App->Muze.Out, Message.U32[0]);
    }
}

internal b32
IsNotePlaying(note *Note, voice *Voice)
{
    b32 Result = false;
    
    if(Voice->IsPlaying)
    {
        f32 NoteStart = (Note->Timestamp);
        f32 NoteEnd = (NoteStart + Note->Duration);
        
        Result = (Voice->PlayPos >= NoteStart - dtForFrame &&
                  Voice->PlayPos < NoteEnd + dtForFrame);
    }
    
    if(Voice->IsRecording)
    {
        Result = (Note->Duration == 0.f);
    }
    
    return Result;
}

internal void
StopAllPlayingNotes(app_memory *Memory, app_state *App, voice *Voice)
{
    for EachNote(Note, Voice->FirstNote)
    {
        b32 NoteIsPlaying = IsNotePlaying(Note, Voice);
        if(NoteIsPlaying)
        {
            if(Voice->IsRecording)
            {
                f32 NoteMaxDuration = (Voice->RecordLength - Note->Timestamp);
                f32 Now = (GetWallTime() - Voice->RecordStart);
                Note->Duration = ClampTop(Now - Note->Timestamp, NoteMaxDuration);
            }
            
            note OffNote = *Note;
            OffNote.Velocity = 0;
            PlayNote(Memory, App, Voice, &OffNote);
        }
    }
}

//~ Muze - MIDI
global_variable command NilCommand = {0};

internal command *
NewCommand(app_state *App)
{
    command *Result = &NilCommand;
    
    if(App->Muze.CommandCount < App->Muze.MaxCommandCount)
    {
        Result = App->Muze.Commands + App->Muze.CommandCount;
        App->Muze.CommandCount += 1;
    }
    
    return Result;
}

internal void
MakePanelMuze(panel *Panel, app_state *App)
{
    Panel->Kind = PanelKind_Muze;
    Panel->Voice = VoiceAdd(App);
}

internal void
ProcessMIDINotes(app_memory *Memory, app_state *App, voice *Voice, app_midi_event *Events, u64 Count)
{
    for EachIndex(Idx, Count)
    {
        app_midi_event *Event = Events + Idx;
        f32 Timestamp = Event->Timestamp;
        
#if MUZE_INTERNAL
        // NOTE(luca): This is a hack to get loop editing to work properly.
        Timestamp = (f32)GetWallTime();
#endif
        
        Timestamp -= Voice->RecordStart;
        
        midi_message Message = {Event->Message};
        
        u8 Status = Message.U8[0];
        u8 Data1  = Message.U8[1];
        u8 Data2 = Message.U8[2];
        u8 Type = Status & 0xF0;
        u8 Channel = Status & 0x0F;
        
        b32 IsOutputDeviceDifferent = !(!App->Muze.IsOutputSynth && 
                                        App->Muze.In.Id == App->Muze.Out.Id);
        if(Voice->IsRecording)
        {
            if(0) {} 
            else if(Type == MIDIEventType_NoteOn && Data2 > 0) 
            {
                note *Note = NoteAdd(Voice);
                
                Note->Timestamp = Timestamp;
                Note->Pitch = Data1;
                Note->Velocity = Data2;
                Note->Kind = NoteKind_Note;
                
                Voice->MaxPitch = Max(Note->Pitch, Voice->MaxPitch);
                Voice->MinPitch = Min(Note->Pitch, Voice->MinPitch);
                
                
                if(IsOutputDeviceDifferent)
                {
                    PlayNote(Memory, App, Voice, Note);
                }
            }
            else if(Type == MIDIEventType_NoteOff || 
                    (Type == MIDIEventType_NoteOn && Data2 == 0))
            {
                u8 Pitch = Data1;
                u8 Velocity = Data2;
                
                // NOTE(luca): NoteCount can be 0 if we start recording when a note is still playing.
                if(Voice->NoteCount > 0)
                {
                    b32 NoteFound = false;
                    // Set the duration of the last note with same pitch
                    for EachNoteBack(Note, Voice->LastNote)
                    {
                        if(Note->Kind == NoteKind_Note && 
                           Note->Pitch == Pitch)
                        {
                            NoteFound = true;
                            Note->Duration = (Timestamp - Note->Timestamp);
                            break;
                        }
                    }
                    
                    if(NoteFound)
                    {                
                        note OffNote = {0};
                        OffNote.Pitch = Pitch;
                        if(IsOutputDeviceDifferent)
                        {
                            PlayNote(Memory, App, Voice, &OffNote);
                        }
                    }
                    else
                    {
                        // Discard.
                    }
                }
            }
            else if(Type == MIDIEventType_Control) 
            {
                u8 Controller = Data1;
                if(Controller == 64)
                {
                    u8 Velocity = Data2;
                    
                    b32 On = (Velocity != 0);
                    
                    if(On)
                    {                    
                        note *Note = NoteAdd(Voice);
                        Note->Kind = NoteKind_Pedal;
                        Note->Controller = Controller;
                        Note->Velocity = Velocity;
                        Note->Timestamp = Timestamp;
                        
                        if(IsOutputDeviceDifferent)
                        {
                            PlayNote(Memory, App, Voice, Note);
                        }
                    }
                    else
                    {
                        note *NoteFound = NilNote;
                        
                        // Find matching pedal start
                        {
                            for EachNoteBack(Note, Voice->LastNote)
                            {
                                if(Note->Kind == NoteKind_Pedal)
                                {
                                    NoteFound = Note;
                                    break;
                                }
                            }
                            Assert(!IsNilNote(NoteFound));
                        }
                        
                        NoteFound->Duration = (Timestamp - NoteFound->Timestamp);
                        
                        // Stop pedal
                        {
                            note OffNote = *NoteFound;
                            OffNote.Velocity = 0;
                            
                            if(IsOutputDeviceDifferent)
                            {
                                PlayNote(Memory, App, Voice, &OffNote);
                            }
                        }
                    }
                }
            }
        }
    }
}

internal void 
TogglePlayingAllVoices(app_memory *Memory, app_state *App)
{
    for EachIndex(VoiceIdx, App->Muze.VoiceCount)
    {
        voice *Voice = App->Muze.Voices + VoiceIdx;
        
        if(Voice->IsPlaying)
        {
            Voice->IsPlaying = false;
        }
        else
        {
            StopAllPlayingNotes(Memory, App, Voice);
            Voice->IsPlaying = true;
        }
    }
}


internal void
StopRecording(voice *Voice)
{
    Voice->IsRecording = false;
}

//~ UI 

typedef struct muze_box_data muze_box_data;
struct muze_box_data
{
    ui_box *Box;
    app_state *App;
    voice *Voice;
    
    f32 ScrollX;
};

internal ui_box *
UI_Label(str8 String)
{
    ui_box *Result = UI_AddBox(String, (UI_BoxFlag_Clip|
                                        UI_BoxFlag_DrawDisplayString|
                                        UI_BoxFlag_DrawBackground|
                                        UI_BoxFlag_DrawBorders|
                                        UI_BoxFlag_CenterTextHorizontally|
                                        UI_BoxFlag_CenterTextVertically));
    return Result;
}

internal ui_box *
UI_Labelf(char *Format, ...)
{
    ui_box *Result = UI_NilBox;
    str8 String = {0};
    
    va_list Args;
    va_start(Args, Format);
    String = Str8VFmt(Format, Args);
    
    Result = UI_Label(String);
    
    return Result;
}

internal void
ArenaLabel(str8 Name, arena *Arena)
{
    f32 PosDigits = floorf(log10f((f32)Arena->Pos)/3.f);
    f32 SizeDigits = floorf(log10f((f32)Arena->Size)/3.f);
    
    struct display_unit
    {
        u64 Value;
        str8 Name;
    }; 
    
    struct display_unit UnitTable[] = 
    {
        {1,     S8("B")},
        {KB(1), S8("KB")},
        {MB(1), S8("MB")},
        {GB(1), S8("GB")},
        {TB(1), S8("TB")},
    };
    
    s32 PosIdx = (s32)(PosDigits);
    s32 SizeIdx = (s32)(SizeDigits);
    
    struct display_unit PosUnit = UnitTable[PosIdx];
    struct display_unit SizeUnit = UnitTable[SizeIdx];
    
    UI_Labelf(S8Fmt 
              ": %.3f" S8Fmt 
              "/%.1f" S8Fmt, 
              S8Arg(Name), 
              (f32)Arena->Pos/(f32)PosUnit.Value, 
              S8Arg(PosUnit.Name), 
              (f32)Arena->Size/(f32)SizeUnit.Value,
              S8Arg(SizeUnit.Name));
}

//- Custom elements 

UI_CUSTOM_DRAW(CustomDrawSheetMusic)
{
    muze_box_data *Data = (muze_box_data *)CustomDrawData;
    ui_box *Box = Data->Box;
    voice *Voice = Data->Voice;
    f32 ScrollX = Data->ScrollX;
    app_input *Input = UI_State->Input;
    app_state *App = Data->App;
    
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
    
    f32 BPS = (App->Muze.BPM/60.f);
    
    // Staff Lines
    v2 StaffPos = BoxPos;
    s32 StaffLineCount = 5;
    f32 StaffLineWidth = 2.f;
    f32 StaffHeight = (f32)(StaffLineCount-1)*NoteSize + StaffLineWidth;
    
    // Bars
    f32 WholeBarWidth = 100.f;
    f32 BarDuration = (1.f/BPS)*(f32)App->Muze.TimeSig;
    
    // NOTE(luca): When it is exactly the length it is wrong, so we add a little offset to unmatch it.
    // TODO(luca): intrinsic
    f32 BarCount = 1.f + floorf(Voice->RecordLength/(BarDuration + .001f));
    v2 BarDim = V2(2.f, StaffHeight);
    
    // Draw staff
    {    
        StaffPos.Y += .5f*(BoxSize.Y - StaffHeight);
        {   
            for EachIndex(Idx, 5)
            {
                f32 X = StaffPos.X;
                f32 Y = StaffPos.Y + (f32)Idx*(NoteSize);
                
                f32 Width = (BarCount)*WholeBarWidth;
                
                v4 Dest = RectFromSize(V2(X, Y), V2(Width, StaffLineWidth));
                Dest = RectIntersect(Dest, Box->Rec);
                
                DrawRect(Dest, ForegroundColor, 0.f, 0.f, 0.f); 
            }
        }
    }
    
    // Draw bars
    {    
        for EachIndex(Idx, (s32)BarCount)
        {
            v2 BarPos = V2(BoxPos.X + ((f32)(Idx + 1) * WholeBarWidth),
                           StaffPos.Y);
            DrawRect(RectFromSize(BarPos, BarDim), BarColor, 0.f, 0.f, 0.f); 
        }
    }
    
    for EachNote(Note, Voice->FirstNote)
    {        
        if(Note->Kind == NoteKind_Note)
        {
            NoteColor = Box->TextColor;
            for EachNode(Node, note_node, Voice->NoteSel)
            {
                if(Node->Value == Note)
                {
                    NoteColor = Color_Blue;
                }
            }
            
            b32 NoteIsPlaying = IsNotePlaying(Note, Voice);
            if(NoteIsPlaying)
            {
                NoteColor = Color_Yellow;
            }
            
            Assert(!(Voice->IsPlaying && Note->Duration == 0.f));
            
            u8 PitchClass = (Note->Pitch%Note_Count);
            
            s32 Length = 1;
            
            f32 NoteBorderSize = 0.f;
            
            f32 Duration = Note->Duration;
            if(Duration == 0.f)
            {
                Assert(Voice->IsRecording);
                f32 Now = (GetWallTime() - Voice->RecordStart);
                Duration = (Now - Note->Timestamp);
            }
            
            f32 NoteLength = (Duration*BPS);
            f32 NoteStart = (Note->Timestamp);
            // NOTE(luca): Get pos relative of time inside bar 
            
            f32 NoteX = (NoteStart*BPS/(f32)App->Muze.TimeSig)*WholeBarWidth;
            
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
    
}

UI_CUSTOM_DRAW(CustomDrawPianoRoll)
{
    muze_box_data *Data = (muze_box_data *)CustomDrawData;
    ui_box *Box = Data->Box;
    voice *Voice = Data->Voice;
    app_state *App = Data->App;
    
    app_input *Input = UI_State->Input;
    
    b32 MouseLeftDown = false;
    b32 MouseRightDown = false;
    v2 MouseP = {0};
    v2 MouseStartP = {0};
    v4 SelDest = {0};
    
    f32 BPS = App->Muze.BPM/60.f;
    
    f32 Zoom = (BPS/(f32)App->Muze.TimeSig*100.f);
    
    // Get Input
    {    
        if(!Input->Consumed)
        {
            MouseLeftDown = Input->Mouse.Buttons[PlatformMouseButton_Left].EndedDown;
            MouseRightDown = Input->Mouse.Buttons[PlatformMouseButton_Right].EndedDown;
        }
        MouseP = MousePosFromInput(Input);
        MouseStartP = V2S32(Input->Mouse.StartX, Input->Mouse.StartY);
        
        // Get selection rectangle
        {        
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
        }
        
        if(MouseLeftDown || MouseRightDown)
        {
            b32 Adding = (MouseLeftDown && !MouseRightDown);
            DrawRect(SelDest, V4(V3Arg(Adding ? Color_Blue : Color_Snow2), .3f), 1.f, 0.f, 0.f);
            DrawRect(SelDest, V4(V3Arg(Adding ? Color_Orange : Color_Black), 1.f), 1.f, 1.f, 0.f);
        }
    }
    
    u8 MaxPitch = Max(Voice->MaxPitch, 75);
    u8 MinPitch = Min(Voice->MinPitch, 40);
    
    // Piano roll
    {
        // NOTE(luca): One extra note for pedal
        u8 Range = (MaxPitch - MinPitch) + 1;
        
        f32 NoteHeight = (Box->FixedSize.Y/(f32)Range);
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
            
            for EachIndex(Idx, Range)
            {
                f32 YOffset = NoteHeight*(f32)(Range - Idx - 1);
                
                f32 X = (PianoPos.X);
                f32 Y = (PianoPos.Y + YOffset);
                
                // NOTE(luca): Minus one for pedal
                u8 PitchClass = ((MinPitch + (u8)Idx)%Note_Count);
                b32 White = NotePianoColors[PitchClass];
                
                // NOTE(luca): First note is pedal.
                v4 KeyColor = (Idx == 0 ?
                               Color_Orange :
                               (White ? Color_Snow0 : Color_Black));
                KeyColor.A = .2f; 
                
                v4 NoteRec = RectFromSize(V2(X, Y), V2(PianoWidth, NoteHeight - PianoKeyGap));
                v4 Dest = RectIntersect(NoteRec, Box->Rec);
                DrawRect(Dest, KeyColor, 0.f, 0.f, 0.f);
            }
        }
        
        // Draw the roll and markers
        {        
            // NOTE(luca): This is also the current time when recording.
            f32 RecordMarkerX;
            {
                f32 Timestamp = Max(Voice->RecordLength, 0.f);
                f32 ZoomedTimestamp = Timestamp*Zoom;
                RecordMarkerX = RollX + roundf(ZoomedTimestamp);
            }
            
            f32 PlayMarkerX;
            {
                f32 Timestamp = Voice->PlayPos;
                f32 ZoomedTimestamp = Timestamp*Zoom;
                PlayMarkerX = RollX + roundf(ZoomedTimestamp); 
            }
            
            for EachNote(Note, Voice->FirstNote)
            {
                b32 NoteIsRecording = (Note->Duration == 0.f);
                
                // TODO(luca): Visualize
                u8 Velocity = Note->Velocity;
                
                f32 StartX = roundf(Note->Timestamp*Zoom);
                
                // NOTE(luca): We have to flip the Y since we have TopDown coordinates for UI.
                // NOTE(luca): Minus one for pedal note
                f32 StartY = NoteHeight*(f32)((Range - 1) - (Note->Pitch - MinPitch));
                
                if(Note->Kind == NoteKind_Pedal)
                {
                    StartY = NoteHeight*(f32)(Range - 1);
                }
                
                f32 Width;
                {                
                    f32 EndX = StartX + NoteWidth;
                    if(NoteIsRecording)
                    {
                        EndX = RecordMarkerX - RollX;
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
                for EachNode(Node, note_node, Voice->NoteSel)
                {
                    if(Node->Value == Note)
                    {
                        Sel = Node;
                        break;
                    }
                }
                
                v4 NoteColor = (IsNotePlaying(Note, Voice) ? Color_Yellow :
                                (Sel ? Color_Blue : 
                                 ((Note->Kind == NoteKind_Pedal) ? Color_Orange : 
                                  (White ? Color_Snow0 : Color_Black))));
                
                v4 NoteDest = RectFromSize(NotePos, V2(Width, NoteHeight)); 
                v4 Dest = RectIntersect(NoteDest, Box->Rec);
                
                // Draw Note
                
                rect_instance *Instance = DrawRect(Dest, NoteColor, 5.f, 0.f, .5f);
                Instance->Color0.A = .2f;
                Instance->Color2.A = .2f;
                
                // Draw selection rectangle
                if(MouseLeftDown || MouseRightDown)
                {
                    b32 Adding = (MouseLeftDown && !MouseRightDown);
                    
                    // NOTE(luca): @Command
                    if(RectOverlap(Dest, SelDest))
                    {
                        if(Adding && !Sel)
                        {
                            note_node *Node;
                            
                            // Allocate
                            {
                                if(!App->Muze.FirstFreeNode)
                                {
                                    Node = PushArray(App->Muze.Arena, note_node, 1);
                                }
                                else
                                {
                                    Node = App->Muze.FirstFreeNode;
                                    App->Muze.FirstFreeNode = Node->Next;
                                }
                                MemoryZero(Node);
                            }
                            
                            // Add to selection
                            {                            
                                Node->Value = Note;
                                
                                if(Voice->NoteSel) 
                                {
                                    Voice->NoteSel->Prev = Node;
                                }
                                Node->Next = Voice->NoteSel;
                                
                                Voice->NoteSel = Node;
                            }
                        }
                        else if(!Adding && Sel)
                        {
                            // Remove from selection
                            {
                                if(Sel->Prev)
                                {
                                    Sel->Prev->Next = Sel->Next;
                                }
                                
                                if(Sel->Next)
                                {
                                    Sel->Next->Prev = Sel->Prev;
                                }
                                
                                if(Sel == Voice->NoteSel)
                                {
                                    Voice->NoteSel = (Sel->Next ? Sel->Next : Sel->Prev);
                                }
                            }
                            
                            // Add to free list
                            {
                                Sel->Next = App->Muze.FirstFreeNode;
                                App->Muze.FirstFreeNode = Sel;
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
                v4 RecordDest = RectFromSize(V2(RecordMarkerX, RollY), 
                                             V2(2.f, RollHeight));
                v4 PlayDest = RectFromSize(V2(PlayMarkerX, RollY), 
                                           V2(2.f, RollHeight));
                
                DrawRect(RectIntersect(RecordDest, Box->Rec), Color_Red, 0.f, 0.f, 0.f);
                DrawRect(RectIntersect(PlayDest, Box->Rec), Color_Green, 0.f, 0.f, 0.f);
            }
        }
    }
}


internal void
UI_ListBegin(str8 Name, f32 ItemHeight)
{
    UI_SemanticHeight(UI_SizeChildren(1.f))
        UI_LayoutAxis(Axis2_Y)
        UI_AddBox(Name, UI_BoxFlag_Clip);
    UI_PushBox();
    
    UI_PushSemanticHeight(UI_SizePx(ItemHeight, 1.f));
    
    UI_Label(Name);
    UI_Spacer(UI_SizeEm(.2f, 1.f));
}

internal void
UI_ListEnd(void)
{
    UI_PopSemanticHeight();
    UI_PopBox();
}

#define UI_List(Name, ItemHeight) DeferLoop(UI_ListBegin(Name, ItemHeight), UI_ListEnd())

//~ EntryPoint
C_LINKAGE
UPDATE_AND_RENDER(UpdateAndRender)
{
    b32 ShouldQuit = false;
    
    //- Inherit globals from platform layer
#if MUZE_INTERNAL
    GlobalDebuggerIsAttached = Memory->IsDebuggerAttached;
#endif
#if OS_WINDOWS
    GlobalPerfCountFrequency = Memory->PerfCountFrequency;
#endif
    GlobalIsProfiling = Memory->IsProfiling;
    ThreadContext = Memory->ThreadCtx;
    ExeDirPath = Memory->ExeDirPath;
    
    //- Some constants 
    Input->PlatformCursor = PlatformCursorShape_Arrow;
    f32 WindowBorderSize = 2.f;
    v2 BufferDim = V2S32(Buffer->Width, Buffer->Height);
    dtForFrame = Input->dtForFrame;
    
    //- Init state 
    OS_ProfileInit(" G");
    
    arena *PermanentArena;
    app_state *App;
    {
        PermanentArena = (arena *)Memory->Memory;
        PermanentArena->Size = Memory->MemorySize - sizeof(arena);
        PermanentArena->Base = (u8 *)Memory->Memory + sizeof(arena);
        PermanentArena->Pos = 0;
        AsanPoisonMemoryRegion(PermanentArena->Base, PermanentArena->Size);
        
        // App
        {        
            App = PushArray(PermanentArena, app_state, 1);
            
            App->FontAtlasArena = PushArena(PermanentArena, MB(100), false);
            App->UIArena = PushArena(PermanentArena, MB(64), false);
            App->Muze.Arena = PushArena(PermanentArena, MB(64), false);
            App->TSFArena = PushArena(PermanentArena, MB(500), false);
            
            App->ReadOnlyArena = PushArray(PermanentArena, arena, 1);
            App->PanelArena = PushArena(PermanentArena, ArenaAllocDefaultSize, false);
        }
        
        // Globals
        {
            FrameArena = PushArena(PermanentArena, MB(64), true);
            
            UI_State = PushArray(PermanentArena, ui_state, 1);
            NilTSF = PushArray(PermanentArena, tsf, 1);
            NilTSF->arena = App->TSFArena;
            
            // Panels
            {
                PanelInput = Input;
                PanelApp = App;
                PanelArena = App->PanelArena;
            }
            
            GlobalTSF = App->TrackerForTSF;
            StringsScratch = FrameArena;
        }
    }
    
    platform_midi_get_devices_result DevicesArray = Memory->PlatformMIDIGetDevices();
    
    local_persist s32 GLADVersion = 0;
    if(Memory->Reloaded)
    {
        GLADVersion = gladLoaderLoadGL();
        OS_ProfileAndPrint("glad Init");
    }
    
    if(!Memory->Initialized)
    {
        //- Init TSF 
        {        
            char *Path = PathFromExe(FrameArena, S8("../data/muze/sounds/sounds.sf2"));
            str8 File = OS_ReadEntireFileIntoMemory(Path);
            GlobalTSF = tsf_load_memory(App->TSFArena, File.Data, (int)File.Size);
            if(GlobalTSF == 0)
            {
                GlobalTSF = NilTSF;
            }
            tsf_set_output(GlobalTSF, TSF_STEREO_INTERLEAVED, 48000, -10);
            
            App->TrackerForTSF = GlobalTSF;
        }
        
        OS_ProfileAndPrint("TSF init");
        
        //- Init Muze 
        {
            App->Muze.TimeSig = 3;
            App->Muze.BPM = 100.f;
            App->Muze.IsOutputSynth = true;
            App->Muze.IsInputVirtualKeyboard = true;
            App->Muze.MaxVoiceCount = 5;
            App->Muze.Voices = PushArray(App->Muze.Arena, voice, App->Muze.MaxVoiceCount);
            App->Muze.LastSelectedVoice = App->Muze.Voices;
            
            App->Muze.MaxCommandCount = KB(4);
            App->Muze.Commands = PushArray(App->Muze.Arena, command, App->Muze.MaxCommandCount);
            
            // Select default input and output device
            {
                b32 InputFound = false;
                b32 OutputFound = false;
                for EachIndex(Idx, DevicesArray.Count)
                {
                    platform_midi_device *Device = DevicesArray.Devices + Idx;
                    if(Device->IsOutput)
                    {
                        App->Muze.Out = *Device;
                        OutputFound = true;
                    }
                    if(Device->IsInput)
                    {
                        App->Muze.In = *Device;
                        InputFound = true;
                    }
                    
                    if(InputFound && OutputFound)
                    {
                        break;
                    }
                }
            }
        }
        
        OS_ProfileAndPrint("Muze init");
        
        //- Init Fonts
        {
            {
                char *FontPath = PathFromExe(FrameArena, S8("../data/font_regular.ttf"));
                InitFont(&App->Font, FontPath);
            }
            {
                char *FontPath = PathFromExe(FrameArena, S8("../data/icons.ttf"));
                InitFont(&App->IconsFont, FontPath);
            }
            
            App->PreviousHeightPx = DefaultHeightPx + 1.f;
            App->HeightPx = DefaultHeightPx;
            App->FrameIdx = 0;
        }
        
        OS_ProfileAndPrint("Font init");
        
        //- Read only structs 
        {        
            arena *Arena = App->ReadOnlyArena;
            
            // Initialize arena
            {            
                MemoryZero(Arena);
                Arena->Size = MB(1);
                Arena->Base = OS_AllocateAtOffset(Arena->Size, TB(3));
            }
            
            // Initialize read only structs
            {
                ui_box *Box = PushArray(Arena, ui_box, 1);
                *Box = (ui_box){Box, Box, Box, Box, Box, Box, Box};
                
                panel *Panel = PushArray(Arena, panel, 1);
                *Panel = (panel){Panel, Panel, Panel, Panel, Panel};
                Panel->Root = Box;
                
                note *Note = PushArray(Arena, note, 1);
                *Note = (note){Note, Note};
            }
            
            OS_MarkReadonly(Arena->Base, Arena->Size);
            
            InitReadOnlyGlobals(Arena);
        }
        OS_ProfileAndPrint("RO Init");
        
        RenderInit(&App->Render);
        OS_ProfileAndPrint("Renderer Init");
        
        // Init UI 
        {
            UI_State->Arena = App->UIArena;
            UI_State->BoxTableSize = 4096;
            UI_State->BoxTable = PushArray(UI_State->Arena, ui_box, UI_State->BoxTableSize);
        }
        
        // Init panels
        { 
            App->FirstPanel = PanelAdd(1.f);
            
            App->SelectedPanel = PanelNextLeaf(App->FirstPanel, false);
            MakePanelMuze(App->SelectedPanel, App);
        }
        
    }
    
    voice *Voice;
    // Frame Initialization
    {
        // Init read-only variables
        {    
            InitReadOnlyGlobals(App->ReadOnlyArena);
        }
        
        // Init Muze
        {
            App->Muze.CommandCount = 0;
            Voice = App->Muze.LastSelectedVoice;
        }
        
        OS_ProfileAndPrint("Misc. Init");
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
            if(!Control)
            {
                switch(Key.Codepoint)
                {
                    
                    default: break;
                }
                
            }
            else
            {
                switch(Key.Codepoint)
                {
                    case 'b': DebugBreak();  break;
                    
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
    local_persist f32 ScrollX = 0.f;
    local_persist f32 ScrollVelX = 0.f;
    
    // TODO(luca): Exponential smoothing curve
    // Scrolling
    {    
        f32 dt = dtForFrame;
        
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
        
        ScrollX += ScrollVelX * dt;
        ScrollX = Max(0.f, ScrollX);
    }
    
    OS_ProfileAndPrint("Input");
    
    RenderBeginFrame(FrameArena, Buffer->Width, Buffer->Height);
    
    OS_ProfileAndPrint("Render setup");
    
    App->HeightPx = DefaultHeightPx;
    
    if(App->PreviousHeightPx != App->HeightPx)
    {
        App->PreviousHeightPx = App->HeightPx;
        
        RenderBuildAtlas(App->FontAtlasArena, &App->FontAtlas, &App->Font, &App->IconsFont, 
                         App->HeightPx);
    }
    
    OS_ProfileAndPrint("Atlas");
    
    // UI
    {
        //- UI Setup
        {    
            UI_State->Atlas = &App->FontAtlas;
            UI_State->FrameIdx = App->FrameIdx;
            UI_State->Input = Input;
            UI_State->FrameArena = FrameArena;
        }
        
        OS_ProfileAndPrint("UI setup");
        
        // NOTE(luca): There are two UI passes, one for the top controls and on for the bottom half which will be the panels.  The free space for the panels is calculated in the first pass.
        // Maybe we can merge them?
        ui_box *RootPanelBox;
        
        //- UI Top "Controls" 
        {
            if(UI_IsNilBox(App->Muze.TopBox))
            {
                App->Muze.TopBox = UI_BoxAlloc(App->UIArena);
            }
            ui_box *Root = App->Muze.TopBox;
            
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
            
            UI_LayoutAxis(Axis2_Y)
                UI_AddBox(S8(""), UI_BoxFlag_Clip);
            
            UI_Push()
                //- All
            {       
                //- Top 
                {
                    UI_SemanticHeight(UI_SizeChildren(1.f))
                        UI_FillWidth()
                        UI_LayoutAxis(Axis2_X)
                        UI_AddBox(S8("TopControls"), UI_BoxFlag_Clip);
                    UI_Push()
                    {                    
                        UI_SemanticWidth(UI_SizePx(300.f, 1.f))
                        {
                            f32 ItemHeight = UI_State->HeightPxTop->Value + 2.f*8.f;
                            
                            UI_List(S8("Input"), ItemHeight) 
                            {                            
                                App->Muze.IsInputVirtualKeyboard ^= UI_Checkbox(S8("Keyboard"), App->Muze.IsInputVirtualKeyboard, Color_Yellow);
                                
                                UI_SemanticHeight(UI_SizeChildren(1.f))
                                    UI_LayoutAxis(Axis2_X)
                                    UI_AddBox(S8("Instruments"), UI_BoxFlag_Clip);
                                
                                s32 InstrumentCount = (0 ? tsf_get_presetcount(GlobalTSF) : 5);
                                f32 ListHeight = (f32)InstrumentCount*ItemHeight;
                                
                                UI_SemanticHeight(UI_SizePx(ListHeight, 1.f))
                                    UI_Push()
                                {
                                    UI_FillWidth()
                                        UI_LayoutAxis(Axis2_Y)
                                        UI_AddBox(S8("Input devices"), UI_BoxFlag_Clip);
                                    UI_FillWidth()
                                        UI_Push()
                                    {                                        
                                        for EachIndex(Idx, InstrumentCount)
                                        {
                                            str8 InstrumentName = Str8Fmt("%s", tsf_get_presetname(GlobalTSF, Idx));
                                                b32 Selected = (Voice->PresetIdx == Idx);
                                                
                                                b32 Clicked = false;
                                                ui_size BorderPadding = UI_SizePx(5.f, 1.f);
                                                UI_SemanticHeight(UI_SizePx(ItemHeight, 1.f))
                                                {                                     
                                                    UI_BackgroundColor(Selected ? Color_Yellow : Color_ButtonBackground)
                                                        Clicked = UI_AddBox(InstrumentName, 
                                                                            UI_BoxFlag_Clip|
                                                                            UI_BoxFlag_MouseClickable|
                                                                            UI_BoxFlag_DrawBackground|
                                                                            UI_BoxFlag_DrawBorders|
                                                                            UI_BoxFlag_DrawHotEffects|
                                                                            UI_BoxFlag_DrawActiveEffects)->WasClicked;
                                                    UI_Push()
                                                        UI_FillAll() 
                                                        UI_Column() UI_Padding(BorderPadding)
                                                        UI_Row() UI_Padding(BorderPadding)
                                                        UI_AddBox(InstrumentName, 
                                                                  UI_BoxFlag_Clip|
                                                                  UI_BoxFlag_DrawDisplayString|
                                                                  UI_BoxFlag_CenterTextVertically);
                                                    if(Clicked)
                                                {
                                                    command *Command = NewCommand(App);
                                                    Command->Kind = Command_SelectInstrument;
                                                    Command->PresetIdx = Idx;
                                                    }
                                                }
                                            }
                                    }
                                    
                                    ui_box *ScrollbarBox;
                                    UI_SemanticWidth(UI_SizePx(16.f, 1.f))
                                        ScrollbarBox = UI_AddBox(S8("Scrollbar"), 
                                                  UI_BoxFlag_Clip|
                                                  UI_BoxFlag_DrawBackground|
                                                             UI_BoxFlag_DrawBorders);
                                    
                                    UI_Push()
                                    {
                                        ui_size Padding = UI_SizePx(UI_BorderThicknessTop(), 1.f);
                                        UI_FillAll()
                                            UI_Row() UI_Padding(Padding)
                                            UI_Column() UI_Padding(Padding)
                                        {    
                                            local_persist f32 PctOnYAxis = 0.f;
                                            
                                            PctOnYAxis = Clamp(0.f, PctOnYAxis, 1.f);
                                            UI_SemanticHeight(UI_SizeParent(PctOnYAxis, 1.f))
                                                UI_AddBox(S8(""), UI_BoxFlag_Clip);

                                            ui_box *Slider;
                                            UI_FillWidth()
                                                UI_SemanticHeight(UI_SizePx(ItemHeight, 1.f))
                                                UI_BorderThickness(2.f)
                                                UI_BorderColor(Color_Black)
                                                UI_BackgroundColor(Color_Yellow)
                                                Slider = UI_AddBox(S8("Slider"), 
                                                                   UI_BoxFlag_Clip|
                                                                   UI_BoxFlag_MouseClickable|
                                                                   UI_BoxFlag_DrawBackground|
                                                                   UI_BoxFlag_DrawBorders|
                                                                   UI_BoxFlag_DrawHotEffects|
                                                                   UI_BoxFlag_DrawActiveEffects);

                                            ui_box *Box = ScrollbarBox;
                                            
                                            //1. Get the position of initial click
                                            //2. Get the travel distance dragged as delta percent
                                            //3. Calculate the new position in function of start + delta
                                            //3. Clamp the Y position to (FixedSize.Y - ItemHeight)
                                            
                                            f32 RelY = ((f32)Input->Mouse.Y - Box->FixedPosition.Y - ItemHeight);
                                            f32 Height = (Box->FixedSize.Y);
                                            if(Input->Mouse.Buttons[PlatformMouseButton_Left].EndedDown)
{
                                            PctOnYAxis = (RelY/Height);
                                            Log("%.3f\n", PctOnYAxis);
}
                                            
                                        }
                                    }
                                }
                            }
                            UI_List(S8("Music"), ItemHeight)
                            {
                                if(UI_Checkbox(S8("Record"), Voice->IsRecording, Color_Red))
                                {
                                    command *Command = NewCommand(App);
                                    Command->Kind = Command_ToggleRecording;
                                }
                                
                                if(UI_Checkbox(S8("Play"), Voice->IsPlaying, Color_Green))
                                {
                                    command *Command = NewCommand(App);
                                    Command->Kind = Command_TogglePlaying;
                                }
                            }
                            
                            UI_List(S8("Debug"), ItemHeight)
                            {
                                UI_State->RectDebugMode ^= UI_Checkbox(S8("UI Rects"), UI_State->RectDebugMode, V4(1.f, 0.f, 1.f, 1.f));
                                
                                
                                ui_box *Hot = UI_BoxFromKey(UI_State->Hot);
                                ui_box *Active = UI_BoxFromKey(UI_State->Active);
                                str8 ActiveDisplayString = Active->String;
                                str8 HotDisplayString = Hot->String;
                                if(ActiveDisplayString.Size == 0) ActiveDisplayString = S8("null");
                                if(HotDisplayString.Size == 0) HotDisplayString = S8("null");
                                
                                ui_box *Box;
                                
                                if(UI_IsNilBox(Active))
                                {
                                    Log("Null.\n");
                                }
                                
                                Box = UI_Labelf("Active");
                                Box->DisplayString = Str8Fmt("Active: " S8Fmt, S8Arg(ActiveDisplayString));
                                Box = UI_Labelf("Hot");
                                Box->DisplayString = Str8Fmt("Hot: " S8Fmt, S8Arg(HotDisplayString));
                                
                                UI_Labelf("Active->tHot = %.2f", Active->tHot);
                                UI_Labelf("Active->tActive = %.2f", Active->tActive);
                                UI_Labelf("Hot->tHot = %.2f", Hot->tHot);
                                UI_Labelf("Hot->tActive = %.2f", Hot->tActive);
                            }
                            
                            
                        }
                    }
                }
                
                UI_FillWidth() UI_SemanticHeight(UI_SizePx(200.f, 1.f))
                    UI_BackgroundColor(Color_Night3)
                    UI_TextColor(Color_Snow2)
                {
                    ui_box *Box = UI_AddBox(S8("MuzeSheetMusic"), UI_BoxFlag_Clip|UI_BoxFlag_DrawBorders);
                    
                    muze_box_data *Data = PushArray(FrameArena, muze_box_data, 1);
                    Data->Box = Box;
                    Data->Voice = Voice;
                    Data->ScrollX = ScrollX;
                    Data->App = App;
                    
                    Box->CustomDraw = CustomDrawSheetMusic;
                    Box->CustomDrawData = Data;
                }
                
                UI_FillAll()
                    UI_LayoutAxis(Axis2_Y)
                    RootPanelBox = UI_AddBox(S8("PanelRoot"), UI_BoxFlag_Clip);
            }
            
            UI_ResolveLayout(Root->First);
        }
        
        // UI Panels
        {
            PanelGetRegionAndInput(App->FirstPanel, RootPanelBox->Rec);
            
            for(panel *Panel = App->FirstPanel; 
                !IsNilPanel(Panel); 
                Panel = PanelRecDepthFirstPreOrder(Panel).Next)
            {
                if(UI_IsNilBox(Panel->Root))
                {
                    Panel->Root = UI_BoxAlloc(App->UIArena);
                }
                Panel->Root->Rec = Panel->Region;
                Panel->Root->FixedPosition = Panel->Region.Min;
                Panel->Root->FixedSize = SizeFromRect(Panel->Region);
                
                UI_DefaultState(Panel->Root, App->HeightPx);
                
                UI_FillAll()
                    UI_LayoutAxis(Axis2_Y)
                    UI_AddBox(Str8Fmt("%p", Panel), 0);
                UI_Push()
                {
                    if(0) {}
                    else if(Panel->Kind == PanelKind_Empty)
                    {
                        if(IsLeafPanel(Panel))
                        {
                            UI_FillAll() UI_Column() UI_Padding(UI_SizeParent(1.f, 0.f))
                                UI_FillAll() UI_Row() UI_Padding(UI_SizeParent(1.f, 0.f))
                            {
                                UI_SemanticWidth(UI_SizeText(4.f, 1.f))
                                    UI_SemanticHeight(UI_SizeText(2.f, 1.f))
                                {
                                    if(UI_Button(S8("New Panel")))
                                    {
                                        MakePanelMuze(Panel, App);
                                    }
                                }
                            }
                        }
                    }
                    else if(Panel->Kind == PanelKind_Muze)
                    {        
                        UI_FillAll()
                        {
                            ui_box *Box = UI_AddBox(S8("MuzePianoRoll"), UI_BoxFlag_Clip|UI_BoxFlag_DrawBorders);
                            
                            muze_box_data *Data = PushArray(FrameArena, muze_box_data, 1);
                            Data->Box = Box;
                            Data->Voice = Panel->Voice;
                            Data->ScrollX = ScrollX;
                            Data->App = App;
                            
                            Box->CustomDraw = CustomDrawPianoRoll;
                            Box->CustomDrawData = Data;
                        }
                    }
                }
                
                UI_ResolveLayout(Panel->Root->First);
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
    }
    
    for EachIndex(Idx, App->Muze.CommandCount)
    {
        command *Command = App->Muze.Commands + Idx;
        
        switch(Command->Kind)
        {
            default: InvalidPath(); break;
            
            case Command_ToggleRecording:
            {
                StopAllPlayingNotes(Memory, App, Voice);
                
                if(Voice->IsRecording)
                {
                    StopRecording(Voice);
                }
                else
                {
                    VoiceReset(Voice);
                    Voice->RecordStart = GetWallTime();
                    Voice->PlayPos = 0.f;
                    Voice->IsRecording = true;
                }
                
                Voice->IsPlaying = false;
            } break;
            
            case Command_TogglePlaying:
            {
                StopAllPlayingNotes(Memory, App, Voice);
                StopRecording(Voice);
                
                Voice->IsPlaying ^= 1;
            } break;
            
            case Command_SelectInstrument:
            {
                if(Command->PresetIdx != Voice->PresetIdx)
                {
                tsf_channel_note_off_all(GlobalTSF, Voice->Channel);
}
                
                Voice->PresetIdx = Command->PresetIdx;
                    tsf_channel_set_presetindex(GlobalTSF, Voice->Channel, Voice->PresetIdx);
            } break;
            
            case Command_SelectInputDevice:
            {
                StopAllPlayingNotes(Memory, App, Voice);
                
                App->Muze.In = *Command->Device;
                Memory->PlatformMIDIListen(App->Muze.In);
            } break;
            
        }
    }
    
    // Window borders
    // TODO(luca): Move to platform
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
    
    // NOTE(luca): N^M ? But I won't ever have many voices, yes?  If I do, I can just cache the last played note and loop form there as an optimization.
    
    for EachIndex(Idx, App->Muze.VoiceCount)
    {    
        voice *VoiceAt = App->Muze.Voices + Idx;
        if(VoiceAt->IsPlaying)
        {
            for EachNote(Note, VoiceAt->FirstNote)
            {
                f32 NoteStart = Note->Timestamp;
                f32 NoteEnd = NoteStart + Note->Duration;
                
                // TODO(luca): If a note is shorter than dtForFrame it could skip it.
                
                if(NoteStart <= VoiceAt->PlayPos &&
                   NoteStart > (VoiceAt->PlayPos - dtForFrame))
                {
                    PlayNote(Memory, App, VoiceAt, Note);
                }
                
                if(NoteEnd <= VoiceAt->PlayPos &&
                   NoteEnd > (VoiceAt->PlayPos - dtForFrame))
                {
                    note OffNote = *Note;
                    OffNote.Velocity = 0;
                    PlayNote(Memory, App, VoiceAt, &OffNote);
                }
            }
            
            VoiceAt->PlayPos += dtForFrame;
            if(VoiceAt->PlayPos >= VoiceAt->RecordLength)
            {
                StopAllPlayingNotes(Memory, App, VoiceAt);
                
                VoiceAt->PlayPos = 0.f;
            }
        }
        
        if(VoiceAt->IsRecording)
        {
            VoiceAt->RecordLength = (GetWallTime() - VoiceAt->RecordStart);
            
            if(VoiceAt->RecordLength >= MaxRecordingLength)
            {
                StopRecording(VoiceAt);
            }
        }
    }
    
    // MIDI Processing
    {    
        //- MIDI notes from virtual keyboard 
        if(App->Muze.IsInputVirtualKeyboard)
        {
            u64 MaxNotesPerFrame = 128; 
            app_midi_event *Events = PushArray(FrameArena, app_midi_event, MaxNotesPerFrame);
            u64 EventCount = 0;
            
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
                    
                    app_midi_event *Event = Events + EventCount;
                    EventCount += 1;
                    Event->Timestamp = (f32)OS_GetWallClock();
                    
                    midi_message Message = {0};
                    
                    if(IsOn)
                    {
                        Message.U8[0] = MIDIEventType_NoteOn;
                        Message.U8[1] = Pitch;
                        Message.U8[2] = Velocity;
                    }
                    else if(IsOff)
                    {
                        Message.U8[0] = MIDIEventType_NoteOff;
                        Message.U8[1] = Pitch;
                    }
                    
                    Event->Message = Message.U32[0];
                }
            }
            
            ProcessMIDINotes(Memory, App, Voice, Events, EventCount);
        }
        
        //- MIDI notes from Input 
        ProcessMIDINotes(Memory, App, Voice, Input->MIDI.Events, Input->MIDI.EventCount);
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
    ShouldQuit = true;
#endif
    
    return ShouldQuit;
}

GET_AUDIO_SAMPLES(GetAudioSamples)
{
#if SAMPLE_FORMAT == f32
    tsf_render_float(GlobalTSF, Sound->Samples, (int)SampleCount, 0);
#elif SAMPLE_FORMAT == s16
    tsf_render_short(GlobalTSF, Sound->Samples, (int)SampleCount, 0);
#endif
}