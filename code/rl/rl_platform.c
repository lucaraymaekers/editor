//~ Libraries 
#if RL_PLATFORM_INTERNAL
# define BASE_CONSOLE_APPLICATION 0
#endif
#include "base/base.h"
#include "base/base.c"

#include "rl/rl_libs.h"
#include "rl/rl_midi.h"

#include "rl/rl_platform.h"
#if OS_LINUX
# include "rl/rl_platform_linux.c"
#elif OS_WINDOWS
# include "rl/rl_platform_windows.c"
#endif

//- For rendering debug UI 
#include "rl/generated/everything.c"
#include "rl/rl_gl.h"
#include "rl/rl_font.h"
#include "rl/rl_renderer.h"
#include "rl/rl_ui.h"
#include "rl/rl_renderer.c"
#include "rl/rl_ui.c"
#include "rl/rl_widgets.c"

//- Third party 
#if OS_WINDOWS
# define RADDBG_MARKUP_IMPLEMENTATION
#else
# define RADDBG_MARKUP_STUBS
#endif
#include "lib/raddbg_markup.h"

//~ Recording 
typedef struct platform_replay platform_replay;
struct platform_replay
{
    void *Buffer;
    u64 PlayingPos;
    u64 RecordingSize;
    
    b32 IsRecording;
    
    b32 IsStepping;
    b32 IsLooping;
    b32 IsSkipping;
    
    u64 StepIdx;
    u64 StepsCount;
    u64 StepTarget;
};

internal void
ReplayStopRecording(platform_replay *Replay, app_memory *Memory)
{
    if(Replay->IsRecording)
    {
        Replay->IsRecording = false;
    }
}

internal void
ReplayLoadMemory(platform_replay *Replay, app_memory *Memory)
{
    if(Replay->RecordingSize)
    {
        MemoryCopy(Memory->Memory, Replay->Buffer, Memory->MemorySize);
        Replay->PlayingPos = Memory->MemorySize;
        Replay->StepIdx = 0;
    }
}

internal void
ReplayRecordMemory(platform_replay *Replay, app_memory *Memory)
{
    MemoryCopy(Replay->Buffer, Memory->Memory, Memory->MemorySize);
    Replay->RecordingSize = Memory->MemorySize;
}

internal void
ReplayToggleRecording(platform_replay *Replay, app_memory *Memory, b32 KeepRecordingIfStarted)
{
    if(!Replay->IsRecording)
    {
        ReplayRecordMemory(Replay, Memory);
        
        Replay->StepIdx = 0;
        Replay->StepTarget = 0;
        Replay->PlayingPos = 0;
        
        Replay->IsLooping = false;
        Replay->IsStepping = false;
        
        Replay->IsRecording = KeepRecordingIfStarted;
    }
    else
    {
        ReplayStopRecording(Replay, Memory);
    }
}

internal void
ReplayToggleLooping(platform_replay *Replay, app_memory *Memory)
{
    if(Replay->RecordingSize > 0)
    {
        ReplayStopRecording(Replay, Memory);
        Replay->IsLooping = !Replay->IsLooping;
        
        if(Replay->IsLooping)
        {
            Replay->IsStepping = true;
        }
        
    }
}

internal void
ReplayStep(platform_replay *Replay, app_memory *Memory)
{
    if(Replay->PlayingPos == 0)
    {
        ReplayLoadMemory(Replay, Memory);
        
        Replay->IsStepping = (Replay->RecordingSize != Replay->PlayingPos);
    }
    else
    {
        Replay->IsStepping = true;
    }
    
    if(Replay->StepsCount == 0)
    {
        Replay->IsStepping = false;
    }
    
    if(Replay->IsStepping)
    {
        ReplayStopRecording(Replay, Memory);
        
        Replay->IsLooping = false;
    }
}

internal void
ReplayStepNext(platform_replay *Replay)
{
    if(Replay->RecordingSize > 0)
    {
        if(Replay->PlayingPos == 0)
        {
            Replay->StepTarget = 0;
        }
        else
        {
            Replay->StepTarget = (Replay->StepIdx + 1)%Replay->StepsCount;
        }
    }
}

//~ UI (Debug) 
internal void
DebugSpacer(void)
{
    UI_Spacer(UI_SizeEm(.2f, 1.f));
}

internal ui_box *
UI_Label(s32 Flags, str8 String)
{
    ui_box *Result = UI_AddBox(String, (UI_BoxFlag_Clip|
                                        UI_BoxFlag_DrawDisplayString|
                                        UI_BoxFlag_DrawBackground|
                                        Flags));
    return Result;
}

