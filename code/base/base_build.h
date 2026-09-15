/* date = March 17th 2026 1:04 pm */
#ifndef BASE_BUILD_H
#define BASE_BUILD_H

// NOTE(luca): These are the default values, if you want to override them create
// a ".base_build.h" file in which you can define the values.
// When none of these values are overridden that means that we are in release mode.

//- Base 
#if !defined(BASE_PROFILE)
# define BASE_PROFILE 0
#endif
// NOTE(luca): Force number of threads to use, 0 means use all threads.
#if !defined(BASE_FORCE_THREADS_COUNT)
# define BASE_FORCE_THREADS_COUNT 0
#endif
// NOTE(luca): On windows this means to open an external console when the program is launched.
#if !defined(BASE_CONSOLE_APPLICATION)
# define BASE_CONSOLE_APPLICATION 0
#endif
// NOTE(luca): Specify that libraries will be included from elsewhere
#if !defined(BASE_EXTERNAL_LIBS)
# define BASE_EXTERNAL_LIBS 0
#endif
// NOTE(luca): This code has no executable entrypoint, e.g., DLL
#if !defined(BASE_NO_ENTRYPOINT)
# define BASE_NO_ENTRYPOINT 0
#endif 

//- RL 
#if !defined(RL_PLATFORM_COLEMAK)
# define RL_PLATFORM_COLEMAK 0
#endif
// NOTE(luca): Adds some debug information and interactions that a normal user shouldn't be able to see.
#if !defined(RL_PLATFORM_INTERNAL)
# define RL_PLATFORM_INTERNAL 0
#endif
// NOTE(luca): Toggles the compiliation of the debug GUI in the platform layer, this is separate from RL_PLATFORM_INTERNAL because sometimes we want to test the "release" version of our app with our platform debug tools. 
#if !defined(RL_PLATFORM_DEBUG_UI)
# define RL_PLATFORM_DEBUG_UI 0
#endif
#if !defined(RL_PLATFORM_HOT_RELOAD_SHADERS)
# define RL_PLATFORM_HOT_RELOAD_SHADERS 0
#endif
#if !defined(RL_PLATFORM_FORCE_X11)
# define RL_PLATFORM_FORCE_X11 0
#endif
#if !defined(RL_PLATFORM_FORCE_SMALL_RESOLUTION)
# define RL_PLATFORM_FORCE_SMALL_RESOLUTION 1
#endif
#if !defined(RL_PLATFORM_FORCE_UPDATE_HZ)
# define RL_PLATFORM_FORCE_UPDATE_HZ 60
#endif

//- Cling 
// NOTE(luca): Default is not defined
//#define CLING_MSVC_SETUP "C:\\msvc\\setup_x64.bat"

//- Editor 
#if !defined(EDITOR_INTERNAL)
# define EDITOR_INTERNAL 0
#endif

//- Muze
#if !defined(MUZE_INTERNAL)
# define MUZE_INTERNAL 0
#endif
#if !defined(MUZE_STARTUP_PROFILE)
# define MUZE_STARTUP_PROFILE 0
#endif


#endif // BASE_BUILD_H