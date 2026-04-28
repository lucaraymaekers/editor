#include <mmsystem.h>
#include "muze/muze_midi.h"

//~ Midi
typedef struct app_midi_notes_queue app_midi_notes_queue;
struct app_midi_notes_queue
{
    app_midi_note *Notes;
    u64 Size;
    u64 ReadPos;
    u64 WritePos;
};

global_variable app_midi_notes_queue *NotesQueue = 0;

#define MIDI_LogIfError(Code, In) MIDI_LogIfError_(Code, In, __FILE__, __LINE__)

internal b32 
MIDI_LogIfError_(MMRESULT Code, b32 In,
                 char *File, s32 Line)
{
    b32 Result = false;
    
    if(Code != MMSYSERR_NOERROR)
    {
        char *ErrorText = PushArrayZero(FrameArena, char, 256);
        if(In) 
        {
            midiInGetErrorTextA(Code, ErrorText, 256);
        }
        else
        {
            midiOutGetErrorTextA(Code, ErrorText, 256);
        }
        
        Log(ERROR_FMT "%s\n", File, Line, ErrorText);
        
        Result = true;
    }
    
    return Result;
}

void CALLBACK
MIDI_InCallback(HMIDIIN Device, UINT Msg, DWORD_PTR Instance, DWORD_PTR Param1, DWORD_PTR Param2)
{
    if(0) {}
    else if(Msg == MIM_DATA)
    {
        u64 WritePos = (NotesQueue->WritePos % NotesQueue->Size);
        
        union { u32 U32[1]; u8 U8[4]; } Message = {0};
        
#if 0            
        if(Param1 != 254) DebugBreak();
#endif
        
        Message.U32[0] = Param1;
        
        u8 Status = Message.U8[0];
        u8 Data1  = Message.U8[1];
        u8 Data2  = Message.U8[2];
        if(0 && Status != 0xFE)
        {            
            Log("Note(%lu): %u\n", WritePos, Data1);
        }
        
        app_midi_note *Note = NotesQueue->Notes + WritePos;
        NotesQueue->WritePos += 1;
        Note->Timestamp = (f32)OS_GetWallClock();
        Note->Message = Param1;
    }
}

//~ Helpers 

typedef struct win32_context win32_context;
struct win32_context
{
    HWND Window;
    HDC OwnDC;
    u8 ClipboardBuffer[KB(64)];
};

global_variable b32 *GlobalRunning;
global_variable LPSTR GlobalCursor;
global_variable s32 GlobalBufferWidth;
global_variable s32 GlobalBufferHeight;
global_variable b32 GlobalWindowIsFocused;

internal rune
ConvertUTF8StringToRune(u8 UTF8String[4])
{
    rune Codepoint = 0;
    
    if((UTF8String[0] & 0x80) == 0x00)
    {
        Codepoint = UTF8String[0];
    }
    else if((UTF8String[0] & 0xE0) == 0xC0)
    {
        Codepoint = (((UTF8String[0] & 0x1F) << 6*1) |
                     ((UTF8String[1] & 0x3F) << 6*0));
    }
    else if((UTF8String[0] & 0xF0) == 0xE0)
    {
        Codepoint = (((UTF8String[0] & 0x0F) << 6*2) |
                     ((UTF8String[1] & 0x3F) << 6*1) |
                     ((UTF8String[2] & 0x3F) << 6*0));
    }
    else if((UTF8String[0] & 0xF8) == 0xF8)
    {
        Codepoint = (((UTF8String[0] & 0x0E) << 6*3) |
                     ((UTF8String[1] & 0x3F) << 6*2) |
                     ((UTF8String[2] & 0x3F) << 6*1) |
                     ((UTF8String[3] & 0x3F) << 6*0));
    }
    else
    {
        Assert(0);
    }
    
    return Codepoint;
}

