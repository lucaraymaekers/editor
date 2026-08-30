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
 
#define APP_MIDIInputKeys app_button_state KeyA; app_button_state KeyW; app_button_state KeyS; app_button_state KeyE; app_button_state KeyD; app_button_state KeyF; app_button_state KeyT; app_button_state KeyG; app_button_state KeyY; app_button_state KeyH; app_button_state KeyU; app_button_state KeyJ; 
typedef enum note_pitch note_pitch;
enum note_pitch
{
Note_C,
 Note_Cs,
 Note_D,
 Note_Ds,
 Note_E,
 Note_F,
 Note_Fs,
 Note_G,
 Note_Gs,
 Note_A,
 Note_As,
 Note_B,
 Note_Count
};

str8 NotePitchStrings[] =
{
{(u8 *)"C", 1},
{(u8 *)"C#", 2},
{(u8 *)"D", 1},
{(u8 *)"D#", 2},
{(u8 *)"E", 1},
{(u8 *)"F", 1},
{(u8 *)"F#", 2},
{(u8 *)"G", 1},
{(u8 *)"G#", 2},
{(u8 *)"A", 1},
{(u8 *)"A#", 2},
{(u8 *)"B", 1},
};

#define Win32ColemakMIDIKeySymbolsDef 'A', 'W', 'R', 'F', 'S', 'T', 'G', 'D', 'J', 'H', 'L', 'N', 
#define Win32QwertyMIDIKeySymbolsDef 'A', 'W', 'S', 'E', 'D', 'F', 'T', 'G', 'Y', 'H', 'U', 'J', 
#define LinuxColemakMIDIKeySymbolsDef XK_a,XK_w,XK_r,XK_f,XK_s,XK_t,XK_g,XK_d,XK_j,XK_h,XK_l,XK_n,
#define LinuxQwertyMIDIKeySymbolsDef XK_a,XK_w,XK_s,XK_e,XK_d,XK_f,XK_t,XK_g,XK_y,XK_h,XK_u,XK_j,
