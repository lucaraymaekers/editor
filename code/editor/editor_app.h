/* date = January 27th 2026 7:06 pm */

#ifndef EDITOR_APP_H
#define EDITOR_APP_H

//~ Types

typedef struct ui_box ui_box;

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
};
raddbg_type_view(panel, 
                 no_addr(rows($,
                              (&First == NilPanel || &First == 0),
                              ParentPct, 
                              (Axis == Axis2_X ? "X" : "Y"))));
#define EachChildPanel(Child, Parent) (panel *Child = Parent->First; !IsNilPanel(Child); Child = Child->Next)

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

typedef struct panel_node panel_node;
struct panel_node
{
    panel *Value;
    app_text *Text;
    
    // TODO(luca): Double Linked List
    panel_node *Next;
};
#define EachPanel(Index, First) EachNode(Index, panel_node, First)

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
    
    panel *TitlebarPanel;
    
    panel_node *TextPanels;
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
#define DefaultHeightPx 24

#endif //EDITOR_APP_H
