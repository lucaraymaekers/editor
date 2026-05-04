// TODO(luca): Intrinsics.
#include <math.h>
//~ Functions

internal void
RenderBuildAtlas(arena *Arena, 
                 font_atlas *Atlas, 
                 font *TextFont, 
                 font *IconsFont,
                 f32 HeightPx)
{ 
    
    if(TextFont->Initialized)
    {
        Atlas->FirstCodepoint = ' ';
        // NOTE(luca): 2 bytes =>small alphabets.
        Atlas->CodepointsCount = ((2048 - 1) - Atlas->FirstCodepoint);
        Atlas->IconsFirstCodepoint = 'a';
        Atlas->IconsCodepointsCount = 2;
        
        Atlas->Width = 1024;
        Atlas->Height = 1024;
        u64 Size = (u64)(Atlas->Width*Atlas->Height);
        
        Arena->Pos = 0;
        Atlas->Data = PushArray(Arena, u8, Size);
        s32 CodepointsCount = (Atlas->CodepointsCount + Atlas->IconsCodepointsCount);
        Atlas->PackedChars = PushArray(Arena, stbtt_packedchar, (u64)CodepointsCount);
        Atlas->AlignedQuads = PushArray(Arena, stbtt_aligned_quad, (u64)CodepointsCount);
        
        Atlas->HeightPx = HeightPx;
        Atlas->FontScale = stbtt_ScaleForPixelHeight(&TextFont->Info, HeightPx);
        Atlas->Font = TextFont;
        
        stbtt_pack_context Ctx;
        {
            stbtt_PackBegin(&Ctx, 
                            Atlas->Data,
                            Atlas->Width, Atlas->Height,
                            0, 1, 0);
            
            stbtt_PackFontRange(&Ctx, TextFont->Info.data, 0, HeightPx, 
                                Atlas->FirstCodepoint, Atlas->CodepointsCount, 
                                Atlas->PackedChars);
            
            // Icons
            {
                // NOTE(luca): We need to scale this so that it matches our font.
                f32 TextEmHeight = (f32)(TextFont->Ascent - TextFont->Descent);
                f32 IconsEmHeight = (f32)(IconsFont->Ascent - IconsFont->Descent);
                f32 ScaledHeight = HeightPx*(IconsEmHeight/TextEmHeight);
                
                stbtt_PackFontRange(&Ctx, IconsFont->Info.data, 0, ScaledHeight, 
                                    Atlas->IconsFirstCodepoint, Atlas->IconsCodepointsCount, 
                                    Atlas->PackedChars + Atlas->CodepointsCount);
            }
            
            stbtt_PackEnd(&Ctx);
        }
        
        for EachIndex(Idx, CodepointsCount)
        {
            float UnusedX, UnusedY;
            stbtt_GetPackedQuad(Atlas->PackedChars, Atlas->Width, Atlas->Height, 
                                Idx,
                                &UnusedX, &UnusedY,
                                &Atlas->AlignedQuads[Idx], 0);
        }
    }
}

internal rect_instance *
DrawRect(v4 Dest, v4 Color, f32 CornerRadius, f32 BorderThickness, f32 Softness)
{
    rect_instance *Result = GlobalRectsInstances + GlobalRectsCount;
    
    b32 RecIsValid = (Dest.Min.X < Dest.Max.X &&
       Dest.Min.Y < Dest.Max.Y);
    if(RecIsValid)
{    
    GlobalRectsCount += 1;
    
    MemoryZero(Result);
    
    Result->Dest = Dest;
    Result->Color0 = Color;
    Result->Color1 = Color;
    Result->Color2 = Color;
    Result->Color3 = Color;
    Result->CornerRadii = V4F32(CornerRadius);
    Result->Border = BorderThickness;
    Result->Softness = Softness;
    }
    else
    {
        NoOp();
    }

    return Result;
}

