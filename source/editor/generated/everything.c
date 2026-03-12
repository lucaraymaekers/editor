//- Colors begin
u32 ColorU32_Frost0 = 0xff8fbcbb;
u32 ColorU32_Frost1 = 0xff88c0d0;
u32 ColorU32_Frost2 = 0xff81a1c1;
u32 ColorU32_Frost3 = 0xff5e81ac;
u32 ColorU32_Snow0 = 0xffeceff4;
u32 ColorU32_Snow1 = 0xffe5e9f0;
u32 ColorU32_Snow2 = 0xffd8dee9;
u32 ColorU32_Night0 = 0xff4c566a;
u32 ColorU32_Night1 = 0xff434c5e;
u32 ColorU32_Night2 = 0xff3b4252;
u32 ColorU32_Night3 = 0xff2e3440;
u32 ColorU32_Red = 0xffbf616a;
u32 ColorU32_Orange = 0xffd08770;
u32 ColorU32_Yellow = 0xffebcb8b;
u32 ColorU32_Green = 0xffa3be8c;
u32 ColorU32_Magenta = 0xffb48ead;
u32 ColorU32_Cyan = 0xff81a1c1;
u32 ColorU32_Blue = 0xff5e81ac;
u32 ColorU32_Black = 0xff000000;
v4 Color_Frost0 = V4(U32ToV4Arg(ColorU32_Frost0));
v4 Color_Frost1 = V4(U32ToV4Arg(ColorU32_Frost1));
v4 Color_Frost2 = V4(U32ToV4Arg(ColorU32_Frost2));
v4 Color_Frost3 = V4(U32ToV4Arg(ColorU32_Frost3));
v4 Color_Snow0 = V4(U32ToV4Arg(ColorU32_Snow0));
v4 Color_Snow1 = V4(U32ToV4Arg(ColorU32_Snow1));
v4 Color_Snow2 = V4(U32ToV4Arg(ColorU32_Snow2));
v4 Color_Night0 = V4(U32ToV4Arg(ColorU32_Night0));
v4 Color_Night1 = V4(U32ToV4Arg(ColorU32_Night1));
v4 Color_Night2 = V4(U32ToV4Arg(ColorU32_Night2));
v4 Color_Night3 = V4(U32ToV4Arg(ColorU32_Night3));
v4 Color_Red = V4(U32ToV4Arg(ColorU32_Red));
v4 Color_Orange = V4(U32ToV4Arg(ColorU32_Orange));
v4 Color_Yellow = V4(U32ToV4Arg(ColorU32_Yellow));
v4 Color_Green = V4(U32ToV4Arg(ColorU32_Green));
v4 Color_Magenta = V4(U32ToV4Arg(ColorU32_Magenta));
v4 Color_Cyan = V4(U32ToV4Arg(ColorU32_Cyan));
v4 Color_Blue = V4(U32ToV4Arg(ColorU32_Blue));
v4 Color_Black = V4(U32ToV4Arg(ColorU32_Black));
//- Colors end
s32 RectVSAttribOffsets[] =
{
4,
4,
4,
4,
4,
4,
4,
1,
1,
1,
};
enum ui_box_flag
{
UI_BoxFlag_None                   = (0 << 0),
UI_BoxFlag_DrawBackground         = (1 << 1),
UI_BoxFlag_DrawBorders            = (1 << 2),
UI_BoxFlag_DrawDebugBorder        = (1 << 3),
UI_BoxFlag_DrawShadow             = (1 << 4),
UI_BoxFlag_DrawDisplayString      = (1 << 5),
UI_BoxFlag_CenterTextHorizontally = (1 << 6),
UI_BoxFlag_CenterTextVertically   = (1 << 7),
UI_BoxFlag_MouseClickability      = (1 << 8),
UI_BoxFlag_TextWrap               = (1 << 9),
UI_BoxFlag_DrawTextCursor         = (1 << 10),
UI_BoxFlag_Clip                   = (1 << 11),
};
typedef enum ui_box_flag ui_box_flag;
