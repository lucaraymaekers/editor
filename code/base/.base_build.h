/* date = May 6th 2026 10:43 am */
// NOTE(luca): This is an example, copy rename it to `base_build.h` to use it.

#ifndef BASE_BUILD_EXAMPLE_H
#define BASE_BUILD_EXAMPLE_H

//- Base 
#define BASE_PROFILE 0

//- Platform 
#define RL_PLATFORM_COLEMAK 1
#define RL_PLATFORM_INTERNAL 1
#define RL_PLATFORM_DEBUG_UI 1
#define RL_PLATFORM_HOT_RELOAD_SHADERS 1
#define RL_PLATFORM_FORCE_X11 0
#define RL_PLATFORM_FORCE_SMALL_RESOLUTION 1
#define RL_PLATFORM_FORCE_UPDATE_HZ 60

//- Editor 
#define EDITOR_INTERNAL 1
#define EDITOR_HOT_RELOAD_SHADERS 1

//- Muze 
#define MUZE_INTERNAL 1
#define MUZE_HOT_RELOAD_SHADERS 1
#define MUZE_STARTUP_PROFILE 0

#endif //BASE_BUILD_EXAMPLE_H