internal ui_box *
UI_Labelf(s32 Flags, char *Format, ...)
{
    ui_box *Result = UI_NilBox;
    str8 String = {0};
    
    va_list Args;
    va_start(Args, Format);
    String = Str8VFmt(Format, Args);
    
    Result = UI_Label(Flags, String);
    
    return Result;
}

global_variable v4 Color_Disabled = {U32ToV4Arg(0xFF616E88)};

internal v4
GetDisabledColorCondition(b32 Condition)
{
    v4 BackgroundColor = UI_State->BackgroundColorTop->Value;
    if(Condition)
    {
        BackgroundColor = V4(U32ToV4Arg(0xFF616E88));
    }
    
    return BackgroundColor;
}


internal b32
DebugReplayToggleButton(str8 Name, b32 State, b32 DisabledCondition)
{
    b32 Result = false;
    
    str8 Label = (!State ? 
                  Str8Fmt(        S8Fmt "###" S8Fmt, S8Arg(Name), S8Arg(Name)) : 
                  Str8Fmt("Stop " S8Fmt "###" S8Fmt, S8Arg(Name), S8Arg(Name)));
    v4 BackgroundColor = (DisabledCondition ? 
                          Color_Disabled :
                          State ? Color_Red : Color_ButtonBackground);
    
    Result = UI_ToggleButton(Label, true, BackgroundColor);
    
    return Result;
}

internal void
ResetButtons(app_button_state *NewButtons, app_button_state *OldButtons, u64 Count)
{
    for EachIndex(Idx, Count)
    {
        app_button_state *NewButton = NewButtons + Idx;
        app_button_state *OldButton = OldButtons + Idx;
        NewButton->EndedDown = OldButton->EndedDown;
        NewButton->HalfTransitionCount = 0;
        NewButton->Modifiers = 0;
    }
}

//~ Entrypoint

