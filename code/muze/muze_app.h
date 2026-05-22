/* date = January 27th 2026 7:06 pm */

#ifndef MUZE_APP_H
#define MUZE_APP_H

//~ Types

//- Muze 
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

typedef enum note_kind note_kind;
enum note_kind
{
    NoteKind_Note,
    NoteKind_Pedal
};

typedef struct note note;
struct note
{
    note *Next;
    note *Prev;
    
    u8 Pitch;
    u8 Velocity;
    u8 Controller;
    f32 Timestamp;
    f32 Duration;
    
    note_kind Kind;
};
raddbg_type_view(note, rows(Note, (s32)Pitch, no_char((note_pitch)(Pitch%Note_Count)), no_char(Velocity), omit($, Pitch, Velocity)));

typedef struct note_node note_node;
struct note_node
{
    note *Value;
    note_node *Next;
    note_node *Prev;
};
#define EachNoteNode(Index, First) \
(note_node *_node = (First); _node; _node = _node->Next) \
for (note *Index = _node->Value, *_note = _node->Value; _note; _note = 0)
#define EachNote(Index, First) (note *Index = First; !IsNilNote(Index); Note = Note->Next)
#define EachNoteBack(Index, Last) (note *Index = Last; !IsNilNote(Index); Note = Note->Prev)

// TODO(luca): Rename to voice
typedef struct voice voice;
struct voice
{
    note *FirstNote;
    note *LastNote;
    s64 NoteCount;
    
    b32 IsRecording;
    b32 IsPlaying;
    
    f32 RecordStart;
    f32 RecordLength;
    f32 PlayPos;
    
    u8 MaxPitch;
    u8 MinPitch;
    
    note_node *NoteSel;
    
    int Channel;
    int PresetIdx;
    
    arena *Arena;
};

//- Panels
typedef enum panel_kind panel_kind;
enum panel_kind
{
    PanelKind_Empty,
    PanelKind_Muze,
};

typedef struct panel panel;
struct panel
{
    // NOTE(luca): This has to be the first field for the raddbg type view to work.
    panel *First;
    panel *Last;
    panel *Next;
    panel *Prev;
    panel *Parent;
    
    s32 Axis;
    f32 ParentPct;
    
    ui_box *Root;
    v4 Region;
    
    panel_kind Kind;
    voice *Voice;
};
raddbg_type_view(panel, 
                 rows($,
                      (&First == NilPanel || &First == 0) ? "Nil" : "",
                      ParentPct,
                      Kind,
                      (Axis == Axis2_X ? "X" : "Y"),
                      list($, Next),
                      First));
#define EachChildPanel(Child, Parent) (panel *Child = Parent->First; !IsNilPanel(Child); Child = Child->Next)

typedef struct panel_rec panel_rec;
struct panel_rec
{
    panel *Next;
    s32 PushCount;
    s32 PopCount;
};

//- State 
enum command_kind
{
    Command_None,
    
    Command_SelectInputDevice,
    Command_SelectInstrument,
    
    Command_SelectVoice,
    Command_AddVoice,
    Command_DeleteVoice,
    
    Command_PlayAllVoices,
    Command_StopAllNotes,
    Command_ToggleRecording,
    Command_TogglePlaying,
    
    Command_ClosePanel,
    Command_SplitPanelLeft,
    Command_SplitPanelRight,
    Command_SplitPanelDown,
    Command_SplitPanelUp,
};
typedef enum command_kind command_kind;

typedef struct command command;
struct command
{
    command_kind Kind;
    
    voice *Voice;
    s32 PresetIdx;
    platform_midi_device *Device;
};

typedef struct app_state app_state;
struct app_state
{
    //- UI 
    // TODO(luca): This is already in the FontAtlas, so it should go away?
    font Font;
    font IconsFont;
    font_atlas FontAtlas;
    arena *FontAtlasArena; 
    f32 PreviousHeightPx;
    f32 HeightPx;
    
    // TODO(luca): Move to UI state ?
    arena *UIArena;
    
    //- Panels 
    panel *SelectedPanel;
    panel *FirstPanel;
    panel *FreePanel;
    arena *PanelArena;
    panel *DebugPanel;
    
    //- Misc. 
    
    u64 FrameIdx;
    arena *ReadOnlyArena;
    
    //- Muze 
    
    struct
    {
        ui_box *TopBox;
        
        platform_midi_device In;
        platform_midi_device Out;
        
        b32 IsOutputSynth;
        b32 IsInputVirtualKeyboard;
        
        u64 MaxVoiceCount;
        s64 VoiceCount;
        voice *Voices;
        voice *LastSelectedVoice;
        
        s32 TimeSig;
        f32 BPM;
        
        arena *Arena;
        note_node *FirstFreeNode;
        
        // NOTE(luca): PushBuffer
        umm MaxCommandCount;
        umm CommandCount;
        command *Commands;
    } Muze;
    
    //- Rendering 
    gl_render_state Render;
    
    //- TSF 
    tsf *TrackerForTSF;
    arena *TSFArena;
};

//~ Globals
#define DefaultHeightPx 20

global_variable note *NilNote;
global_variable f32 dtForFrame;

#endif //MUZE_APP_H