internal LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
                        UINT Message,
                        WPARAM WParam,
                        LPARAM LParam)
{       
    LRESULT Result = 0;
    
    switch(Message)
    {
        case WM_SETFOCUS:
        {
            GlobalWindowIsFocused = true;
        } break;
        case WM_KILLFOCUS:
        {
            GlobalWindowIsFocused = false;
        } break;
        
        case WM_CLOSE:
        {
            // TODO(casey): Handle this with a message to the user?
            *GlobalRunning = false;
        } break;
        
        case WM_SIZE:
        {
#if 0
            GlobalBufferWidth = LOWORD(LParam);
            GlobalBufferHeight = HIWORD(LParam);
#endif
            
        } break;
        
        case WM_SETCURSOR:
        {
            if(GlobalCursor)
            {
                Result = DefWindowProcA(Window, Message, WParam, LParam);
                
                SetCursor(LoadCursorA(0, GlobalCursor));
            }
            else
            {
                SetCursor(0);
            }
        } break;
        
        case WM_DESTROY:
        {
            // TODO(casey): Handle this as an error - recreate window?
            *GlobalRunning = false;
        } break;
        
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            TrapMsg("Keyboard input came in through a non-dispatch message!");
        } break;
        
        default:
        {
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } break;
    }
    
    return Result;
}

//~ MIDI API
global_variable HMIDIIN SelectedIn = 0;
global_variable u32 SelectedInPort = 0;
global_variable b32 SelectedInOpened = false;

global_variable HMIDIOUT SelectedOut = 0;
global_variable u32 SelectedOutPort = 0;
global_variable b32 SelectedOutOpened = false;

PLATFORM_MIDI_GET_DEVICES(P_MIDIGetDevices)
{
    platform_midi_get_devices_result Result = {0};
    
    u64 MaxDevicesCount = 128;
    Result.Devices = PushArray(FrameArena, platform_midi_device, MaxDevicesCount);
    
    u64 OutDevicesCount = midiOutGetNumDevs();
    for EachIndex(Idx, OutDevicesCount)
    {    
        platform_midi_device *Device = Result.Devices + Result.Count;
        Result.Count += 1;
        
        MIDIOUTCAPS Caps;
        MMRESULT Code = midiOutGetDevCaps(Idx, &Caps, sizeof(Caps));
        
        if(!MIDI_LogIfError(Code, false))
        {
            Device->Name = Str8Fmt("%s", Caps.szPname);
            Device->Id = Idx;
        }
        
        Device->IsOutput = true;
    }
    
    u64 InDevicesCount = midiInGetNumDevs();
    for EachIndex(Idx, InDevicesCount)
    {        
        platform_midi_device *Device = Result.Devices + Result.Count;
        Result.Count += 1;
        
        MIDIINCAPS Caps;
        MMRESULT Code = midiInGetDevCaps(Idx, &Caps, sizeof(Caps));
        if(!MIDI_LogIfError(Code, true))
        {
            Device->Name = Str8Fmt("%s", Caps.szPname);
            Device->Id = Idx;
        }
        
        Device->IsOutput = false;
    }
    
    return Result;
}

PLATFORM_MIDI_SEND(P_MIDISend) 
{
    Assert(Device.IsOutput);
    
    if(Device.Id != SelectedOutPort)
    {
        if(SelectedOutOpened)
        {
            MMRESULT Code = midiOutReset(SelectedOut);
            MIDI_LogIfError(Code, false);
            
            Code = midiOutClose(SelectedOut);
            MIDI_LogIfError(Code, false);
            
            SelectedOutOpened = false;
        }
        
        SelectedOutPort = Device.Id;
    }
    
    if(!SelectedOutOpened)
    {
        MMRESULT Code = midiOutOpen(&SelectedOut, Device.Id, 0, 0, CALLBACK_NULL);
        MIDI_LogIfError(Code, false);
        SelectedOutOpened = true;
    }
    
    // TODO(luca): More checks
    midi_message MessageEvent = {Message};
    u8 Channel = MessageEvent.U8[0] & 0x0F;
    Assert(Channel < 16);
    
    MMRESULT Code = midiOutShortMsg(SelectedOut, Message);
    MIDI_LogIfError(Code, false);
}

PLATFORM_MIDI_LISTEN(P_MIDIListen)
{
    Assert(!Device.IsOutput);
    
    if(Device.Id != SelectedInPort)
    {
        if(SelectedInOpened)
        {
            MMRESULT Code = midiInReset(SelectedIn);
            MIDI_LogIfError(Code, true);
            Code = midiInClose(SelectedIn);
            MIDI_LogIfError(Code, true);
            
            SelectedInOpened = false;
        }
        
        SelectedInPort = Device.Id;
    }
    
    if(!SelectedInOpened)
    {
        MMRESULT Code = midiInOpen(&SelectedIn, SelectedInPort, (DWORD_PTR)MIDI_InCallback, 0, CALLBACK_FUNCTION);
        if(!MIDI_LogIfError(Code, true)) 
        {
            midiInStart(SelectedIn);
        }
        SelectedInOpened = true;
    }
}

