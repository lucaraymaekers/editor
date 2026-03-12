#include "base/base.h"
//- OpenGL 
#include <GL/gl.h>
#include <GL/glx.h>
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, 
                                                     const int*);
//- X11 
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysymdef.h>
#include <X11/extensions/Xrandr.h>
#include <X11/cursorfont.h>

#define MWM_HINTS_DECORATIONS (1L << 1) 

typedef struct XMotifWMHints XMotifWMHints;
struct XMotifWMHints
{
    unsigned long flags;
    unsigned long functions; 
    unsigned long decorations;
    unsigned long input_mode;
};

//- Linux 
#include <sys/stat.h>
#include <signal.h>
#include <dlfcn.h>

#include "base/base_os_linux_x11_keysyms.c"

typedef struct linux_x11_context linux_x11_context;
struct linux_x11_context
{
    XImage *WindowImage;
    Display *DisplayHandle;
    Window WindowHandle;
    GC DefaultGC;
    XIC InputContext;
    Atom WM_DELETE_WINDOW;
    b32 OpenGLMode;
    b32 Initialized;
};

global_variable b32 *GlobalRunning;
global_variable b32 XCtxError;

//- Helpers 
static int XCtxErrorHandler(Display *dpy, XErrorEvent *ev)
{
    XCtxError = true;
    return 0;
}

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

// Check for extension string presence.  Adapted from: http://www.opengl.org/resources/features/OGLextensions/
internal b32
IsExtensionSupported(const char *ExtList, char *Extension)
{
    b32 Result = false;
    
    const char *Start;
    const char *Where, *Terminator;
    
    // Extension names should not have spaces.
    Where = strchr(Extension, ' ');
    if(!Where && *Extension != '\0')
    {    
        Start = ExtList;
        Where = strstr(Start, Extension);
        while(Where)
        {
            Terminator = Where + strlen(Extension);
            
            if(Where == Start || *(Where - 1) == ' ')
            {
                if(*Terminator == ' ' || *Terminator == '\0')
                {
                    Result = true;
                    break;
                }
                
                Start = Terminator;
            }
            
            Where = strstr(Start, Extension);
        }
    }
    
    return Result;
}

internal void 
LinuxSetSizeHint(Display *DisplayHandle, Window WindowHandle,
                 int MinWidth, int MinHeight,
                 int MaxWidth, int MaxHeight)
{
    XSizeHints Hints = {};
    if(MinWidth > 0 && MinHeight > 0) Hints.flags |= PMinSize;
    if(MaxWidth > 0 && MaxHeight > 0) Hints.flags |= PMaxSize;
    
    Hints.min_width = MinWidth;
    Hints.min_height = MinHeight;
    Hints.max_width = MaxWidth;
    Hints.max_height = MaxHeight;
    
    XSetWMNormalHints(DisplayHandle, WindowHandle, &Hints);
}

internal void
LinuxSigIntHandler(int Signal)
{
    // TODO(luca): Add the received signal as an event to a queue to unify the processing.
    *GlobalRunning = false;
}

internal void
LinuxShowCursor(linux_x11_context *Linux, b32 Entered)
{                
    if(Entered)
    {                            
        XColor black = {};
        char NoData[8] = {};
        
        Pixmap BitmapNoData = XCreateBitmapFromData(Linux->DisplayHandle, Linux->WindowHandle, NoData, 8, 8);
        Cursor InvisibleCursor = XCreatePixmapCursor(Linux->DisplayHandle, BitmapNoData, BitmapNoData, 
                                                     &black, &black, 0, 0);
        XDefineCursor(Linux->DisplayHandle, Linux->WindowHandle, InvisibleCursor);
        XFreeCursor(Linux->DisplayHandle, InvisibleCursor);
        XFreePixmap(Linux->DisplayHandle, BitmapNoData);
    }
    else
    {
        XUndefineCursor(Linux->DisplayHandle, Linux->WindowHandle);
    }
}


