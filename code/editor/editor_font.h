#if !defined(EDITOR_FONT_H)
#define EDITOR_FONT_H

#include <stdarg.h>
#include <stdio.h>
#include "lib/stb_truetype.h"
#include "lib/stb_sprintf.h"

// TODO(luca): Better UTF8 handling instead of passing a flag in parameters.

typedef struct app_font app_font;
struct app_font
{
    stbtt_fontinfo Info;
    s32 Ascent;
    s32 Descent;
    s32 LineGap;
    v2 BoundingBox[2];
    b32 Initialized; // For debugging.
};

//~ API

internal f32
GetBaseline(app_font *Font, f32 Scale)
{
    f32 Result = Scale*(f32)(Font->Ascent - Font->Descent + Font->LineGap);
    return Result;
}

internal void
InitFont(app_font *Font, char *FilePath)
{
    str8 File = OS_ReadEntireFileIntoMemory(FilePath);
    
    if(File.Size)
    {
        if(stbtt_InitFont(&Font->Info, File.Data, stbtt_GetFontOffsetForIndex(File.Data, 0)))
        {
            Font->Info.data = (u8 *)File.Data;
            
            s32 X0, Y0, X1, Y1;
            stbtt_GetFontBoundingBox(&Font->Info, &X0, &Y0, &X1, &Y1);
            Font->BoundingBox[0] = V2S32(X0, Y0);
            Font->BoundingBox[1] = V2S32(X1, Y1);
            stbtt_GetFontVMetrics(&Font->Info, &Font->Ascent, &Font->Descent, &Font->LineGap);
            Font->Initialized = true;
        }
        else
        {
            // TODO(luca): Logging
        }
    }
    else
    {
        // TODO(luca): Logging
    }
}

//- Rendering 
internal void
DrawCharacter(app_offscreen_buffer *Buffer,  u8 *FontBitmap,
              int FontWidth, int FontHeight, 
              int XOffset, int YOffset,
              u32 Color)
{
    s32 MinX = 0;
    s32 MinY = 0;
    s32 MaxX = FontWidth;
    s32 MaxY = FontHeight;
    
    if(XOffset < 0)
    {
        MinX = -XOffset;
        XOffset = 0;
    }
    if(YOffset < 0)
    {
        MinY = -YOffset;
        YOffset = 0;
    }
    if(XOffset + FontWidth > Buffer->Width)
    {
        MaxX -= ((XOffset + FontWidth) - Buffer->Width);
    }
    if(YOffset + FontHeight > Buffer->Height)
    {
        MaxY -= ((YOffset + FontHeight) - Buffer->Height);
    }
    
    u8 *Row = (u8 *)(Buffer->Pixels) + 
    (YOffset*Buffer->Pitch) +
    (XOffset*Buffer->BytesPerPixel);
    
    for(int  Y = MinY;
        Y < MaxY;
        Y++)
    {
        u32 *Pixel = (u32 *)Row;
        for(int X = MinX;
            X < MaxX;
            X++)
        {
            u8 Brightness = FontBitmap[Y*FontWidth+X];
            
            f32 Alpha = ((f32)Brightness/255.0f);
            
#if 0
            f32 R = Lerp(Color.R*255.0f, DR, Alpha);
            f32 G = Lerp(Color.G*255.0f, DG, Alpha);
            f32 B = Lerp(Color.B*255.0f, DB, Alpha);
            f32 A = 255.0f;
#else
            f32 SR = (f32)((Color >> 16) & 0xFF);
            f32 SG = (f32)((Color >> 8) & 0xFF);
            f32 SB = (f32)((Color >> 0) & 0xFF);
            
            f32 R = SR*Alpha;
            f32 G = SG*Alpha;
            f32 B = SB*Alpha;
            f32 A = Brightness;
#endif
            
            
            u32 Value = (((u32)(A) << 24) |
                         ((u32)(R) << 16) |
                         ((u32)(G) << 8) |
                         ((u32)(B) << 0));
            *Pixel = Value;
            
            Pixel += 1;
        }
        
        Row += Buffer->Pitch;
    }
}


#endif //EDITOR_FONT_H