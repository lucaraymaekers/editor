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
            
            f32 DR = (f32)((*Pixel >> 16) & 0xFF);
            f32 DG = (f32)((*Pixel >> 8) & 0xFF);
            f32 DB = (f32)((*Pixel >> 0) & 0xFF);
            
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


internal void
DrawText(arena *Arena, app_offscreen_buffer *Buffer, app_font *Font, 
         f32 HeightPixels,
         v2 Offset, b32 IsUTF8, u32 Color, str8 Text)
{
    if(Font->Initialized)
    {        
        Offset.X = roundf(Offset.X);
        Offset.Y = roundf(Offset.Y);
        
        f32 FontScale = stbtt_ScaleForPixelHeight(&Font->Info, HeightPixels);
        
        for EachIndex(Idx, Text.Size)
        {
            rune CharAt = (IsUTF8 ? ((rune *)Text.Data)[Idx] : Text.Data[Idx]);
            
            s32 FontWidth, FontHeight;
            s32 AdvanceWidth, LeftSideBearing;
            s32 X0, Y0, X1, Y1;
            u8 *FontBitmap = 0;
            stbtt_GetCodepointBitmapBox(&Font->Info, CharAt, 
                                        FontScale, FontScale, 
                                        &X0, &Y0, &X1, &Y1);
            FontWidth = (X1 - X0);
            FontHeight = (Y1 - Y0);
            FontBitmap = PushArray(Arena, u8, (FontWidth*FontHeight));
            stbtt_MakeCodepointBitmap(&Font->Info, FontBitmap, 
                                      FontWidth, FontHeight, FontWidth, 
                                      FontScale, FontScale, CharAt);
            
            stbtt_GetCodepointHMetrics(&Font->Info, CharAt, &AdvanceWidth, &LeftSideBearing);
            
            s32 XOffset = (s32)floorf(Offset.X + (f32)LeftSideBearing*FontScale);
            s32 YOffset = (s32)Offset.Y + Y0;
            
            DrawCharacter(Buffer, FontBitmap, FontWidth, FontHeight, XOffset, YOffset, Color);
            
            Offset.X += roundf((f32)AdvanceWidth*FontScale);
        }
    }
    else
    {
        DebugBreak();
    }
    
}

// 1. First pass where we check each character's size.
// 2. Save positions where we need to wrap.
// Wrapping algorithm
// 1. When a character exceeds the box maximum width search backwards for whitespace.
//    I. Whitespace found?
//       Y -> Length of string until whitespace would fit?
//            Y -> Save whitespace's position.  This becomes the new searching start position.
//            N -> Break on the character that exceeds the maximum width.
//       N -> Break on the character that exceeds the maximum width.
//    II. Continue until end of string.