internal rect_instance *
DrawRectChar(font_atlas *Atlas, v2 Pos, rune Codepoint, v4 Color)
{
    rect_instance *Result = 0;
    v4 Dest = {0};
    
    // NOTE(luca): Evereything happens in pixel coordinates in here.
    
    b32 Supported = (Codepoint >= Atlas->FirstCodepoint && 
                     Codepoint < Atlas->CodepointsCount - Atlas->FirstCodepoint);
    // TODO(luca): Proper glyph cache
    if(Supported)
    {        
        rune CharIdx = Codepoint - Atlas->FirstCodepoint;
        stbtt_packedchar *PackedChar = &Atlas->PackedChars[CharIdx];
        stbtt_aligned_quad *Quad = &Atlas->AlignedQuads[CharIdx];
        // TODO(luca): Investigate why floorf needed.
        f32 Width = floorf(PackedChar->x1 - PackedChar->x0);
        f32 Height = floorf(PackedChar->y1 - PackedChar->y0);
        {    
            f32 Baseline = GetBaseline(Atlas->Font, Atlas->FontScale);
            f32 Descent = (f32)Atlas->Font->Descent*Atlas->FontScale;
            
            v2 Min = V2(floorf(Pos.X + PackedChar->xoff), 
                        floorf(Pos.Y + PackedChar->yoff + Baseline + Descent));
            v2 Max = V2(Min.X + Width, Min.Y + Height);
            
                Dest = Rect(Min.X, Min.Y, Max.X, Max.Y);
            
            Result = DrawRect(Dest, Color, 0.f, 0.f, 0.f);
            Result->TexSrc = Rect(Quad->s0, Quad->t0, Quad->s1, Quad->t1);
        }
        
        Result->HasTexture = true;
        
#if 0
        DrawRect(Result->Dest, V4(0.f, 0.f, 1.f, 1.f), 0.f, 1.f, 0.f);
#endif
    }
    
    return Result;
}

internal void
RenderInit(gl_render_state *Render)
{
    glGenVertexArrays(ArrayCount(Render->VAOs), &Render->VAOs[0]);
    glGenTextures(ArrayCount(Render->Textures), &Render->Textures[0]);
    glGenBuffers(ArrayCount(Render->VBOs), &Render->VBOs[0]);
    glBindVertexArray(Render->VAOs[0]);
    glBindBuffer(GL_ARRAY_BUFFER, Render->VBOs[0]);
}

internal void
RenderCleanup(gl_render_state *Render)
{
    glDeleteVertexArrays(ArrayCount(Render->VAOs), &Render->VAOs[0]);
    glDeleteTextures(ArrayCount(Render->Textures), &Render->Textures[0]);
    glDeleteBuffers(ArrayCount(Render->VBOs), &Render->VBOs[0]);
    glDeleteProgram(Render->RectShader);
}

internal void
RenderBeginFrame(arena *Arena, s32 Width, s32 Height)
{
    glViewport(0, 0, Width, Height);
    
    GlobalRectsCount = 0;
    GlobalRectsInstances = PushArray(Arena, rect_instance, MaxRectsCount);
    GL_FrameArena = Arena;
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
}

internal void 
RenderClear(void)
{
    glClearColor(U32ToV4Arg(0xFF2E3440));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

internal void
RenderDrawAllRectangles(gl_render_state *Render, v2 BufferDim, font_atlas *Atlas)
{
    
    if(!Render->ShadersCompiled)
    {
        gl_uint RectShader = Render->RectShader;
        glDeleteProgram(RectShader);
        
        RectShader = GL_ProgramFromShaders(S8("../code/rl/generated/rect_vert.glsl"),
                                           S8("../code/rl/generated/rect_frag.glsl"));
        glUseProgram(RectShader);
        
        GL_LoadTextureFromImage(Render->Textures[0], Atlas->Width, Atlas->Height, Atlas->Data, GL_RED, RectShader, "Texture");
        // Uniforms
        {
            gl_int UViewport = glGetUniformLocation(RectShader, "Viewport");
            glUniform2f(UViewport, V2Arg(BufferDim));
        }
        
        Render->RectShader = RectShader;
        Render->ShadersCompiled = true;
    }
    
#if RL_PLATFORM_HOT_RELOAD_SHADERS
    Render->ShadersCompiled = false;
#endif
    
    Assert(GlobalRectsCount < MaxRectsCount);
    u64 Size = GlobalRectsCount*sizeof(rect_instance);
    glBufferData(GL_ARRAY_BUFFER, (s32)Size, GlobalRectsInstances, GL_STATIC_DRAW);
    
    u64 Offset = 0;
    
    s32 TotalSize = 0;
    for EachElement(Idx, RectVSAttribOffsets)
    {            
        u64 Count = (u64)RectVSAttribOffsets[Idx];
        
        glEnableVertexAttribArray((gl_uint)Idx);
        glVertexAttribDivisor((gl_uint)Idx, 1);
        glVertexAttribPointer((gl_uint)Idx, (gl_int)Count, GL_FLOAT, false, sizeof(rect_instance), (void *)((Offset)*sizeof(f32)));
        Offset += Count;
        
        TotalSize += RectVSAttribOffsets[Idx];
    }
    Assert(TotalSize == (sizeof(rect_instance)/sizeof(f32)));
    
    glEnable(GL_MULTISAMPLE);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (gl_int)GlobalRectsCount);
}
