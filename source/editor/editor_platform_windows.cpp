typedef struct win32_context win32_context;
struct win32_context
{
    HWND Window;
    HDC OwnDC;
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

internal P_context 
P_ContextInit(arena *Arena, app_offscreen_buffer *Buffer, b32 *Running)
{
    P_context Result = {};
    
    win32_context *Context = PushStruct(Arena, win32_context);
    
    GlobalCursor = IDC_ARROW;
    GlobalRunning = Running;
    
    HINSTANCE Instance = GetModuleHandle(0);
    
    WNDCLASSA WindowClass = {};
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
                                      "Handmade Hero",
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
            
            PIXELFORMATDESCRIPTOR PixelFormat = {};
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
    win32_context *Win32Context = (win32_context *)Context;
    
    Input->PlatformWindowIsFocused = GlobalWindowIsFocused;
    
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
    
    if(Win32Context)
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
                    u32 VKCode = (u32)Message.wParam;
                    u32 ScanCode = (Message.lParam >> 16) & 0xFF;
                    
                    b32 WasDown = ((Message.lParam & (1 << 30)) != 0);
                    b32 IsDown = ((Message.lParam & (1 << 31)) == 0);
                    
                    if(IsDown)
                    {
                        b32 Shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                        b32 Ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                        b32 Alt = (Message.lParam & (1 << 29));
                        
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
                        *Button = {};
                        Input->Text.Count += 1;
                        
                        if(Alt)   Button->Modifiers |= PlatformKeyModifier_Alt;
                        if(Shift) Button->Modifiers |= PlatformKeyModifier_Shift;
                        if(Ctrl)  Button->Modifiers |= PlatformKeyModifier_Control;
                        
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
                                char KeyName[64] = {};
                                GetKeyNameTextA(Message.lParam, KeyName, sizeof(KeyName));
                                Log("Unhandled key(%d): %s\n", VKCode, KeyName);
                                Input->Text.Count -= 1;
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
            ScreenToClient(Win32Context->Window, &MouseP);
            Input->MouseX = MouseP.x;
            Input->MouseY = MouseP.y;
            // TODO(luca): Support mousewheel
            Input->MouseZ = 0; 
            
            ProcessKeyPress(&Input->MouseButtons[PlatformMouseButton_Left], GetKeyState(VK_LBUTTON) & (1 << 15));
            ProcessKeyPress(&Input->MouseButtons[PlatformMouseButton_Middle], GetKeyState(VK_MBUTTON) & (1 << 15));
            ProcessKeyPress(&Input->MouseButtons[PlatformMouseButton_Right], GetKeyState(VK_RBUTTON) & (1 << 15));
            ProcessKeyPress(&Input->MouseButtons[PlatformMouseButton_ScrollUp], GetKeyState(VK_XBUTTON1) & (1 << 15));
            ProcessKeyPress(&Input->MouseButtons[PlatformMouseButton_ScrollDown], GetKeyState(VK_XBUTTON2) & (1 << 15));
        }
        
    }
}

internal void
P_LoadAppCode(arena *Arena, app_code *Code, app_memory *Memory)
{
    HMODULE Library = (HMODULE)Code->LibraryHandle;
    
    char *LockFileName = PathFromExe(Arena, Memory->ExeDirPath, S8("lock.tmp"));
    StringsScratch = Arena;
    str8 TempDLLFileName = Str8Fmt("editor_app_temp_%lu.dll", (u64)OS_GetWallClock());
    
    char *TempDLLPath = PathFromExe(Arena, Memory->ExeDirPath, TempDLLFileName);
    
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
            Code->LastWriteTime = WriteTime;
            
            if(Library)
            {
                Code->Loaded = false;
                Code->LibraryHandle = 0;
            }
            
            b32 Result = CopyFile(Code->LibraryPath, TempDLLPath, FALSE);
            if(!Result)
            {
                DebugBreak();
                Win32LogIfError();
            }
            
            Library = LoadLibraryA(TempDLLPath);
            if(Library)
            {
                Code->UpdateAndRender = (update_and_render *)GetProcAddress(Library, "UpdateAndRender");
                if(Code->UpdateAndRender)
                {
                    Code->Loaded = true;
                    Memory->Reloaded = true;
                    Code->LibraryHandle = (u64)Library;
                    Log("\nLibrary reloaded.\n");
                }
                else
                {
                    Code->Loaded = false;
                    ErrorLog("Could not find UpdateAndRender.\n");
                }
            }
            else
            {
                Code->Loaded = false;
                ErrorLog("Could not open library.\n");
            }
        }
    }
    
    if(!Code->Loaded)
    {
        Code->UpdateAndRender = UpdateAndRenderStub;
    }
}
