/* date = January 27th 2026 7:06 pm */

#ifndef EDITOR_APP_H
#define EDITOR_APP_H

#define DefaultHeightPx 24

NO_STRUCT_PADDING_BEGIN
typedef struct rect_instance rect_instance;
struct rect_instance
{
    rect Dest;
    rect TexSrc;
    
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

struct ui_box;

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
    
    rect Region;
};
raddbg_type_view(panel, ParentPct);


typedef struct app_state app_state;
struct app_state
{
    // TODO(luca): This is already in the FontAtlas, so it should go away?
    app_font Font;
    font_atlas FontAtlas;
    arena *FontAtlasArena; 
    f32 PreviousHeightPx;
    f32 HeightPx;
    
    s32 CursorPos;
    u64 TextCount;
    rune Text[KB(1)];
    
    // TODO(luca): Move this over to UI state
    arena *UIBoxArena;
    u64 UIBoxTableSize;
    ui_box *UIBoxTable;
    
    panel *TitlebarPanel;
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

// X macros

#define U32ToV3Arg(Hex) \
((f32)((Hex >> 8*2) & 0xFF)/255.0f), \
((f32)((Hex >> 8*1) & 0xFF)/255.0f), \
((f32)((Hex >> 8*0) & 0xFF)/255.0f)

#define ColorList \
SET(Frost0,           0xff8fbcbb) \
SET(Frost1,           0xff88c0d0) \
SET(Frost2,           0xff81a1c1) \
SET(Frost3,           0xff5e81ac) \
SET(Snow0,            0xffeceff4) \
SET(Snow1,            0xffe5e9f0) \
SET(Snow2,            0xffd8dee9) \
SET(Night0,           0xff4c566a) \
SET(Night1,           0xff434c5e) \
SET(Night2,           0xff3b4252) \
SET(Night3,           0xff2e3440) \
SET(Red,              0xffbf616a) \
SET(Orange,           0xffd08770) \
SET(Yellow,           0xffebcb8b) \
SET(Green,            0xffa3be8c) \
SET(Magenta,          0xffb48ead) \
SET(Cyan,             0xff81a1c1) \
SET(Blue,             0xff5e81ac) \
SET(Black,            0xff000000) \

#define SET(Name, Value) u32 ColorU32_##Name = Value;
ColorList
#undef SET

#define SET(Name, Value) v4 Color_##Name = {U32ToV3Arg(Value), 1.f};
ColorList
#undef SET

global_variable v4 Color_Background = Color_Night3;
global_variable v4 Color_Foreground = Color_Snow0;
global_variable v4 Color_ButtonBorder = Color_Night2;
global_variable v4 Color_ButtonBackground = Color_Night0;
global_variable v4 Color_ButtonText = Color_Black;

#endif //EDITOR_APP_H