C_LINKAGE ENTRY_POINT(EntryPoint)
{
    if(LaneIndex() == 0)
    {
        OS_ProfileInit("S");
        
        u64 PlatformMemorySize = GB(4);
        u64 AppMemorySize = GB(1);
        
        // NOTE(luca): Total memory also for game.
        arena *PermanentArena = ArenaAlloc(.Size = PlatformMemorySize, .Offset = TB(2));
        FrameArena = ArenaAlloc();
        
        StringsScratch = FrameArena;
        
        OS_ProfileAndPrint("Memory");
        
        b32 *Running = PushArray(PermanentArena, b32, 1);
        *Running = true;
        
        app_offscreen_buffer WindowBuffer = {0};
#if RL_PLATFORM_FORCE_SMALL_RESOLUTION
        WindowBuffer.Width = 1920/2;
        WindowBuffer.Height = 1080/2;
#else
        WindowBuffer.Width = 1920;
        WindowBuffer.Height = 1080;
#endif
        WindowBuffer.BytesPerPixel = 4;
        WindowBuffer.Pitch = WindowBuffer.BytesPerPixel*WindowBuffer.Width;
        WindowBuffer.Pixels = PushArray(PermanentArena, u8, (u64)(WindowBuffer.Pitch*WindowBuffer.Height));
        
        WindowBuffer.Width = 1600;
        WindowBuffer.Height = 900;
        
        app_sound_buffer SoundBuffer = {0};
        
        // TODO(luca): Detect refresh rate
#if defined(RL_PLATFORM_FORCE_UPDATE_HZ)
        f32 GameUpdateHz = RL_PLATFORM_FORCE_UPDATE_HZ;
#else
        f32 GameUpdateHz = 60.0f;
#endif
        f32 TargetSecondsPerFrame = 1.0f/GameUpdateHz; 
        
        P_context PlatformContext = P_Init(PermanentArena, &WindowBuffer, &SoundBuffer, Running, Stringify(RL_PLATFORM_WINDOW_NAME), GameUpdateHz);
        
        OS_ProfileAndPrint("Context");
        
        if(!PlatformContext)
        {
            ErrorLog("Could not initialize graphical context, running in headless mode.");
        }
        
        // Get ExeDirPath
        {
            u64 OnePastLastSlash = 0;
            char *FileName = Params->Args[0];
            u64 SizeOfFileName = StringLength(FileName);
            for EachIndex(Idx, SizeOfFileName)
            {
                if(FileName[Idx] == OS_SlashChar)
                {
                    OnePastLastSlash = Idx + 1;
                }
            }
            
            ExeDirPath.Data = (u8 *)FileName;
            ExeDirPath.Size = OnePastLastSlash;
        }
        
        app_memory AppMemory = {0};
        AppMemory.MemorySize = AppMemorySize;
        AppMemory.Memory = PushArray(PermanentArena, u8, AppMemory.MemorySize);
        AppMemory.ThreadCtx = ThreadContext;
        AppMemory.ExeDirPath = ExeDirPath;
        AppMemory.IsDebuggerAttached = GlobalDebuggerIsAttached;
#if OS_WINDOWS
        AppMemory.PerfCountFrequency = GlobalPerfCountFrequency;
#endif
        
        AppMemory.PlatformMIDIGetDevices = P_MIDIGetDevices;
        AppMemory.PlatformMIDISend = P_MIDISend;
        AppMemory.PlatformMIDIListen = P_MIDIListen;
        
        app_input _Input[3] = {0};
        app_input *NewInput = &_Input[0];
        app_input *OldInput = &_Input[1];
        app_input *ReplayInput = &_Input[2];
        
        f64 LastCounter = OS_GetWallClock();
        f64 FlipWallClock = LastCounter;
        
        //- Sound 
        
        umm BytesPerSample = sizeof(sample_format)*SoundBuffer.ChannelCount;
        
        //- Code
        app_code Code = {0};
        
        b32 CurrentOSLinux = false;
#if OS_LINUX
        CurrentOSLinux = true;
#endif
        
        str8 LibraryPath = Str8Fmt(Stringify(RL_PLATFORM_APP_NAME) ".%s", (CurrentOSLinux ? "so" : "dll"));
        Code.LibraryPath = PathFromExe(PermanentArena, LibraryPath);
        
        //- Debug UI 
#if RL_PLATFORM_DEBUG_UI
        b32 ShowDebugUI = false;
        f32 DebugHeightPx = 20.f;
        gl_render_state DebugRender = {0};
        font_atlas DebugRenderAtlas = {0};
        font TextFont = {0};
        font IconsFont = {0};
        s32 GLADVersion = 0;
        {
            char *FontPath = 0;
            FontPath = PathFromExe(FrameArena, S8("../data/icons.ttf"));
            InitFont(&IconsFont, FontPath);
            FontPath = PathFromExe(FrameArena, S8("../data/font_regular.ttf"));
            InitFont(&TextFont, FontPath);
            
            GLADVersion = gladLoaderLoadGL();
            RenderInit(&DebugRender);
            arena *FontAtlasArena = PushArena(PermanentArena, MB(150), false);
            // TODO(luca): Building this atlas is slow and tanks our startup time.  So we should speed it up by using a glyph cache.
            RenderBuildAtlas(FontAtlasArena, &DebugRenderAtlas, &TextFont, &IconsFont, DebugHeightPx);
            
            arena *ROArena = ArenaAlloc();
            
            // Init UI state
            {            
                UI_State = PushArray(PermanentArena, ui_state, 1);
                UI_State->Arena = PushArena(PermanentArena, ArenaAllocDefaultSize, false);
                UI_State->BoxTableSize = 4096;
                UI_State->BoxTable = PushArray(UI_State->Arena, ui_box, UI_State->BoxTableSize);
                UI_State->FrameArena = FrameArena;
                
                ui_box *Box = PushArray(ROArena, ui_box, 1);
                *Box = (ui_box){Box, Box, Box, Box, Box, Box, Box};
                UI_NilBox = Box;
            }
            
            OS_MarkReadonly(ROArena->Base, ROArena->Size);
        }
        
        OS_ProfileAndPrint("Debug UI setup");
#endif
        
        platform_replay Replay = {0};
        
        b32 Paused = false;
        b32 Logging = false;
        
        // NOTE(luca): 10 minutes of gameplay
        u64 ReplayMaxRecordingFramesCount = (u64)GameUpdateHz * 60 * 10;
        u64 ReplayMaxBufferSize = AppMemory.MemorySize + (ReplayMaxRecordingFramesCount*sizeof(app_input));
        Replay.Buffer = PushArray(PermanentArena, u8, ReplayMaxBufferSize);
        u64 MaxReplaySlots = 5;
        u64 ReplaySlot = 0;
        
        OS_ProfileAndPrint("Misc");
        
        OS_ProfileInit("P");
        
        u64 FrameIdx = 0;
        f64 LastSoundCounter = 0.f;
        f64 LastWorkMSPerFrame = 0.f;
        while(*Running)
        {
            Scratch(FrameArena)
            {
                RenderBeginFrame(FrameArena, WindowBuffer.Width, WindowBuffer.Height);
                OS_ProfileAndPrint("Render Setup");
                
                P_LoadAppCode(FrameArena, &Code, &AppMemory);
                OS_ProfileAndPrint("Code");
                
                NewInput->PlatformWindowIsFocused = OldInput->PlatformWindowIsFocused;
                b32 PlatformWindowIsFocused;
                
                // Input
                {
                    NewInput->Consumed = false;
                    NewInput->SkipRendering = false;
                    NewInput->MIDI.EventCount = 0;
                    NewInput->Text.Count = 0;
                    
                    ResetButtons(NewInput->Mouse.Buttons, OldInput->Mouse.Buttons, ArrayCount(NewInput->Mouse.Buttons));
                    ResetButtons(NewInput->GameButtons, OldInput->GameButtons, ArrayCount(NewInput->GameButtons));
                    ResetButtons(NewInput->MIDI.Buttons, OldInput->MIDI.Buttons, ArrayCount(NewInput->MIDI.Buttons));
                    
                    NewInput->dtForFrame = TargetSecondsPerFrame;
                    NewInput->PlatformCursor = OldInput->PlatformCursor;
                    NewInput->PlatformSetClipboard = OldInput->PlatformSetClipboard;
                    
                    NewInput->PlatformClipboard = OldInput->PlatformClipboard;
                    NewInput->PlatformSetClipboard = OldInput->PlatformSetClipboard;
                    NewInput->Mouse.X = OldInput->Mouse.X;
                    NewInput->Mouse.Y = OldInput->Mouse.Y;
                    NewInput->Mouse.StartX = OldInput->Mouse.StartX;
                    NewInput->Mouse.StartY = OldInput->Mouse.StartY;
                    
                    P_ProcessMessages(PlatformContext, NewInput, &WindowBuffer, Running);
                    
                    PlatformWindowIsFocused = NewInput->PlatformWindowIsFocused;
                    
                    app_button_state MouseLeft = NewInput->Mouse.Buttons[PlatformMouseButton_Left];
                    app_button_state MouseRight = NewInput->Mouse.Buttons[PlatformMouseButton_Right];
                    
                    b32 OnlyOnePressed = (!!MouseLeft.EndedDown ^ !!MouseRight.EndedDown); 
                    
                    if(OnlyOnePressed &&
                       (WasPressed(MouseLeft) || WasPressed(MouseRight)))
                    {                        
                        NewInput->Mouse.StartX = NewInput->Mouse.X;
                        NewInput->Mouse.StartY = NewInput->Mouse.Y;
                    }
                }
                
                OS_ProfileAndPrint("Messages");
                
#if RL_PLATFORM_DEBUG_UI                
                for EachIndex(Idx, NewInput->Text.Count)
                {
                    app_text_button Key = NewInput->Text.Buffer[Idx];
                    b32 Alt = (Key.Modifiers == PlatformKeyModifier_Alt);
                    b32 AltShift = (Key.Modifiers == (PlatformKeyModifier_Alt|
                                                      PlatformKeyModifier_Shift));
                    b32 AltControl = (Key.Modifiers == (PlatformKeyModifier_Alt|
                                                        PlatformKeyModifier_Control));
                    b32 AltControlShift = (Key.Modifiers == (PlatformKeyModifier_Alt|
                                                             PlatformKeyModifier_Control|
                                                             PlatformKeyModifier_Shift));
                    
                    switch(ToLowercase((u8)Key.Codepoint))
                    {
                        case 'b':
                        {
                            if(0) {}
                            else if(AltShift)
                            {
                                DebugBreak();
                            }
                        }
                        
                        case 'p':
                        {
                            if(0) {}
                            else if(Alt)
                            {
                                Paused = !Paused;
                                Log("%s\n", (Paused) ? "Paused" : "Unpaused");
                            }
                            else if(AltShift)
                            {
                                GlobalIsProfiling = !GlobalIsProfiling;
                            }
                        } break;
                        
                        case 'd':
                        {
                            if(0) {}
                            else if(Alt)
                            {
                                ShowDebugUI = !ShowDebugUI;
                            }
                        } break;
                        
                        case 'g': 
                        {
                            if(0) {}
                            else if(Alt)
                            {
                                Logging = !Logging;
                            }
                        } break;
                        
                        case 'r':
                        {
                            if(0) {}
                            if(Alt)
                            { 
                                ReplayToggleRecording(&Replay, &AppMemory, true);
                            }
                            else if(AltShift)
                            {
                                ReplayToggleRecording(&Replay, &AppMemory, false);
                            }
                        } break;
                        
                        case 'l':
                        {
                            if(0) {}
                            if(Alt)
                            {
                                ReplayToggleLooping(&Replay, &AppMemory);
                            }
                            else if(AltShift)
                            {
                                ReplayStep(&Replay, &AppMemory);
                                Replay.IsSkipping = true;
                                Replay.StepTarget = 0;
                            }
                        } break;
                        
                        case 's':
                        {
                            if(0) {}
                            else if(Alt)
                            {
                                ReplayStep(&Replay, &AppMemory);
                                if(Replay.StepsCount)
                                {
                                    Replay.StepTarget = (Replay.StepIdx + 1)%Replay.StepsCount;
                                }
                            }
                            else if(AltControl)
                            {
                                Replay.IsStepping = !Replay.IsStepping;
                            }
                        } break;
                        
                        default: break;
                    }
                }
                
                if(ShowDebugUI)
                {
                    local_persist ui_box *Root = 0;
                    if(Root == 0) Root = UI_BoxAlloc(UI_State->Arena);
                    f32 BorderSize = 2.f;
                    v2 BufferDim = V2S32(WindowBuffer.Width, WindowBuffer.Height);
                    V2Math Root->FixedPosition.E = (BorderSize);
                    V2Math Root->FixedSize.E = (BufferDim.E - 2.f*BorderSize);
                    Root->Rec = RectFromSize(Root->FixedPosition, Root->FixedSize);
                    
                    UI_State->Atlas = &DebugRenderAtlas;
                    UI_State->FrameIdx = FrameIdx;
                    UI_State->Input = NewInput;
                    if(UI_IsActive(UI_NilBox))
                    {
                        UI_State->Hot = UI_KeyNull();
                    }
                    
                    if(!Paused)
                    {
                        // NOTE(luca): When paused we cannot show this because it will turn the screen to black.
                        DrawRect(Root->Rec, V4(0.f, 0.f, 0.f, 0.1f), 0.f, 0.f, 0.f);
                    }
                    
                    UI_DefaultState(Root, DebugHeightPx);
                    
                    // UI
                    {
                        // TODO(luca): 
                        // if shift held other texts
                        // if shift click
                        // key bind information on 300ms hover?
                        
                        // When done start doing serialization. (+ (de)compression?)
                        UI_BorderThickness(1.f)
                            UI_BackgroundColor(V4(V3Arg(Color_Background), .8f))
                        {                        
                            UI_LayoutAxis(Axis2_X)
                                UI_SemanticWidth(UI_SizeChildren(1.f))
                                UI_SemanticHeight(UI_SizeChildren(1.f))
                                UI_AddBox(S8(""), UI_BoxFlag_Clip|UI_BoxFlag_DrawBackground);
                            {                         
                                UI_Push()
                                    UI_LayoutAxis(Axis2_Y)
                                    UI_SemanticHeight(UI_SizeChildren(1.f))
                                    UI_SemanticWidth(UI_SizeChildren(1.f))
                                {
                                    UI_AddBox(S8(""), UI_BoxFlag_Clip);
                                    UI_Push()
                                        UI_SemanticWidth(UI_SizePx(200.f, 1.f))
                                        UI_SemanticHeight(UI_SizeText(2.f, 1.f))
                                    {
                                        
                                        UI_SemanticWidth(UI_SizeText(2.f, 1.f))
                                            UI_FontKind(FontKind_Icon)
                                            if(UI_Button(S8("b")))
                                        {
                                            ShowDebugUI = false;
                                        }
                                        
                                        b32 RecordingIsEmpty = (Replay.RecordingSize == 0);
                                        
                                        if(UI_ToggleButton(S8("Recording"), Replay.IsRecording, Color_Red))
                                        {
                                            ReplayToggleRecording(&Replay, &AppMemory, true);
                                        }
                                        
                                        if(DebugReplayToggleButton(S8("Looping"), Replay.IsLooping, RecordingIsEmpty))
                                        {
                                            ReplayToggleLooping(&Replay, &AppMemory);
                                        }
                                        
                                        if(DebugReplayToggleButton(S8("Stepping"), Replay.IsStepping, RecordingIsEmpty))
                                        {
                                            if(Replay.RecordingSize && Replay.StepsCount > 0)
                                            {
                                                Replay.IsStepping = !Replay.IsStepping;
                                                if(!Replay.IsStepping) Replay.IsLooping = false;
                                            }
                                        }
                                        
                                        DebugSpacer();
                                        
                                        if(UI_ToggleButton(S8("Step"), RecordingIsEmpty, Color_Disabled))
                                        {
                                            ReplayStep(&Replay, &AppMemory);
                                            if(Replay.StepsCount)
                                            {
                                                Replay.StepTarget = (Replay.StepIdx + 1)%Replay.StepsCount;
                                            }
                                        }
                                        
                                        if(UI_ToggleButton(S8("Skip"), RecordingIsEmpty, Color_Disabled))
                                        {
                                            if(Replay.RecordingSize)
                                            {
                                                ReplayStep(&Replay, &AppMemory);
                                                Replay.IsSkipping = true;
                                            }
                                        }
                                        
                                        if(UI_ToggleButton(S8("Set target"), RecordingIsEmpty, Color_Disabled))
                                        {
                                            Replay.StepTarget = Replay.StepIdx;
                                        }
                                        
                                        if(UI_ToggleButton(S8("Load record"), RecordingIsEmpty, Color_Disabled))
                                        {
                                            if(Replay.RecordingSize)
                                            {
                                                ReplayLoadMemory(&Replay, &AppMemory);
                                            }
                                        }
                                        
                                        DebugSpacer();
                                        
                                        if(UI_Button(S8("Load from disk")))
                                        {
                                            char *FileName = PathFromExe(FrameArena, Str8Fmt("replay_%lu.edr", ReplaySlot));
                                            
                                            str8 ReplayBuffer = OS_ReadEntireFileIntoMemory(FileName);
                                            
                                            Assert(ReplayBuffer.Size < ReplayMaxBufferSize);
                                            
                                            if(ReplayBuffer.Size)
                                            {                                            
                                                MemoryCopy(Replay.Buffer, ReplayBuffer.Data, ReplayBuffer.Size);
                                                Replay.RecordingSize = ReplayBuffer.Size;
                                                Replay.StepIdx = 0;
                                                Replay.StepTarget = 0;
                                                ReplayLoadMemory(&Replay, &AppMemory);
                                            }
                                            
                                            OS_FreeFileMemory(ReplayBuffer);
                                            
                                        }
                                        
                                        if(UI_ToggleButton(S8("Save to disk"), RecordingIsEmpty, Color_Disabled))
                                        {
                                            if(Replay.RecordingSize)
                                            {
                                                char *FileName = PathFromExe(FrameArena, Str8Fmt("replay_%lu.edr", ReplaySlot));
                                                str8 ReplayBuffer = {0};
                                                ReplayBuffer.Data = Replay.Buffer;
                                                ReplayBuffer.Size = Replay.RecordingSize;
                                                OS_WriteEntireFile(FileName, ReplayBuffer);
                                            }
                                        }
                                        
                                        DebugSpacer();
                                        
                                        if(GlobalDebuggerIsAttached)
                                        {
                                            if(UI_Button(S8("DebugBreak")))
                                            {
                                                DebugBreak();
                                            }
                                            
                                            DebugSpacer();
                                        }
                                        
                                        if(UI_Button(S8("Prev slot")))
                                        {
                                            ReplaySlot = ((ReplaySlot == 0) ? (MaxReplaySlots - 1) : (ReplaySlot - 1));
                                        }
                                        
                                        if(UI_Button(S8("Next slot")))
                                        {
                                            ReplaySlot = ((ReplaySlot == (MaxReplaySlots - 1)) ? (0) : (ReplaySlot + 1));
                                        }
                                        
                                        DebugSpacer();
                                        
                                        
                                        if(UI_ToggleButton(S8("Logging"), Logging, Color_Red))
                                        {
                                            Logging = !Logging;
                                        }
                                        
                                        if(UI_ToggleButton(S8("Pause"), Paused, Color_Red))
                                        {
                                            Paused = !Paused;
                                        }
                                        
                                        if(UI_ToggleButton(S8("Profiling"), GlobalIsProfiling, Color_Red))
                                        {                                    
                                            GlobalIsProfiling = !GlobalIsProfiling;
                                        }
                                    }
                                    
                                    UI_AddBox(S8(""), UI_BoxFlag_Clip);
                                    UI_Push()
                                        UI_SemanticWidth(UI_SizeText(2.f, 1.f))
                                        UI_SemanticHeight(UI_SizeText(2.f, 1.f))
                                    {
                                        UI_Labelf(0, "cpu: %.2fms/f###cpu frame time", LastWorkMSPerFrame);
                                        
                                        {                                    
                                            str8 StateName = S8("App");
                                            if(Replay.IsStepping) StateName = S8("Stepping");
                                            if(Replay.IsLooping) StateName = S8("Looping");
                                            if(Replay.IsRecording) StateName = S8("Recording");
                                            if(Paused) StateName = S8("Paused");
                                            UI_Labelf(0, "[" S8Fmt "]###state", S8Arg(StateName));
                                        }
                                        
                                        u64 LastStepIdx = (Replay.StepsCount > 0 ? Replay.StepsCount - 1 : 0); 
                                        UI_Labelf(UI_BoxFlag_DrawBorders, "Steps");
                                        UI_Labelf(0, "idx:    %-4lu###StepIdx", Replay.StepIdx);
                                        UI_Labelf(0, "target: %-4lu###StepTarget", Replay.StepTarget);
                                        UI_Labelf(0, "count:  %-4lu###StepsCount", Replay.StepsCount);
                                        UI_Labelf(0, "Slot: %lu###Slot", ReplaySlot);
                                    }
                                }
                            }
                        }
                        
                        UI_ResolveLayout(Root->First);
                        
                        if(!UI_IsActive(UI_NilBox) || !UI_IsHot(UI_NilBox))
                        {
                            NewInput->Consumed = true;
                        }
                    }
                    
                    // Prune unused boxes
                    for EachIndex(Idx, UI_State->BoxTableSize)
                    {
                        ui_box *First = UI_State->BoxTable + Idx;
                        
                        for UI_EachHashBox(Node, First)
                        {
                            if(FrameIdx > Node->LastTouchedFrameIdx)
                            {
                                if(!UI_KeyMatch(Node->Key, UI_KeyNull()))
                                {
                                    Node->Key = UI_KeyNull();
                                }
                            }
                        }
                    }
                }
                
                OS_ProfileAndPrint("Debug UI");
#endif
                
                if(!Paused)
                {
                    // Playback
                    {
                        if(Replay.RecordingSize)
                        {                        
                            Replay.StepsCount = (Replay.RecordingSize - AppMemory.MemorySize)/sizeof(app_input);
                        }
                        
                        if(Replay.IsRecording)
                        {
                            MemoryCopy((u8 *)Replay.Buffer + Replay.RecordingSize, NewInput, sizeof(*ReplayInput));
                            Replay.RecordingSize += sizeof(*ReplayInput);
                            Assert(Replay.RecordingSize < ReplayMaxBufferSize);
                        }
                        
                        if(Replay.IsLooping)
                        {
                            Replay.StepTarget = (Replay.StepIdx + 1)%Replay.StepsCount;
                            Assert(Replay.IsStepping);
                        }
                        
                        if(Replay.IsSkipping)
                        {
                            if(Replay.StepTarget < Replay.StepIdx)
                            {
                                ReplayLoadMemory(&Replay, &AppMemory);
                                Replay.StepIdx = 0;
                            }
                        }
                        
                        if(Replay.IsStepping)
                        {
                            b32 HasReachedStepTarget = false;
                            if(Replay.PlayingPos == 0)
                            {
                                ReplayLoadMemory(&Replay, &AppMemory);
                            }
                            else
                            {                                
                                HasReachedStepTarget = (Replay.StepIdx == Replay.StepTarget);
                                if(!HasReachedStepTarget)
                                {
                                    Replay.StepIdx = (Replay.StepIdx + 1)%Replay.StepsCount;
                                }
                            }
                            
                            // NOTE(luca): We are "pausing" the application by not passing zeroed inputs while the step target is reached. 
                            if(HasReachedStepTarget)
                            {
                                MemoryZero(ReplayInput);
                                ReplayInput->Consumed = true;
                                Replay.IsSkipping = false;
                            }
                            else
                            {
                                // NOTE(luca): Have we thought about the fact that we don't need to do this copy at all.
                                *ReplayInput = *(app_input *)((u8 *)Replay.Buffer + Replay.PlayingPos);
                                Replay.PlayingPos += sizeof(*ReplayInput);
                                
                                if(Replay.PlayingPos >= Replay.RecordingSize)
                                {
                                    Assert(Replay.PlayingPos == Replay.RecordingSize);
                                    ReplayLoadMemory(&Replay, &AppMemory);
                                }
                            }
                        }
                    }
                    OS_ProfileAndPrint("Playback");
                    
                    AppMemory.IsProfiling = GlobalIsProfiling;
                    
                    app_input *AppInput = (Replay.IsStepping ? ReplayInput : NewInput);
                    {
                        // NOTE(luca): Need to be overwritten. Since they reflect the platform state but the the window border is drawn by the app based on the input.
                        AppInput->PlatformIsRecording = Replay.IsRecording;
                        AppInput->PlatformIsStepping = Replay.IsStepping;
                        AppInput->PlatformWindowIsFocused = PlatformWindowIsFocused;
                        AppInput->SkipRendering = Replay.IsSkipping;
                    }
                    if(Code.UpdateAndRender)
                    {
                        b32 ShouldQuit = Code.UpdateAndRender(&AppMemory, &WindowBuffer, AppInput);
                        // NOTE(luca): Since UpdateAndRender can take some time, there could have been a signal sent to INT the app.
                        ReadWriteBarrier();
                        *Running = *Running && !ShouldQuit;
                    }
                    
                    OS_ProfileAndPrint("UpdateAndRender");
                }
                
                b32 SkipRendering = Replay.IsSkipping;
                if(!SkipRendering)
                {
                    // TODO(luca): Would be nice if we could be non blocking in the case where the debug UI wants to be shown but the rendering of the app happens offscreen.
#if RL_PLATFORM_DEBUG_UI
                    RenderDrawAllRectangles(&DebugRender, V2S32(WindowBuffer.Width, WindowBuffer.Height), &DebugRenderAtlas);
#endif
                    P_UpdateImage(PlatformContext, &WindowBuffer);
                    OS_ProfileAndPrint("UpdateImage");
                }
                
                P_PlaySound(PlatformContext, &SoundBuffer, &Code);
                OS_ProfileAndPrint("Sound output");
                
                f64 WorkCounter = OS_GetWallClock();
                f64 WorkMSPerFrame = OS_MSElapsed(LastCounter, WorkCounter);
                LastWorkMSPerFrame = WorkMSPerFrame;
                
                // Sleep
                {
                    // TODO(luca): Think about framerate.
                    if(!SkipRendering)
                    {            
                        f64 SecondsElapsedForFrame = OS_SecondsElapsed(LastCounter, WorkCounter);
                        
                        if(SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            f64 SleepUS = ((TargetSecondsPerFrame - 0.001f - SecondsElapsedForFrame)*1e6);
                            
                            if(SleepUS > 0)
                            {
                                // TODO(luca): Intrinsic
                                OS_Sleep((u32)SleepUS);
                            }
                            else
                            {
                                // TODO(luca): Logging
                            }
                            
                            // NOTE(luca): This is to help against sleep granularity.
                            f64 TestSecondsElapsedForFrame;
                            do
                            {
                                TestSecondsElapsedForFrame = OS_SecondsElapsed(LastCounter, OS_GetWallClock());
                            } while(TestSecondsElapsedForFrame < TargetSecondsPerFrame);
                        }
                        else
                        {
                            // TODO(luca): Log missed frame rate!
                        }
                    }
                    
                    LastCounter = OS_GetWallClock();
                    
                    OS_ProfileAndPrint("Sleep");
                }
                
                NewInput->Text.Buffer[NewInput->Text.Count].Codepoint = 0;
                
                Swap(OldInput, NewInput);
                
                u8 Codepoint = (u8)NewInput->Text.Buffer[0].Codepoint;
                
                if(Logging)
                {
                    Log("'%c' (%d %d -> %d, %d) 1:%c 2:%c 3:%c", 
                        ((Codepoint == 0) ?
                         '\a' : Codepoint),
                        NewInput->Mouse.StartX, NewInput->Mouse.StartY,
                        NewInput->Mouse.X, NewInput->Mouse.Y,
                        (NewInput->Mouse.Buttons[PlatformMouseButton_Left  ].EndedDown ? 'x' : 'o'),
                        (NewInput->Mouse.Buttons[PlatformMouseButton_Middle].EndedDown ? 'x' : 'o'),
                        (NewInput->Mouse.Buttons[PlatformMouseButton_Right ].EndedDown ? 'x' : 'o')); 
                    
                    Log(" %.2fms/f", (f64)WorkMSPerFrame);
                    Log("\n");
                }
                
                FlipWallClock = OS_GetWallClock();
                
                FrameIdx += 1;
            }
        }
        
        P_Cleanup(PlatformContext);
        
    }
    
    return 0;
}
