#define BASE_EXTERNAL_LIBS 1
#define BASE_NO_ENTRYPOINT 1
#include "base/base.h"
#include "base/base.c"
#include "editor_math.h"
#include "editor_platform.h"
#include "editor_font.h"
#include "editor_random.h"
#include "editor_libs.h"
#include "editor_gl.h"
#include "editor_ui.h"
#include "editor_app.h"

internal app_offscreen_buffer
LoadImage(arena *Arena, str8 ExeDirPath, str8 Path)
{
    app_offscreen_buffer Result = {};
    
    char *FilePath = PathFromExe(Arena, ExeDirPath, Path);
    str8 File = OS_ReadEntireFileIntoMemory(FilePath);
    if(File.Size)
    {
        s32 Width, Height, Components;
        s32 BytesPerPixel = 4;
        u8 *Image = stbi_load_from_memory(File.Data, (int)File.Size, &Width, &Height, &Components, BytesPerPixel);
        Assert(Components == BytesPerPixel);
        
        Result.Width = Width;
        Result.Height = Height;
        Result.BytesPerPixel = BytesPerPixel;
        Result.Pitch = Result.BytesPerPixel*Width;
        Result.Pixels = Image;
    }
    
    return Result;
}

internal inline b32
InRange(f32 P, f32 Min, f32 Max, f32 Size)
{
    b32 Result = ((P >= Min        && P < Min + Size) ||
                  (P >= Max - Size && P < Max));
    
    return Result;
}

internal inline u32 *
GetPixel(app_offscreen_buffer *Buffer, s32 X, s32 Y)
{
    u32 *Pixel = (u32 *)(Buffer->Pixels + Buffer->BytesPerPixel*(Y*Buffer->Width + X));
    return Pixel;
}

void
AppendChar(app_state *App, rune Codepoint)
{
    if (Codepoint == ' ')
    {
        // NOTE(nasr): do we check the space here?
    }
    App->Text[App->TextCount] = Codepoint;
    App->TextCount += 1;
    App->CursorPos += 1;
    Assert(App->TextCount < ArrayCount(App->Text));
}

void
DeleteChar(app_state *App)
{
    if(App->TextCount)
    {
        App->TextCount -= 1;
        App->CursorPos -= 1;
    }
}

internal inline b32
EqualsWithEpsilon(f32 A, f32 B, f32 Epsilon)
{
    b32 Result = (A < (B + Epsilon) && 
                  A > (B - Epsilon));
    return Result;
}

void
InitAtlas(arena *Arena, app_font *Font, font_atlas *Atlas, f32 HeightPx)
{
    Atlas->FirstCodepoint = ' ';
    // NOTE(luca): 2 bytes =>small alphabets.
    Atlas->CodepointsCount = ((0xFFFF - 1) - Atlas->FirstCodepoint);
    
    Atlas->Width = 1024;
    Atlas->Height = 1024;
    u32 Size = (Atlas->Width*Atlas->Height);
    
    Arena->Pos = 0;
    Atlas->Data = PushArray(Arena, u8, Size);
    Atlas->PackedChars = PushArray(Arena, stbtt_packedchar, Size);
    Atlas->AlignedQuads = PushArray(Arena, stbtt_aligned_quad, Size);
    
    Atlas->HeightPx = HeightPx;
    Atlas->FontScale = stbtt_ScaleForPixelHeight(&Font->Info, HeightPx);
    Atlas->Font = Font;
    
    stbtt_pack_context ctx;
    {
        stbtt_PackBegin(&ctx, 
                        Atlas->Data,
                        Atlas->Width, Atlas->Height,
                        0, 1, 0);
        
        stbtt_PackFontRange(&ctx, Font->Info.data, 0, HeightPx, 
                            Atlas->FirstCodepoint, Atlas->CodepointsCount, 
                            Atlas->PackedChars);
        stbtt_PackEnd(&ctx);
    }
    
    for EachIndex(Idx, Atlas->CodepointsCount)
    {
        float UnusedX, UnusedY;
        stbtt_GetPackedQuad(Atlas->PackedChars, Atlas->Width, Atlas->Height, 
                            (u32)Idx,
                            &UnusedX, &UnusedY,
                            &Atlas->AlignedQuads[Idx], 0);
    }
    
}

