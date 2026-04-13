/* date = January 27th 2026 7:06 pm */

#ifndef MUZE_APP_H
#define MUZE_APP_H

//~ Types
typedef struct ui_box ui_box;

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

typedef struct note note;
struct note
{
    u8 Pitch;
    u8 Velocity;
    f32 Timestamp;
    f32 Duration;
};

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

typedef struct app_state app_state;
struct app_state
{
    // TODO(luca): This is already in the FontAtlas, so it should go away?
    font Font;
    font_atlas FontAtlas;
    arena *FontAtlasArena; 
    f32 PreviousHeightPx;
    f32 HeightPx;
    
    // TODO(luca): Move to UI state ?
    arena *UIArena;
    
    struct
    {
        platform_midi_device In;
        platform_midi_device Out;
        
        s64 NotesCount;
        note *Notes;
        note_node *NoteSel;
        arena *NoteNodesArena;
        note_node *FreeNode;
        
        f32 RecordStart;
        f32 RecordEnd;
        
        u8 MaxPitch;
        u8 MinPitch;
        
        b32 IsRecording;
        b32 IsPlaying;
        
        f32 PlayPos;
        
        s32 TimeSig;
        f32 BPM;
    };
    
    arena *TextArena;
    
    panel *SelectedPanel;
    panel *FirstPanel;
    panel_node *FreePanel;
    arena *PanelArena;
    panel *DebugPanel;
    
    u64 FrameIdx;
    
    gl_render_state Render;
    
    // Nils
    ui_box *TrackerForUI_NilBox;
    panel *TrackerForNilPanel;
};

//~ Globals
#define DefaultHeightPx 20

#endif //MUZE_APP_H
