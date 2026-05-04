/* date = January 27th 2026 7:06 pm */

#ifndef MUZE_APP_H
#define MUZE_APP_H

//~ Types
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

typedef struct app_text app_text; 
struct app_text
{
    u64 Cursor;
    u64 Count;
    u64 Capacity;
    u64 Trail;
    u64 CurRelLine;
    f32 CursorAnimTime;
    rune *Data;
    
    u64 PrevCursor;
    // Computed each frame
    u64 Lines;
};

typedef enum panel_kind panel_kind;
enum panel_kind
{
    PanelKind_Free,
    PanelKind_Text,
    PanelKind_Muze,
};

typedef struct panel panel;
struct panel
{
    panel *First;
    panel *Last;
    panel *Next;
    panel *Prev;
    panel *Parent;
    
    s32 Axis;
    f32 ParentPct;
    
    ui_box *Root;
    v4 Region;
    
    b32 CannotClose;
    
    panel_kind Kind;
    app_text *Text;
};
raddbg_type_view(panel, 
                 no_addr(rows($,
                              (&First == NilPanel || &First == 0),
                              ParentPct,
                              Kind,
                              (Axis == Axis2_X ? "X" : "Y"))));
#define EachChildPanel(Child, Parent) (panel *Child = Parent->First; !IsNilPanel(Child); Child = Child->Next)

typedef struct panel_node panel_node;
struct panel_node
{
    panel *Value;
    
    // TODO(luca): Double Linked List
    panel_node *Next;
};
#define EachPanel(Index, First) EachNode(Index, panel_node, First)

typedef struct panel_rec panel_rec;
struct panel_rec
{
    panel *Next;
    s32 PushCount;
    s32 PopCount;
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
    u8 Pitch;
    u8 Velocity;
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
#define EachNote(Note, Notes, Count) \
(note *Note = Notes; Note != (Notes + Count); Note = Note + 1)

typedef struct song song;
struct song
{
    platform_midi_device In;
    platform_midi_device Out;
    b32 IsOutputSynth;
    b32 IsInputVirtualKeyboard;
    
    s64 NotesCount;
    note *Notes;
    note_node *NoteSel;
    arena *NoteNodesArena;
    note_node *FreeNode;
    
    f32 RecordStart;
    f32 RecordLength;
    
    u8 MaxPitch;
    u8 MinPitch;
    
    b32 IsRecording;
    b32 IsPlaying;
    
    f32 PlayPos;
    
    s32 TimeSig;
    f32 BPM;
};

typedef struct app_state app_state;
struct app_state
{
    // TODO(luca): This is already in the FontAtlas, so it should go away?
    font Font;
    font IconsFont;
    font_atlas FontAtlas;
    arena *FontAtlasArena; 
    f32 PreviousHeightPx;
    f32 HeightPx;
    
    // TODO(luca): Move to UI state ?
    arena *UIArena;
    
    song Song;
    
    struct
    {
        arena *TextArena;
    };
    
    struct
    {
        panel *SelectedPanel;
        panel *FirstPanel;
        panel_node *FreePanel;
        arena *PanelArena;
        panel *DebugPanel;
    };
    
    u64 FrameIdx;
    
    gl_render_state Render;
    
    // Nils
    tsf *TrackerForTSF;
    ui_box *TrackerForUI_NilBox;
    ui_state *TrackerForUI_State;
    panel *TrackerForNilPanel;
};

//~ Globals
#define DefaultHeightPx 20

#endif //MUZE_APP_H