//~ Platform API
internal P_context
P_ContextInit(arena *Arena, app_offscreen_buffer *Buffer, b32 *Running)
{
#if EDITOR_FORCE_X11
    b32 OpenGLMode = false;
#else
    b32 OpenGLMode = true;
#endif
    
    P_context Result = 0;
    s32 XRet = 0;
    char *WindowName = "Handmade Window";
    
    linux_x11_context *Context = PushStruct(Arena, linux_x11_context);
    
    GlobalRunning = Running;
    signal(SIGINT, LinuxSigIntHandler);
    
    Display *DisplayHandle = XOpenDisplay(0);
    
    if(DisplayHandle)
    {
        Window RootWindow = XDefaultRootWindow(DisplayHandle);
        s32 ScreenHandle = XDefaultScreen(DisplayHandle);
        
        b32 Found = false;
        XVisualInfo WindowVisualInfo = {};
        GLXFBConfig FBConfig = {};
        
        if(!OpenGLMode)
        {
            s32 ScreenBitDepth = 24;
            Found = XMatchVisualInfo(DisplayHandle, ScreenHandle, ScreenBitDepth, TrueColor, &WindowVisualInfo);
        }
        
        if(OpenGLMode)
        {       
            int VisualAttributes[] =
            {
                GLX_X_RENDERABLE    , True,
                GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
                GLX_RENDER_TYPE     , GLX_RGBA_BIT,
                GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
                GLX_RED_SIZE        , 8,
                GLX_GREEN_SIZE      , 8,
                GLX_BLUE_SIZE       , 8,
                GLX_ALPHA_SIZE      , 8,
                GLX_DEPTH_SIZE      , 24,
                GLX_STENCIL_SIZE    , 8,
                GLX_DOUBLEBUFFER    , True,
                //GLX_SAMPLE_BUFFERS  , 1,
                //GLX_SAMPLES         , 4,
                None
            };
            
            int Major, Minor;
            b32 IsCorrectVersion = glXQueryVersion(DisplayHandle, &Major, &Minor);
            IsCorrectVersion |= (((Major == 1) && (Minor < 3)) || (Major < 1));
            
            if(IsCorrectVersion)
            {
                int FBConfigsCount;
                GLXFBConfig* FBConfigs = glXChooseFBConfig(DisplayHandle, ScreenHandle, VisualAttributes, &FBConfigsCount);
                
                if(FBConfigs)
                {
                    s32 BestFBIdx = -1, WorstFBIdx = -1, BestSamples = -1, WorstSamples = 999;
                    
                    for EachIndexType(s32, FBIdx, FBConfigsCount)
                    {
                        XVisualInfo *DisplayVisualInfo = glXGetVisualFromFBConfig(DisplayHandle, FBConfigs[FBIdx]);
                        if(DisplayVisualInfo)
                        {
                            int SampleBuffers, SamplesCount;
                            glXGetFBConfigAttrib(DisplayHandle, FBConfigs[FBIdx], GLX_SAMPLE_BUFFERS, &SampleBuffers);
                            glXGetFBConfigAttrib(DisplayHandle, FBConfigs[FBIdx], GLX_SAMPLES, &SamplesCount);
                            
#if 0
                            Log(" Matching FBConfigsonfig %d, DisplayVisualInfosual ID 0x%2x: "
                                "SAMPLE_BUFFERS = %d, SAMPLES = %d\n", 
                                FBIdx, DisplayVisualInfo->visualid, SampleBuffers, SamplesCount);
#endif
                            
                            if(BestFBIdx < 0 || (SampleBuffers && SamplesCount > BestSamples))
                            {
                                BestFBIdx = FBIdx;
                                BestSamples = SamplesCount;
                            }
                            if(WorstFBIdx < 0 || !SampleBuffers || SamplesCount < WorstSamples)
                            {
                                WorstFBIdx = FBIdx;
                                WorstSamples = SamplesCount;
                            }
                            XFree(DisplayVisualInfo);
                        }
                    }
                    
                    FBConfig = FBConfigs[BestFBIdx];
                    XFree(FBConfigs);
                    
                    XVisualInfo *Info = glXGetVisualFromFBConfig(DisplayHandle, FBConfig);
                    WindowVisualInfo = *Info;
                    XFree(Info);
                    
                    Found = true;
                }
                else
                {
                    TrapMsg("Failed to retrieve a framebuffer config");
                }
            }
            else
            {
                TrapMsg("Invalid GLX version");
            }
        }
        
        if(Found)
        {
            
            XSetWindowAttributes WindowAttributes = {};
            WindowAttributes.bit_gravity = StaticGravity;
#if EDITOR_INTERNAL            
            WindowAttributes.background_pixel = 0xFF00FF;
#endif
            WindowAttributes.colormap = XCreateColormap(DisplayHandle, RootWindow, WindowVisualInfo.visual, AllocNone);
            WindowAttributes.event_mask = (StructureNotifyMask | 
                                           KeyPressMask | KeyReleaseMask | 
                                           ButtonPressMask | ButtonReleaseMask |
                                           EnterWindowMask | LeaveWindowMask |
                                           FocusChangeMask |
                                           PointerMotionMask);
            
            u64 WindowAttributeMask = CWBitGravity | CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
            
            // TODO(luca): Query information
            s32 ScreenWidth = 1920;
            s32 ScreenHeight = 1080;
            
            s32 X = ScreenWidth - Buffer->Width; 
            s32 Y = 0; 
            
            // NOTE(luca): Align window with the right edge of the screen.
            Window WindowHandle = XCreateWindow(DisplayHandle, RootWindow,
                                                X, Y, Buffer->Width, Buffer->Height,
                                                0,
                                                WindowVisualInfo.depth, InputOutput,
                                                WindowVisualInfo.visual, WindowAttributeMask, &WindowAttributes);
            
            if(WindowHandle)
            {
                // Window attributes and hints
                {            
                    XRet = XStoreName(DisplayHandle, WindowHandle, WindowName);
                    
                    LinuxSetSizeHint(DisplayHandle, WindowHandle, Buffer->Width, Buffer->Height, Buffer->Width, Buffer->Height);
                    
                    // Disable window decorations
                    {
                        Atom MotifWMHintsAtom = XInternAtom(DisplayHandle, "_MOTIF_WM_HINTS", false);
                        if(MotifWMHintsAtom == None) 
                        {
                            ErrorLog("_MOTIF_WM_HINTS atom not supported by your WindowHandle manager.\n");
                            // Proceed anyway; WindowHandle manager will ignore the hint
                        }
                        
                        XMotifWMHints hints = 
                        {
                            .flags = MWM_HINTS_DECORATIONS,  
                            .decorations = 0                 
                        };
                        
                        XChangeProperty(DisplayHandle, WindowHandle,
                                        MotifWMHintsAtom, MotifWMHintsAtom,
                                        32,
                                        PropModeReplace,      
                                        (unsigned char *)&hints,  
                                        sizeof(hints) / sizeof(long));
                        
                    }
                    
                    // NOTE(luca): Tiling window managers should treat windows with the WM_TRANSIENT_FOR property as pop-up windows.  This way we ensure that we will be a floating window.  This works on my setup (dwm). 
                    XRet = XSetTransientForHint(DisplayHandle, WindowHandle, RootWindow);
                    
                    XClassHint ClassHint = {};
                    ClassHint.res_name = WindowName;
                    ClassHint.res_class = WindowName;
                    XSetClassHint(DisplayHandle, WindowHandle, &ClassHint);
                }
                
                // Input method
                {            
                    XSetLocaleModifiers("");
                    
                    XIM InputMethod = XOpenIM(DisplayHandle, 0, 0, 0);
                    if(!InputMethod){
                        XSetLocaleModifiers("@im=none");
                        InputMethod = XOpenIM(DisplayHandle, 0, 0, 0);
                    }
                    Context->InputContext = XCreateIC(InputMethod,
                                                      XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                                                      XNClientWindow, WindowHandle,
                                                      XNFocusWindow,  WindowHandle,
                                                      NULL);
                    XSetICFocus(Context->InputContext);
                }
                
                if(!OpenGLMode)
                {            
                    s32 BitsPerPixel = Buffer->BytesPerPixel*8;
                    Context->WindowImage = XCreateImage(DisplayHandle, WindowVisualInfo.visual, WindowVisualInfo.depth, ZPixmap, 0, (char *)Buffer->Pixels, Buffer->Width, Buffer->Height, BitsPerPixel, 0);
                }
                
                if(OpenGLMode)
                {
                    const char *GLXExtensions = glXQueryExtensionsString(DisplayHandle, ScreenHandle);
                    
                    // NOTE: It is not necessary to create or make current to a context before
                    // calling glXGetProcAddressARB
                    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
                    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
                        glXGetProcAddressARB((const GLubyte *) "glXCreateContextAttribsARB");
                    
                    GLXContext GLContext = 0;
                    
                    XCtxError = false;
                    int (*XOldErrorHandler)(Display*, XErrorEvent*) =
                        XSetErrorHandler(&XCtxErrorHandler);
                    
                    b32 ExtensionSupported = (IsExtensionSupported(GLXExtensions, "GLX_ARB_create_context") &&
                                              glXCreateContextAttribsARB);
                    if(ExtensionSupported)
                    {
                        s32 Attributes[] =
                        {
#if 1
                            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
                            GLX_CONTEXT_MINOR_VERSION_ARB, 3,
                            // Mask
                            GLX_CONTEXT_PROFILE_MASK_ARB, 0|GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
#else
                            GLX_CONTEXT_MAJOR_VERSION_ARB, 2,
                            GLX_CONTEXT_MINOR_VERSION_ARB, 0,
#endif
                            0,
                        };
                        
                        GLContext = glXCreateContextAttribsARB(DisplayHandle, FBConfig, 0, true, Attributes);
                        
                        // Sync to ensure any errors generated are processed.
                        XSync(DisplayHandle, False);
                        if(XCtxError || !GLContext)
                        {
                            // GLX_CONTEXT_MAJOR_VERSION_ARB = 1
                            // GLX_CONTEXT_MINOR_VERSION_ARB = 0
                            Attributes[1] = 1;
                            Attributes[3] = 0;
                            
                            XCtxError = false;
                            
                            ErrorLog("Failed to create GL 3.0 context, using old-style GLX 2.x context\n");
                            GLContext = glXCreateContextAttribsARB(DisplayHandle, FBConfig, 0, 
                                                                   True, Attributes);
                        }
                    }
                    else
                    {
                        // Use GLX 1.3 context creation method.
                        GLContext = glXCreateNewContext(DisplayHandle, FBConfig, GLX_RGBA_TYPE, 0, True);
                    }
                    
                    XSync(DisplayHandle, false);
                    XSetErrorHandler(XOldErrorHandler);
                    
                    if(GLContext && !XCtxError)
                    {
                        b32 IsGLXDirect = glXIsDirect(DisplayHandle, GLContext);
                        
                        glXMakeCurrent(DisplayHandle, WindowHandle, GLContext);
                    }
                    else
                    {
                        ErrorLog("Could not create OpenGL context.");
                    }
                }
                
                Context->DefaultGC = DefaultGC(DisplayHandle, ScreenHandle);
                
                Context->WM_DELETE_WINDOW = XInternAtom(DisplayHandle, "WM_DELETE_WINDOW", False);
                XRet = XSetWMProtocols(DisplayHandle, WindowHandle, 
                                       &Context->WM_DELETE_WINDOW, 1);
                Assert(XRet);
                
                Context->DisplayHandle = DisplayHandle;
                Context->WindowHandle = WindowHandle;
                Context->OpenGLMode = OpenGLMode;
                Context->Initialized = true;
                Result = (u64)Context;
            }
        }
        else
        {
            ErrorLog("Could not find matching visual info");
        }
        
    }
    
    return Result;
}

