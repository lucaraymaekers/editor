#define BASE_EXTERNAL_LIBS 1
#define BASE_NO_ENTRYPOINT 1
#include "base/base.h"
#include "base/base.c"

#define TSF_IMPLEMENTATION
NO_WARNINGS_BEGIN
#include "lib/tsf.h"
NO_WARNINGS_END

#include "rl/generated/rl_tables.meta.c"
#include "rl/generated/rl_ui.meta.c"
#include "rl/rl_platform.h"
#include "rl/rl_libs.h"
#include "rl/rl_font.h"
#include "rl/rl_random.h"
#include "rl/rl_gl.h"
#include "rl/rl_renderer.h"
#include "rl/rl_ui.h"
#include "rl/rl_midi.h"

#include "muze/generated/muze.meta.c"
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
global_variable panel *NilPanel;
global_variable drag_data *DragData;
global_variable ui_size GlobalItemPadding;

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
SizeOnAxis(v4 Rec, axis2 Axis)
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

global_variable u64 PanelsAllocated;

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
  PanelsAllocated += 1;
 }
 
 ZeroPanel(Result);
 
 return Result;
}

internal void PanelPushAxis(axis2 Axis) { StackPush(PanelArena, axis2_stack_node, Axis, PanelAxisTop); }
internal void PanelPopAxis() { PanelAxisTop = PanelAxisTop->Prev; }

internal void PanelPushChildren() 
{
 PanelAppendToParent = true;
 
 axis2 Axis = 1 - PanelAxisTop->Value;
 PanelPushAxis(Axis);
}
internal void PanelPopChildren(void)
{ 
 PanelCurrent = PanelCurrent->Parent; 
 PanelPopAxis();
}

internal panel *
PanelAdd(f32 ParentPct)
{
 panel *New = PanelAlloc();
 
 New->ParentPct = ParentPct;
 New->Axis = PanelAxisTop->Value;
 
 if(!IsNilPanel(PanelCurrent))
 {            
  if(PanelAppendToParent)
  {
   PanelCurrent->First = New;
   New->Parent = PanelCurrent;
  }
  else
  {
   PanelCurrent->Next = New;
   New->Prev = PanelCurrent;
   New->Parent = PanelCurrent->Parent;
  }
  
  New->Parent->Last = New;
 }
 
 PanelCurrent = New;
 PanelAppendToParent = false;
 
 return New;
}

#define PanelChildren() DeferLoop(PanelPushChildren(), PanelPopChildren())
#define PanelAxis(Axis) DeferLoop(PanelPushAxis(Axis), PanelPopAxis())
internal inline b32 
IsLeafPanel(panel *Panel)
{
 b32 Result = IsNilPanel(Panel->First);
 return Result;
}

internal panel *
SplitPanel(panel *To, axis2 Axis, b32 Backwards)
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
  Result = (IsNilPanel(Search) ? Start : Search);
 }
 
 return Result;
}

// NOTE(luca): Returns the panel that absorbed the size of the closed panel
internal panel *
ClosePanel(app_state *App, panel *Panel)
{
 panel *Collapse = NilPanel;
 panel *Parent = Panel->Parent;
 
 if(!IsNilPanel(Panel->Next)) Collapse = Panel->Next;
 if(!IsNilPanel(Panel->Prev)) Collapse = Panel->Prev;
 
 if(IsNilPanel(Collapse))
 {
  // If there is no other panel, just make this one an empty one.
  {
   Panel->Kind = PanelKind_Empty;
   Collapse = Panel;
  }
 }
 else if(Collapse != Panel)
 {
  // Remove the panel from the tree.
  {        
   if(!IsNilPanel(Panel->Next)) Panel->Next->Prev = Panel->Prev;
   if(!IsNilPanel(Panel->Prev)) Panel->Prev->Next = Panel->Next;
   if(!IsNilPanel(Parent))
   {
    if(Parent->First == Panel) Parent->First = Parent->First->Next;
    if(Parent->Last  == Panel) Parent->Last  = Parent->Last->Prev;
   }
  }
  
  // Absorb the size of the closed panel
  {        
   Collapse->ParentPct += Panel->ParentPct;
  }
  
  // Collapse could be a non-leaf panel, use the next leaf from there.
  {
   if(!IsLeafPanel(Collapse)) Collapse = PanelNextLeaf(Collapse, false);
   Assert(!IsNilPanel(Collapse));
   Assert(IsLeafPanel(Collapse));
  }
  
  // If this is the only child of the parent, collapse the child into the parent.
  {
   if(Collapse == Parent->First && 
      Collapse == Parent->Last)
   {
    Assert(EqualsWithEpsilon(Collapse->ParentPct, 1.f, 0.001f));
    
    Parent->First = NilPanel;
    Parent->Last = NilPanel;
    
    Parent->Kind = Collapse->Kind;
    Parent->Voice = Collapse->Voice;
    Parent->Scroll = Collapse->Scroll;
    
    Collapse = Parent;
   }
  }
  
  // Add removed panel to the free list
  {
   Panel->Next = App->FreePanel;
   App->FreePanel = Panel;
  }
 }
 else
 {
  InvalidPath();
 }
 
 return Collapse;
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
 
 Voice->NoteSetLength = 0.f;
 
 ArenaSetPos(Voice->Arena, 0);
}

