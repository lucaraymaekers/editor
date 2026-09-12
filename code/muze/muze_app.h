/* date = January 27th 2026 7:06 pm */

#ifndef MUZE_APP_H
#define MUZE_APP_H

//~ Types

//- Muze 
typedef enum piece_note_kind piece_note_kind;
enum piece_note_kind
{
 PieceNoteKind_Silence,
 PieceNoteKind_Pitch,
};

typedef struct piece_note piece_note;
struct piece_note
{
 piece_note_kind Kind;
 f32 Length;
 note_pitch Pitch;
 s32 Octave;
};

typedef struct piece piece; 
struct piece 
{
 f32 BPM;
 f32 TimeSigNum;
 f32 TimeSigDen;
 note_pitch Key;
 b32 Major;
 
 u64 NoteMaxCount;
 u64 NoteCount;
 piece_note *Notes;
};

typedef enum note_kind note_kind;
enum note_kind
{
 NoteKind_Note,
 NoteKind_Pedal
};

typedef struct rec_note rec_note;
struct rec_note
{
 rec_note *Next;
 rec_note *Prev;
 
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
 rec_note *Value;
 note_node *Next;
 note_node *Prev;
};
#define EachNoteNode(Index, First) \
(note_node *_node = (First); _node; _node = _node->Next) \
for (rec_note *Index = _node->Value, *_note = _node->Value; _note; _note = 0)
#define EachNote(Index, First) (rec_note *Index = First; !IsNilNote(Index); Note = Note->Next)
#define EachNoteBack(Index, Last) (rec_note *Index = Last; !IsNilNote(Index); Note = Note->Prev)

typedef struct voice voice;
struct voice
{
 rec_note *FirstNote;
 rec_note *LastNote;
 s64 NoteCount;
 
 b32 IsRecording;
 b32 IsPlaying;
 b32 AutoScroll;
 
 f32 RecordStart;
 f32 RecordLength;
 f32 PlayPos;
 
 u8 MaxPitch;
 u8 MinPitch;
 
 f32 NoteSetLength;
 
 note_node *NoteSel;
 
 int Channel;
 int PresetIdx;
 f32 Volume;
 
 f32 SheetScrollX;
 
 arena *Arena;
};

//- Panels

typedef struct panel panel;
struct panel
{
 // NOTE(luca): This has to be the first field for the raddbg type view to work.
 panel *First;
 panel *Last;
 panel *Next;
 panel *Prev;
 panel *Parent;
 
 axis2 Axis;
 f32 ParentPct;
 
 v4 Region;
 
 panel_kind Kind;
 voice *Voice;
 v2 Scroll;
 piece *Piece;
};
raddbg_type_view(panel, 
                 rows($,
                      (&First == NilPanel || &First == 0) ? "Nil" : "",
                      ParentPct,
                      Kind,
                      (Axis == Axis2_X ? "X" : "Y"),
                      list($, Next),
                      First));
#define EachChildPanel(Child, Parent) \
(panel *Child = Parent->First; !IsNilPanel(Child); Child = Child->Next)

typedef struct panel_rec panel_rec;
struct panel_rec
{
 panel *Next;
 s32 PushCount;
 s32 PopCount;
};

typedef struct drag_data drag_data;
struct drag_data
{
 s32 StartPos;
 f32 StartPct;
 f32 NextStartPct;
};

//- State 
enum command_kind
{
 Command_None,
 
 Command_SelectInputDevice,
 Command_SelectOutputDevice,
 Command_SelectInstrument,
 Command_ToggleMIDIInput,
 Command_ToggleMIDIOutput,
 Command_ToggleVirtualKeyboardInput,
 Command_ToggleSynthOutput,
 
 Command_AddVoice,
 Command_DeleteVoice,
 Command_SetPanelVoice,
 Command_PlayAllVoices,
 Command_StopAllVoices,
 
 Command_Enqueue,
 Command_Dequeue,
 
 Command_ToggleRecording,
 Command_TogglePlaying,
 Command_ToggleAutoScroll,
 
 Command_Trim,
 Command_Stop,
 Command_Reset,
 Command_PlayFromStart,
 Command_PlayFromNote,
 
 Command_Round,
 Command_Sync,
 Command_GuessBPM,
 Command_SetLength,
 Command_ClearSelection,
 Command_DeleteSelection,
 
 Command_CloseLister,
 Command_OpenLister,
 
 Command_ClosePanel,
 Command_SplitPanel,
 Command_SetPanelType,
};
typedef enum command_kind command_kind;

enum lister_kind
{
 ListerKind_Instruments,
 ListerKind_MIDIInput,
 ListerKind_MIDIOutput,
 ListerKind_DebugInfo,
 ListerKind_PanelTypes,
};
typedef enum lister_kind lister_kind;

typedef struct command command;
struct command
{
 command_kind Kind;
 
 panel *Panel;
 panel_kind PanelKind;
 voice *Voice;
 s32 PresetIdx;
 platform_midi_device *Device;
 lister_kind ListerKind;
 f32 Length;
 axis2 Axis;
};

typedef struct app_state app_state;
struct app_state
{
 //- UI 
 arena *UIArena;
 font Font;
 font IconsFont;
 font_atlas FontAtlas;
 arena *FontAtlasArena; 
 f32 PreviousHeightPx;
 f32 HeightPx;
 
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
 
 // TSF 
 tsf *TrackerForTSF;
 arena *TSFArena;
 
 s32 InstrumentCount;
 str8 *InstrumentNames;
 
 // UI 
 ui_box *TopBox;
 ui_box *ListerBox;
 f32 ListerScroll;
 b32 ListerOpened;
 lister_kind ListerKind;
 
 u64 MaxTextSize;
 str8 Text;
 f32 CursorAnimTime;
 
 platform_midi_device In;
 platform_midi_device Out;
 
 b32 OutputMIDIEnabled;
 b32 OutputSynthEnabled;
 b32 InputMIDIEnabled;
 b32 InputVirtualKeyboardEnabled;
 
 s64 MaxVoiceCount;
 s64 VoiceCount;
 voice *Voices;
 voice *SelectedVoice;
 
 s32 TimeSig;
 f32 BPM;
 
 // Application memory
 arena *Arena;
 note_node *FirstFreeNode;
 
 // NOTE(luca): PushBuffer
 umm MaxCommandCount;
 umm CommandCount;
 command *Commands;
 
 // Debug
 u64 MaxDebugStringCount;
 u64 DebugStringCount;
 str8 *DebugStrings;
 
 //- Rendering 
 gl_render_state Render;
};

typedef struct lister_item lister_item;
struct lister_item
{
 str8 Name;
 u64 Idx;
};

//- UI 
typedef struct piece_box_data piece_box_data;
struct piece_box_data
{
 ui_box *Box;
 piece *Piece;
};

typedef struct muze_box_data muze_box_data;
struct muze_box_data
{
 ui_box *Box;
 app_state *App;
 voice *Voice;
};

typedef struct custom_draw__single_line_text_input_params custom_draw__single_line_text_input_params;
struct custom_draw__single_line_text_input_params
{
 ui_box *Box;
 str8 Text;
 f32 CursorAnimTime;
};

//~ Globals
#define DefaultHeightPx 20

global_variable rec_note *NilNote;
global_variable piece_note *NilPieceNote;
global_variable f32 dtForFrame;

#endif //MUZE_APP_H