internal void 
P_ProcessMessages(P_context Context, app_input *Input, app_offscreen_buffer *Buffer, b32 *Running)
{
    linux_x11_context *Linux = (linux_x11_context *)Context;
    
	if(Linux)
	{
        
        {        
            XUndefineCursor(Linux->DisplayHandle, Linux->WindowHandle);
            switch(Input->PlatformCursor)
            {
                default:
                case PlatformCursorShape_Arrow:
                {
                    
                } break;
                case PlatformCursorShape_None:
                {
                    XColor black = {};
                    char NoData[8] = {};
                    
                    Pixmap BitmapNoData = XCreateBitmapFromData(Linux->DisplayHandle, Linux->WindowHandle, NoData, 8, 8);
                    Cursor InvisibleCursor = XCreatePixmapCursor(Linux->DisplayHandle, BitmapNoData, BitmapNoData, 
                                                                 &black, &black, 0, 0);
                    XDefineCursor(Linux->DisplayHandle, Linux->WindowHandle, InvisibleCursor);
                    XFreeCursor(Linux->DisplayHandle, InvisibleCursor);
                    XFreePixmap(Linux->DisplayHandle, BitmapNoData);
                } break;
                case PlatformCursorShape_ResizeHorizontal:
                {
                    Cursor vresize = XCreateFontCursor(Linux->DisplayHandle, XC_sb_h_double_arrow);
                    XDefineCursor(Linux->DisplayHandle, Linux->WindowHandle, vresize);
                } break;
                case PlatformCursorShape_ResizeVertical:
                {
                    Cursor vresize = XCreateFontCursor(Linux->DisplayHandle, XC_sb_v_double_arrow);
                    XDefineCursor(Linux->DisplayHandle, Linux->WindowHandle, vresize);
                } break;
            }
            
        }
        
        
        XEvent WindowEvent = {};
        while(XPending(Linux->DisplayHandle) > 0)
        {
            XNextEvent(Linux->DisplayHandle, &WindowEvent);
            b32 FilteredEvent = XFilterEvent(&WindowEvent, 0);
            if(FilteredEvent)
            {
                // TODO(luca): Logging
                // NOTE(luca): I really don't know what I should expect here.
            }
            
            switch(WindowEvent.type)
            {
                case KeyPress:
                case KeyRelease:
                {
                    KeySym Symbol = XLookupKeysym(&WindowEvent.xkey, 0);
                    b32 IsDown = (WindowEvent.type == KeyPress);
                    
                    b32 Alt = (WindowEvent.xkey.state & Mod1Mask);
                    b32 Shift  = (WindowEvent.xkey.state & ShiftMask);
                    b32 Control = (WindowEvent.xkey.state & ControlMask);
                    
                    //- How text input works 
                    // The needs:
                    //  1. Preserve game buttons, so that we can switch between a "game mode" or 
                    //     "text input mode".
                    //  2. Text input using the input method of the user which should allow for utf8 characters.
                    //  3. Hotkey support.  Eg. quickly navigating text.
                    // 3 will be supported by 2 for code reuse.
                    //
                    // We are going to send a buffer text button presses to the game layer, this solves these
                    // issues:
                    // - Pressing the same key multiple times in one frame.
                    // - Having modifiers be specific to each key press.
                    // - Not having to create a button record for each possible character in the structure.
                    // - Key events come in one at a time in the event loop, thus we need to have a buffer for
                    //   multiple keys pressed on a single frame.
                    //
                    // We store a count along the buffer and in the buffer we store the utf8 codepoint and its
                    // modifiers.
                    // The app code is responsible for traversing this buffer and applying the logic. 
                    
                    // The problem of input methods and hotkeys: 
                    // Basically the problem is that if we allow the input method and combo's that could be 
                    // filtered by the input method it won't seem consistent to the user.
                    // So we don't allow key bound to the input method to have an effect and we only pass key
                    // inputs that have not been filtered.
                    //
                    // In the platform layer we handle the special case were the input methods creates non-
                    // printable characters and we decompose those key inputs since non-printable characters
                    // have no use anymore.
                    
                    // Extra:
                    // - I refuse to check which keys bind to what modifiers. It's not important.
                    
                    // - Handy resources: 
                    //   - https://www.coderstool.com/unicode-text-converter
                    //   - man Compose(5).
                    //   - https://en.wikipedia.org/wiki/Control_key#History
                    
                    // Text input
                    {                    
                        // TODO(luca): Refresh mappings.
                        // NOTE(luca): Only KeyPress events  see man page of Xutf8LookupString().  And skip filtered events for text input, but keep them for controller.
                        if(IsDown && !FilteredEvent)
                        {
                            // TODO(luca): Choose a better error value.
                            rune Codepoint = L'\b';
                            u8 LookupBuffer[4] = {};
                            Status LookupStatus = {};
                            
                            app_text_button *Button = &Input->Text.Buffer[Input->Text.Count];
                            MemoryZero(Button);
                            
                            Input->Text.Count += 1;
                            
                            if(Shift)   Button->Modifiers |= PlatformKeyModifier_Shift;
                            if(Control) Button->Modifiers |= PlatformKeyModifier_Control;
                            if(Alt)     Button->Modifiers |= PlatformKeyModifier_Alt;
                            
                            s32 BytesLookepdUp = Xutf8LookupString(Linux->InputContext, &WindowEvent.xkey, 
                                                                   (char *)&LookupBuffer, ArrayCount(LookupBuffer), 
                                                                   0, &LookupStatus);
                            Assert(LookupStatus != XBufferOverflow);
                            Assert(BytesLookepdUp <= 4);
                            
                            b32 KeyHasLookup = ((LookupStatus != XLookupNone) && 
                                                (LookupStatus != XLookupKeySym));
                            
                            if(KeyHasLookup)
                            {
                                if(BytesLookepdUp)
                                {
                                    Assert(Input->Text.Count < ArrayCount(Input->Text.Buffer));
                                    
                                    Codepoint = ConvertUTF8StringToRune(LookupBuffer);
                                    
                                    b32 IsControlChar = (Control && BytesLookepdUp == 1 && 
                                                         (Codepoint < ' ' || Codepoint > '~'));
                                    if(IsControlChar)
                                    {
                                        Button->IsSymbol = true;
                                        if(Codepoint != 8 &&
                                           Codepoint != 127)
                                        {
                                            Button->IsSymbol = false;
                                            Codepoint = (rune)((u8)(Symbol - XK_a) + 'a');
                                        }
                                    }
                                    
                                    // NOTE(luca): Input methods might produce non printable characters (< ' '). 
                                    if(!Button->IsSymbol && (Codepoint >= ' ' || Codepoint < 0))
                                    {                            
                                        Button->Codepoint = Codepoint;
                                        
#if 0
                                        Log("%d bytes '%c' %d (%c|%c|%c)\n", 
                                            BytesLookepdUp, 
                                            (((char)Codepoint) >= ' ' ? (char)Codepoint : '\0'), Codepoint,
                                            ((Shift)   ? 'S' : ' '),
                                            ((Control) ? 'C' : ' '),
                                            ((Alt)     ? 'A' : ' '));
#endif
                                        
                                    }
                                    else
                                    {
                                        // NOTE(luca): Fallthrough in next if statement
                                        Button->IsSymbol = true;
                                    }
                                }
                                else
                                {
                                    InvalidPath();
                                }
                            }
                            
                            if(!KeyHasLookup || Button->IsSymbol)
                            {
                                Button->IsSymbol = true;
                                
                                if(0) {}
                                else if(Symbol == XK_Up     || Symbol == XK_KP_Up)     
                                    Button->Symbol = PlatformKey_Up;
                                else if(Symbol == XK_Left   || Symbol == XK_KP_Left)   
                                    Button->Symbol = PlatformKey_Left;
                                else if(Symbol == XK_Right  || Symbol == XK_KP_Right)  
                                    Button->Symbol = PlatformKey_Right;
                                else if(Symbol == XK_Down   || Symbol == XK_KP_Down)   
                                    Button->Symbol = PlatformKey_Down;
                                else if(Symbol == XK_Delete || Symbol == XK_KP_Delete) 
                                    Button->Symbol = PlatformKey_Delete;
                                else if(Symbol == XK_Home   || Symbol == XK_KP_Home)   
                                    Button->Symbol = PlatformKey_Home;
                                else if(Symbol == XK_End    || Symbol == XK_KP_End)     
                                    Button->Symbol = PlatformKey_End;
                                else if(Symbol == XK_Prior  || Symbol == XK_KP_Prior)  
                                    Button->Symbol = PlatformKey_PageUp;
                                else if(Symbol == XK_Next   || Symbol == XK_KP_Next)   
                                    Button->Symbol = PlatformKey_PageDown;
                                else if(Symbol == XK_Insert || Symbol == XK_KP_Insert)
                                    Button->Symbol = PlatformKey_Insert;
                                else if(Symbol == XK_Shift_L || Symbol == XK_Shift_R)
                                    Button->Symbol = PlatformKey_Shift;
                                else if(Symbol == XK_Control_L || Symbol == XK_Control_R)
                                    Button->Symbol = PlatformKey_Control;
                                else if(Symbol == XK_Alt_L || Symbol == XK_Alt_R)
                                    Button->Symbol = PlatformKey_Alt;
                                else if(Symbol == XK_Return || Symbol == XK_KP_Enter)
                                    Button->Symbol = PlatformKey_Return;
                                
                                else if(Symbol == XK_Escape)    Button->Symbol = PlatformKey_Escape;
                                else if(Symbol == XK_Tab)       Button->Symbol = PlatformKey_Tab;
                                else if(Symbol == XK_BackSpace) Button->Symbol = PlatformKey_BackSpace;
                                
                                else if(Symbol >= XK_F1 && Symbol <= XK_F12) 
                                {
                                    Button->Symbol = (platform_key)(PlatformKey_F1 + (Symbol - XK_F1));
                                }
                                
                                // NOTE(luca): Ignored
                                else if(Symbol == XK_KP_Begin || 
                                        Symbol == XK_Super_L ||
                                        Symbol == XK_Super_R || 
                                        Symbol == XK_Num_Lock ||
                                        Symbol == XK_Caps_Lock ||
                                        Symbol == XK_ISO_Level3_Shift) 
                                {
                                    Input->Text.Count -= 1;
                                }
                                
                                else
                                {
                                    Input->Text.Count -= 1;
                                    DebugBreakMsg("Unhandled special key(%lu): %s (Consider adding it to the if statement above)", Symbol, LinuxReturnStringForSymbol(Symbol));
                                }
                                
                            }
                        }
                    }
                    
                    // Debug keys
                    
                    if(IsDown)
                    {
                        if(0) {}
                        else if(Alt && (Symbol == XK_F2))
                        {
                            DebugBreak();
                        }
                        else if(Alt && (Symbol == XK_F3))
                        {
                            Trap();
                        }
                        else if(Alt && (Symbol == XK_F4))
                        {
                            *Running = false;
                        }
                    }
                    
                    // Game input
                    
                    if(0) {}
                    else if(Symbol == XK_Left)  ProcessKeyPress(&Input->ActionLeft, IsDown);
                    else if(Symbol == XK_Right) ProcessKeyPress(&Input->ActionRight, IsDown);
                    else if(Symbol == XK_Up)    ProcessKeyPress(&Input->ActionUp, IsDown);
                    else if(Symbol == XK_Down)  ProcessKeyPress(&Input->ActionDown, IsDown);
                    else if(Symbol == XK_a) ProcessKeyPress(&Input->MoveLeft, IsDown);
                    else if(Symbol == XK_s) ProcessKeyPress(&Input->MoveRight, IsDown);
                    else if(Symbol == XK_w) ProcessKeyPress(&Input->MoveUp, IsDown);
                    else if(Symbol == XK_r) ProcessKeyPress(&Input->MoveDown, IsDown);
                    
                } break;
                
                case ButtonPress:
                case ButtonRelease:
                {
                    b32 IsDown = (WindowEvent.type == ButtonPress);
                    u32 Button = WindowEvent.xbutton.button;
                    
                    if(0) {}
                    else if(Button == Button1)
                    {
                        ProcessKeyPress(&Input->MouseButtons[PlatformMouseButton_Left], IsDown);
                    }
                    else if(Button == Button2)
                    {
                        ProcessKeyPress(&Input->MouseButtons[PlatformMouseButton_Middle], IsDown);
                    }
                    else if(Button == Button3)
                    {
                        ProcessKeyPress(&Input->MouseButtons[PlatformMouseButton_Right], IsDown);
                    }
                    else if(Button == Button4)
                    {
                        ProcessKeyPress(&Input->MouseButtons[PlatformMouseButton_ScrollUp], IsDown);
                    }
                    else if(Button == Button5)
                    {
                        ProcessKeyPress(&Input->MouseButtons[PlatformMouseButton_ScrollDown], IsDown);
                    }
                } break;
                
                case MotionNotify:
                {
                    // TODO(luca): There can be multiple MotionNotify events per frame.  We should handle this if we want higher precision mouse movement.
                    XMotionEvent *Event = (XMotionEvent *)&WindowEvent;
                    if(Event->x >= 0 && Event->x < Buffer->Width &&
                       Event->y >= 0 && Event->y < Buffer->Height)
                    {                    
                        //Log("MotionNotify: %d,%d\n", Input->MouseX, Input->MouseY);
                        Input->MouseX = Event->x;
                        Input->MouseY = Event->y;
                    }
                    
                } break;
                
                case ConfigureNotify:
                {
                    XConfigureEvent *Event = (XConfigureEvent *)&WindowEvent;
                    // TODO(luca): Implement smooth resizing.
#if 0
                    Buffer->Width = Event->width;
                    Buffer->Height = Event->height;
                    Buffer->Pitch = Buffer->BytesPerPixel*Buffer->Width;
#endif
                    
                } break;
                
                case DestroyNotify:
                {
                    XDestroyWindowEvent *Event = (XDestroyWindowEvent *)&WindowEvent;
                    if(Event->window == Linux->WindowHandle)
                    {
                        *Running = false;
                    }
                } break;
                
                case ClientMessage:
                {
                    XClientMessageEvent *Event = (XClientMessageEvent *)&WindowEvent;
                    if((Atom)Event->data.l[0] == Linux->WM_DELETE_WINDOW)
                    {
                        XDestroyWindow(Linux->DisplayHandle, Linux->WindowHandle);
                        *Running = false;
                    }
                } break;
                
                // TODO(luca): Collapse with Enter- and LeaveNotify
                // TODO(luca): Check if window is focused at startup as well.
                case FocusIn:
                case FocusOut:
                {
                    XFocusChangeEvent *Event = (XFocusChangeEvent *)&WindowEvent;
                    b32 IsFocused = (Event->type == FocusIn); 
                    
                    s32 Mode = Event->mode;
                    
                    if(Mode == NotifyWhileGrabbed ||
                       Mode == NotifyNormal)
                    {
                        Input->PlatformWindowIsFocused = IsFocused;
                        
#if 0                        
                        // TODO(luca): If cursor is inside window
                        LinuxShowCursor(Linux, IsFocused);
#endif
                        
                    }
                    
                } break;
                
                case EnterNotify:
                case LeaveNotify:
                {
                    XCrossingEvent *Event = (XCrossingEvent *)&WindowEvent;
                    
                    b32 Entered = (Event->type = EnterNotify);
                    
#if 0                    
                    if(Input->WindowIsFocused)
                    {
                        LinuxShowCursor(Linux, Entered);
                    }
#endif
                    
                } break;
                
            }
        }
	}
}