internal rect_instance *
DrawRect(rect Dest, v4 Color, f32 CornerRadius, f32 BorderThickness, f32 Softness)
{
    rect_instance *Result = GlobalRectsInstances + GlobalRectsCount;
    GlobalRectsCount += 1;
    
    MemoryZero(Result);
    
    Result->Dest = Dest;
    Result->Color0 = Color;
    Result->Color1 = Color;
    Result->Color2 = Color;
    Result->Color3 = Color;
    Result->CornerRadii.e[0] = CornerRadius;
    Result->CornerRadii.e[1] = CornerRadius;
    Result->CornerRadii.e[2] = CornerRadius;
    Result->CornerRadii.e[3] = CornerRadius;
    Result->Border = BorderThickness;
    Result->Softness = Softness;
    
    return Result;
}

internal rect_instance *
DrawRectChar(font_atlas *Atlas, v2 Pos, rune Codepoint, v4 Color)
{
    rect_instance *Result = 0;
    rect Dest = {};
    
    // NOTE(luca): Evereything happens in pixel coordinates in here.
    
    rune CharIdx = Codepoint - Atlas->FirstCodepoint;
    stbtt_packedchar *PackedChar = &Atlas->PackedChars[CharIdx];
    stbtt_aligned_quad *Quad = &Atlas->AlignedQuads[CharIdx];
    f32 Width = (PackedChar->x1 - PackedChar->x0);
    f32 Height = (PackedChar->y1 - PackedChar->y0);
    {    
        v2 Min = Pos;
        Min.X += (PackedChar->xoff);
        f32 Baseline = GetBaseline(Atlas->Font, Atlas->FontScale);
        f32 Descent = (f32)Atlas->Font->Descent*Atlas->FontScale;
        Min.Y += (PackedChar->yoff) + Baseline + Descent;
        
        v2 Max = V2(Min.X + Width, Min.Y + Height);
        
        Result = DrawRect(Dest, Color, 0.f, 0.f, 0.f);
        
        Result->Dest = Rect(Min.X, Min.Y, Max.X, Max.Y);
        Result->TexSrc = Rect(Quad->s0, Quad->t0, Quad->s1, Quad->t1);
    }
    
    Result->HasTexture = true;
    
#if 0    
    DrawRect(Result->Dest, V4(0.f, 0.f, 1.f, 1.f), 0.f, 1.f, 0.f);
#endif
    
    return Result;
}

#include "editor_ui.cpp"


internal void
gl_InitState(gl_render_state *Render)
{
    glGenVertexArrays(ArrayCount(Render->VAOs), &Render->VAOs[0]);
    glGenTextures(ArrayCount(Render->Textures), &Render->Textures[0]);
    glGenBuffers(ArrayCount(Render->VBOs), &Render->VBOs[0]);
    glBindVertexArray(Render->VAOs[0]);
    glBindBuffer(GL_ARRAY_BUFFER, Render->VBOs[0]);
    
}

internal void
gl_CleanupState(gl_render_state *Render)
{
    glDeleteVertexArrays(ArrayCount(Render->VAOs), &Render->VAOs[0]);
    glDeleteTextures(ArrayCount(Render->Textures), &Render->Textures[0]);
    glDeleteBuffers(ArrayCount(Render->VBOs), &Render->VBOs[0]);
    glDeleteProgram(Render->RectShader);
}

//- Panels 
read_only global_variable panel _NilPanel =
{
    &_NilPanel, 
    &_NilPanel,
    &_NilPanel,
    &_NilPanel,
    &_NilPanel,
};
global_variable panel *NilPanel = &_NilPanel;

