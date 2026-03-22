/* date = January 27th 2026 7:06 pm */

#ifndef EDITOR_APP_H
#define EDITOR_APP_H

#define DefaultHeightPx 24

NO_STRUCT_PADDING_BEGIN
typedef struct rect_instance rect_instance;
struct rect_instance
{
    // TODO(luca): Make one type for everything and let it be rect (renamed to v4)
    v4 Dest;
    v4 TexSrc;
    
    // TODO(luca): Metaprogram
    v4 Color0;
    v4 Color1;
    v4 Color2;
    v4 Color3;
    v4 CornerRadii;
    
    f32 Border;
    f32 Softness;
    
    f32 HasTexture;
};
NO_STRUCT_PADDING_END

typedef struct font_atlas font_atlas;
struct font_atlas
{
    u32 Width;
    u32 Height;
    u8 *Data;
    
    f32 HeightPx;
    f32 FontScale;
    app_font *Font;
    
    rune FirstCodepoint;
    u32 CodepointsCount;
    
    f32 PixelScaleWidth;
    f32 PixelScaleHeight;
    
    stbtt_packedchar *PackedChars;
    stbtt_aligned_quad *AlignedQuads;
};

typedef struct gl_render_state gl_render_state;
struct gl_render_state
{
    gl_handle VAOs[1];
    gl_handle VBOs[1];
    gl_handle Textures[1];
    gl_handle RectShader;
    b32 ShadersCompiled;
};

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


#define EachPanel(Child, Parent) (panel *Child = Parent->First; !IsNilPanel(Child); Child = Child->Next)

typedef struct app_text app_text; 
struct app_text
{
    u64 Cursor;
    u64 Count;
    u64 Capacity;
    u64 Trail;
    u64 LineScrollOffset;
    f32 CursorAnimTime;
    rune *Data;
};

typedef struct app_state app_state;
struct app_state
{
    // TODO(luca): This is already in the FontAtlas, so it should go away?
    app_font Font;
    font_atlas FontAtlas;
    arena *FontAtlasArena; 
    f32 PreviousHeightPx;
    f32 HeightPx;
    
    app_text Text;
    
    // TODO(luca): Move this over to UI state
    arena *UIBoxArena;
    u64 UIBoxTableSize;
    ui_box *UIBoxTable;
    
    panel *TitlebarPanel;
    panel *DebugPanel;
    
    panel *SelectedPanel;
    panel *FirstPanel;
    panel *LastPanel;
    panel *FreePanel;
    arena *PanelArena;
    
    u64 FrameIndex;
    
    gl_render_state Render;
    
    // Nils
    ui_box *TrackerForUI_NilBox;
    panel *TrackerForNilPanel;
};

//~ Globals
global_variable rect_instance *GlobalRectsInstances;
global_variable s32 GlobalRectsCount;

global_variable arena *FrameArena = 0;

global_variable v4 Color_Background = {U32ToV4Arg(0xff2e3440)};
global_variable v4 Color_Foreground = {U32ToV4Arg(0xffeceff4)};
global_variable v4 Color_ButtonBorder = {U32ToV4Arg(0xff3b4252)};
global_variable v4 Color_ButtonBackground = {U32ToV4Arg(0xff4c566a)};
global_variable v4 Color_ButtonText = {U32ToV4Arg(0xff000000)};

#endif //EDITOR_APP_H
