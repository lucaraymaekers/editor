NO_STRUCT_PADDING_BEGIN
typedef struct rect_instance rect_instance;
struct rect_instance
{
v4 Dest;
 v4 TexSrc;
 v4 Clip;
 v4 Color0;
 v4 Color1;
 v4 Color2;
 v4 Color3;
 v4 CornerRadii;
 f32 BorderThickness;
 f32 Softness;
 f32 HasTexture;
 
};
NO_STRUCT_PADDING_END

s32 RectVSAttributeOffsets[] = {
4,4,4,4,4,4,4,4,1,1,1,
};

