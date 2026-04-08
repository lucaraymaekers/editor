
#version 330 core

#define v2 vec2
#define v3 vec3
#define v4 vec4 
#define f32 float
#define b32 int

uniform v2 Viewport;

out v2  VS_Pos;
out v4  VS_Dest;
out v2  VS_TexCoord;
out v2  VS_Center;
out v2  VS_HalfSize;
out v4  VS_Color;
out f32 VS_CornerRadius;
out f32 VS_BorderThickness;
out f32 VS_Softness;
flat out f32 VS_HasTexture;

//- Generated code start
layout (location =  0) in  v4 I_Dest;
layout (location =  1) in  v4 I_TexSrc;
layout (location =  2) in  v4 I_Color0;
layout (location =  3) in  v4 I_Color1;
layout (location =  4) in  v4 I_Color2;
layout (location =  5) in  v4 I_Color3;
layout (location =  6) in  v4 I_CornerRadii;
layout (location =  7) in f32 I_BorderThickness;
layout (location =  8) in f32 I_Softness;
layout (location =  9) in f32 I_HasTexture;

//- Generated code end

void main()
{
    v4 Colors[] = v4[](I_Color0, I_Color1, I_Color2, I_Color3);
    v2 Vertices[] = v2[](v2(-1.0, +1.0), v2(+1.0, +1.0), v2(-1.0, -1.0), v2(+1.0, -1.0));
    
    v2 PixelPos, HalfSize, Center, ClipPos;
    {    
        v2 Min = I_Dest.xy;
        v2 Max = I_Dest.zw;
        
        v2 PosInQuad = (Vertices[gl_VertexID] + 1.0) * 0.5;
        PixelPos = mix(Min, Max, PosInQuad);
        
        ClipPos = (PixelPos / Viewport) * 2.0 - 1.0;
        ClipPos.y = -ClipPos.y;
        
        HalfSize = (Max - Min)/2.0;
        Center = Min + HalfSize;
    }
    
    v2 Min = I_TexSrc.xy;
    v2 Max = I_TexSrc.zw;
    
    v2 BL = v2(Min.x, Max.y);
    v2 BR = v2(Max.x, Max.y);
    v2 TL = v2(Min.x, Min.y);
    v2 TR = v2(Max.x, Min.y);
    v2 TexCoords[] = v2[](BL, BR, TL, TR);
    
    v2 TexCoord = TexCoords[gl_VertexID];
    
    VS_Center = Center;
    VS_HalfSize = HalfSize;
    VS_Pos = PixelPos - Center;
    VS_Dest = I_Dest;
    VS_TexCoord = TexCoord;
    VS_Color = Colors[gl_VertexID];
    VS_CornerRadius = I_CornerRadii[gl_VertexID];
    VS_BorderThickness = I_BorderThickness;
    VS_Softness = I_Softness;
    VS_HasTexture = I_HasTexture;
    
    gl_Position = v4(ClipPos, 0.0, 1.0);
}