internal b32
IsNilPanel(panel *Panel)
{
    b32 Result = (!Panel || Panel == NilPanel);
    return Result;
}

global_variable arena *PanelArena = 0;
global_variable b32 PanelAppendToParent = false;
global_variable panel *PanelCurrent = 0;
global_variable panel *PanelRoot = 0;
global_variable app_state *PanelApp = 0;
global_variable app_input *PanelInput = 0;

global_variable u32 PanelDebugIndentation = 0;
global_variable u32 PanelDebugLevel = 0;
global_variable v4 PanelDebugColors[] = {Color_Red, Color_Green, Color_Orange, Color_Magenta, Color_Yellow, Color_Blue};

global_variable axis2_stack_node *PanelAxisTop = 0; 

internal panel *
PanelAdd(f32 ParentPct)
{
    panel *New = PushStructZero(PanelArena, panel);
    
    New->First = New->Last = New->Next = New->Prev = New->Parent = NilPanel;
    
    New->ParentPct = ParentPct;
    New->Axis = PanelAxisTop->Value;
    
    if(!IsNilPanel(PanelCurrent))
    {            
        if(PanelAppendToParent)
        {
            PanelCurrent->First = New;
            New->Parent = PanelCurrent;
        }
        else
        {
            PanelCurrent->Next = New;
            New->Prev = PanelCurrent;
            New->Parent = PanelCurrent->Parent;
        }
        
        New->Parent->Last = New;
    }
    
    PanelCurrent = New;
    PanelAppendToParent = false;
    
    return New;
}

internal void
PanelPush()
{
    PanelAppendToParent = true;
}

internal void
PanelPop(void)
{
    PanelCurrent = PanelCurrent->Parent;
}

internal void PanelPushAxis(axis2 Axis) { StackPush(FrameArena, axis2_stack_node, Axis, PanelAxisTop); }
internal void PanelPopAxis() { StackPop(PanelAxisTop); }



#define PanelGroup() DeferLoop(PanelPush(), PanelPop())
#define PanelAxis(Axis) DeferLoop(PanelPushAxis(Axis), PanelPopAxis())

internal void
PanelDebugPrint(panel *Panel)
{
    Log("%*s %.2f %u:\n"
        "%*s  (%.0f,%.0f)\n"
        "%*s  (%.0f,%.0f)\n",
        PanelDebugIndentation, "", Panel->ParentPct, Panel->Axis,
        PanelDebugIndentation, "", V2Arg(Panel->Region.Min),
        PanelDebugIndentation, "", V2Arg(Panel->Region.Max));
    
    if(!IsNilPanel(Panel->First))
    {
        PanelDebugIndentation += 1;
        PanelDebugPrint(Panel->First);
        PanelDebugIndentation -= 1;
    }
    
    if(!IsNilPanel(Panel->Next))
    {
        PanelDebugPrint(Panel->Next);
    }
}

