//- Colors begin
const u32 ColorU32_Frost0 = 0xff8fbcbb;
const u32 ColorU32_Frost1 = 0xff88c0d0;
const u32 ColorU32_Frost2 = 0xff81a1c1;
const u32 ColorU32_Frost3 = 0xff5e81ac;
const u32 ColorU32_Snow0 = 0xffeceff4;
const u32 ColorU32_Snow1 = 0xffe5e9f0;
const u32 ColorU32_Snow2 = 0xffd8dee9;
const u32 ColorU32_Night0 = 0xff4c566a;
const u32 ColorU32_Night1 = 0xff434c5e;
const u32 ColorU32_Night2 = 0xff3b4252;
const u32 ColorU32_Night3 = 0xff2e3440;
const u32 ColorU32_Red = 0xffbf616a;
const u32 ColorU32_Orange = 0xffd08770;
const u32 ColorU32_Yellow = 0xffebcb8b;
const u32 ColorU32_Green = 0xffa3be8c;
const u32 ColorU32_Magenta = 0xffb48ead;
const u32 ColorU32_Cyan = 0xff81a1c1;
const u32 ColorU32_Blue = 0xff5e81ac;
const u32 ColorU32_Black = 0xff000000;
v4 Color_Frost0 = {U32ToV4Arg(0xff8fbcbb)};
v4 Color_Frost1 = {U32ToV4Arg(0xff88c0d0)};
v4 Color_Frost2 = {U32ToV4Arg(0xff81a1c1)};
v4 Color_Frost3 = {U32ToV4Arg(0xff5e81ac)};
v4 Color_Snow0 = {U32ToV4Arg(0xffeceff4)};
v4 Color_Snow1 = {U32ToV4Arg(0xffe5e9f0)};
v4 Color_Snow2 = {U32ToV4Arg(0xffd8dee9)};
v4 Color_Night0 = {U32ToV4Arg(0xff4c566a)};
v4 Color_Night1 = {U32ToV4Arg(0xff434c5e)};
v4 Color_Night2 = {U32ToV4Arg(0xff3b4252)};
v4 Color_Night3 = {U32ToV4Arg(0xff2e3440)};
v4 Color_Red = {U32ToV4Arg(0xffbf616a)};
v4 Color_Orange = {U32ToV4Arg(0xffd08770)};
v4 Color_Yellow = {U32ToV4Arg(0xffebcb8b)};
v4 Color_Green = {U32ToV4Arg(0xffa3be8c)};
v4 Color_Magenta = {U32ToV4Arg(0xffb48ead)};
v4 Color_Cyan = {U32ToV4Arg(0xff81a1c1)};
v4 Color_Blue = {U32ToV4Arg(0xff5e81ac)};
v4 Color_Black = {U32ToV4Arg(0xff000000)};
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
UI_BoxFlag_FloatingX              = (1 << 9),
UI_BoxFlag_FloatingY              = (1 << 10),
UI_BoxFlag_Clip                   = (1 << 11),
};
typedef enum ui_box_flag ui_box_flag;
