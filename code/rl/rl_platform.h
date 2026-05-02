/* date = December 14th 2025 5:30 pm */

#if !defined(PLATFORM_H)
#define PLATFORM_H

// TODO(luca): So that everyone has the same sense of what the "release" version is of the app maybe the sets of flags set in release mode should be defined?

// NOTE(luca): Adds some debug information and interactions that a normal user shouldn't be able to see.
#if !defined(RL_PLATFORM_INTERNAL)
# define RL_PLATFORM_INTERNAL 0
#endif

// NOTE(luca): Toggles the compiliation of the debug GUI in the platform layer, this is separate from RL_PLATFORM_INTERNAL because sometimes we want to test the "release" version of our app with our platform debug tools. 
#if !defined(RL_PLATFORM_DEBUG_UI)
# define RL_PLATFORM_DEBUG_UI 0
#endif

// NOTE(luca): For performance (specifically on my laptop).
#if !defined(RL_PLATFORM_HOT_RELOAD_SHADERS)
# define RL_PLATFORM_HOT_RELOAD_SHADERS 0
#endif

#if !defined(RL_PLATFORM_FORCE_X11)
# define RL_PLATFORM_FORCE_X11 0
#endif

#if !defined(RL_PLATFORM_FORCE_SMALL_RESOLUTION)
# define RL_PLATFORM_FORCE_SMALL_RESOLUTION 0
#endif

//~ Globals
#if RL_PLATFORM_INTERNAL
global_variable b32 IsEditorBuildInternal = true;
#else
global_variable b32 IsEditorBuildInternal = false;
#endif

global_variable str8 ExeDirPath = {0};;

global_variable arena *FrameArena = 0;

//~ Types
typedef struct app_offscreen_buffer app_offscreen_buffer;
struct app_offscreen_buffer
{
    s32 Width;
    s32 Height;
    u8 *Pixels;
    s32 Pitch;
    s32 BytesPerPixel;
};

// TODO(luca): Metaprogram
enum platform_key
{
    PlatformKey_None = 0,
    PlatformKey_Tab,
    PlatformKey_Return,
    PlatformKey_Escape,
    
    PlatformKey_Delete,
    PlatformKey_BackSpace,
    PlatformKey_Insert,
    
    PlatformKey_F1,
    PlatformKey_F2,
    PlatformKey_F3,
    PlatformKey_F4,
    PlatformKey_F5,
    PlatformKey_F6,
    PlatformKey_F7,
    PlatformKey_F8,
    PlatformKey_F9,
    PlatformKey_F10,
    PlatformKey_F11,
    PlatformKey_F12,
    
    PlatformKey_Home,
    PlatformKey_End,
    PlatformKey_PageUp,
    PlatformKey_PageDown,
    
    PlatformKey_Up,
    PlatformKey_Down,
    PlatformKey_Left,
    PlatformKey_Right,
    
    PlatformKey_Shift,
    PlatformKey_Control,
    PlatformKey_Alt,
    
    PlatformKey_Count,
};
typedef enum platform_key platform_key;

enum platform_key_modifier
{
    PlatformKeyModifier_None    = 0,
    PlatformKeyModifier_Shift   = (1 << 0),
    PlatformKeyModifier_Control = (1 << 1),
    PlatformKeyModifier_Alt     = (1 << 2),
};
typedef enum platform_key_modifier platform_key_modifier;

#define PlatformKeyModifier_Any (PlatformKeyModifier_Control|PlatformKeyModifier_Shift|PlatformKeyModifier_Alt)

typedef struct app_text_button app_text_button;
struct app_text_button
{
    union
    {
        rune Codepoint;
        platform_key Symbol;
    };
    s32 Modifiers;
    b32 IsSymbol;
};

typedef struct app_button_state app_button_state;
struct app_button_state
{
    s32 Modifiers;
    s32 HalfTransitionCount;
    b32 EndedDown;
};

typedef struct app_midi_note app_midi_note;
struct app_midi_note
{
    u32 Message;
    f32 Timestamp;
};

enum platform_cursor_shape
{
    PlatformCursorShape_Arrow = 0,
    PlatformCursorShape_None,
    PlatformCursorShape_Grab,
    PlatformCursorShape_ResizeHorizontal,
    PlatformCursorShape_ResizeVertical,
};
typedef enum platform_cursor_shape platform_cursor_shape;

enum platform_mouse_button_type
{
    PlatformMouseButton_Left = 0,
    PlatformMouseButton_Right,
    PlatformMouseButton_Middle,
    PlatformMouseButton_ScrollUp,
    PlatformMouseButton_ScrollDown,
    PlatformMouseButton_Count
};
typedef enum platform_mouse_button_type platform_mouse_button_type;

// NOTE(luca): If I want to support multiple players, I probably want to support multiple
// mice as well, so I may just want to duplicate this input struct.

typedef struct app_input app_input;
struct app_input
{
    struct
    {
        app_button_state Buttons[PlatformMouseButton_Count];
        s32 X, Y, Z;
        
        s32 StartX, StartY;
    } Mouse;
    
    union
    {
        app_button_state GameButtons[8];
        struct 
        {
            app_button_state ActionLeft;
            app_button_state ActionRight;
            app_button_state ActionUp;
            app_button_state ActionDown;
            app_button_state MoveLeft;
            app_button_state MoveRight;
            app_button_state MoveUp;
            app_button_state MoveDown;
        };
    };
    
    struct
    {
        u64 Count;
        // NOTE(luca): Who can press more than 256 keys in under one frame?
        app_text_button Buffer[256];
    } Text;
    
