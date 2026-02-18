/* date = January 27th 2026 7:06 pm */

#ifndef EDITOR_APP_H
#define EDITOR_APP_H

#define DefaultHeightPx 24

NO_STRUCT_PADDING_BEGIN
typedef struct rect_instance rect_instance;
struct rect_instance
{
    rect Dest;
    v4 Color0;
    v4 Color1;
    v4 Color2;
    v4 Color3;
    v4 CornerRadii;
    f32 Border;
    f32 Softness;
};
NO_STRUCT_PADDING_END

typedef struct font_atlas font_atlas;
struct font_atlas
{
    u32 Width;
    u32 Height;
    u8 *Data;
    
    rune FirstCodepoint;
    u32 CodepointsCount;
    
    f32 PixelScaleWidth;
    f32 PixelScaleHeight;
    
    stbtt_packedchar *PackedChars;
    stbtt_aligned_quad *AlignedQuads;
};

typedef struct app_state app_state;
struct app_state
{
    app_font Font;
    font_atlas FontAtlas;
    arena *FontAtlasArena; 
    f32 PreviousHeightPx;
    f32 HeightPx;
    
    u64 TextCount;
    rune Text[1024];
    
    s32 CursorPos;
};

//~ Globals
global_variable rect_instance *GlobalRectQuadData;
global_variable s32 GlobalRectsCount;

#endif //EDITOR_APP_H
