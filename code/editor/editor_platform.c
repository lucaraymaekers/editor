//~ Libraries 
#if EDITOR_INTERNAL
# define BASE_CONSOLE_APPLICATION 0
#endif
#include "base/base.h"
#include "base/base.c"

#include "editor/editor_platform.h"
#if OS_LINUX
# include "editor_platform_linux.c"
#elif OS_WINDOWS
# include "editor_platform_windows.c"
#endif

//- Third party 
#if OS_WINDOWS
# define RADDBG_MARKUP_IMPLEMENTATION
#else
# define RADDBG_MARKUP_STUBS
#endif
#include "lib/raddbg_markup.h"

//-

C_LINKAGE ENTRY_POINT(EntryPoint)
{
    if(LaneIndex() == 0)
    {
        OS_ProfileInit("S");
        
        u64 PlatformMemorySize = GB(4);
        u64 AppMemorySize = GB(1);
        
        // NOTE(luca): Total memory also for game.
        arena *PermanentArena = ArenaAlloc(.Size = PlatformMemorySize, .Offset = TB(2));
        arena *FrameArena = ArenaAlloc();
        
        OS_ProfileAndPrint("Memory");
        
        b32 *Running = PushStruct(PermanentArena, b32);
        *Running = true;
        
        app_offscreen_buffer Buffer = {0};
        #if EDITOR_FORCE_SMALL_RESOLUTION
        Buffer.Width = 1920/2;
        Buffer.Height = 1080/2;
#else
        Buffer.Width = 1920;
        Buffer.Height = 1080;
        #endif
        Buffer.BytesPerPixel = 4;
        Buffer.Pitch = Buffer.BytesPerPixel*Buffer.Width;
        Buffer.Pixels = PushArray(PermanentArena, u8, (u64)(Buffer.Pitch*Buffer.Height));
        
        P_context PlatformContext = P_ContextInit(PermanentArena, &Buffer, Running);
        
        OS_ProfileAndPrint("Context");
        
        if(!PlatformContext)
        {
            ErrorLog("Could not initialize graphical context, running in headless mode.");
        }
        
        str8 ExeDirPath = {0};
        {        
            u64 OnePastLastSlash = 0;
#if OS_LINUX || OS_ANDROID
            char *FileName = Params->Args[0];
            u64 SizeOfFileName = StringLength(FileName);
#elif OS_WINDOWS
            char *FileName = PushArray(PermanentArena, char, 1024);
            u64 SizeOfFileName = GetModuleFileNameA(0, FileName, 1024);
            Win32LogIfError();
#else
# error "OS not supported" 
#endif
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
#if EDITOR_INTERNAL
        AppMemory.IsDebuggerAttached = GlobalDebuggerIsAttached;
#endif
#if OS_WINDOWS
        AppMemory.PerfCountFrequency = GlobalPerfCountFrequency;
#endif
        
        app_input _Input[2] = {0};
        app_input *NewInput = &_Input[0];
        app_input *OldInput = &_Input[1];
        
        f64 LastCounter = OS_GetWallClock();
        f64 FlipWallClock = LastCounter;
#if EDITOR_FORCE_UPDATE_HZ
        f32 GameUpdateHz = EDITOR_FORCE_UPDATE_HZ;
#else
        f32 GameUpdateHz = 144.0f;
#endif
        f32 TargetSecondsPerFrame = 1.0f/GameUpdateHz; 
        
        app_code Code = ZeroStruct;
        
#if OS_LINUX || OS_ANDROID        
        str8 LibraryPath = S8("editor_app.so");
#else
        str8 LibraryPath = S8("editor_app.dll");
#endif
        Code.LibraryPath = PathFromExe(PermanentArena, AppMemory.ExeDirPath, LibraryPath);
        
        b32 Paused = false;
        b32 IsRecording = false;
        b32 IsPlaying = false;
        b32 Logging = false;
        
        u64 MaxRecordingFramesCount = (u64)GameUpdateHz * 60 * 10;
        u64 RecordingBufferMaxSize = AppMemory.MemorySize + (MaxRecordingFramesCount*sizeof(app_input));
        void *RecordingBuffer = PushArray(PermanentArena, u8, RecordingBufferMaxSize);
        u64 RecordingBufferSize = 0;
        u64 RecordingBufferPos = 0;
        char *RecordingPath = PathFromExe(FrameArena, AppMemory.ExeDirPath, S8("loop_edit.edi"));
        
#if 1
        RecordingPath = "loop_edit.edi";
#endif
        
        OS_ProfileAndPrint("Misc");
        
        OS_ProfileInit("P");
        while(*Running)
        {
            Scratch(FrameArena)
            {
                OS_ProfileAndPrint("InitSetup");
                
                P_LoadAppCode(FrameArena, &Code, &AppMemory);
                OS_ProfileAndPrint("Code");
                
                NewInput->PlatformWindowIsFocused = OldInput->PlatformWindowIsFocused;
                
                // Input
                { 
                    NewInput->Text.Count = 0;
                    for EachElement(Idx, NewInput->Mouse.Buttons)
                    {
                        NewInput->Mouse.Buttons[Idx].EndedDown = OldInput->Mouse.Buttons[Idx].EndedDown;
                        NewInput->Mouse.Buttons[Idx].HalfTransitionCount = 0;
                        NewInput->Mouse.Buttons[Idx].Modifiers = 0;
                    }
                    for EachElement(Idx, NewInput->GameButtons)
                    {
                        NewInput->GameButtons[Idx].EndedDown = OldInput->GameButtons[Idx].EndedDown;
                        NewInput->GameButtons[Idx].HalfTransitionCount = 0;
                        NewInput->GameButtons[Idx].Modifiers = 0;
                    }
                    NewInput->dtForFrame = TargetSecondsPerFrame;
                    NewInput->PlatformCursor = OldInput->PlatformCursor;
                    NewInput->PlatformSetClipboard = OldInput->PlatformSetClipboard;
                    
                    NewInput->PlatformClipboard = OldInput->PlatformClipboard;
                    NewInput->PlatformSetClipboard = OldInput->PlatformSetClipboard;
                    NewInput->Mouse.X = OldInput->Mouse.X;
                    NewInput->Mouse.Y = OldInput->Mouse.Y;
                    NewInput->Mouse.StartX = OldInput->Mouse.StartX;
                    NewInput->Mouse.StartY = OldInput->Mouse.StartY;
                    
                    P_ProcessMessages(PlatformContext, NewInput, &Buffer, Running);
                    
                    if(WasPressed(NewInput->Mouse.Buttons[PlatformMouseButton_Left]))
                    {                        
                        NewInput->Mouse.StartX = NewInput->Mouse.X;
                        NewInput->Mouse.StartY = NewInput->Mouse.Y;
                    }
                }
                
                OS_ProfileAndPrint("Messages");
                
                if(CharPressed(NewInput, 'p', PlatformKeyModifier_Alt)) 
                {
                    Paused = !Paused;
                    Log("%s\n", (Paused) ? "Paused" : "Unpaused");
                }
                
                if(CharPressed(NewInput, 'p', PlatformKeyModifier_Alt | PlatformKeyModifier_Shift)) 
                {
                    GlobalIsProfiling = !GlobalIsProfiling;
                }
                
                if(CharPressed(NewInput, 'g', PlatformKeyModifier_Alt)) 
                {
                    Logging = !Logging;
                }
                
                // Save/load state
                {                
                    if(CharPressed(NewInput, 's', PlatformKeyModifier_Alt))
                    {
                        char *Path = PathFromExe(FrameArena, AppMemory.ExeDirPath, S8("save.editor_state"));
                        
                        str8 Buffer = {0};
                        Buffer.Data = (u8 *)AppMemory.Memory;
                        Buffer.Size = AppMemory.MemorySize;
                        
                        OS_WriteEntireFile(Path, Buffer);
                        Log("Saved state.\n");
                    }
                    
                    if(CharPressed(NewInput, 's', PlatformKeyModifier_Alt | PlatformKeyModifier_Shift))
                    {
                        char *Path = PathFromExe(FrameArena, AppMemory.ExeDirPath, S8("save.editor_state"));
                        str8 Buffer = OS_ReadEntireFileIntoMemory(Path);
                        
                        AppMemory.MemorySize = Buffer.Size;
                        
                        MemoryCopy(AppMemory.Memory, Buffer.Data, Buffer.Size);
                        OS_FreeFileMemory(Buffer);
                        Log("Loaded state.\n");
                    }
                }
                
                // Playback
                {
                    if(!Paused)
                    {
                        if(CharPressed(NewInput, 'l', PlatformKeyModifier_Alt))
                        {
                            if(0) {}
                            else if(!IsRecording && !IsPlaying)
                            {
                                // StartRecording()
                                {
                                    RecordingBufferSize = 0;
                                    MemoryCopy(RecordingBuffer, AppMemory.Memory, AppMemory.MemorySize);
                                    RecordingBufferSize += AppMemory.MemorySize;
                                    
                                    IsRecording = true;
                                }
                            }
                            else if(IsRecording)
                            {
                                // EndRecording()
                                {
                                    IsRecording = false;
                                }
                                
                                // StartPlaying()
                                {
                                    RecordingBufferPos = 0;
                                    MemoryCopy(AppMemory.Memory, RecordingBuffer, AppMemory.MemorySize);
                                    RecordingBufferPos += AppMemory.MemorySize;
                                    
                                    IsPlaying = true;
                                }
                            }
                            else if(IsPlaying)
                            {
                                // StopPlaying()
                                {
                                    IsPlaying = false;
                                }
                            }
                            else
                            {
                                InvalidPath();
                            }
                            Log("Playing/Recording %d/%d\n", IsPlaying, IsRecording);
                        }
                        
                        Assert((IsRecording != IsPlaying) ||
                               (!IsPlaying  && !IsPlaying));
                        
                        if(IsRecording)
                        {
                            MemoryCopy((u8 *)RecordingBuffer + RecordingBufferSize, NewInput, sizeof(*NewInput));
                            RecordingBufferSize += sizeof(*NewInput);
                            Assert(RecordingBufferSize < RecordingBufferMaxSize);
                        }
                        
                        if(IsPlaying)
                        {
                            MemoryCopy(NewInput, (u8 *)RecordingBuffer + RecordingBufferPos, sizeof(*NewInput));
                            RecordingBufferPos += sizeof(*NewInput);
                            
                            if(RecordingBufferPos >= RecordingBufferSize)
                            {
                                Log("Replay end\n\n");
                                
                                // StartPlaying()
                                {
                                    RecordingBufferPos = 0;
                                    MemoryCopy(AppMemory.Memory, RecordingBuffer, AppMemory.MemorySize);
                                    RecordingBufferPos += AppMemory.MemorySize;
                                }
                            }
                        }
                        
                        // NOTE(luca): Need to be overwritten.
                        NewInput->PlatformIsRecording = IsRecording;
                        NewInput->PlatformIsPlaying = IsPlaying;
                    }
                    
                    AppMemory.IsProfiling = GlobalIsProfiling;
                    
                    b32 ShouldQuit = Code.UpdateAndRender(ThreadContext, &AppMemory, &Buffer, NewInput);
                    // NOTE(luca): Since UpdateAndRender can take some time, there could have been a signal sent to INT the app.
                    ReadWriteBarrier;
                    *Running = *Running && !ShouldQuit;
                }
                
                OS_ProfileAndPrint("UpdateAndRender");
                
                P_UpdateImage(PlatformContext, &Buffer);
                
                OS_ProfileAndPrint("UpdateImage");
                
                f64 WorkCounter = OS_GetWallClock();
                f64 WorkMSPerFrame = OS_MSElapsed(LastCounter, WorkCounter);
                // Sleep
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
                        
                        f64 TestSecondsElapsedForFrame = OS_SecondsElapsed(LastCounter, OS_GetWallClock());
                        if(TestSecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            // TODO(luca): Log missed sleep
                        }
                        
                        // NOTE(luca): This is to help against sleep granularity.
                        while(SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            SecondsElapsedForFrame = OS_SecondsElapsed(LastCounter, OS_GetWallClock());
                        }
                    }
                    else
                    {
                        // TODO(luca): Log missed frame rate!
                    }
                    
                    LastCounter = OS_GetWallClock();
                }
                
                NewInput->Text.Buffer[NewInput->Text.Count].Codepoint = 0;
                
                Swap(OldInput, NewInput);
                
                OS_ProfileAndPrint("Sleep");
                
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
            }
        }
    }
    
    return 0;
}