#if 0
#define PLATFORM_MIDI_CLOSE(Name) void Name(platform_midi_device Device)
PLATFORM_MIDI_CLOSE(P_MIDIClose) 
{
    {
        if(!Device.IsOutput)
        {    
            if(SelectedInOpened)
            {
                midiInStop(SelectedIn);
                midiInReset(SelectedIn);
                midiInClose(SelectedIn);
                SelectedInOpened = false;
            }
        }
        else
        {
            if(SelectedOutOpened)
            {
                midiOutReset(SelectedOut);
                midiOutClose(SelectedOut);
                SelectedOutOpened = false;
            }
        }
    }
}
#endif

//~ Platform API

internal P_context 
P_Init(arena *Arena, app_offscreen_buffer *Buffer, b32 *Running)
{
    P_context Result = {0};
    
    NotesQueue = PushStruct(Arena, app_midi_notes_queue);
    NotesQueue->Size = KB(1);
    NotesQueue->Notes = PushArray(Arena, app_midi_note, NotesQueue->Size);
    
    win32_context *Context = PushStruct(Arena, win32_context);
    
    GlobalCursor = IDC_ARROW;
    GlobalRunning = Running;
    
    HINSTANCE Instance = GetModuleHandle(0);
    
    WNDCLASSA WindowClass = {0};
    WindowClass.style = CS_HREDRAW|CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
    //    WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";
    if(RegisterClassA(&WindowClass))
    {
        RECT WindowRect = { 0, 0, Buffer->Width, Buffer->Height };
        DWORD Style = WS_VISIBLE | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
        
        AdjustWindowRect(&WindowRect, Style, FALSE);
        
        int WindowWidth  = WindowRect.right - WindowRect.left;
        int WindowHeight = WindowRect.bottom - WindowRect.top;
        
        
        
        HWND Window = CreateWindowExA(
                                      0,
                                      WindowClass.lpszClassName,
                                      "Muze",
                                      Style,
                                      0,
                                      0,
                                      WindowWidth,
                                      WindowHeight,
                                      0,
                                      0,
                                      Instance,
                                      0);
        if(Window)
        {
            HDC OwnDC = GetDC(Window);
            
            int Win32RefreshRate = GetDeviceCaps(OwnDC, VREFRESH);
            
            PIXELFORMATDESCRIPTOR PixelFormat = {0};
            PixelFormat.nSize = sizeof(PixelFormat);
            PixelFormat.nVersion = 1;
            PixelFormat.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
            PixelFormat.iPixelType = PFD_TYPE_RGBA;
            PixelFormat.cColorBits = 32;
            PixelFormat.cDepthBits = 24;
            PixelFormat.cStencilBits = 8;
            PixelFormat.iLayerType = PFD_MAIN_PLANE;
            
            int ChosenFormat = ChoosePixelFormat(OwnDC, &PixelFormat);
            SetPixelFormat(OwnDC, ChosenFormat, &PixelFormat);
            
            HGLRC GLContext = wglCreateContext(OwnDC);
            wglMakeCurrent(OwnDC, GLContext);
            
            Context->OwnDC = OwnDC;
            Context->Window = Window;
            
            Result = (umm)Context;
        }
    }
    
    return Result;
}

internal void      
P_UpdateImage(P_context Context, app_offscreen_buffer *Buffer)
{
    win32_context *Win32 = (win32_context *)Context; 
    if(Win32)
    {
        SwapBuffers(Win32->OwnDC);
    }
}

