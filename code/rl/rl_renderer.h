/* date = April 9th 2026 7:52 pm */

#pragma once
//~ Types
typedef struct font_atlas font_atlas;
struct font_atlas
{
 s32 Width;
 s32 Height;
 u8 *Data;
 
 f32 HeightPx;
 f32 FontScale;
 font *Font;
 
 rune FirstCodepoint;
 s32 CodepointsCount;
 
 rune IconsFirstCodepoint;
 s32 IconsCodepointsCount;
 
 f32 PixelScaleWidth;
 f32 PixelScaleHeight;
 
 stbtt_packedchar *PackedChars;
 stbtt_aligned_quad *AlignedQuads;
};


typedef struct clip_stack clip_stack;
struct clip_stack
{
 v4 V;
 clip_stack *Prev;
};

//~ Globals
global_variable rect_instance *GlobalRectsInstances;
global_variable u64 GlobalRectsCount;
global_variable clip_stack *GlobalClipStackTop;

#define MaxRectsCount KB(64)

//~ API
internal void RenderBuildAtlas(arena *Arena, font_atlas *Atlas, font *TextFont, font *IconsFont, f32 HeightPx);
internal rect_instance *DrawRect(v4 Dest, v4 Color, 
                                 f32 CornerRadius, f32 BorderThickness, f32 Softness);
internal rect_instance *DrawRectChar(font_atlas *Atlas, v2 Pos, rune Codepoint, v4 Color);
internal void RenderInit(gl_render_state *Render);
internal void RenderCleanup(gl_render_state *Render);
internal void RenderBeginFrame(arena *Arena, s32 X, s32 Y, s32 Width, s32 Height);
internal void RenderClear(void);
internal void RenderDrawAllRectangles(gl_render_state *Render, v2 BufferDim, font_atlas *Atlas);
internal void RenderPushClip(v4 Value);
internal void RenderPopClip(void);
#define RenderClip(Value) DeferLoop(RenderPushClip(Value), RenderPopClip())