internal void
P_UpdateImage(P_context Context, app_offscreen_buffer *Buffer)
{
    linux_x11_context *Linux = (linux_x11_context *)Context;
	if(Linux)
	{
        
        if(!Linux->OpenGLMode)
        {
            XPutImage(Linux->DisplayHandle,
                      Linux->WindowHandle, 
                      Linux->DefaultGC, 
                      Linux->WindowImage, 0, 0, 0, 0, 
                      Buffer->Width, 
                      Buffer->Height);
        }
        
        DoOnce
        {
            XMapWindow(Linux->DisplayHandle, Linux->WindowHandle);
            XFlush(Linux->DisplayHandle);
        }
        
        if(Linux->OpenGLMode)
        {
            glXSwapBuffers(Linux->DisplayHandle, Linux->WindowHandle);
        }
        
	}
}

internal void
P_LoadAppCode(arena *Arena, app_code *Code, app_memory *Memory)
{
	void *Library = (void *)(Code->LibraryHandle);
    struct stat Stats = {};
    stat(Code->LibraryPath, &Stats);
    u64 Size = Stats.st_size;
    s64 CurrentWriteTime = LinuxTimeSpecToNS(Stats.st_mtim);
    
    // NOTE(luca): Compilers will write twice to the executable for I know what reason, this
    // hack only detects the file as written to if it happened in over 100ms.
    s64 TimeDiff = (CurrentWriteTime - Code->LastWriteTime);  
    b32 WasWritten = (TimeDiff > (s64)(100 * 1000 * 1000));
    
    Memory->Reloaded = false;
    
    if(Size && (!Code->Loaded || WasWritten))
    {
        Code->LastWriteTime = CurrentWriteTime;
        
        StringsScratch = Arena;
        str8 TempDLLFileName = Str8Fmt("editor_app_temp_%lu.so", (u64)OS_GetWallClock());
        char *TempDLLPath = PathFromExe(Arena, Memory->ExeDirPath, TempDLLFileName);
        
        int File = open(Code->LibraryPath, O_RDONLY, 0600);
        if(File != -1)
        {            
            int TempFile = open(TempDLLPath, O_WRONLY|O_CREAT|O_TRUNC, 0700);
            smm BytesCopied = copy_file_range(File, 0, TempFile, 0, Size, 0);
            close(TempFile);
        }
        
        Library = dlopen(TempDLLPath, RTLD_NOW);
        if(Library)
        {
            // Load code from library
            Code->UpdateAndRender = (update_and_render *)dlsym(Library, "UpdateAndRender");
            if(Code->UpdateAndRender)
            {
                Code->Loaded = true;
                Memory->Reloaded = true;
                Code->LibraryHandle = (u64 )Library;
                Log("\nLibrary reloaded.\n");
            }
            else
            {
                Code->Loaded = false;
                ErrorLog("Could not find UpdateAndRender.");
            }
        }
        else
        {
            Code->Loaded = false;
            ErrorLog("%s", dlerror());
        }
    }
    
    if(!Code->Loaded)
    {
        Code->UpdateAndRender = UpdateAndRenderStub;
    }
}