internal void      
P_ProcessMessages(P_context Context, app_input *Input, app_offscreen_buffer *Buffer, b32 *Running)
{
    win32_context *Win32 = (win32_context *)Context;
    
    // Process MIDI notes queue
    {
        u64 ReadPos = (NotesQueue->ReadPos%NotesQueue->Size);
        u64 ItemsInQueueCount = (NotesQueue->WritePos - NotesQueue->ReadPos); 
        Input->MIDI.Count = ItemsInQueueCount;
        Input->MIDI.Notes = NotesQueue->Notes + ReadPos;
        NotesQueue->ReadPos += ItemsInQueueCount;
    }
    
    Input->PlatformWindowIsFocused = GlobalWindowIsFocused;
    
    // Clipboard handling
    if(!Input->PlatformSetClipboard)
    {    
        Input->PlatformClipboard.Data = Win32->ClipboardBuffer;
        if(OpenClipboard(0) == 0)
        {
            Win32LogIfError();
        }
        
        if(GetClipboardOwner() != Win32->Window)
        {
            HANDLE ClipboardData = GetClipboardData(CF_TEXT);
            if(ClipboardData)
            {
                // NOTE(luca): Minus null terminating character
                u64 Size = GlobalSize(ClipboardData);
                
                char *Mem = (char*)GlobalLock(ClipboardData);
                if(Mem)
                {
                    // NOTE(luca): Getting the length and looping over the string could be done faster.
                    str8 Clip = S8FromCString(Mem);
                    
                    Assert(Size < ArrayCount(Win32->ClipboardBuffer));
                    
                    MemoryCopy(Input->PlatformClipboard.Data, Mem, Size);
                    Input->PlatformClipboard.Size = Size - 1;
                    
                    GlobalUnlock(ClipboardData);
                }
                else
                {
                    ErrorLog("Failed to lock global memory\n");
                    CloseClipboard();
                }
                
            }
            else
            {
                //ErrorLog("No text data in clipboard\n");
            }
        }
        else
        {
            Input->PlatformSetClipboard = false;
            
            Assert(Input->PlatformClipboard.Size < ArrayCount(Win32->ClipboardBuffer));
            
            EmptyClipboard();
            
            u64 Len = Input->PlatformClipboard.Size;
            
            HGLOBAL W32GlobalMemory = GlobalAlloc(GMEM_MOVEABLE, Len + 1);
            
            if(W32GlobalMemory)
            {
                u8 *Mem = (u8 *)GlobalLock(W32GlobalMemory);
                MemoryCopy(Mem, Input->PlatformClipboard.Data, Input->PlatformClipboard.Size);
                Mem[Len] = 0;
                GlobalUnlock(W32GlobalMemory);
                
                if(SetClipboardData(CF_TEXT, W32GlobalMemory) == 0)
                {
                    Win32LogIfError();
                }
                
            }
            else
            {
                ErrorLog("GlobalAlloc failed\n");
            }
            
            MemoryZero(&Input->PlatformSetClipboard);
        }
        
        CloseClipboard();
    }
    
    switch(Input->PlatformCursor)
    {
        default:
        case PlatformCursorShape_Arrow:
        {
            GlobalCursor = IDC_ARROW;
        } break;
        case PlatformCursorShape_None:
        {
            GlobalCursor = 0;
        } break;
        case PlatformCursorShape_Grab:
        {
            GlobalCursor = IDC_HAND;
        } break;
        case PlatformCursorShape_ResizeHorizontal:
        {
            GlobalCursor = IDC_SIZEWE;
        } break;
        case PlatformCursorShape_ResizeVertical:
        {
            GlobalCursor = IDC_SIZENS;
        } break;
    }
    
    if(Win32)
    {    
        MSG Message;
        while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            switch(Message.message)
            {
                case WM_QUIT:
                {
                    *GlobalRunning = false;
                } break;
                
                case WM_SYSKEYDOWN:
                case WM_SYSKEYUP:
                case WM_KEYDOWN:
                case WM_KEYUP:
                {
                    s32 VKCode = Message.wParam;
                    s32 ScanCode = (Message.lParam >> 16) & 0xFF;
                    
                    b32 WasDown = ((Message.lParam & (1 << 30)) != 0);
                    b32 IsDown = ((Message.lParam & (1 << 31)) == 0);
                    
                    // Raw input
                    {                        
                        if(WasDown != IsDown)
                        {
                            if(0) {}
                            else if(VKCode == VK_UP)
                            {
                                ProcessKeyPress(&Input->ActionUp, IsDown);
                            }
                            else if(VKCode == VK_LEFT)
                            {
                                ProcessKeyPress(&Input->ActionLeft, IsDown);
                            }
                            else if(VKCode == VK_DOWN)
                            {
                                ProcessKeyPress(&Input->ActionDown, IsDown);
                            }
                            else if(VKCode == VK_RIGHT)
                            {
                                ProcessKeyPress(&Input->ActionRight, IsDown);
                            }
                            
                            // TODO(luca): Metaprogram
#if defined(MUZE_COLEMAK)
                            uint Symbols[] = { 'A', 'W', 'R', 'F', 'S', 'T', 'G', 'D', 'J', 'H', 'L', 'N', 'E', 'Y', 'I', };
#else
                            uint Symbols[] = { 'A', 'W', 'S', 'E', 'D', 'F', 'T', 'G', 'Y', 'H', 'U', 'J', 'K', 'I', 'L', };
#endif
                            for EachElement(Idx, Symbols)
                            {
                                if(VKCode == Symbols[Idx]) ProcessKeyPress(&Input->MIDI.Buttons[Idx], IsDown);
                            }
                            
                            
                        }
                    }
                    
                    if(IsDown)
                    {
                        b32 Shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                        b32 Ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                        b32 Alt = (GetKeyState(VK_MENU) & 0x800) != 0;
                        
                        if((VKCode == VK_F4) && Alt)
                        {
                            *GlobalRunning = false;
                        }
                        
                        if((VKCode == VK_F2) && Alt)
                        {
                            DebugBreak();
                        }
                        
                        if((VKCode == VK_F3) && Alt)
                        {
                            Trap();
                        }
                        
                        app_text_button *Button = &Input->Text.Buffer[Input->Text.Count];
                        *Button = (app_text_button){0};
                        Input->Text.Count += 1;
                        
                        if(Alt)   Button->Modifiers |= PlatformKeyModifier_Alt;
                        if(Shift) Button->Modifiers |= PlatformKeyModifier_Shift;
                        if(Ctrl)  Button->Modifiers |= PlatformKeyModifier_Control;
                        
                        // Text input
                        {
                            // Try to convert to Unicode character
                            BYTE KeyboardState[256];
                            GetKeyboardState(KeyboardState);
                            
                            WCHAR UnicodeBuffer[4] = {0};
                            int CharCount = ToUnicode(VKCode, ScanCode, KeyboardState, UnicodeBuffer, 4, 0);
                            
                            if(CharCount > 0)
                            {
                                rune Codepoint = (rune)UnicodeBuffer[0];
                                if(IsPrintable(Codepoint))
                                {
                                    Button->Codepoint = Codepoint;
                                }
                                else if (Codepoint > 0 && IsPrintable(VKCode))
                                {
                                    // NOTE(luca): VKCodes are uppercase by default
                                    if(IsAlpha(VKCode)) VKCode += 32;
                                    Button->Codepoint = VKCode;
                                }
                                else
                                {
                                    Button->IsSymbol = true;
                                    if(0) {}
                                    else if(Codepoint == '\b' || Codepoint == 127) Button->Symbol = PlatformKey_BackSpace;
                                    else if(Codepoint == '\t') Button->Symbol = PlatformKey_Tab;
                                    else if(Codepoint == 27) Button->Symbol = PlatformKey_Escape;
                                    else if(Codepoint == 13) Button->Symbol = PlatformKey_Return; 
                                    else 
                                    {
                                        Input->Text.Count -= 1;
                                        // Not implemented
                                        Log("Unhandled codepoint: %s\n", Codepoint);
                                    };
                                }
                            }
                            else
                            {
                                Button->IsSymbol = true;
                                if(0) {}
                                else if(VKCode == VK_UP) Button->Symbol = PlatformKey_Up;
                                else if(VKCode == VK_DOWN) Button->Symbol = PlatformKey_Down;
                                else if(VKCode == VK_LEFT) Button->Symbol = PlatformKey_Left;
                                else if(VKCode == VK_RIGHT) Button->Symbol = PlatformKey_Right;
                                else if(VKCode == VK_CONTROL) Button->Symbol = PlatformKey_Control;
                                else if(VKCode == VK_SHIFT) Button->Symbol = PlatformKey_Shift;
                                else if(VKCode == VK_DELETE) Button->Symbol = PlatformKey_Delete;
                                else if(VKCode == VK_MENU) Button->Symbol = PlatformKey_Alt;
                                else if(VKCode == VK_OEM_MINUS) 
                                {
                                    Button->IsSymbol = false;
                                    Button->Codepoint = L'-';
                                }
                                else if(VKCode == VK_OEM_COMMA)
                                {
                                    Button->IsSymbol = false;
                                    Button->Codepoint = L',';
                                }
                                else if(IsPrintable(VKCode))
                                {
                                    Button->IsSymbol = false;
                                    if(IsAlpha(VKCode)) VKCode += 32;
                                    Button->Codepoint = VKCode;
                                }
                                else
                                {
                                    char KeyName[64] = {0};
                                    GetKeyNameTextA(Message.lParam, KeyName, sizeof(KeyName));
                                    Log("Unhandled key(%d): %s\n", VKCode, KeyName);
                                    Input->Text.Count -= 1;
                                }
                            }
                        }
                    }
                    
                } break;
                
                default:
                {
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                } break;
            }
        }
        
#if 0       
        // TODO(luca): Does not work when window is minimized.
        Buffer->Width = GlobalBufferWidth;
        Buffer->Height = GlobalBufferHeight;
#endif
        
        // Mouse
        {        
            POINT MouseP;
            GetCursorPos(&MouseP);
            ScreenToClient(Win32->Window, &MouseP);
            Input->Mouse.X = MouseP.x;
            Input->Mouse.Y = MouseP.y;
            // TODO(luca): Support mousewheel
            Input->Mouse.Z = 0; 
            
            ProcessKeyPress(&Input->Mouse.Buttons[PlatformMouseButton_Left], GetKeyState(VK_LBUTTON) & (1 << 15));
            ProcessKeyPress(&Input->Mouse.Buttons[PlatformMouseButton_Middle], GetKeyState(VK_MBUTTON) & (1 << 15));
            ProcessKeyPress(&Input->Mouse.Buttons[PlatformMouseButton_Right], GetKeyState(VK_RBUTTON) & (1 << 15));
            ProcessKeyPress(&Input->Mouse.Buttons[PlatformMouseButton_ScrollUp], GetKeyState(VK_XBUTTON1) & (1 << 15));
            ProcessKeyPress(&Input->Mouse.Buttons[PlatformMouseButton_ScrollDown], GetKeyState(VK_XBUTTON2) & (1 << 15));
        }
        
    }
}