internal voice *
VoiceAdd(app_state *App)
{
 voice *Voice = App->Voices + App->VoiceCount;
 
 if(App->VoiceCount < App->MaxVoiceCount)
 {        
  MemoryZero(Voice);
  Voice->Arena = PushArena(App->Arena, KB(64), false);
  
  VoiceReset(Voice);
  Voice->Channel = (int)App->VoiceCount;
  tsf_channel_set_presetindex(GlobalTSF, Voice->Channel, Voice->PresetIdx);
  
  Voice->Volume = 1.f;
  
  App->VoiceCount += 1;
 }
 else
 {
  Assert(App->VoiceCount > 0);
  Voice = App->Voices + App->VoiceCount - 1;
  // TODO(luca): Show error message that max amount of voices has been reached.
 }
 
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
 if(App->OutputSynthEnabled)
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
 
 if(App->OutputMIDIEnabled)
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
  
  Memory->PlatformMIDISend(App->Out, Message.U32[0]);
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
PushCommand(app_state *App, command_kind Kind)
{
 command *Result = &NilCommand;
 
 if(App->CommandCount < App->MaxCommandCount)
 {
  Result = App->Commands + App->CommandCount;
  MemoryZero(Result);
  
  App->CommandCount += 1;
  Result->Kind = Kind;
 }
 
 return Result;
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
  
  b32 IsOutputDeviceDifferent = !(!App->OutputSynthEnabled && 
                                  App->In.Id == App->Out.Id);
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
 for EachIndex(VoiceIdx, App->VoiceCount)
 {
  voice *Voice = App->Voices + VoiceIdx;
  
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

typedef struct display_unit display_unit;
struct display_unit
{
 u64 Value;
 str8 Name;
}; 

internal display_unit
DisplayUnitFromSize(f32 Size)
{
 display_unit Result = {0};
 
 f32 X = Size;
 if(X > 0.f)
 {
  X = Log10F32(X);
  X = X/3.f; // Thousands
  X = FloorF32(X);
 }
 
 display_unit UnitTable[] = 
 {
  {   1,  S8("B")},
  {KB(1), S8("KB")},
  {MB(1), S8("MB")},
  {GB(1), S8("GB")},
  {TB(1), S8("TB")},
 };
 
 Result = UnitTable[(s32)X];
 
 return Result;
}

internal void
DebugStringAddArena(app_state *App, str8 Name, arena *Arena)
{
 if(Arena)
  
 {  
  display_unit PosUnit = DisplayUnitFromSize((f32)Arena->Pos);
  display_unit SizeUnit = DisplayUnitFromSize((f32)Arena->Size);
  
  str8 String = Str8Fmt("(mem) %S: %.3f%S/%.1f%S", 
                        Name, 
                        (f32)Arena->Pos/(f32)PosUnit.Value, 
                        PosUnit.Name, 
                        (f32)Arena->Size/(f32)SizeUnit.Value,
                        SizeUnit.Name);
  
  App->DebugStrings[App->DebugStringCount] = String;
  App->DebugStringCount += 1;
 }
}

//~ Custom draw UI elements 
internal note_node *
FindSelForNote(note_node *Selection, note *Note)
{
 note_node *Result = 0;
 
 for EachNode(Node, note_node, Selection)
 {
  if(Node->Value == Note)
  {
   Result = Node;
   break;
  }
 }
 
 return Result;
}


internal void
AddNoteToSelection(b32 MouseLeftDown, b32 MouseRightDown, b32 Adding,
                   v4 Dest, v4 SelDest,
                   app_state *App, voice *Voice, note_node *Sel, note *Note)
{
 // Draw selection rectangle
 if(MouseLeftDown || MouseRightDown)
 {
  // NOTE(luca): @Command
  if(RectOverlap(Dest, SelDest))
  {
   if(Adding && !Sel)
   {
    note_node *Node;
    
    // Allocate
    {
     if(!App->FirstFreeNode)
     {
      Node = PushArray(App->Arena, note_node, 1);
     }
     else
     {
      Node = App->FirstFreeNode;
      App->FirstFreeNode = Node->Next;
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
     Sel->Next = App->FirstFreeNode;
     App->FirstFreeNode = Sel;
    }
   }
  }
 }
}

UI_CUSTOM_DRAW(CustomDrawSheetMusic)
{
 muze_box_data *Data = (muze_box_data *)CustomDrawData;
 ui_box *Box = Data->Box;
 voice *Voice = Data->Voice;
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
 
 v4 BarColor = ForegroundColor;
 
 v2 BoxPos = V2(Box->FixedPos.X - Box->Scroll.X, Box->FixedPos.Y);
 v2 BoxSize = Box->FixedSize;
 
 f32 BPS = (App->BPM/60.f);
 
 // Staff Lines
 v2 StaffPos = BoxPos;
 s32 StaffLineCount = 5;
 f32 StaffLineWidth = 2.f;
 f32 StaffHeight = (f32)(StaffLineCount-1)*NoteSize + StaffLineWidth;
 
 // Bars
 f32 WholeBarWidth = 100.f;
 f32 BarDuration = (1.f/BPS)*(f32)App->TimeSig;
 
 // NOTE(luca): When it is exactly the length it is wrong, so we add a little offset to unmatch it.
 f32 BarCount = 1.f + FloorF32(Voice->RecordLength/(BarDuration + 1e-6f));
 v2 BarDim = V2(2.f, StaffHeight);
 
 // Input
 b32 MouseLeftDown = false;
 b32 MouseRightDown = false;
 b32 Adding = false;
 v2 MouseP = {0};
 v2 MouseStartP = {0};
 v4 SelDest = {0};
 
 // Get Input
 {
  if(UI_IsActive(Box))
  {
   MouseLeftDown = Input->Mouse.Buttons[PlatformMouseButton_Left].EndedDown;
   MouseRightDown = Input->Mouse.Buttons[PlatformMouseButton_Right].EndedDown;
   
   UI_ConsumeInput(Input, Box);
  }
  
  Adding = (MouseLeftDown && !MouseRightDown);
  MouseP = MousePosFromInput(Input);
  MouseStartP = V2V2S32(Input->Mouse.Start);
  
  // Get selection rectangle
  if(IsInsideRectV2(MouseStartP, Box->Rec))
  {        
   SelDest = (v4){.Min = MouseP, .Max = MouseStartP};
   if(SelDest.Min.X > SelDest.Max.X) Swap(SelDest.Min.X, SelDest.Max.X);
   if(SelDest.Min.Y > SelDest.Max.Y) Swap(SelDest.Min.Y, SelDest.Max.Y);
   SelDest = RectIntersect(Box->Rec, SelDest);
  }
  
  if(MouseLeftDown || MouseRightDown)
  {
   DrawRect(SelDest, V4(V3Arg(Adding ? Color_Blue : Color_Snow2), .3f), 1.f, 0.f, 0.f);
   DrawRect(SelDest, V4(V3Arg(Adding ? Color_Orange : Color_Black), 1.f), 1.f, 1.f, 0.f);
  }
 }
 
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
   
   v4 Dest = RectIntersect(RectFromSize(BarPos, BarDim), Box->Rec);
   DrawRect(Dest, BarColor, 0.f, 0.f, 0.f); 
  }
 }
 
 for EachNote(Note, Voice->FirstNote)
 {        
  if(Note->Kind == NoteKind_Note)
  {
   note_node *Sel = FindSelForNote(Voice->NoteSel, Note);
   b32 NoteIsPlaying = IsNotePlaying(Note, Voice);
   
   v4 NoteColor = (Sel ? Color_Blue : 
                   (NoteIsPlaying ? Color_Yellow :
                    ForegroundColor));
   
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
   
   // NOTE(luca): Get pos relative of time inside bar 
   f32 NoteStart = Note->Timestamp;
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
    Dest = RectIntersect(Dest, Box->Rec);
    
    rect_instance *Inst = DrawRect(Dest, NoteColor, 0.f, 0.f, 0.f);
    Inst->CornerRadii.e[2] = .5f;
    
    // NOTE(luca): Will be negative for notes longer than 1
    s32 NumOfSideTails = (s32)RoundF32(-Log2F32(NoteLength));
    for EachIndex(TailIdx, NumOfSideTails)
    {
     f32 SideTailWidth = (TailDim.X);
     f32 SideTailY = ((Y + (2.f*SideTailWidth)*(f32)TailIdx) - TailDim.Y);
     v4 TailDest = RectFromSize(V2(X, SideTailY), V2(8.f, SideTailWidth));
     TailDest = RectIntersect(TailDest, Box->Rec);
     
     DrawRect(TailDest, NoteColor, 0.f, 0.f, 0.f);
    }
    
   }
   
   if(RoundF32(NoteLength) >= 2.f)
   {
    NoteBorderSize = 2.f;
   }
   
   // Draw note and do input
   {
    v4 Dest = RectFromSize(NotePos, NoteDim);
    Dest = RectIntersect(Dest, Box->Rec);
    DrawRect(Dest, NoteColor, .5f*NoteSize, NoteBorderSize, .5);
    
    AddNoteToSelection(MouseLeftDown, MouseRightDown, Adding, 
                       Dest, SelDest, App, Voice, Sel, Note);
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
 
 // Draw the roll and markers
 {
  f32 X = (Box->FixedPos.X - Box->Scroll.X);
  f32 Y = (Box->FixedPos.Y - Box->Scroll.Y);
  f32 Height = Box->FixedSize.Y;
  
  f32 Zoom = (BPS/(f32)App->TimeSig*100.f);
  
  // NOTE(luca): This is also the current time when recording.
  f32 RecordMarkerX;
  {
   f32 Timestamp = Max(Voice->RecordLength, 0.f);
   f32 ZoomedTimestamp = Timestamp*Zoom;
   RecordMarkerX = X + RoundF32(ZoomedTimestamp);
  }
  
  f32 PlayMarkerX;
  {
   f32 Timestamp = Voice->PlayPos;
   f32 ZoomedTimestamp = Timestamp*Zoom;
   PlayMarkerX = X + RoundF32(ZoomedTimestamp); 
  }
  
  v4 RecDest = RectFromSize(V2(PlayMarkerX, Y), 
                            V2(2.f, Height));
  v4 RecordDest = RectFromSize(V2(RecordMarkerX, Y), 
                               V2(2.f, Height));
  v4 PlayDest = RectFromSize(V2(PlayMarkerX, Y), 
                             V2(2.f, Height));
  
  DrawRect(RectIntersect(RecordDest, Box->Rec), Color_Red, 0.f, 0.f, 0.f);
  DrawRect(RectIntersect(PlayDest, Box->Rec), Color_Green, 0.f, 0.f, 0.f);
 }
 
 if(Box->Flags & UI_BoxFlag_DrawBorders)
 {
  DrawRect(Box->Rec, Box->BorderColor, 0.f, Box->BorderThickness, Box->Softness);
 }
}

typedef struct custom_draw__single_line_text_input_params custom_draw__single_line_text_input_params;
struct custom_draw__single_line_text_input_params
{
 ui_box *Box;
 str8 Text;
 f32 CursorAnimTime;
};

UI_CUSTOM_DRAW(CustomDrawPianoRoll)
{
 muze_box_data *Data = (muze_box_data *)CustomDrawData;
 ui_box *Box = Data->Box;
 voice *Voice = Data->Voice;
 app_state *App = Data->App;
 
 app_input *Input = UI_State->Input;
 
 b32 MouseLeftDown = false;
 b32 MouseRightDown = false;
 b32 Adding = false;
 v2 MouseP = {0};
 v2 MouseStartP = {0};
 v4 SelDest = {0};
 
 f32 BPS = App->BPM/60.f;
 
 f32 Zoom = (BPS/(f32)App->TimeSig*100.f);
 
 // Get Input
 {    
  if(UI_State->InputConsumerBox == Box)
  {
   MouseLeftDown = Input->Mouse.Buttons[PlatformMouseButton_Left].EndedDown;
   MouseRightDown = Input->Mouse.Buttons[PlatformMouseButton_Right].EndedDown;
   UI_ConsumeInput(Input, Box);
  }
  
  Adding = (MouseLeftDown && !MouseRightDown);
  MouseP = MousePosFromInput(Input);
  MouseStartP = V2V2S32(Input->Mouse.Start);
  
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
  
  f32 RollX = (Box->FixedPos.X - Box->Scroll.X);
  f32 RollY = (Box->FixedPos.Y - Box->Scroll.Y);
  f32 RollHeight = Box->FixedSize.Y;
  f32 RollWidth = Box->FixedSize.X;
  
  // Draw piano overlay
  {
   f32 PianoWidth = 1.5f*NoteWidth;
   v2 PianoPos = Box->FixedPos;
   
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
    RecordMarkerX = RollX + RoundF32(ZoomedTimestamp);
   }
   
   f32 PlayMarkerX;
   {
    f32 Timestamp = Voice->PlayPos;
    f32 ZoomedTimestamp = Timestamp*Zoom;
    PlayMarkerX = RollX + RoundF32(ZoomedTimestamp); 
   }
   
   for EachNote(Note, Voice->FirstNote)
   {
    b32 NoteIsRecording = (Note->Duration == 0.f);
    
    // TODO(luca): Visualize
    u8 Velocity = Note->Velocity;
    
    f32 StartX = RoundF32(Note->Timestamp*Zoom);
    
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
      EndX = RoundF32((Note->Timestamp + Note->Duration)*Zoom);
     }
     
     Width = (EndX - StartX);
    }
    
    v2 NotePos = V2((RollX + StartX),
                    (RollY + StartY));
    
    u8 PitchClass = (Note->Pitch%Note_Count);
    b32 White = NotePianoColors[PitchClass];
    
    note_node *Sel = FindSelForNote(Voice->NoteSel, Note);
    
    v4 NoteColor = (IsNotePlaying(Note, Voice) ? Color_Yellow :
                    (Sel ? Color_Blue : 
                     ((Note->Kind == NoteKind_Pedal) ? Color_Orange : 
                      (White ? Color_Snow0 : Color_Black))));
    
    v4 NoteDest = RectFromSize(NotePos, V2(Width, NoteHeight)); 
    v4 Dest = RectIntersect(NoteDest, Box->Rec);
    
    AddNoteToSelection(MouseLeftDown, MouseRightDown, Adding, Dest, SelDest, App, Voice, Sel, Note);
    
    // Draw Note
    {                
     rect_instance *Instance = DrawRect(Dest, NoteColor, 5.f, 0.f, .5f);
     Instance->Color0.A = .2f;
     Instance->Color2.A = .2f;
     
     if(IsInsideRectV2(MouseP, Dest))
     {
      // Draw border around when hovered
      if(Sel) White = !White;
      v4 BorderColor = (!White ? Color_Snow0 : Color_Black);
      DrawRect(Dest, BorderColor, 5.f, 2.f, 0.5f);
     }
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

UI_CUSTOM_DRAW(CustomDrawSingleLineTextInput)
{
 custom_draw__single_line_text_input_params *Params = (custom_draw__single_line_text_input_params *)CustomDrawData;
 ui_box *Box = Params->Box;
 str8 Text = Params->Text;
 font_atlas *Atlas = UI_State->Atlas;
 f32 CursorAnimTime = Params->CursorAnimTime;
 
 DrawRect(Box->Rec, Color_Cyan, 0.f, 0.f, 0.f);
 
 f32 HorizontalPadding = 5.f;
 
 v2 Cur = Box->FixedPos;
 Cur.X += HorizontalPadding;
 Cur.Y += RoundF32(.5f*(Box->FixedSize.Y - Box->HeightPx));
 
 f32 EllipsisWidth = Atlas->PackedChars[' ' - Atlas->FirstCodepoint].xadvance;
 
 f32 CurMaxX = (Box->FixedPos.X + Box->FixedSize.X - 
                (HorizontalPadding + EllipsisWidth));
 
 b32 Overflowed = false;
 for EachIndex(Idx, Text.Size)
 {
  rune Char = Text.Data[Idx]; 
  
  DrawRectChar(Atlas, Cur, Char, Box->TextColor);
  f32 CharWidth = (Atlas->PackedChars[Char - Atlas->FirstCodepoint].xadvance);
  Cur.X += CharWidth;
  
  if(Cur.X > CurMaxX)
  {
   break;
  }
 }
 
 v2 CursorPos = Cur;
 v4 CurRec = RectFromSize(CursorPos, V2(1.f, Box->HeightPx));
 if(Box->Flags & UI_BoxFlag_Clip)
 {
  CurRec = RectIntersect(Box->Rec, CurRec);
 }
 
 if(RectValid(CurRec))
 {
  f32 Speed = 2.f;
  v4 Color = Box->TextColor;
  if(cos(Pi32*CursorAnimTime*Speed) < 0.f)
  {
   Color.A = 0.f;
  }
  DrawRect(CurRec, Color, 0.f, 0.f, 0.f);
 }
}

//~ UI 

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

internal void
DebugStringAdd(app_state *App, char *Format, ...)
{
 if(App->DebugStringCount < App->MaxDebugStringCount)
 {
  va_list Args;
  va_start(Args, Format);
  
  str8 String = Str8VFmt(Format, Args);
  App->DebugStrings[App->DebugStringCount] = String;
  App->DebugStringCount += 1;
 }
}

internal b32
SimpleButton(str8 Name)
{
 b32 Clicked = Button(.Text = Name, 
                      .CenterText = true,
                      .Padding = GlobalItemPadding).Pressed;
 
 return Clicked;
}

typedef struct simple_slider_result simple_slider_result;
struct simple_slider_result
{
 f32 Value;
 b32 Clicked;
};

internal simple_slider_result
SimpleSlider(str8 Label, str8 DisplayString, 
             f32 Value, f32 Min, f32 Max, f32 StepSize,
             b32 Clickable)
{
 simple_slider_result Result = {0};
 
 s32 Flags = (UI_BoxFlag_Clip|
              UI_BoxFlag_DrawBorders|
              UI_BoxFlag_DrawBackground);
 
 if(Clickable)
 {
  Flags |= (UI_BoxFlag_MouseClickable|
            UI_BoxFlag_DrawHotEffects|
            UI_BoxFlag_DrawActiveEffects);
 }
 
 {
  ui_box *Box;
  UI_BackgroundColor(Color_ButtonBackground)
   Box = UI_AddBox(Label, Flags);
  
  UI_PaddingAround(GlobalItemPadding)
  {
   ui_box *Slider;
   UI_SemanticWidth(UI_SizeText(1.f, 1.f))
    Slider = UI_AddBox(Label, UI_BoxFlag_DrawDisplayString|
                       UI_BoxFlag_CenterTextVertically);
   Slider->DisplayString = DisplayString;
   
   UI_Spacer(UI_SizePx(5.f, 1.f));
   
   UI_BackgroundColor(Color_Orange)
    UI_BorderColor(Color_Black)
    Result.Value = UI_Slider(Value, Min, Max, StepSize, 0, true);
  }
  
  Result.Clicked = Box->Clicked;
 }
 
 return Result;
}

//~ UI macro's
#define UI_List(Name, ItemHeight) \
DeferLoop(UI_ListBegin(Name, ItemHeight), UI_ListEnd())

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
 v2 BufferDim = V2S32(Buffer->Width, Buffer->Height);
 dtForFrame = Input->dtForFrame;
 
 //- Init state 
 OS_ProfileInit(" G");
 
 arena *PermanentArena;
 app_state *App;
 {
  {
   umm MemoryPos = (umm)Memory->Memory;
   umm AlignedPos = AlignPow2(MemoryPos, AlignOf(arena));
   PermanentArena = (arena *)AlignedPos;
   
   PermanentArena->Size = Memory->MemorySize - sizeof(arena);
   PermanentArena->Base = (u8 *)AlignedPos + sizeof(arena);
   PermanentArena->Pos = 0;
   AsanPoisonMemoryRegion(PermanentArena->Base, PermanentArena->Size);
  }
  
  // App
  {        
   App = PushArray(PermanentArena, app_state, 1);
   
   App->FontAtlasArena = PushArena(PermanentArena, MB(100), false);
   App->UIArena = PushArena(PermanentArena, MB(64), false);
   App->Arena = PushArena(PermanentArena, MB(64), false);
   App->TSFArena = PushArena(PermanentArena, MB(500), false);
   App->ReadOnlyArena = PushArray(PermanentArena, arena, 1);
   App->PanelArena = PushArena(PermanentArena, MB(64), false);
  }
  
  // Globals
  {
   FrameArena = PushArena(PermanentArena, MB(64), true);
   SetStringsScratch(FrameArena);
   
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
   
   DragData = PushArray(PermanentArena, drag_data, 1);
  }
 }
 
 platform_midi_get_devices_result Devices = Memory->PlatformMIDIGetDevices();
 
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
   
   s32 InstrumentCount = tsf_get_presetcount(GlobalTSF);
   App->InstrumentCount = InstrumentCount;
   App->InstrumentNames = PushArray(App->TSFArena, str8, (umm)InstrumentCount);
   
   for EachIndex(Idx, InstrumentCount)
   {
    App->InstrumentNames[Idx] = Str8DupCString(App->TSFArena, (char *)tsf_get_presetname(GlobalTSF, Idx));
   }
   
   App->TrackerForTSF = GlobalTSF;
  }
  
  OS_ProfileAndPrint("TSF init");
  
  //- Init Muze 
  {
   App->MaxTextSize = 256;
   App->Text = PushS8(App->Arena, App->MaxTextSize);
   App->Text.Size = 0;
   
   App->TimeSig = 3;
   App->BPM = 100.f;
   App->InputMIDIEnabled = true;
   App->OutputSynthEnabled = true;
   App->InputVirtualKeyboardEnabled = true;
   App->MaxVoiceCount = 99;
   App->Voices = PushArray(App->Arena, voice, (u64)App->MaxVoiceCount);
   
   App->MaxCommandCount = KB(4);
   App->Commands = PushArray(App->Arena, command, App->MaxCommandCount);
   
   // Select default input and output device
   {
    b32 InputFound = false;
    b32 OutputFound = false;
    for EachIndex(Idx, Devices.Count)
    {
     platform_midi_device *Device = Devices.Devices + Idx;
     if(Device->IsOutput)
     {
      App->Out = *Device;
      OutputFound = true;
     }
     if(Device->IsInput)
     {
      App->In = *Device;
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
    
#if MUZE_INTERNAL
    umm Offset = TB(2) + GB(64);
#else
    umm Offset = 0;
#endif
    
    Arena->Base = OS_AllocateAtOffset(Arena->Size, Offset);
   }
   
   // Initialize read only structs
   {
    ui_box *Box = PushArray(Arena, ui_box, 1);
    *Box = (ui_box){Box, Box, Box, Box, Box, Box, Box};
    
    panel *Panel = PushArray(Arena, panel, 1);
    *Panel = (panel){Panel, Panel, Panel, Panel, Panel};
    
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
   UI_InitState(App->UIArena);
   App->ListerBox = UI_BoxAlloc(UI_State->Arena);
   App->TopBox = UI_BoxAlloc(UI_State->Arena);
  }
  
  // Init panels
  {
   ArenaSetPos(App->PanelArena, 0);
   App->FreePanel = NilPanel;
   App->SelectedPanel = NilPanel;
   App->DebugPanel = NilPanel;
   PanelCurrent = NilPanel;
   PanelAppendToParent = false;
   
   // Default panel setup
   {
    PanelAxis(Axis2_X)
    {            
     App->FirstPanel = PanelAdd(1.f);
     PanelChildren()
     {
      PanelAdd(.5f);
      PanelAdd(.5f);
      PanelChildren()
      {
       PanelAdd(.5f);
       PanelAdd(.5f);
      }
     }
    }
    
    for(panel *Panel = App->FirstPanel;
        !IsNilPanel(Panel);
        Panel = PanelRecDepthFirstPreOrder(Panel).Next)
    {
     if(IsLeafPanel(Panel))
     {
      Panel->Kind = PanelKind_Sheet;
      Panel->Voice = VoiceAdd(App);
     }
    }
    
    App->SelectedPanel = PanelNextLeaf(App->FirstPanel, false);
   }
  }
  
  App->MaxDebugStringCount = 128;
 }
 
 voice *SelectedVoice;
 
 // Frame Initialization
 {
  // Init read-only variables
  {    
   InitReadOnlyGlobals(App->ReadOnlyArena);
  }
  
  // Init Muze
  {
   App->CommandCount = 0;
   
   
   if(App->SelectedPanel->Kind == PanelKind_Roll ||
      App->SelectedPanel->Kind == PanelKind_Sheet)
   {
    App->SelectedVoice = App->SelectedPanel->Voice;
   }
   SelectedVoice = App->SelectedVoice;
   
  }
  
  
  
  OS_ProfileAndPrint("Misc. Init");
 }
 
 if(App->ListerOpened)
 {            
  for EachTextButton(Key, Id, Input)
  {
   s32 ModifiersWithoutShift = (Key->Modifiers & ~PlatformKeyModifier_Shift);
   
   if(ModifiersWithoutShift == PlatformKeyModifier_None)
   {
    if(!Key->IsSymbol)
    {
     App->Text.Data[App->Text.Size] = (u8)Key->Codepoint;
     App->Text.Size += 1;
     App->CursorAnimTime = 0.f;
     
     RemoveTextButton(Input, Id);
    }
    else
    {
     switch(Key->Symbol)
     {
      default: break;
      
      case PlatformKey_BackSpace:
      {
       App->Text.Size -= !!(App->Text.Size > 0);
       App->CursorAnimTime = 0.f;
      } break;
     }
    }
   }
  }
 }
 
 for EachTextButton(Key, Id, Input)
 {
  b32 Control = Key->Modifiers & PlatformKeyModifier_Control;
  b32 Shift = Key->Modifiers & PlatformKeyModifier_Shift;
  b32 Alt = Key->Modifiers & PlatformKeyModifier_Alt;
  s32 ModifiersShiftIgnored = (Key->Modifiers & ~PlatformKeyModifier_Shift);
  b32 None = (Key->Modifiers == PlatformKeyModifier_None);
  
  b32 Handled = true;
  
  if(!Key->IsSymbol)
  {
   if(Control)
   {
    switch(Key->Codepoint)
    {
     case 'b':
     {
      if(!Shift) App->BPM += 1;
      else       App->BPM -= 1;
     } break;
     
     case 'p': 
     {
      if(!Alt)
      {
       command *Command = PushCommand(App, Command_SplitPanel);
       Command->Panel = App->SelectedPanel;
       if(Shift) Command->Axis = Axis2_Y;
      }
      else
      {
       PushCommand(App, Command_ClosePanel)->Panel = App->SelectedPanel;
      }
     } break;
     
     case 's': PushCommand(App, Command_OpenLister)->ListerKind = ListerKind_Instruments; break;
     
     default: 
     {
      Handled = false;
     } break;
    }
   }
   else if(Alt)
   {
    switch(Key->Codepoint)
    {
     case 'b': DebugBreak(); break;
     
     default:
     {
      Handled = false;
     } break;
    }
   }
   else
   {
    switch(Key->Codepoint)
    {
     case ' ': PushCommand(App, Command_ToggleRecording); break;
     default: 
     {
      Handled = false;
     } break;
    }
   }
   
  }
  else
  {
   switch(Key->Codepoint)
   {
    case PlatformKey_Escape:
    {
     PushCommand(App, Command_CloseLister);
     
     default: 
     {
      Handled = false;
     } break;
    } break;
   }
  }
  
  if(Handled)
  {
   RemoveTextButton(Input, Id);
  }
 }
 
 OS_ProfileAndPrint("Input");
 
 RenderBeginFrame(FrameArena, 0, 0, Buffer->Width, Buffer->Height);
 
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
  f32 ItemHeight = App->HeightPx + 2.f*8.f;
  ui_size ItemPadding = UI_SizePx(5.f, 1.f);
  GlobalItemPadding = ItemPadding;
  f32 ListWidth = 1600.f/6.f;
  b32 SoundFontLoaded = (GlobalTSF != NilTSF);
  
  // Setup
  {        
   UI_BeginFrame(&App->FontAtlas, App->FrameIdx, Input);
   OS_ProfileAndPrint("Render setup");
   
   App->DebugStrings = PushArray(FrameArena, str8, App->MaxDebugStringCount);
   App->DebugStringCount = 0;
   { 
    ui_box *Hot = UI_BoxFromKey(UI_State->Hot);
    ui_box *Active = UI_BoxFromKey(UI_State->Active);
    str8 ActiveString = Active->String;
    str8 HotString = Hot->String;
    if(ActiveString.Size == 0) ActiveString = S8("null");
    if(HotString.Size == 0) HotString = S8("null");
    
    ui_box *Box;
    
#if 1                
    StringsScratchTemp(UI_State->FrameArenaBack)
    {
     DebugStringAdd(App, "(ui) Active: %S", ActiveString);
     DebugStringAdd(App, "(ui) Hot: %S", HotString);
     DebugStringAdd(App, "(ui) Active->tHot = %.2f", Active->tHot);
     DebugStringAdd(App, "(ui) Active->tActive = %.2f", Active->tActive);
     DebugStringAdd(App, "(ui) Hot->tHot = %.2f", Hot->tHot);
     DebugStringAdd(App, "(ui) Hot->tActive = %.2f", Hot->tActive);
     
     DebugStringAddArena(App, S8("Perm"), PermanentArena);
     DebugStringAddArena(App, S8("Frame"), FrameArena);
     DebugStringAddArena(App, S8("R/O"), App->ReadOnlyArena);
     DebugStringAddArena(App, S8("Font"), App->FontAtlasArena);
     DebugStringAddArena(App, S8("UI"), App->UIArena);
     DebugStringAddArena(App, S8("Panel"), App->PanelArena);
     DebugStringAddArena(App, S8("Muze"), App->Arena);
     DebugStringAddArena(App, S8("Voice"), SelectedVoice->Arena);
     DebugStringAddArena(App, S8("TSF"), App->TSFArena);
    }
#endif
    
   }
   
   OS_ProfileAndPrint("UI setup");
  }
  
  if(App->ListerOpened)
  {
   s32 ShowItemCount = 10.f;
   f32 ListerBorderSize = 2.f;
   
   // Input
   u64 SelectedIdx = 0; 
   u64 NameListSize = 0;
   str8 *NameList = 0;
   u64 StartIdx = 0;
   
   // Output
   s32 FilteredItemCount = 0;
   lister_item *FilteredItems = 0;
   platform_midi_device *OutDevices = 0;
   
   switch(App->ListerKind)
   {
    default: InvalidPath(); break;
    case ListerKind_Instruments:
    {
     SelectedIdx = (u64)SelectedVoice->PresetIdx; 
     NameListSize = (u64)App->InstrumentCount;
     NameList = App->InstrumentNames;
    } break;
    
    case ListerKind_MIDIInput:
    case ListerKind_MIDIOutput:
    {
     b32 IsInput = (App->ListerKind == ListerKind_MIDIInput);
     
     NameList = PushArray(FrameArena, str8, Devices.Count);
     OutDevices = PushArray(FrameArena, platform_midi_device, Devices.Count);
     for EachIndex(Idx, Devices.Count)
     {
      platform_midi_device *Device = Devices.Devices + Idx;
      platform_midi_device *Out = OutDevices + NameListSize;
      str8 *Name = NameList + NameListSize;
      
      if((IsInput && Device->IsInput) ||
         (!IsInput && Device->IsOutput))
      {
       *Name = Device->Name;
       *Out = *Device;
       
       if(Device->Id == App->In.Id)
       {
        SelectedIdx = NameListSize;
       }
       
       NameListSize += 1;
      }
     }
    } break;
    
    case ListerKind_DebugInfo:
    {
     SelectedIdx = (u64)-1;
     NameList = App->DebugStrings;
     NameListSize = App->DebugStringCount;
    } break;
    
    case ListerKind_PanelTypes:
    {
     panel *Panel = App->SelectedPanel;
     SelectedIdx = Panel->Kind;
     NameList = PanelTypeStrings;
     NameListSize = ArrayCount(PanelTypeStrings);
     StartIdx = 1;
    } break;
   }
   
   FilteredItems = PushArray(FrameArena, lister_item, NameListSize);
   
   for(u64 Idx = StartIdx; Idx < NameListSize; Idx += 1)
   {
    lister_item *Item = FilteredItems + FilteredItemCount;
    str8 Name = NameList[Idx];
    
    b32 Matched = false;
    
    // Match if Name contains App->Text case-insensitive
    {                
     if(App->Text.Size == 0)
     {
      Matched = true;
     }
     else
     {                    
      u64 TextIdx = 0;
      for EachIndex(NameIdx, Name.Size)
      {
       if(ToLowercase(Name.Data[NameIdx]) == 
          ToLowercase(App->Text.Data[TextIdx]))
       {
        Matched = true;
       }
       else if(Matched)
       {
        // NOTE(luca): Start over
        Matched = false;
        TextIdx = 0;
       }
       
       if(Matched)
       {
        TextIdx += 1;
        if(TextIdx == App->Text.Size)
        {
         break;
        }
       }
      }
      
      // NOTE(luca): Whole text must have matched
      if(TextIdx < App->Text.Size)
      {
       Matched = false;
      }
     }
    }
    
    if(Matched)
    {
     Item->Name = Name;
     Item->Idx = Idx;
     FilteredItemCount += 1;
    }
   }
   
   f32 PctOnYAxis = App->ListerScrollPct;
   f32 SpaceBeforeThumb = (1.f - 1.f/(f32)ShowItemCount)*PctOnYAxis;
   
   v2 ListerDim = V2(600.f, (f32)ShowItemCount*ItemHeight);
   V2Math ListerDim.E += 2.f*ListerBorderSize;
   ListerDim.Y += ItemHeight + ListerBorderSize;
   
   ui_box *Root = App->ListerBox;
   {    
    Root->Key.U64[0] = U64HashFromSeedStr8((u64)Root, S8("Root"));
    Root->FixedPos = V2(0.f, 0.f);
    Root->FixedSize = BufferDim;
    Root->Rec = RectFromSize(Root->FixedPos, Root->FixedSize);
    Root->LastTouchedFrameIdx = UI_State->FrameIdx;
   }
   
   if(Root->First->First->First == (ui_box *)0x21000424d30)
   {
    DebugBreak();
   }
   
   UI_BeginLayout(Root, App->HeightPx);
   {        
    UI_FillAll()
     UI_Column() UI_Center()
    {           
     
     UI_SemanticHeight(UI_SizeChildren(1.f))
      UI_Row() UI_Center()
     {
      UI_SemanticWidth(UI_SizePx(ListerDim.X, 1.f))
       UI_SemanticHeight(UI_SizePx(ListerDim.Y, 1.f))
       UI_BorderThickness(ListerBorderSize)
       UI_BorderColor(Color_Snow2)
       UI_AddBox(S8("Lister"), 
                 UI_BoxFlag_Clip|
                 UI_BoxFlag_DrawBorders|
                 UI_BoxFlag_DrawBackground);
      
      UI_PaddingAround(UI_SizePx(ListerBorderSize, 1.f)) 
      {
       UI_Column()
       {
        ui_box *SelectedBox;
        
        // Top input box
        UI_FillWidth()
         UI_SemanticHeight(UI_SizePx(ItemHeight, 1.f))
         UI_Row()
        {
         ui_box *InputBox;
         
         UI_BackgroundColor(Color_Blue)
          UI_FillAll()
          InputBox = UI_AddBox(S8("InputBox"), UI_BoxFlag_DrawBackground);
         
         custom_draw__single_line_text_input_params *Params = PushArray(FrameArena, custom_draw__single_line_text_input_params, 1);
         Params->Box = InputBox;
         Params->Text = App->Text;
         Params->CursorAnimTime = App->CursorAnimTime;
         InputBox->CustomDraw = CustomDrawSingleLineTextInput;
         InputBox->CustomDrawData = Params;
        }
        
        UI_FillWidth()
         UI_SemanticHeight(UI_SizePx(ListerBorderSize, 1.f))
         UI_BackgroundColor(Color_Snow2)
         UI_AddBox(S8("Separator"),
                   UI_BoxFlag_Clip|
                   UI_BoxFlag_DrawBackground);
        
        UI_Row()
        {
         s32 MinIdx;
         
         // Instruments
         {                            
          UI_BackgroundColor(Color_Red)
           UI_FillAll()
           UI_LayoutAxis(Axis2_Y)
          {
           ui_box *Box = UI_AddBox(S8("Items"), UI_BoxFlag_Clip);
           f32 FullHeight = (f32)FilteredItemCount*ItemHeight;
           Box->Scroll.Y = PctOnYAxis*FullHeight;
           MinIdx = (s32)(Box->Scroll.Y/ItemHeight);
          }
          
          UI_FillAll()
           UI_Push()
          {
           s32 MaxIdx = Min(FilteredItemCount, MinIdx + ShowItemCount + 1);
           UI_Spacer(UI_SizePx(ItemHeight*(f32)MinIdx, 1.f));
           for(s32 Idx = MinIdx; Idx < MaxIdx; Idx += 1)
           {
            lister_item *Item = FilteredItems + Idx;
            str8 ItemString = Str8Fmt("%S###Item%d", Item->Name, Idx);
            b32 Selected = (SelectedIdx == Item->Idx);
            
            UI_SemanticHeight(UI_SizePx(ItemHeight, 1.f))
            {   
             if(Button(.Text = ItemString, 
                       .Disabled = Selected, 
                       .DisabledBackgroundColor = Color_Yellow, 
                       .Padding = ItemPadding).Released)
             {
              switch(App->ListerKind)
              {
               default: InvalidPath(); break;
               
               case ListerKind_Instruments:
               {
                command *Command = PushCommand(App, Command_SelectInstrument);
                Command->PresetIdx = (s32)Item->Idx;
               } break;
               
               case ListerKind_MIDIInput:
               {
                command *Command = PushCommand(App, Command_SelectInputDevice);
                Command->Device = OutDevices + Item->Idx;
               } break;
               
               case ListerKind_MIDIOutput:
               {
                command *Command = PushCommand(App, Command_SelectOutputDevice);
                Command->Device = OutDevices + Item->Idx;
               } break;
               
               case ListerKind_PanelTypes:
               {
                command *Command = PushCommand(App, Command_SetPanelType);
                Command->Panel = App->SelectedPanel;
                Command->PanelKind = (panel_kind)Item->Idx;
               } break;
              }
              
              PushCommand(App, Command_CloseLister);
             }
            }
           }
          }
         }
         
         // Scrollbar
         {
          ui_box *Scrollbar;
          UI_BorderColor(Color_Black)
           UI_SemanticWidth(UI_SizePx(16.f, 1.f))
           UI_FillHeight()
           Scrollbar = UI_AddBox(S8("Scrollbar"), 
                                 UI_BoxFlag_Clip|
                                 UI_BoxFlag_MouseClickable|
                                 UI_BoxFlag_DrawBackground|
                                 UI_BoxFlag_DrawBorders);
          
          ui_size Padding = UI_SizePx(UI_BorderThicknessTop(), 1.f);
          
          UI_PaddingAround(Padding)
          { 
           UI_Spacer(UI_SizeParent(SpaceBeforeThumb, 0.f));
           
           ui_box *Thumb;
           UI_SemanticHeight(UI_SizePx(ItemHeight, 0.f))
            UI_FillWidth()
            UI_AddBox(S8("Scrollbar"), UI_BoxFlag_Clip);
           UI_PaddingAround(UI_SizePx(1.f, 0.f))
            UI_BorderThickness(1.f)
            UI_BorderColor(Color_Black)
            UI_BackgroundColor(Color_Orange)
            Thumb = UI_AddBox(S8("Thumb"), 
                              UI_BoxFlag_Clip|
                              UI_BoxFlag_MouseClickable|
                              UI_BoxFlag_DrawHotEffects|
                              UI_BoxFlag_DrawActiveEffects|
                              UI_BoxFlag_DrawBackground|
                              UI_BoxFlag_DrawBorders);
           
           // Compute PctOnYAxis
           {
            if(UI_IsActive(Thumb))
            {
             local_persist f32 BoxStart = 0.f;
             
             if(Thumb->WasClicked)
             {
              BoxStart = PctOnYAxis;
             }
             
             f32 Start = Scrollbar->FixedPos.Y + ItemHeight/2.f;
             f32 End = Scrollbar->FixedPos.Y + Scrollbar->FixedSize.Y - ItemHeight/2.f;
             
             f32 Delta = (f32)(Input->Mouse.Pos.Y - Input->Mouse.Start.Y);
             
             PctOnYAxis = Clamp(0.f, BoxStart + Delta/(End - Start), 1.f);
             
#if 0
             Log("%.0f (%.0f-%.0f) %.2f%%\n", 
                 Delta, Start, End, PctOnYAxis);
#endif
             
             App->ListerScrollPct = PctOnYAxis;
            }
           }
           
          }
         }
        }
       }
      }
     }
    }
    
   }
   
   App->CursorAnimTime += Input->dtForFrame;
  }
  
  //- UI Top "Controls" 
  {
   ui_box *Root = App->TopBox;
   {    
    Root->Key.U64[0] = U64HashFromSeedStr8((u64)Root, S8("Root"));
    Root->FixedPos = V2(0.f, 0.f);
    Root->FixedSize = BufferDim;
    Root->Rec = RectFromSize(Root->FixedPos, Root->FixedSize);
    Root->LastTouchedFrameIdx = UI_State->FrameIdx;
   }
   
   UI_BeginLayout(Root, App->HeightPx);
   {            
    UI_LayoutAxis(Axis2_Y)
     UI_AddBox(S8("RootFirst"), UI_BoxFlag_Clip);
    
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
       UI_Center()
        UI_SemanticHeight(UI_SizePx(ItemHeight, 1.f))
        UI_SemanticWidth(UI_SizePx(ListWidth, 1.f))
       {
        
        UI_List(S8("Song"), ItemHeight)
        {
         if(UI_ButtonWithToggle(S8("Record"), SelectedVoice->IsRecording, ItemPadding, Color_Red).OneClicked)
         {
          PushCommand(App, Command_ToggleRecording);
         }
         
         if(UI_ButtonWithToggle(S8("Play"), SelectedVoice->IsPlaying, ItemPadding, Color_Green).OneClicked)
         {
          PushCommand(App, Command_TogglePlaying);
         }
         
         if(UI_ButtonWithToggle(S8("Auto Scroll"), SelectedVoice->AutoScroll, ItemPadding, Color_Green).OneClicked)
         {
          PushCommand(App, Command_ToggleAutoScroll);
         }
         
         
         UI_Row() UI_SemanticWidth(UI_SizeParent(1.f/3.f, 1.f))
         {         
          if(SimpleButton(S8("Stop"))) PushCommand(App, Command_Stop);
          if(SimpleButton(S8("Trim"))) PushCommand(App, Command_Trim);
          if(SimpleButton(S8("Reset"))) PushCommand(App, Command_Reset);
         }
         UI_Row() UI_SemanticWidth(UI_SizeParent(.5f, 1.f))
         {         
          if(SimpleButton(S8("Start"))) PushCommand(App, Command_PlayFromStart);
          if(SimpleButton(S8("From note"))) PushCommand(App, Command_PlayFromNote);
         }
        }
        
        UI_List(S8("Notes"), ItemHeight)
        {
         
         UI_Row() UI_SemanticWidth(UI_SizeParent(.5f, 1.f))
         {
          if(SimpleButton(S8("Round"))) PushCommand(App, Command_Round);
          if(SimpleButton(S8("Sync"))) PushCommand(App, Command_Sync);
         }
         
         UI_Row() UI_SemanticWidth(UI_SizeParent(.5f, 1.f))
         {
          if(SimpleButton(S8("Guess"))) PushCommand(App, Command_GuessBPM);
          if(SimpleButton(S8("Delete"))) PushCommand(App, Command_DeleteSelection);
         }
         
         {
          ui_box *ButtonBox;
          UI_BackgroundColor(Color_ButtonBackground)
           ButtonBox = UI_AddBox(S8("Length"), 
                                 UI_BoxFlag_Clip|
                                 UI_BoxFlag_MouseClickable|
                                 UI_BoxFlag_DrawHotEffects|
                                 UI_BoxFlag_DrawActiveEffects|
                                 UI_BoxFlag_DrawBorders|
                                 UI_BoxFlag_DrawBackground);
          UI_PaddingAround(ItemPadding)
          {
           ui_box *Box;
           UI_SemanticWidth(UI_SizeText(1.f, 1.f))
            Box = UI_AddBox(S8("SetLength"), UI_BoxFlag_DrawDisplayString|
                            UI_BoxFlag_CenterTextVertically);
           
           f32 Length = SelectedVoice->NoteSetLength;
           char *Format = (Length >= 0.f ? 
                           "Length %3.0f" :
                           "Length 1/%1.0f");
           f32 Value = PowF32(2.f, AbsF32(Length));
           Box->DisplayString = Str8Fmt(Format, Value);
           
           UI_Spacer(UI_SizePx(5.f, 1.f));
           
           UI_BackgroundColor(Color_Orange)
            UI_BorderColor(Color_Black)
            SelectedVoice->NoteSetLength = UI_Slider(Length, -3, 2, 1.f, 0, true);
          }
          
          if(ButtonBox->Clicked)
          {
           PushCommand(App, Command_SetLength)->Length = SelectedVoice->NoteSetLength;;
          }
         }
         
         UI_Row() UI_SemanticWidth(UI_SizeParent(.5f, 1.f))
         {
          if(SimpleButton(S8("Clear"))) PushCommand(App, Command_ClearSelection);
         }
        }
        
        UI_List(S8("Voice"), ItemHeight)
        {
         UI_BackgroundColor(Color_ButtonBackground)
          UI_AddBox(S8("VoiceSelect"), 
                    UI_BoxFlag_Clip|
                    UI_BoxFlag_DrawBorders|
                    UI_BoxFlag_DrawBackground);
         UI_PaddingAround(ItemPadding)
         {
          u64 Idx = (u64)(SelectedVoice - App->Voices);
          f32 NewIdx = 0.f;
          
          UI_SemanticWidth(UI_SizeText(1.f, 1.f))
           UI_AddBox(Str8Fmt("Voice %2llu/%-2llu###VoiceText", Idx + 1, App->VoiceCount),
                     UI_BoxFlag_DrawDisplayString|
                     UI_BoxFlag_CenterTextVertically);
          
          UI_BorderColor(Color_Black)
           NewIdx = UI_Slider((f32)Idx + 1, 1.f, (f32)App->VoiceCount, 1.f, 0, true); 
          NewIdx -= 1.f;
          
          // Set panel voice
          if((u64)NewIdx != Idx)
          {
           voice *NewVoice = App->Voices + (u64)NewIdx;
           PushCommand(App, Command_SetPanelVoice)->Voice = NewVoice;
          }
         }
         
         UI_Row() UI_SemanticWidth(UI_SizeParent(.5f, 1.f))
         {         
          if(SimpleButton(S8("Delete"))) PushCommand(App, Command_DeleteVoice);
          if(SimpleButton(S8("New"))) PushCommand(App, Command_AddVoice);
         }
         UI_Row() UI_SemanticWidth(UI_SizeParent(.5f, 1.f))
         {
          if(SimpleButton(S8("Enqueue"))) PushCommand(App, Command_Enqueue);
          if(SimpleButton(S8("Dequeue"))) PushCommand(App, Command_Dequeue);
         }
         UI_Row() UI_SemanticWidth(UI_SizeParent(.5f, 1.f))
         {
          if(SimpleButton(S8("Play all"))) PushCommand(App, Command_PlayAllVoices);
          if(SimpleButton(S8("Stop all"))) PushCommand(App, Command_StopAllVoices);
         }
         
         // Volume control
         {                                    
          f32 Volume = SelectedVoice->Volume;
          Volume = SimpleSlider(S8("Volume"), 
                                Str8Fmt("%3.0f%%", Volume*100.f),
                                Volume, 0.f, 1.f, .01f,
                                false).Value;
          tsf_channel_set_volume(GlobalTSF, SelectedVoice->Channel, Volume);
          SelectedVoice->Volume = Volume;
         }
         
        }
        
       }
      }
     }
     
     UI_Spacer(UI_SizePx(3.f, 1.f));
     
     ui_box *PanelsBox;
     UI_FillAll()
      PanelsBox = UI_AddBox(S8("Panels"), UI_BoxFlag_Clip);
     
     v4 Region = PanelsBox->Rec;
     
     UI_FillAll() UI_Push()
     {      
      panel *ResizePanel = NilPanel;
      
      panel *Panel = App->FirstPanel;
      for(;!IsNilPanel(Panel);)
      {
       ui_box *PanelBox;
       axis2 LayoutAxis;
       
       // Create UI box for panel
       {                            
        LayoutAxis = (axis2)Panel->Parent->Axis;
        
        UI_BorderThickness(2.f)
         UI_SemanticSizeOnAxis(LayoutAxis, UI_SizeParent(Panel->ParentPct, 0.f))
         UI_TextColor(Color_Snow2)
         UI_BorderColor((Panel == App->SelectedPanel ? Color_Black : Color_Night0))
         UI_LayoutAxis((axis2)Panel->Axis)
         PanelBox = UI_AddBox(Str8Fmt("%p", Panel), 0);
        
        if(IsLeafPanel(Panel))
        {
         PanelBox->Flags |= UI_BoxFlag_DrawBorders;
         PanelBox->Flags |= UI_BoxFlag_MouseClickable;
         
         UI_PaddingAround(UI_SizePx(2.f, 1.f))
         {
          UI_FillAll()
           UI_LayoutAxis(Axis2_Y)
           UI_AddBox(S8("Panel"), 0);
          
          UI_Push()
          {
           //- Top contents 
           {
            UI_BackgroundColor(Color_ButtonBackground)
             UI_FillWidth() 
             UI_SemanticHeight(UI_SizeText(4.f, 1.f))
             UI_AddBox(S8("Top"), UI_BoxFlag_DrawBackground|
                       UI_BoxFlag_DrawBorders);
            
            UI_Push()
            {
             // Type button
             {                                                    
              ui_box *Type ;
              UI_BackgroundColor(Color_ButtonBackground)
               UI_SemanticWidth(UI_SizeText(2.f, 1.f))
               Type = UI_AddBox(S8("PanelType"), (UI_BoxFlag_DrawBackground|
                                                  UI_BoxFlag_DrawBorders|
                                                  UI_BoxFlag_MouseClickable|
                                                  UI_BoxFlag_DrawHotEffects|
                                                  UI_BoxFlag_DrawActiveEffects|
                                                  UI_BoxFlag_DrawDisplayString|
                                                  UI_BoxFlag_CenterTextVertically|
                                                  UI_BoxFlag_CenterTextHorizontally));
              
              str8 TypeName = PanelTypeStrings[Panel->Kind];
              Type->DisplayString = TypeName;
              
              if(Type->WasClicked)
              {
               command *Command = PushCommand(App, Command_OpenLister);
               Command->Panel = Panel;
               Command->ListerKind = ListerKind_PanelTypes;
              }
             }
             
             UI_Spacer(UI_SizeParent(1.f, 0.f));
             
             // Close button 
             {
              ui_box *Close;
              UI_BackgroundColor(Color_ButtonBackground)
               UI_SemanticWidth(UI_SizeText(2.f, 1.f))
               Close = UI_AddBox(S8("X"), (UI_BoxFlag_DrawBackground|
                                           UI_BoxFlag_DrawBorders|
                                           UI_BoxFlag_MouseClickable|
                                           UI_BoxFlag_DrawHotEffects|
                                           UI_BoxFlag_DrawActiveEffects|
                                           UI_BoxFlag_DrawDisplayString|
                                           UI_BoxFlag_CenterTextVertically|
                                           UI_BoxFlag_CenterTextHorizontally));
              if(Close->WasClicked)
              {
               command *Command = PushCommand(App, Command_ClosePanel);
               Command->Panel = Panel;
              }
             }
            }
           }
           
           ui_box *Contents;
           UI_FillAll()
            Contents = UI_AddBox(S8("Contents"), UI_BoxFlag_MouseClickable);
           
           //- Panel contents 
           {     
            if(0) {}
            else if(Panel->Kind == PanelKind_Settings)
            {
             Contents->Flags |= UI_BoxFlag_DrawBackground;
             Contents->BackgroundColor = Color_Night2;
             
             ui_box *ConfigList;
             
             UI_Push()
             {                                                    
              UI_FillAll()
               UI_Row()
              {
               UI_FillAll()
                UI_LayoutAxis(Axis2_Y)
                ConfigList = UI_AddBox(S8("Column"), UI_BoxFlag_Clip);
               UI_FillAll()
                UI_Push()
                UI_SemanticHeight(UI_SizePx(ItemHeight, 1.f))
               {             
                
                //- Music 
                {
                 UI_BackgroundColor(Color_ButtonBackground)
                  UI_AddBox(S8("BPM"), 
                            UI_BoxFlag_Clip|
                            UI_BoxFlag_DrawBorders|
                            UI_BoxFlag_DrawBackground);
                 UI_PaddingAround(ItemPadding)
                 {
                  UI_SemanticWidth(UI_SizeText(1.f, 1.f))
                   UI_AddBox(S8("Set BPM           "), (UI_BoxFlag_Clip|
                                                        UI_BoxFlag_DrawDisplayString|
                                                        UI_BoxFlag_CenterTextVertically));
                  
                  UI_Spacer(UI_SizeParent(1.f, 0.f));
                  
                  UI_BackgroundColor(Color_Orange)
                   UI_BorderColor(Color_Black)
                   App->BPM = UI_Slider(App->BPM, 30.f, 300.f, 1.0f, "%3.0f", true);
                 }
                 
                 UI_BackgroundColor(Color_ButtonBackground)
                  UI_AddBox(S8("TimeSig"), 
                            UI_BoxFlag_Clip|
                            UI_BoxFlag_DrawBorders|
                            UI_BoxFlag_DrawBackground);
                 UI_PaddingAround(ItemPadding)
                 {
                  UI_SemanticWidth(UI_SizeText(1.f, 1.f))
                   UI_AddBox(S8("Set time signature"), UI_BoxFlag_Clip|
                             UI_BoxFlag_DrawDisplayString|
                             UI_BoxFlag_CenterTextVertically);
                  
                  UI_Spacer(UI_SizeParent(1.f, 0.f));
                  
                  UI_BackgroundColor(Color_Orange)
                   UI_BorderColor(Color_Black)
                   App->TimeSig = (s32)UI_Slider((f32)App->TimeSig, 1.f, 4.f, 1.f, "%1.0f/4", true);
                 }
                }
                
                //- Input
                {
                 // Select MIDI Input device
                 {
                  
                  if(UI_ButtonWithToggle(S8("Toggle keyboard input"), App->InputVirtualKeyboardEnabled,
                                         ItemPadding, Color_Yellow).OneClicked)
                  {
                   PushCommand(App, Command_ToggleVirtualKeyboardInput);
                  }
                  
                  str8 Name = Str8Fmt("Set MIDI Input device  - \"%S\"###MIDIInput", Devices.Devices[App->In.Id].Name);
                  
                  ui_button_result Click = UI_ButtonWithToggle(Name, App->InputMIDIEnabled,
                                                               ItemPadding, Color_Yellow);
                  if(Click.Toggled)
                  {
                   PushCommand(App, Command_ToggleMIDIInput);
                  }
                  if(Click.Pressed)
                  {
                   PushCommand(App, Command_OpenLister)->ListerKind = ListerKind_MIDIInput;
                  }
                  
                 }
                }
                
                //- Output 
                {
                 // Select MIDI output device
                 {
                  str8 Name = Str8Fmt("Set MIDI Output device - \"%S\"###MIDIOutput", Devices.Devices[App->Out.Id].Name);
                  
                  ui_button_result Click = UI_ButtonWithToggle(Name, App->OutputMIDIEnabled, 
                                                               ItemPadding, Color_Yellow);
                  if(Click.Pressed)
                  {
                   PushCommand(App, Command_OpenLister)->ListerKind = ListerKind_MIDIOutput;
                  }
                  
                  if(Click.Toggled)
                  {
                   PushCommand(App, Command_ToggleMIDIOutput);
                  }
                 }
                 
                 // Synth toggle and open instrument lister
                 {      
                  str8 Text = (SoundFontLoaded ? 
                               App->InstrumentNames[SelectedVoice->PresetIdx] : 
                               S8("No soundfont loaded."));
                  
                  Text = Str8Fmt("Set synth instrument   - \"%S\"###Synth", Text);
                  
                  ui_button_result Click = UI_ButtonWithToggle(Text, App->OutputSynthEnabled, ItemPadding, Color_Yellow);
                  
                  App->OutputSynthEnabled ^= Click.Toggled;
                  if(Click.Pressed)
                  {
                   PushCommand(App, Command_OpenLister)->ListerKind = ListerKind_Instruments;
                  }
                 }
                }
               }
               
               // Scrollbar
               {                                                            
                f32 TotalElementSize = 6.f*ItemHeight;
                TotalElementSize -= ItemHeight;
                
                f32 Scroll = UI_Scrollbar(Axis2_Y, TotalElementSize, Panel->Scroll.Y);
                ConfigList->Scroll.Y = Scroll;
                Panel->Scroll.Y = Scroll;
               }
               
              }
             }
             
             
            }
            else if(Panel->Kind == PanelKind_Debug)
            {
             ui_box *ConfigList;
             
             Contents->Flags |= UI_BoxFlag_DrawBackground;
             Contents->BackgroundColor = Color_Night2;
             
             UI_Push()
             {                                                    
              UI_FillAll()
               UI_Row()
              {
               UI_FillAll()
                UI_LayoutAxis(Axis2_Y)
                ConfigList = UI_AddBox(S8("Column"), UI_BoxFlag_Clip);
               UI_FillAll()
                UI_Push()
                UI_SemanticHeight(UI_SizePx(ItemHeight, 1.f))
               {
                UI_State->RectDebugMode ^= UI_ButtonWithToggle(S8("UI Rects"), 
                                                               UI_State->RectDebugMode, 
                                                               ItemPadding, V4(1.f, 0.f, 1.f, 1.f)).OneClicked;
                
                if(SimpleButton(S8("Info")))
                {
                 PushCommand(App, Command_OpenLister)->ListerKind = ListerKind_DebugInfo;
                }
               }
               
               // Scrollbar
               {                                                            
                f32 TotalElementSize = 2.f*ItemHeight;
                TotalElementSize -= ItemHeight;
                
                f32 Scroll = UI_Scrollbar(Axis2_Y, TotalElementSize, Panel->Scroll.Y);
                ConfigList->Scroll.Y = Scroll;
                Panel->Scroll.Y = Scroll;
               }
              }
             }
             
            }
            else if(Panel->Kind == PanelKind_Sheet)
            {
             muze_box_data *Data = PushArray(FrameArena, muze_box_data, 1);
             Data->Box = Contents;
             Data->Voice = Panel->Voice;
             Data->App = App;
             
             Contents->CustomDraw = CustomDrawSheetMusic;
             Contents->CustomDrawData = Data;
             
             //- Scrollbar 
             {
              f32 ThumbWidth = 32.f;
              f32 HalfThumbWidth = ThumbWidth/2.f;
              f32 PaddingInsideScrollbar = 2.f;
              ui_box *Scrollbar;
              
              UI_LayoutAxis(Axis2_X)
               UI_FillWidth() 
               UI_SemanticHeight(UI_SizePx(16.f, 1.f))
               UI_AddBox(S8("ScrollbarParent"), 0);
              UI_PaddingAround(UI_SizePx(PaddingInsideScrollbar, 1.f))
              {
               UI_FillAll()
                Scrollbar = UI_AddBox(S8("Scrollbar"), UI_BoxFlag_MouseClickable);
               UI_Push()
               {                                                    
                // NOTE(luca): ]Scrollable width = BarWidth * BarCount + BoxWidth - BarWidth 
                
                f32 WholeBarWidth = 100.f;
                
                f32 BPS = (App->BPM/60.f);
                f32 BarDuration = (1.f/BPS)*(f32)App->TimeSig;
                f32 BarCount = 1.f + FloorF32(Panel->Voice->RecordLength/(BarDuration + 1e-6f));
                
                // NOTE(luca): Make it so that we can scroll until there is only one bar of space left empty.
                f32 ScrollableWidth = Scrollbar->FixedSize.X - ThumbWidth;
                
                f32 ExtraWidth = (Contents->FixedSize.X - WholeBarWidth);
                
                f32 Zoom = (BPS/(f32)App->TimeSig*WholeBarWidth);
                f32 WholeSheetWidth = Panel->Voice->RecordLength*Zoom;
                WholeSheetWidth -= ExtraWidth;
                
                f32 Factor = WholeSheetWidth/ScrollableWidth;
                
                ui_box *Spacer;
                UI_FillHeight()
                 Spacer = UI_AddBox(S8(""), 0);
                
                ui_box *Thumb;
                
                UI_SemanticWidth(UI_SizePx(ThumbWidth, 1.f))
                 UI_BorderColor(Color_Black)
                 UI_BackgroundColor(Color_Orange)
                 Thumb = UI_AddBox(S8("Thumb"), 
                                   UI_BoxFlag_DrawBackground|
                                   UI_BoxFlag_DrawBorders|
                                   UI_BoxFlag_MouseClickable|
                                   UI_BoxFlag_DrawHotEffects|
                                   UI_BoxFlag_DrawActiveEffects);
                
                f32 ScrollX = Panel->Scroll.X;
                
                if(Panel->Voice->AutoScroll)
                {
                 f32 Pad = Contents->FixedSize.X/2.f;
                 
                 // TODO(luca): When user stops playing it should be playing position and we user stops recording it should be RecordLength;
                 f32 End = (Panel->Voice->IsPlaying ? 
                            Panel->Voice->PlayPos :
                            Panel->Voice->RecordLength);
                 
                 f32 Width = (End*BPS/(f32)App->TimeSig)*WholeBarWidth;
                 
                 f32 AutoScrollFactor = (Width - (Contents->FixedSize.X - Pad))/ScrollableWidth;
                 
                 ScrollX = ScrollableWidth*AutoScrollFactor;
                 
                 f32 MaxScrollX = ScrollableWidth*Factor;
                 ScrollX = ClampTop(ScrollX, MaxScrollX);
                 
                 // When it all fits, scrolling shouldn't be possible.
                 {                                                                    
                  if(WholeSheetWidth < 0.f)
                  {
                   ScrollX = 0.f;
                  }
                  
                  if(Width + Pad < Contents->FixedSize.X)
                  {
                   ScrollX = 0.f;
                  }
                 }
                }
                
                if(UI_IsActive(Thumb) || UI_IsActive(Scrollbar))
                {
                 if(Panel->Voice->AutoScroll)
                 {
                  PushCommand(App, Command_ToggleAutoScroll);
                 }
                 
                 // TODO(luca): It should not be half thumb width, it should be divided by where the mouse clicked inside the thumb.  E.g., if it clicked on the left part the first part will be small and the second part will be bigger.
                 f32 MouseX = (f32)Input->Mouse.Pos.X;
                 f32 RelX = MouseX - Scrollbar->FixedPos.X;
                 RelX = Clamp(HalfThumbWidth, 
                              RelX, 
                              Scrollbar->FixedSize.X - HalfThumbWidth); 
                 RelX -= HalfThumbWidth;
                 
                 
                 ScrollX = RelX*Factor;
                 
                 if(WholeSheetWidth < Contents->FixedSize.X)
                 {
                  ScrollX = 0.f;
                 }
                }
                
                Panel->Scroll.X = ScrollX;
                Contents->Scroll.X = Panel->Scroll.X;
                
                f32 SpaceBeforeThumb = Panel->Scroll.X/Factor;
                Spacer->SemanticSize[Axis2_X] = UI_SizePx(SpaceBeforeThumb, 1.f);
                
               }
               
              }
              
             }
            }
            else if(Panel->Kind == PanelKind_Roll)
            {
             muze_box_data *Data = PushArray(FrameArena, muze_box_data, 1);
             Data->Box = Contents;
             Data->Voice = Panel->Voice;
             Data->App = App;
             
             Contents->CustomDraw = CustomDrawPianoRoll;
             Contents->CustomDrawData = Data;
            }
           }
          }
         }
         
         
         // Set selected panel
         {         
          b32 ChildIsActive = false;
          
          for(ui_box *Child = PanelBox->First;
              (!UI_IsNilBox(Child) && Child != PanelBox);
              Child = UI_BoxDepthFirstPreOrder(Child).Next)
          {
           if(UI_IsActive(Child) && !UI_KeyMatch(UI_KeyNull(), Child->Key))
           {
            ChildIsActive = true;
            break;
           }
          }
          
          if(PanelBox->WasClicked || ChildIsActive)
          {
           App->SelectedPanel	= Panel;
          }
         }
         
        }
        else
        {
         PanelBox->Flags |= UI_BoxFlag_Clip;
        }
        
        Panel->Region = PanelBox->Rec;
       }
       
       panel *NextPanel = NilPanel;
       
       //- Find the next panel in depth first pre order, if this panel is a leaf panel add a resize UI box 
       {                            
        if(!IsNilPanel(Panel->First))
        {
         NextPanel = Panel->First;
         
         UI_PushBox();
        }
        else for(panel *P = Panel; !IsNilPanel(P); P = P->Parent)
        {
         if(!IsNilPanel(P->Next))
         {
          LayoutAxis = P->Parent->Axis;
          
          ui_box *GapBox;
          
          // Create UI box for gap
          {                                    
           f32 GapSize = 12.f;
           
           s32 GapFlags = (UI_BoxFlag_MouseClickable|
                           UI_BoxFlag_DrawHotEffects|
                           UI_BoxFlag_DrawActiveEffects|
                           UI_BoxFlag_DrawBackground);
           if(LayoutAxis == Axis2_X) GapFlags |= UI_BoxFlag_MirrorEffects;
           
           UI_BackgroundColor(Color_Night2)
            UI_SemanticSizeOnAxis(LayoutAxis, UI_SizePx(GapSize, 1.f))
            UI_SemanticSizeOnAxis(1 - LayoutAxis, UI_SizeParent(1.f, 0.f))
            GapBox = UI_AddBox(S8("Gap"), GapFlags);
          }
          
          // Set cursor shape to resizing one
          {                                    
           if(UI_IsHot(GapBox))
           {
            Input->PlatformCursor = (PlatformCursorShape_ResizeHorizontal +
                                     !!(LayoutAxis == Axis2_Y));
           }
          }
          
          // Gap box (resize border)
          {                                    
           if(UI_IsActive(GapBox))
           {
            s32 PosOnAxis = Input->Mouse.Pos.e[LayoutAxis];
            
            if(GapBox->WasClicked)
            {
             DragData->StartPos = PosOnAxis;
             DragData->StartPct = P->ParentPct;
             DragData->NextStartPct = P->Next->ParentPct;
            }
            
            Assert(IsNilPanel(ResizePanel));
            ResizePanel = P; 
           }
           
          }
          
          NextPanel = P->Next;
          break;
         }
         
         UI_PopBox();
        }
       }
       
       Panel = NextPanel;
      }
      
      if(!IsNilPanel(ResizePanel))
      {                        
       panel *P = ResizePanel;
       
       axis2 LayoutAxis = P->Parent->Axis;
       s32 PosOnAxis = Input->Mouse.Pos.e[LayoutAxis];
       f32 Size = SizeOnAxis(P->Parent->Region, LayoutAxis);
       
       s32 dPos = PosOnAxis - DragData->StartPos;
       f32 dPct = (f32)dPos/Size;
       
       if(DragData->NextStartPct - dPct > .05f &&
          DragData->StartPct     + dPct > .05f)
       {                                                
        P->ParentPct = DragData->StartPct + dPct;
        P->Next->ParentPct = DragData->NextStartPct - dPct; 
       }
      }
      
      
     }
     
     NoOp();
    }
   }
   UI_EndLayout(Root->First);
   
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
  
  if(App->ListerOpened)
  {
   UI_EndLayout(App->ListerBox);
  }
  
 }
 
 for EachIndex(Idx, App->CommandCount)
 {
  command *Command = App->Commands + Idx;
  
  switch(Command->Kind)
  {
   default: InvalidPath(); break;
   
   //- Device commands 
   {   
    case Command_SelectInputDevice:
    {
     // NOTE(luca): Stop all notes on all voices. 
     StopAllPlayingNotes(Memory, App, SelectedVoice);
     
     App->In = *Command->Device;
     Memory->PlatformMIDIListen(App->In);
    } break;
    
    case Command_SelectOutputDevice:
    {
     App->Out = *Command->Device;
    } break;
    
    case Command_SelectInstrument:
    {
     if(Command->PresetIdx != SelectedVoice->PresetIdx)
     {
      tsf_channel_note_off_all(GlobalTSF, SelectedVoice->Channel);
     }
     
     SelectedVoice->PresetIdx = Command->PresetIdx;
     tsf_channel_set_presetindex(GlobalTSF, SelectedVoice->Channel, SelectedVoice->PresetIdx);
    } break;
    
    case Command_ToggleMIDIInput:
    {
     App->InputMIDIEnabled ^= 1;
    } break;
    case Command_ToggleMIDIOutput:
    {
     App->OutputMIDIEnabled ^= 1;
    } break;
    case Command_ToggleVirtualKeyboardInput:
    {
     App->InputVirtualKeyboardEnabled ^= 1;
    } break;
    case Command_ToggleSynthOutput:
    {
     App->OutputSynthEnabled ^= 1;
    } break;
   }
   
   //- Voice commands 
   {   
    case Command_AddVoice: 
    { 
     App->SelectedPanel->Voice = VoiceAdd(App);
    } break;
    case Command_DeleteVoice:
    {
     NotImplemented();
     // 1. Find all references to this voice
     // 2. Set them to the previous voice ?
     // 3. Delete the voice
    } break;
    
    case Command_SetPanelVoice:
    {
     App->SelectedPanel->Voice = Command->Voice;
    } break;
    
    case Command_PlayAllVoices:
    {
     for EachIndex(VoiceIdx, App->VoiceCount)
     {
      voice *Voice = App->Voices + (u64)VoiceIdx;
      StopAllPlayingNotes(Memory, App, Voice);
      StopRecording(Voice);
      
      Voice->PlayPos = 0.f;
      Voice->IsPlaying = true;
     }
    } break;
    
    case Command_StopAllVoices:
    {
     for EachIndex(VoiceIdx, App->VoiceCount)
     {
      voice *Voice = App->Voices + (u64)VoiceIdx;
      StopAllPlayingNotes(Memory, App, Voice);
      StopRecording(Voice);
      
      Voice->IsPlaying = false;
     }
    } break;
    
    case Command_Enqueue:
    {
     NotImplemented();
    } break;
    
    case Command_Dequeue:
    {
     NotImplemented();
    } break;
    
   }
   
   //- Note commands 
   {   
    case Command_ToggleRecording:
    {
     StopAllPlayingNotes(Memory, App, SelectedVoice);
     
     if(SelectedVoice->IsRecording)
     {
      StopRecording(SelectedVoice);
     }
     else
     {
      VoiceReset(SelectedVoice);
      SelectedVoice->RecordStart = GetWallTime();
      SelectedVoice->PlayPos = 0.f;
      SelectedVoice->IsRecording = true;
     }
     
     SelectedVoice->IsPlaying = false;
    } break;
    
    case Command_TogglePlaying:
    {
     StopAllPlayingNotes(Memory, App, SelectedVoice);
     StopRecording(SelectedVoice);
     
     SelectedVoice->IsPlaying ^= 1;
    } break;
    
    case Command_ToggleAutoScroll:
    {
     SelectedVoice->AutoScroll ^= 1;
    } break;
    
    case Command_Reset:
    {
     VoiceReset(SelectedVoice);
     SelectedVoice->IsRecording = false;
     SelectedVoice->IsPlaying = false;
     StopAllPlayingNotes(Memory, App, SelectedVoice);
    } break;
    
    case Command_Stop:
    {
     StopRecording(SelectedVoice);
     StopAllPlayingNotes(Memory, App, SelectedVoice);
     SelectedVoice->IsPlaying = false;
     SelectedVoice->PlayPos = 0.f;
    } break;
    
    case Command_PlayFromNote:
    case Command_PlayFromStart:
    {
     StopRecording(SelectedVoice);
     
     f32 Pos = 0.f;
     
     if(Command->Kind == Command_PlayFromNote)
     {
      note_node *Note = SelectedVoice->NoteSel;
      if(Note)
      {
       Pos = Note->Value->Timestamp;
      }
     }
     
     SelectedVoice->PlayPos = Pos;
     SelectedVoice->IsPlaying = true;
    } break;
    
    case Command_Trim:
    {
     if(!IsNilNote(SelectedVoice->FirstNote))
     {
      StopAllPlayingNotes(Memory, App, SelectedVoice);
      
      note *LastNote = SelectedVoice->LastNote;
      
      f32 LastNoteEnd = (LastNote->Timestamp + LastNote->Duration); 
      //- Find the last note's end.
      {
       for EachNote(Note, SelectedVoice->FirstNote)
       {
        if(Note->Kind == NoteKind_Note)
        {
         LastNoteEnd = Max(LastNoteEnd, (Note->Timestamp + Note->Duration));
        }
       }
      }
      
      SelectedVoice->RecordLength = LastNoteEnd;
      
      note *FirstNote = SelectedVoice->FirstNote;
      //- Find the first note played.
      {
       for EachNote(Note, FirstNote)
       {
        if(Note->Kind == NoteKind_Note)
        {
         FirstNote = Note;
         break;
        }
       }
      }
      
      f32 StartSilence = FirstNote->Timestamp;
      
      //- Get new voice length 
      {                                    
       SelectedVoice->RecordLength -= StartSilence;
       
       f32 BeatsPerSecond = App->BPM/60.f;
       f32 SecondsPerBeat = 1.f/BeatsPerSecond;
       
       f32 BarTime = SecondsPerBeat*(f32)App->TimeSig;
       // TODO(luca): Intrinsic
       f32 Pad = CeilF32(SelectedVoice->RecordLength/BarTime)*BarTime;
       
       SelectedVoice->RecordLength = Pad;
      }
      
      //- Update notes .
      {
       for EachNote(Note, SelectedVoice->FirstNote)
       {
        
        if(Note->Kind == NoteKind_Pedal)
        {
         // If the pedal note is before the first note, then we forward it up to the firstnote.
         f32 Diff = (FirstNote->Timestamp - Note->Timestamp);
         if(Diff > 0.f)
         {
          Note->Duration -= Diff;
          Note->Timestamp += Diff;
         }
         
         // Clamp the end to the record's length.
         {
          f32 NewTimestamp = Note->Timestamp - StartSilence;
          
          f32 NoteEnd = NewTimestamp + Note->Duration;
          f32 NewEnd = Min(NoteEnd, SelectedVoice->RecordLength);
          Note->Duration = (NewEnd - NewTimestamp);
         }
        }
        
        Note->Timestamp -= StartSilence;
        
       }
      }
     }
    } break;
    
    case Command_Round:
    {
     f32 BPS = App->BPM/60.f;
     
     for EachNoteNode(Note, SelectedVoice->NoteSel)
     {
      f32 NoteLength = Note->Duration*BPS;
      f32 DenomOfFraction = RoundF32(-Log2F32(NoteLength));
      f32 NoteValue = (DenomOfFraction > 0.f ? 
                       (1.f/2.f*DenomOfFraction) : 
                       RoundF32(NoteLength));
      f32 NewDuration = NoteValue/BPS;
      Note->Duration = NewDuration;
     }
    } break;
    
    case Command_Sync:
    {
     f32 BPS = App->BPM/60.f;
     for EachNoteNode(Note, SelectedVoice->NoteSel)
     {
      f32 NoteLength = Note->Duration*BPS;
      f32 DenomOfFraction = RoundF32(-Log2F32(NoteLength));
      f32 NoteValue = (DenomOfFraction > 0.f ? 
                       (1.f/2.f*DenomOfFraction) : 
                       RoundF32(NoteLength));
      
      f32 Timestamp = BPS*Note->Timestamp;
      
      f32 NewTimestamp = Note->Timestamp;
      NewTimestamp = (NoteValue >= 1.f ? 
                      RoundF32(Timestamp) : 
                      (RoundF32(Timestamp/NoteValue)*NoteValue));
      
      f32 RealNewTimestamp = NewTimestamp/BPS;
      Note->Timestamp = RealNewTimestamp;
      
     }
    } break;
    
    case Command_GuessBPM:
    {
     note_node *Node = SelectedVoice->NoteSel;
     
     f32 AveragedDiff = 0.f;
     f32 Diff = 0.f;
     
     if(Node && Node->Next)
     {
      note *Start = Node->Value;
      note *End = Node->Next->Value;
      
      if(Start->Timestamp > End->Timestamp) Swap(Start, End);
      
      Diff = (End->Timestamp - Start->Timestamp);
      
      Node = Node->Next;
      
      // Time that goes in a bar
      f32 SecondsPerBeat = Diff/(f32)App->TimeSig;
      App->BPM = (1.f/SecondsPerBeat)*60.f;
     }
     
    } break;
    
    case Command_SetLength:
    {
     
     f32 NoteSetLength = SelectedVoice->NoteSetLength;
     f32 Length = PowF32(2.f, SelectedVoice->NoteSetLength);
     
     f32 SecondsPerBeat = 1.f/(App->BPM/60.f);
     f32 Duration = SecondsPerBeat * Length;
     
     for EachNoteNode(Note, SelectedVoice->NoteSel)
     {
      Note->Duration = Duration;
     }
     
    } break;
    
    case Command_ClearSelection:
    {
     SelectedVoice->NoteSel = 0;
    } break;
    
    case Command_DeleteSelection: 
    {
     for(note_node *Node = SelectedVoice->NoteSel;
         Node;)
     {
      SelectedVoice->NoteCount -= 1;
      
      note *Note = Node->Value;
      
      // Remove the note
      {                                
       if(!IsNilNote(Note->Next))
       {
        Note->Next->Prev = Note->Prev;
       }
       
       if(!IsNilNote(Note->Prev))
       {
        Note->Prev->Next = Note->Next;
       }
       
       if(Note == SelectedVoice->FirstNote)
       {
        SelectedVoice->FirstNote = Note->Next;
       }
       
       if(Note == SelectedVoice->LastNote)
       {
        SelectedVoice->LastNote = Note->Prev;
       }
      }
      
      note_node *NextNode = Node->Next;
      
      // Remove from selection
      {                                
       // Remove links
       {
        if(Node->Prev)
        {
         Node->Prev->Next = Node->Next;
        }
        
        if(Node->Next)
        {
         Node->Next->Prev = Node->Prev;
        }
        
        if(Node == SelectedVoice->NoteSel)
        {
         SelectedVoice->NoteSel = (Node->Next ? Node->Next : Node->Prev);
        }
       }
       
       // Put on free list
       {
        Node->Next = App->FirstFreeNode;
        App->FirstFreeNode = Node;
       }
      }
      
      Node = NextNode;
     }
    } break;
   }
   
   //- Lister commands 
   {   
    case Command_CloseLister:
    {
     App->ListerOpened = false;
    } break;
    case Command_OpenLister:
    {
     App->ListerOpened = true;
     App->ListerKind = Command->ListerKind;
     App->Text.Size = 0;
     App->ListerScrollPct = 0.f;
    } break;
   }
   
   //- Panel commands 
   case Command_ClosePanel:
   {
    App->SelectedPanel = ClosePanel(App, Command->Panel);
   } break;
   
   case Command_SplitPanel:
   {
    App->SelectedPanel = SplitPanel(Command->Panel, Command->Axis, false);
    App->SelectedPanel->Voice = SelectedVoice;
   } break;
   
   case Command_SetPanelType:
   {
    Command->Panel->Kind = Command->PanelKind;
    MemoryZero(&Command->Panel->Scroll);
   } break;
  }
 }
 
 f32 MaxRecordingLength = 3600.f;
 
 for EachIndex(Idx, App->VoiceCount)
 {    
  voice *Voice = App->Voices + Idx;
  
  if(Voice->IsPlaying)
  {
   // NOTE(luca): N^M ? But I won't ever have many voices, yes?  If I do, I can just cache the last played note and loop form there as an optimization.
   for EachNote(Note, Voice->FirstNote)
   {
    f32 NoteStart = Note->Timestamp;
    f32 NoteEnd = NoteStart + Note->Duration;
    
    // TODO(luca): If a note is shorter than dtForFrame it could skip it.
    
    if(NoteStart <= Voice->PlayPos &&
       NoteStart > (Voice->PlayPos - dtForFrame))
    {
     PlayNote(Memory, App, Voice, Note);
    }
    
    if(NoteEnd <= Voice->PlayPos &&
       NoteEnd > (Voice->PlayPos - dtForFrame))
    {
     note OffNote = *Note;
     OffNote.Velocity = 0;
     PlayNote(Memory, App, Voice, &OffNote);
    }
   }
   
   Voice->PlayPos += dtForFrame;
   if(Voice->PlayPos >= Voice->RecordLength)
   {
    StopAllPlayingNotes(Memory, App, Voice);
    
    Voice->PlayPos = 0.f;
   }
  }
  
  if(Voice->IsRecording)
  {
   Voice->RecordLength = (GetWallTime() - Voice->RecordStart);
   
   if(Voice->RecordLength >= MaxRecordingLength)
   {
    StopRecording(Voice);
   }
  }
  
 }
 
 // MIDI Processing
 {    
  //- MIDI notes from virtual keyboard 
  if(App->InputVirtualKeyboardEnabled && !App->ListerOpened)
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
   
   ProcessMIDINotes(Memory, App, SelectedVoice, Events, EventCount);
  }
  
  //- MIDI notes from Input 
  if(App->InputMIDIEnabled)
  {
   ProcessMIDINotes(Memory, App, SelectedVoice, Input->MIDI.Events, Input->MIDI.EventCount);
  }
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
#if 0
#elif SAMPLE_FORMAT == f32
 tsf_render_float(GlobalTSF, Sound->Samples, (int)SampleCount, 0);
#elif SAMPLE_FORMAT == s16
 tsf_render_short(GlobalTSF, Sound->Samples, (int)SampleCount, 0);
#endif
}