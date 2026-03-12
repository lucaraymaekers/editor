
#version 330 core

#define Pi32 3.1415926535897
#define v2 vec2
#define v3 vec3
#define v4 vec4 
#define f32 float
#define b32 int

in v2  VS_Pos;
in v4  VS_Dest;
in v2  VS_TexCoord;
in v2  VS_Center;
in v2  VS_HalfSize;
in v4  VS_Color;

in f32 VS_CornerRadius;
in f32 VS_BorderThickness;
in f32 VS_Softness;
flat in f32 VS_HasTexture;

uniform sampler2D Texture;

out v4 FragColor;

v3
HSV2RGB(v3 Color)
{
    v4 K = v4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    v3 p = abs(fract(Color.xxx + K.xyz) * 6.0 - K.www);
    return Color.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), Color.y);
}

f32
BoxSDF(v2 Pos, v2 HalfDim, f32 CornerRadius)
{
    v2 q = abs(Pos) - (HalfDim - CornerRadius);
    
    f32 Distance = min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - CornerRadius;
    
    return Distance;
}

void main()
{
    v4 Color = VS_Color;
    
    f32 tBorder = 1.0;
    if(VS_BorderThickness > 0.0)
    {
        f32 sBorder = BoxSDF(VS_Pos,
                             VS_HalfSize - v2(VS_Softness*2.0, VS_Softness*2.0) - VS_BorderThickness,
                             max(VS_CornerRadius-VS_BorderThickness, 0.0));
        tBorder = smoothstep(0.0, 2.0*VS_Softness, sBorder);
    }
    if(tBorder < 0.001f)
    {
        discard;
    }
    
    f32 tCorner = 1.0;
    if(VS_CornerRadius > 0.0 || VS_Softness > 0.75f)
    {
        f32 sCorner = BoxSDF(VS_Pos,
                             VS_HalfSize - v2(VS_Softness*2.0, VS_Softness*2.0),
                             VS_CornerRadius);
        tCorner = 1.0-smoothstep(0.0, 2.0*VS_Softness, sCorner);
    }
    
    if(VS_HasTexture > 0)
    {
        f32 Alpha = texture(Texture, VS_TexCoord).r;
        Color.a *= Alpha;;
    }
    
    Color.a *= tBorder;
    Color.a *= tCorner;
    
    FragColor = Color;
}