internal void
P_LoadAppCode(arena *Arena, app_code *Code, app_memory *Memory)
{
    // TODO(luca): Make a report about this tricking RAD debugger.
    b32 KeepOldDLLsAllocated = false;
    
    HMODULE Library = (HMODULE)Code->LibraryHandle;
    
    char *LockFileName = PathFromExe(Arena, S8("lock.tmp"));
    StringsScratch = Arena;
    str8 TempDLLFileName = Str8Fmt("editor_app_temp_%lu.dll", (u64)OS_GetWallClock());
    
    char *TempDLLPath = PathFromExe(Arena, TempDLLFileName);
    
    WIN32_FILE_ATTRIBUTE_DATA Data;
    
    WIN32_FILE_ATTRIBUTE_DATA Ignored;
    if(!GetFileAttributesEx(LockFileName, GetFileExInfoStandard, &Ignored))
    {
        s64 WriteTime = Code->LastWriteTime;
        if(GetFileAttributesEx(Code->LibraryPath, GetFileExInfoStandard, &Data))
        {
            WriteTime = (s64)((((u64)Data.ftLastWriteTime.dwHighDateTime & 0xFFFF) << 32) | 
                              (((u64)Data.ftLastWriteTime.dwLowDateTime & 0xFFFF) << 0));
        }
        
        if(Code->LastWriteTime != WriteTime)
        {
            if(Library)
            {
                Code->Loaded = false;
                Code->LibraryHandle = 0;
                if(!KeepOldDLLsAllocated)
                {
                    FreeLibrary(Library);
                }
            }
            
            b32 Result = CopyFile(Code->LibraryPath, TempDLLPath, FALSE);
            if(!Result)
            {
                Win32LogIfError();
            }
            
            Library = LoadLibraryA(TempDLLPath);
            if(Library)
            {
                Code->UpdateAndRender = (update_and_render *)GetProcAddress(Library, "UpdateAndRender");
                if(Code->UpdateAndRender)
                {
                    Code->LastWriteTime = WriteTime;
                    
                    Code->Loaded = true;
                    Code->LibraryHandle = (u64)Library;
                    Log("\nLibrary reloaded.\n");
                }
                else
                {
                    Code->Loaded = false;
                    ErrorLog("Could not find UpdateAndRender.");
                }
                Memory->Reloaded = true;
            }
            else
            {
                Code->Loaded = false;
                Code->LibraryHandle = 0;
                ErrorLog("Could not open library.\n");
            }
        }
    }
    
    if(!Code->Loaded)
    {
        Code->UpdateAndRender = UpdateAndRenderStub;
    }
}