internal void
DrawTextInBox(arena *Arena, app_offscreen_buffer *Buffer, app_font *Font, 
              str8 Text, f32 HeightPx, u32 Color,
              v2 BoxMin, v2 BoxMax, b32 Centered)
{
    s32 *CharacterPixelWidths = PushArray(Arena, s32, Text.Size);
    u32 *WrapPositions = PushArray(Arena, u32, 0);
    u32 WrapPositionsCount = 0;
    
    f32 FontScale = stbtt_ScaleForPixelHeight(&Font->Info, HeightPx);
    
    // TODO(luca): UTF8 support
    // e.g. (https://en.wikipedia.org/wiki/Whitespace_character) these are all whitespace characters that we might want to support.
    for EachIndex(TextIndex, Text.Size)
    {
        u8 CharAt = Text.Data[TextIndex];
        
        s32 AdvanceWidth, LeftSideBearing;
        stbtt_GetCodepointHMetrics(&Font->Info, CharAt, &AdvanceWidth, &LeftSideBearing);
        
        CharacterPixelWidths[TextIndex] = (s32)roundf(FontScale*(f32)AdvanceWidth);
    }
    
    s32 MaxWidth = (s32)(BoxMax.X - BoxMin.X);
    Assert(MaxWidth >= 0);
    
    u32 SearchStart = 0;
    while(SearchStart < Text.Size)
    {
        s32 CumulatedWidth = 0;
        u32 SearchIndex = SearchStart;
        for(;
            ((SearchIndex < Text.Size) &&
             (CumulatedWidth <= MaxWidth));
            SearchIndex++)
        {
            s32 Width = CharacterPixelWidths[SearchIndex];
            CumulatedWidth += Width;
        }
        
        if(CumulatedWidth > MaxWidth)
        {
            // We need to search backwards for wrapping.
            SearchIndex--;
            u32 SearchIndexStop = SearchIndex;
            
            while(SearchIndex > SearchStart)
            {
                if(Text.Data[SearchIndex] == ' ')
                {
                    PushStruct(Arena, u32);
                    WrapPositions[WrapPositionsCount++] = SearchIndex;
                    break;
                }
                
                SearchIndex--;
            }
            
            if(SearchIndex > SearchStart)
            {
                Assert(SearchIndex > SearchStart);
                // luca: We skip the character we wrapped on.
                SearchStart = SearchIndex + 1;
            }
            else if(SearchIndex == SearchStart)
            {
                Assert(SearchIndexStop > SearchStart);
                PushStruct(Arena, u32);
                WrapPositions[WrapPositionsCount++] = SearchIndexStop;
                SearchStart = SearchIndexStop;
            }
            else
            {
                Assert(0);
            }
            
        }
        else
        {
            // luca: We don't need to wrap, we've reached the end of the text.
            break;
        }
    }
    
    f32 YAdvance = GetBaseline(Font, FontScale);
    v2 TextOffset = BoxMin;
    TextOffset.Y += YAdvance;
    
    if(Centered)
    {
        s32 TextHeight = (s32)(YAdvance * (f32)(WrapPositionsCount + 1));
        s32 CenterHOffset = ((s32)(BoxMax.Y - BoxMin.Y) - TextHeight)/2;
        if(CenterHOffset >= 0)
        {
            TextOffset.Y += (f32)CenterHOffset;
        }
    }
    
    u32 Start = 0;
    for EachIndex(WrapIndex, WrapPositionsCount)
    {
        u32 Position = WrapPositions[WrapIndex];
        
        if(TextOffset.Y - FontScale*((f32)Font->Descent) < BoxMax.Y)
        {
            
            b32 DoCenter = (Centered && 
                            ((WrapIndex == 0) ||
                             (Text.Data[(WrapPositions[WrapIndex - 1])] == ' ')));
            if(DoCenter)
            {
                s32 TextWidth = 0;
                for(u32 WidthIndex = Start;
                    WidthIndex < Position;
                    WidthIndex++)
                {
                    TextWidth += CharacterPixelWidths[WidthIndex];
                }
                TextOffset.X = BoxMin.X + (f32)((MaxWidth - TextWidth)/2);
            }
            
            DrawText(Arena, Buffer, Font, HeightPx, 
                     TextOffset, false, Color, S8FromTo(Text, Start, Position));
        }
        
        TextOffset.Y += YAdvance;
        
        if(Text.Data[Position] == ' ')
        {
            Position++;
        }
        
        Start = Position;
    }
    
    TextOffset.X = BoxMin.X;
    b32 DoCenter = (Centered &&
                    ((WrapPositionsCount == 0) || (Text.Data[WrapPositions[WrapPositionsCount - 1]] == ' ')));
    if(DoCenter)
    {                
        s32 TextWidth = 0;
        for EachIndex(Idx, Text.Size)
        {
            TextWidth += CharacterPixelWidths[Idx];
        }
        TextOffset.X = BoxMin.X + (f32)((MaxWidth - TextWidth)/2);
    }
    
    if(TextOffset.Y - FontScale*((f32)Font->Descent) < BoxMax.Y)
    {
        DrawText(Arena, Buffer, Font, HeightPx, TextOffset, false, Color, S8From(Text, Start)); 
    }
}

#define DrawTextFormat(Arena, Buffer, Font, HeightPx, Offset, IsUTF8, Color, Format, ...) \
DrawText(Arena, Buffer, Font, HeightPx, Offset, IsUTF8, Color, FormatText(Arena, Format, ##__VA_ARGS__));

#endif //EDITOR_FONT_H