internal void
PanelGetRegion(panel *Panel, rect FreeRegion)
{
    panel *Parent = Panel->Parent;
    s32 Axis = Panel->Parent->Axis;
    s32 OtherAxis = 1 - Axis;
    
    v2 Pos = FreeRegion.Min;
    v2 Size = {};
    
    f32 ParentSize = (Parent->Region.Max.e[Axis] - Parent->Region.Min.e[Axis]);
    f32 OtherSize = (FreeRegion.Max.e[OtherAxis] - FreeRegion.Min.e[OtherAxis]);
    
    Size.e[Axis] = (!IsNilPanel(Parent) ?
                    (Panel->ParentPct*ParentSize) :
                    (FreeRegion.Max.e[Axis] - FreeRegion.Min.e[Axis]));
    Size.e[OtherAxis] = OtherSize;
    
    Panel->Region = RectFromSize(Pos, Size);
    
    if(!IsNilPanel(Panel->First))
    {
        PanelDebugLevel += 1;
        PanelGetRegion(Panel->First, Panel->Region);
        PanelDebugLevel -= 1;
    }
    
    if(!IsNilPanel(Panel->Next))
    {
        FreeRegion.Min.e[Axis] = Panel->Region.Max.e[Axis];
        PanelGetRegion(Panel->Next, FreeRegion);
    }
    
    v2 MouseP = V2S32(PanelInput->MouseX, PanelInput->MouseY);
    
    f32 BorderSize = 2.f;
    
    // NOTE(luca): This happens post-order so that the borders are overlaid correctly.
    if(!IsNilPanel(Panel->Next) && !Panel->DisableInteraction)
    {
        rect Border = Panel->Region;
        Border.Min.e[Axis] = Panel->Region.Max.e[Axis] - BorderSize;
        
        // TODO(luca): Only one border should be dragged at a time, this should be taken care of by an input queue
        local_persist b32 IsDragging = false;
        local_persist v2 OldMouseP = {};
        
        // TODO(luca): Make this actually good.
#if 1        
        b32 MouseIsDown = PanelInput->MouseButtons[PlatformMouseButton_Left].EndedDown;
        b32 MouseWasPressed = WasPressed(PanelInput->MouseButtons[PlatformMouseButton_Left]);
        
        if(!MouseIsDown)
        {
            IsDragging = false;
        }
        
        v4 BorderColor = Color_Blue;
        
        if(IsInsideRecV2(MouseP, Border) && !IsDragging)
        {
            BorderColor = Color_Cyan;
            
            if(MouseWasPressed)
            {
                IsDragging = true;
                OldMouseP = MouseP;
                PanelApp->SelectedPanel = Panel;
            }
        }
        
        if(IsDragging && Panel == PanelApp->SelectedPanel)
        {
            BorderColor = Color_Yellow;
            
            f32 dP = (OldMouseP.e[Axis] - MouseP.e[Axis]);
            f32 ParentOtherSize = (Parent->Region.Max.e[OtherAxis] - Parent->Region.Min.e[OtherAxis]);
            f32 dPPct = (dP/ParentSize);
            
            panel *Next = Panel->Next;
            
            Panel->ParentPct = Clamp(0.05f, Panel->ParentPct - dPPct, .95f);
            Next->ParentPct  = Clamp(0.05f, Next->ParentPct  + dPPct, .95f);
            
            OldMouseP = MouseP;
        }
        
        Panel->Region.Max.e[Axis] -= BorderSize;
#endif
        
        DrawRect(Border, BorderColor, 0.f, 0.f, 0.f);
    }
}

//- 