    struct
    {
        app_midi_note *Notes;
        u64 Count;
        
        union
        {
            struct 
            {
                // NOTE(luca): Keys are chosen to be like in Ableton
                // TODO(luca): Metaprogram
                app_button_state KeyA;
                app_button_state KeyW;
                app_button_state KeyS;
                app_button_state KeyE;
                app_button_state KeyD;
                app_button_state KeyF;
                app_button_state KeyT;
                app_button_state KeyG;
                app_button_state KeyY;
                app_button_state KeyH;
                app_button_state KeyU;
                app_button_state KeyJ;
                app_button_state KeyK;
                app_button_state KeyI;
                app_button_state KeyL;
            };
            app_button_state Buttons[15];
        };
    } MIDI;
    
    // NOTE(luca): We can invalidate the input with this.
    b32 Consumed;
    
    f32 dtForFrame;
    
    b32 SkipRendering;
    
    b32 PlatformWindowIsFocused;
    b32 PlatformIsRecording;
    b32 PlatformIsStepping;
    
    // Communication with the platform layer
    s32 PlatformCursor;
    str8 PlatformClipboard;
    b32 PlatformSetClipboard;
};

typedef struct platform_midi_device platform_midi_device;
struct platform_midi_device
{
    u64 Id;
    str8 Name;
    b32 IsOutput;
};

typedef struct platform_midi_get_devices_result platform_midi_get_devices_result;
struct platform_midi_get_devices_result
{
    u64 Count;
    platform_midi_device *Devices;
};

#define PLATFORM_MIDI_GET_DEVICES(Name) platform_midi_get_devices_result Name(void)
typedef PLATFORM_MIDI_GET_DEVICES(platform_midi_get_devices);
#define PLATFORM_MIDI_SEND(Name) void Name(platform_midi_device Device, u32 Message)
typedef PLATFORM_MIDI_SEND(platform_midi_send);
#define PLATFORM_MIDI_LISTEN(Name) void Name(platform_midi_device Device)
typedef PLATFORM_MIDI_LISTEN(platform_midi_listen);

typedef struct app_memory app_memory;
struct app_memory
{
    void *Memory;
    u64 MemorySize;
    
    // NOTE(luca): Globals shared between platform and app.
    str8 ExeDirPath;
    b32 IsDebuggerAttached;
    b32 IsProfiling;
    // NOTE(luca): So that time from GetWallTime() is correct even when replaying.
    
    b32 Reloaded;
    
    thread_context *ThreadCtx;
    
#if OS_WINDOWS
    f64 PerfCountFrequency;
#endif
    
    platform_midi_get_devices *PlatformMIDIGetDevices;
    platform_midi_send *PlatformMIDISend;
    platform_midi_listen *PlatformMIDIListen;
    
    b32 Initialized;
};

#define UPDATE_AND_RENDER(Name) b32 Name(thread_context *Context, app_memory *Memory, app_offscreen_buffer *Buffer, app_input *Input)
typedef UPDATE_AND_RENDER(update_and_render);
UPDATE_AND_RENDER(UpdateAndRenderStub) { return false; }

#define GET_AUDIO_SAMPLES(Name) void Name(thread_context *Context, void *Buffer, s64 FramesCount)
typedef GET_AUDIO_SAMPLES(get_audio_samples);
GET_AUDIO_SAMPLES(GetAudioSamplesStub) {}

typedef struct app_code app_code;
struct app_code
{
    update_and_render *UpdateAndRender;
    get_audio_samples *GetAudioSamples;
    
    char *LibraryPath;
    u64 LibraryHandle;
    b32 Loaded;
    s64 LastWriteTime;
};

//~ Platform API
typedef u64  P_context;

internal P_context P_Init(arena *Arena, app_offscreen_buffer *Buffer, b32 *Running, char *WindowName);
internal void      P_UpdateImage(P_context Context, app_offscreen_buffer *Buffer);
internal void      P_ProcessMessages(P_context Context, app_input *Input, app_offscreen_buffer *Buffer, b32 *Running);
internal void      P_LoadAppCode(arena *Arena, app_code *Code, app_memory *Memory);
PLATFORM_MIDI_GET_DEVICES(P_MIDIGetDevices);
PLATFORM_MIDI_SEND(P_MIDIPlay);
PLATFORM_MIDI_LISTEN(P_MIDIListen);

//- Helpers 
internal inline b32 
WasPressed(app_button_state State)
{
    b32 Result = ((State.HalfTransitionCount > 1) || 
                  (State.HalfTransitionCount == 1 && State.EndedDown));
    return Result;
}

internal char *
PathFromExe(arena *Arena, str8 Path)
{
    char *Result = 0;
    
    u64 Size = ExeDirPath.Size + Path.Size + 1;
    
    Result = PushArray(Arena, char, Size);
    
    u64 At = 0;
    for EachIndex(Idx, ExeDirPath.Size)
    {
        Result[At] = (char)ExeDirPath.Data[Idx];
        At += 1;
    }
    for EachIndex(Idx, Path.Size)
    {
        Result[At] = (char)Path.Data[Idx];
        At += 1;
    }
    
    Result[Size - 1] = 0;
    
    return Result;
}

internal void 
ProcessKeyPress(app_button_state *ButtonState, b32 IsDown)
{
    if(ButtonState->EndedDown != IsDown)
    {
        ButtonState->EndedDown = IsDown;
        ButtonState->HalfTransitionCount += 1;
    }
}

internal v2
MousePosFromInput(app_input *Input)
{
    v2 Result = V2S32(Input->Mouse.X, Input->Mouse.Y);
    return Result;
}

#endif //PLATFORM_H