C_LINKAGE
UPDATE_AND_RENDER(UpdateAndRender)
{
    b32 ShouldQuit = false;
    
#if EDITOR_INTERNAL
    GlobalDebuggerIsAttached = Memory->IsDebuggerAttached;
#endif
#if OS_WINDOWS
    GlobalPerfCountFrequency = Memory->PerfCountFrequency;
#endif
    GlobalIsProfiling = Memory->IsProfiling;
    
    OS_ProfileInit(" G");
    
    arena *PermanentArena;
    app_state *App;
    {    
        PermanentArena = (arena *)Memory->Memory;
        PermanentArena->Size = Memory->MemorySize - sizeof(arena);
        PermanentArena->Base = (u8 *)Memory->Memory + sizeof(arena);
        PermanentArena->Pos = 0;
        AsanPoisonMemoryRegion(PermanentArena->Base, PermanentArena->Size);
        
        FrameArena = PushArena(PermanentArena, Memory->MemorySize/2);
        
        App = PushStruct(PermanentArena, app_state);
    }
    
    font_atlas *Atlas = &App->FontAtlas;
    
    // NOTE(luca): Will be rerun when reloaded.
    local_persist s32 GLADVersion = 0;
    if(!Memory->Initialized || Memory->Reloaded)
    {
        GLADVersion = gladLoaderLoadGL();
        OS_ProfileAndPrint("glad Init");
    }
    
    OS_ProfileAndPrint("Init");
    
    f32 WindowBorderSize = 2.f;
    v2 BufferDim = V2S32(Buffer->Width, Buffer->Height);
    v2 OldMouseP = V2S32(OldInput->MouseX, OldInput->MouseY);
    v2 MouseP = V2S32(Input->MouseX, Input->MouseY);
    
    if(!Memory->Initialized)
    {
        char *FontPath = PathFromExe(FrameArena, Memory->ExeDirPath, S8("../data/font_regular.ttf"));
        InitFont(&App->Font, FontPath);
        App->FontAtlasArena = PushArena(PermanentArena, MB(100));
        App->PreviousHeightPx = DefaultHeightPx + 1.0f;
        App->HeightPx = DefaultHeightPx;
        
        App->TextCount = 0;
        App->CursorPos = 0;
        
        App->UIBoxTableSize = 4096;
        App->UIBoxTable = PushArray(PermanentArena, ui_box, App->UIBoxTableSize);
        App->UIBoxArena = PushArena(PermanentArena, 256*sizeof(ui_box));
        
        App->TrackerForUI_NilBox = &_UI_NilBox;
        App->TrackerForNilPanel = &_NilPanel;
        
        gl_InitState(&App->Render);
        
        // Panels
        {        
            f32 TopSplitPct = (App->HeightPx + 2.f*2.f)/(BufferDim.Y - 2.f*WindowBorderSize);
            
            PanelArena = App->PanelArena = ArenaAlloc();
            PanelAxis(Axis2_Y) PanelGroup()
            {
                panel *RootPanel = PanelAdd(1.f);
                
                PanelAxis(Axis2_X) PanelGroup()
                {
                    panel *TitlebarPanel = PanelAdd(TopSplitPct);
                    App->TitlebarPanel = TitlebarPanel;
                    TitlebarPanel->DisableInteraction = true;
                    
                    panel *AppPanel      = PanelAdd(1.f - TopSplitPct);
                    PanelAxis(Axis2_Y) PanelGroup()
                    {
                        panel *LeftPanel = PanelAdd(.4f);
                        PanelGroup()
                        {            
                            panel *TopLeftPanel    = PanelAdd(.33f);
                            panel *MiddleLeftPanel = PanelAdd(.33f);
                            panel *BottomLeftPanel = PanelAdd(.34f);
                        }
                        panel *MiddlePanel = PanelAdd(.2f);
                        panel *RightPanel  = PanelAdd(.4f);
                        PanelGroup()
                        {            
                            panel *TopRightPanel    = PanelAdd(.6f);
                            panel *BottomRightPanel = PanelAdd(.4f);
                        }
                    }
                }
                App->FirstPanel = RootPanel;
                App->LastPanel = RootPanel;
            }
        }
        
        Memory->Initialized = true;
        OS_ProfileAndPrint("Memory Init");
    }
    
    UI_NilBox = App->TrackerForUI_NilBox;
    NilPanel = App->TrackerForNilPanel;
    
    PanelArena = App->PanelArena;
    PanelInput = Input;
    PanelApp = App;
    
    // Text input
    for EachIndex(Idx, Input->Text.Count)
    {
        app_text_button Key = Input->Text.Buffer[Idx];
        
        if(!Key.IsSymbol)
        {
            // Ignore shift.
            u32 ModifiersShiftIgnored = (Key.Modifiers & ~PlatformKeyModifier_Shift);
            
            if(Key.Modifiers & PlatformKeyModifier_Alt && Key.Codepoint == 'b')
            {
                DebugBreak();
            }
            
            if(ModifiersShiftIgnored == PlatformKeyModifier_Control)
            {
                if(0) {}
                else if(Key.Codepoint == 'h')
                {
                    DeleteChar(App);
                }
                else if(Key.Codepoint == 's')
                {
                    str8 File = PushS8(FrameArena, App->TextCount);
                    for EachIndex(Idx, App->TextCount)
                    {
                        File.Data[Idx] = (u8)App->Text[Idx];
                    }
                    
                    char *OutPath = PathFromExe(FrameArena, Memory->ExeDirPath, S8("file.c"));
                    OS_WriteEntireFile(OutPath, File);
                    
                    Log("Saved.\n");
                }
                else if(Key.Codepoint == '+')
                {
                    App->HeightPx += 1.f;
                }
                else if(Key.Codepoint == '-')
                {
                    App->HeightPx -= 1.f;
                }
                else if(Key.Codepoint == '0')
                {
                    App->HeightPx = DefaultHeightPx;
                }
            }
            
            if(ModifiersShiftIgnored == PlatformKeyModifier_None)
            {
                AppendChar(App, Key.Codepoint);
            }
        }
        else
        {
            if(0) {}
            else if(Key.Codepoint == PlatformKey_BackSpace)
            {
                DeleteChar(App);
            }
            else if(Key.Codepoint == PlatformKey_Return)
            {
                AppendChar(App, L'\n');
            }
            else if(Key.Codepoint == PlatformKey_Escape)
            {
                ShouldQuit = true;
            }
        }
    }
    
    OS_ProfileAndPrint("Input");
    
    glViewport(0, 0, Buffer->Width, Buffer->Height);
    OS_ProfileAndPrint("GL setup");
    
    v4 WindowBorderColor;
    if(0) {}
    else if(Input->PlatformIsRecording) WindowBorderColor = Color_Red;
    else if(Input->PlatformIsPlaying) WindowBorderColor = Color_Green;
    else if(Input->PlatformWindowIsFocused) WindowBorderColor = Color_Snow0;
    else WindowBorderColor = Color_Black;
    
    //- Prepare rects rendering 
    u64 MaxRectsCount = 1024;
    GlobalRectsCount = 0;
    GlobalRectsInstances = PushArray(FrameArena, rect_instance, MaxRectsCount);
    
    OS_ProfileAndPrint("Misc setup");
    
    //- Draw text
    if(App->Font.Initialized)
    {
        if(App->PreviousHeightPx != App->HeightPx)
        {
            App->PreviousHeightPx = App->HeightPx;
            InitAtlas(App->FontAtlasArena, &App->Font, &App->FontAtlas, App->HeightPx);
        }
    }
    
    OS_ProfileAndPrint("Atlas");
    
    // Draw rectangles 
    {
        // Window borders
        {
            rect Dest = RectFromSize(V2(0.f, 0.f), BufferDim);
            rect_instance *Inst = DrawRect(Dest, WindowBorderColor, 0.f, WindowBorderSize, 0.f);
            Inst->Color0 = WindowBorderColor;
            Inst->Color1.W = 0.2f;
            Inst->Color2.W = 0.2f;
            Inst->Color3 = WindowBorderColor;
        }
    }
    
    UI_BoxTableSize = App->UIBoxTableSize;
    UI_BoxTable = App->UIBoxTable;
    UI_BoxArena = App->UIBoxArena;
    UI_State = PushStructZero(FrameArena, ui_state);
    
    u32 Flags = (UI_BoxFlag_DrawBorders | UI_BoxFlag_DrawBackground |
                 UI_BoxFlag_DrawDisplayString |
                 UI_BoxFlag_CenterTextHorizontally | UI_BoxFlag_CenterTextVertically );
    u32 ButtonFlags = (Flags | UI_BoxFlag_MouseClickability);
    u32 TitlebarFlags = (Flags ^ 
                         UI_BoxFlag_DrawDisplayString ^ 
                         UI_BoxFlag_DrawBackground);
    
    OS_ProfileAndPrint("UI setup");
    
    v2 Pos = V2(0.f, 0.f);
    v2 Size = BufferDim;
    Pos = V2AddF32(Pos, WindowBorderSize);
    Size = V2SubF32(Size, 2.f*WindowBorderSize);
    
    rect FreeRegion = RectFromSize(Pos, Size);
    
    
    // UI Panels
    {
        //1. For each panel 
        // 2. Add split borders for resizing
        // 3. Capture input events for dragging
        // 4. Compute the region for UI and create root ui_box
        // 5. Solve layout for UI
        // 6. Solve UI for each panel
        
        PanelGetRegion(App->FirstPanel, FreeRegion);
        
        //PanelDebugPrint(App->FirstPanel);
        // UI for this panel
        {
            App->TitlebarPanel->Root = PushStructZero(FrameArena, ui_box);
            
            ui_box *Root = App->TitlebarPanel->Root;
            Root->First = Root->Last = Root->Next = Root->Prev = Root->Parent = UI_NilBox;
            
            Root->FixedPosition = App->TitlebarPanel->Region.Min;
            Root->FixedSize = SizeFromRect(App->TitlebarPanel->Region);
            
            UI_InitState(Root, Input, App, false);
            
            UI_SemanticWidth(UI_SizeParent(1.f/5.f, 1.f)) 
                UI_SemanticHeight(UI_SizeText(2.f, 1.f))
            {    
                UI_AddBox(S8("Open"), ButtonFlags);
                UI_AddBox(S8("Help"), ButtonFlags);
                UI_AddBox(S8("Save"), ButtonFlags);
                
                UI_AddBox(S8(""), UI_BoxFlag_None);
                
                if(UI_AddBox(S8("Close"), ButtonFlags)->Clicked)
                {
                    ShouldQuit = true;
                }
            }
            
            UI_ResolveLayout(Root->First);
        }
        
    }
    
    OS_ProfileAndPrint("UI");
    
    //- Rendering 
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    
    glClearColor(V3Arg(Color_Background), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    OS_ProfileAndPrint("R Clear");
    
    // Render rectangles
    {
        if(!App->Render.ShadersCompiled)
        {            
            gl_handle RectShader = App->Render.RectShader;
            
            RectShader = GL_ProgramFromShaders(FrameArena, Memory->ExeDirPath,
                                               S8("../source/shaders/rect_vert.glsl"),
                                               S8("../source/shaders/rect_frag.glsl"));
            glUseProgram(RectShader);
            
            // Uniforms
            {
                gl_handle UViewport = glGetUniformLocation(RectShader, "Viewport");
                glUniform2f(UViewport, V2Arg(BufferDim));
                
                GL_LoadTextureFromImage(App->Render.Textures[0], Atlas->Width, Atlas->Height, Atlas->Data, GL_RED, RectShader, "Texture");
            }
            
            App->Render.ShadersCompiled = true;
        }
        
#if EDITOR_HOT_RELOAD_SHADERS
        App->Render.ShadersCompiled = false;
#endif
        
        Assert(GlobalRectsCount < MaxRectsCount);
        u64 Size = GlobalRectsCount*sizeof(rect_instance);
        glBufferData(GL_ARRAY_BUFFER, Size, GlobalRectsInstances, GL_STATIC_DRAW);
        
        u64 Offset = 0;
        
        // TODO(luca): Metaprogram
        s32 Counts[] = {4,4, 4,4,4,4, 4, 1,1, 1};
        
        s32 TotalSize = 0;
        for EachElement(Idx, Counts)
        {            
            GL_SetQuadAttribute((s32)Idx + 1, Counts[Idx], &Offset);
            TotalSize += Counts[Idx];
        }
        Assert(TotalSize == (sizeof(rect_instance)/sizeof(f32)));
        
        glEnable(GL_MULTISAMPLE);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GlobalRectsCount);
    }
    
    OS_ProfileAndPrint("R rects");
    
    App->FrameIndex += 1;
    
    return ShouldQuit;
}