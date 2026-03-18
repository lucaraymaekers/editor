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

// language stuff
//


#include "editor_lexer.h"
#include "editor_parser.h"

#include "editor_lexer.c"
#include "editor_parser.c"


//- Globals
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

global_variable u64 GlobalTextCursor = 0;
//- 

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

//- Text editing 
internal void
AppendChar(app_state *App, rune Codepoint)
{
    MemoryCopy(App->Text + (App->TextCursor),
               App->Text + (App->TextCursor - 1),
               sizeof(rune)*((App->TextCount - App->TextCursor) + 1));
    
    App->Text[App->TextCursor] = Codepoint;
    
    App->TextCount += 1;
    App->TextCursor += 1;
    Assert(App->TextCount < ArrayCount(App->Text));
}

internal void
DeleteChar(app_state *App)
{
    if(App->TextCursor)
    {
        MemoryCopy(App->Text + (App->TextCursor - 1),
                   App->Text + App->TextCursor,
                   sizeof(rune)*((App->TextCount - App->TextCursor) + 1));
        
        App->TextCount -= 1;
        App->TextCursor -= 1;
    }
}

internal void
MoveRight(app_state *App)
{
    App->TextCursor += (App->TextCursor < App->TextCount);
}

internal void
MoveLeft(app_state *App)
{
    App->TextCursor -= (App->TextCursor > 0);
}
//- 

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
    
    stbtt_pack_context Ctx;
    {
        stbtt_PackBegin(&Ctx, 
                        Atlas->Data,
                        Atlas->Width, Atlas->Height,
                        0, 1, 0);
        
        stbtt_PackFontRange(&Ctx, Font->Info.data, 0, HeightPx, 
                            Atlas->FirstCodepoint, Atlas->CodepointsCount, 
                            Atlas->PackedChars);
        stbtt_PackEnd(&Ctx);
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

typedef struct u64_array u64_array;
struct u64_array
{
    u64 Count;
    u64 *V;
};

internal u64_array
GetWrapPositions(arena *Arena, str8 Text, font_atlas *Atlas, f32 MaxWidth)
{
    u64_array Result = {};
    
    // 1px per char
    u64 MaxChars = (u64)MaxWidth;
    
    Result.V = PushArray(Arena, u64, MaxChars);
    
    f32 X = 0.f;
    for EachIndex(Idx, Text.Size)
    {
        u8 Char = Text.Data[Idx];
        f32 CharWidth = Atlas->PackedChars[Char].xadvance;
        
        if(Char == '\n' || (X + CharWidth > MaxWidth))
        {
            Result.V[Result.Count] = Idx;
            Result.Count += 1;
            X = 0.f;
        }
        else
        {            
            X += CharWidth;
        }
    }
    
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
        // TODO(luca): Looks off, maybe rounding is incorrect, investigate...
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
#define PanelAdd(ParentPct) PanelAdd_(PanelArena, PanelAxisTop->Value, PanelCurrent, PanelAppendToParent, ParentPct)

internal panel *
PanelAlloc(arena *Arena)
{
    panel *New = PushStructZero(Arena, panel);
    New->First = New->Last = New->Next = New->Prev = New->Parent = NilPanel;
    return New;
}

internal panel *
PanelAdd_(arena *Arena, axis2 Axis, panel *Current, b32 AppendToParent, f32 ParentPct)
{
    panel *New = PanelAlloc(Arena);
    
    New->First = New->Last = New->Next = New->Prev = New->Parent = NilPanel;
    
    New->ParentPct = ParentPct;
    New->Axis = Axis;
    
    if(!IsNilPanel(Current))
    {            
        if(AppendToParent)
        {
            Current->First = New;
            New->Parent = Current;
        }
        else
        {
            Current->Next = New;
            New->Prev = Current;
            New->Parent = Current->Parent;
        }
        
        New->Parent->Last = New;
    }
    
    PanelCurrent = New;
    PanelAppendToParent = false;
    
    return New;
}

internal panel *
SplitPanel(arena *Arena, panel *To, s32 Axis, b32 Backwards)
{
    
    panel *Result = To;
    
    panel *New = PanelAlloc(Arena);
    panel *Parent = To->Parent;
    
    // NOTE(luca): Must be a leaf node.
    Assert(IsNilPanel(To->First));
    
    if(Axis == Parent->Axis)
    {
        //The new child's ParentPct becomes 1/n where n is the children count
        //Other children's ParentPct *= (1-1/n) 
        {        
            u32 ChildrenCount = 0;
            for EachPanel(Child, Parent)
            {
                ChildrenCount += 1;
            }
            
            Assert(ChildrenCount > 0);
            // The new child
            ChildrenCount += 1;
            
            New->ParentPct = 1.f/(f32)ChildrenCount;;
            
            for EachPanel(Child, Parent)
            {
                Child->ParentPct *= (1.f - New->ParentPct);
            }
        }
        
        New->Parent = Parent;
        
        if(!Backwards)
        {
            if(To == Parent->Last)
            {
                Parent->Last = New;
            }
        }
        else
        {
            if(To == Parent->First)
            {
                Parent->First = New;
            }
        }
        
        if(!Backwards)
        {    
            New->Next = To->Next;
            
            if(!IsNilPanel(To->Next)) To->Next->Prev = New;
            
            To->Next = New;
            New->Prev = To;
        }
        else
        {
            New->Next = To;
            
            if(!IsNilPanel(To->Prev)) To->Prev->Next = New;
            
            New->Prev = To->Prev;
            To->Prev = New;
        }
        
        Result = New;
    }
    else
    {
        //1. Create NewParent replacing Parent
        //  - update To siblings and Parent child links
        //2. To becomes child of NewParent
        //3. Split To
        
        panel *NewParent = New;
        NewParent->Axis = Axis;
        NewParent->ParentPct = To->ParentPct;
        NewParent->Parent = Parent;
        
        //1.
        if(To == Parent->First)
        {
            Parent->First = NewParent;
        }
        
        if(To == Parent->Last)
        {
            Parent->Last = NewParent;
        }
        
        if(!IsNilPanel(To->Prev))
        {
            To->Prev->Next = NewParent;
            NewParent->Prev = To->Prev;
        }
        
        if(!IsNilPanel(To->Next))
        {
            To->Next->Prev = NewParent;
            NewParent->Next = To->Next;
        }
        
        //2. 
        To->Parent = NewParent;
        NewParent->First = To;
        NewParent->Last = To;
        To->Next = To->Prev = NilPanel;
        
        //3. 
        To->ParentPct = 1.f;
        Result = SplitPanel(Arena, To, Axis, Backwards); 
    }
    
    return Result;
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

internal inline b32 
IsLeafPanel(panel *Panel)
{
    b32 Result = IsNilPanel(Panel->First);
    return Result;
}

internal panel *
PanelNextLeaf(panel *Start, b32 Backwards)
{
    panel *Result = Start;
    // NOTE(luca): If nothing was found we will return the starting point
    
    b32 IsLeaf = false;
    
    panel *Search = Start;
    
    while(!IsLeaf && !IsNilPanel(Search))
    {
        panel *Next = (Backwards ? Search->Prev : Search->Next);
        
        if(!IsNilPanel(Next) || IsNilPanel(Search->Parent))
        {
            if(!IsNilPanel(Next))
            {            
                Search = Next;
                
                IsLeaf = IsLeafPanel(Search);
            }
            
            while(!IsLeaf)
            {
                Search = (Backwards ? Search->Last : Search->First);
                
                IsLeaf = IsLeafPanel(Search);
            }
        }
        else
        {
            Search = Search->Parent;
        }
    }
    
    if(IsLeaf)
    { 
        Assert(!IsNilPanel(Search) || Search == Start);
        Result = Search;
    }
    
    return Result;
}

internal panel *
ClosePanel(panel *Panel)
{
    // NOTE(luca): The panel which will take over the side of the deleted one.
    panel *Collapse = NilPanel;
    
    if(!Panel->CannotClose)
    {    
        panel *Parent = Panel->Parent;
        
        if(!IsNilPanel(Panel))
        {        
            if(Parent->First == Panel)
            {
                Parent->First = Panel->Next;
            }
            
            if(Parent->Last == Panel)
            {
                Parent->Last = Panel->Prev;
            }
            
            // TODO(luca): When this becomes the first and last child, should we collapse it together with the parent?
            
            if(!IsNilPanel(Panel->Next)) 
            {
                Panel->Next->Prev = Panel->Prev;
                
                Collapse = Panel->Next;
            }
            
            if(!IsNilPanel(Panel->Prev)) 
            {
                Panel->Prev->Next = Panel->Next;
                
                Collapse = Panel->Prev;
            }
            
            if(!IsNilPanel(Collapse))
            {
                Collapse->ParentPct += Panel->ParentPct;
            }
            else
            {
                // Last node of its parent, parent should get deleted.  End of the bloodline.
                Collapse = ClosePanel(Panel->Parent);
            }
            
            // TODO(luca): Push onto the free list
        }
        
        if(!IsLeafPanel(Collapse))
        {
            Collapse = PanelNextLeaf(Collapse, false);
        }
    }
    else
    {
        Collapse = Panel;
    }
    
    return Collapse;
}

internal void
PanelGetRegion(panel *Panel, rect FreeRegion)
{
    panel *Parent = Panel->Parent;
    s32 Axis = Panel->Parent->Axis;
    s32 OtherAxis = 1 - Axis;
    f32 GapSize = 1.f;
    f32 PanelBorderSize = 2.f;
    
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
    b32 MouseIsDown = PanelInput->MouseButtons[PlatformMouseButton_Left].EndedDown;
    b32 MouseWasPressed = WasPressed(PanelInput->MouseButtons[PlatformMouseButton_Left]);
    
    b32 HasResizeBorder = !IsNilPanel(Panel->Next); 
    if(HasResizeBorder)
    {
        v4 ResizeBorderColor = Color_Red;
        f32 ResizeBorderSize = 8.f;
        
        rect ResizeBorder = Panel->Region;
        ResizeBorder.Min.e[Axis] = Panel->Region.Max.e[Axis] - ResizeBorderSize;
        ResizeBorder.Max.e[Axis] += GapSize; 
        
        // TODO(luca): Only one border should be dragged at a time, this should be taken care of by an input queue
        local_persist b32 IsDragging = false;
        local_persist v2 OldMouseP = {};
        local_persist panel *DraggingPanel = 0;
        
        if(!MouseIsDown)
        {
            IsDragging = false;
        }
        
        if(IsInsideRectV2(MouseP, ResizeBorder) && !IsDragging)
        {
            ResizeBorderColor = Color_Green;
            PanelInput->PlatformCursor = (PlatformCursorShape_ResizeHorizontal + Axis);
            
            if(MouseWasPressed)
            {
                IsDragging = true;
                OldMouseP = MouseP;
                DraggingPanel = Panel;
            }
        }
        
        if(IsDragging)
        {
            PanelInput->PlatformCursor = (PlatformCursorShape_ResizeHorizontal + DraggingPanel->Parent->Axis);
            PanelInput->Consumed = true;
        }
        
        if(IsDragging && Panel == DraggingPanel)
        {
            ResizeBorderColor = Color_Yellow;
            
            f32 dP = (MouseP.e[Axis] - OldMouseP.e[Axis]);
            f32 ParentOtherSize = (Parent->Region.Max.e[OtherAxis] - Parent->Region.Min.e[OtherAxis]);
            f32 dPPct = (dP/ParentSize);
            
            panel *Next = Panel->Next;
            
            // NOTE(luca): Proper resizing
            // Other panels should be clamped to a minimum size, which should be pixel based since percents can mean visible on large screen but invisible on small screens 
            // More size taken from bigger panels than from smaller panels.
            // TODO(luca): Make good resizing ->
            // The delta distance computed should be relative to the new position of the Panel Min.  This elimitates being able to resize to the right when being on the left of the border. (User should pass over the border to resize in that direction).
            
            //Attempt #2
            // Resize between this panel and next panel
            // Clamp panel between .05 and .95 of the root panel
            
            f32 SizeTaken = 0.f;
            
            f32 RelMin = .05f;
            
            f32 RelFactor = 1.f;
            for(panel *Parent = Panel->Parent; !IsNilPanel(Parent); Parent = Parent->Parent)
            {
                if(Parent->Axis == Panel->Parent->Axis && !IsNilPanel(Parent->Parent))
                {                    
                    RelFactor *= Parent->Parent->ParentPct;
                }
            }
            RelMin /= RelFactor;
            
            panel *Inc = Panel;
            panel *Dec = Next;
            if(dPPct < 0.f)
            {
                Swap(Inc, Dec);
                dPPct = -dPPct;
            }
            
            if(Dec->ParentPct >= RelMin)
            {                
                SizeTaken = Min(Dec->ParentPct - RelMin, dPPct);
            }
            else
            {
                // Clamp
                // NOTE(luca): Unless other is also below 
                if(Inc->ParentPct >= 0.5f)
                {                    
                    SizeTaken = -(RelMin - Dec->ParentPct);
                }
            }
            
            Dec->ParentPct -= SizeTaken;
            Inc->ParentPct += SizeTaken;
            
            OldMouseP = MouseP;
        }
        
#if 0
        DrawRect(ResizeBorder, ResizeBorderColor, 0.f, 0.f, 0.f);
#endif
    }
    
    v4 PanelBorderColor = Color_Blue;
    
    // NOTE(luca): This happens post-order so that the borders are overlaid correctly.
    
    b32 HasContents = IsNilPanel(Panel->First);
    
    // draw panel rectangle
    if(HasContents)
    {
        if(!PanelInput->Consumed && IsInsideRectV2(MouseP, Panel->Region) && MouseWasPressed)
        {
            PanelApp->SelectedPanel = Panel;
        }
        
        if(Panel == PanelApp->SelectedPanel)
        {
            PanelBorderColor = Color_Cyan;
        }
        
        Panel->Region = RectShrink(Panel->Region, GapSize);
        DrawRect(Panel->Region, PanelBorderColor, 0.f, PanelBorderSize, 0.f);
        Panel->Region = RectShrink(Panel->Region, PanelBorderSize);
    }
}

internal app_input *
GetInput(app_input *Input)
{
    app_input *Result = 0;
    
    if(Input && !Input->Consumed)
    {
        Result = Input;
    }
    
    return Input;
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
    ThreadContext = Memory->ThreadCtx;
    
    Input->Consumed = false;
    Input->PlatformCursor = PlatformCursorShape_Arrow;
    
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
    v2 MouseP = V2S32(Input->MouseX, Input->MouseY);
    
    if(!Memory->Initialized)
    {
        char *FontPath = PathFromExe(FrameArena, Memory->ExeDirPath, S8("../data/font_regular.ttf"));
        InitFont(&App->Font, FontPath);
        App->FontAtlasArena = PushArena(PermanentArena, MB(100));
        App->PreviousHeightPx = DefaultHeightPx + 1.0f;
        App->HeightPx = DefaultHeightPx;
        
        App->TextCount = 0;
        App->TextCursor = 0;
        
        App->UIBoxTableSize = 4096;
        App->UIBoxTable = PushArray(PermanentArena, ui_box, App->UIBoxTableSize);
        App->UIBoxArena = PushArena(PermanentArena, 256*sizeof(ui_box));
        
        App->TrackerForUI_NilBox = &_UI_NilBox;
        App->TrackerForNilPanel = &_NilPanel;
        
        gl_InitState(&App->Render);
        
        // Panels
        {        
            f32 TextPadding = 2.f + 1.f + 2.f;
            
            f32 TopSplitPct = (App->HeightPx + TextPadding*2.f)/(BufferDim.Y - 2.f*WindowBorderSize);
            
            PanelArena = App->PanelArena = PushArena(PermanentArena, ArenaAllocDefaultSize);
            PanelAxis(Axis2_Y) PanelGroup()
            {
                panel *RootPanel = PanelAdd(1.f);
                
                PanelAxis(Axis2_X) PanelGroup()
                {
                    panel *TitlebarPanel = PanelAdd(TopSplitPct);
                    TitlebarPanel->CannotClose = true;
                    
                    App->TitlebarPanel = TitlebarPanel;
                    
                    panel *AppPanel = PanelAdd(1.f - TopSplitPct);
                    
                    PanelAxis(Axis2_Y) PanelGroup()
                    {
                        panel *LeftPanel = PanelAdd(.4f);
                        PanelGroup()
                        {            
                            panel *TopLeftPanel = PanelAdd(.33f);
                            panel *MiddleLeftPanel = PanelAdd(.33f);
                            panel *BottomLeftPanel = PanelAdd(.34f);
                        }
                        panel *MiddlePanel = PanelAdd(.2f);
                        panel *RightPanel = PanelAdd(.4f);
                        PanelGroup()
                        {            
                            panel *DebugPanel = PanelAdd(.6f);
                            App->DebugPanel = DebugPanel;
                            App->DebugPanel->CannotClose = true;
                            App->DebugPanel->Root = UI_BoxAlloc(PermanentArena);
                            App->SelectedPanel = App->DebugPanel;
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
    
    GlobalTextCursor = App->TextCursor;
    
    StringsScratch = FrameArena;
    
    PanelArena = App->PanelArena;
    PanelInput = Input;
    PanelApp = App;
    
    // Text input
    for EachIndex(Idx, Input->Text.Count)
    {
        app_text_button Key = Input->Text.Buffer[Idx];
        // Ignore shift.
        u32 ModifiersShiftIgnored = (Key.Modifiers & ~PlatformKeyModifier_Shift);
        
        if(!Key.IsSymbol)
        {
            
            if(Key.Modifiers & PlatformKeyModifier_Alt && Key.Codepoint == 'b')
            {
                DebugBreak();
            }
            
            if(ModifiersShiftIgnored == PlatformKeyModifier_Control)
            {
                //- Panels 
                if(0) {}
                else if(Key.Codepoint == 'p')
                {
                    if(!(Key.Modifiers & PlatformKeyModifier_Shift))
                    {                        
                        App->SelectedPanel = SplitPanel(PanelArena, App->SelectedPanel, Axis2_X, false);
                    }
                    else
                    {
                        App->SelectedPanel = ClosePanel(App->SelectedPanel);
                    }
                }
                else if(Key.Codepoint == '-')
                {
                    if(!(Key.Modifiers & PlatformKeyModifier_Shift))
                    {                        
                        App->SelectedPanel = SplitPanel(PanelArena, App->SelectedPanel, Axis2_Y, false);
                    }
                }
                else if(Key.Codepoint == ',')
                {
                    if(!(Key.Modifiers & PlatformKeyModifier_Shift))
                    {
                        App->SelectedPanel = PanelNextLeaf(App->SelectedPanel, false);
                    }
                    else
                    {
                        App->SelectedPanel = PanelNextLeaf(App->SelectedPanel, true);
                    }
                }
                //- Text Input
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
                else if(Key.Codepoint == 'u')
                {
                    while(App->TextCursor > 0) DeleteChar(App);
                }
                //- Font 
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
            
            if(!(Key.Modifiers & PlatformKeyModifier_Control))
            {                
                if(0) {}
                else if(Key.Codepoint == PlatformKey_Escape)
                {
                    ShouldQuit = true;
                }
                //- Text Input 
                else if(Key.Codepoint == PlatformKey_BackSpace)
                {
                    DeleteChar(App);
                }
                else if(Key.Codepoint == PlatformKey_Return)
                {
                    AppendChar(App, L'\n');
                }
                else if(Key.Codepoint == PlatformKey_Right)
                {
                    MoveRight(App);
                }
                else if(Key.Codepoint == PlatformKey_Left)
                {
                    MoveLeft(App);
                }
                else if(Key.Codepoint == PlatformKey_Delete)
                {
                    MoveRight(App);
                    DeleteChar(App);
                }
            }
            else
            {
                if(0) {}
                else if(Key.Codepoint == PlatformKey_Left)
                {
                    while(App->TextCursor > 0 && 
                          IsWhiteSpace(App->Text[App->TextCursor - 1]))
                    {
                        MoveLeft(App);
                    }
                    
                    while(App->TextCursor > 0 && 
                          !IsWhiteSpace(App->Text[App->TextCursor - 1]))
                    {
                        MoveLeft(App);
                    }
                }
                else if(Key.Codepoint == PlatformKey_Right)
                {
                    while(App->TextCursor < App->TextCount &&
                          IsWhiteSpace(App->Text[App->TextCursor]))
                    {
                        MoveRight(App);
                    }
                    
                    while(App->TextCursor < App->TextCount && 
                          !IsWhiteSpace(App->Text[(App->TextCursor)]))
                    {
                        MoveRight(App);
                    }
                }
                else if(Key.Codepoint == PlatformKey_BackSpace)
                {
                    while(App->TextCursor > 0 && 
                          IsWhiteSpace(App->Text[App->TextCursor - 1]))
                    {
                        DeleteChar(App);
                    }
                    
                    while(App->TextCursor > 0 && 
                          !IsWhiteSpace(App->Text[App->TextCursor - 1]))
                    {
                        DeleteChar(App);
                    }
                }
                else if(Key.Codepoint == PlatformKey_Delete)
                {
                    while(App->TextCursor < App->TextCount &&
                          IsWhiteSpace(App->Text[App->TextCursor]))
                    {
                        MoveRight(App);
                        DeleteChar(App);
                    }
                    
                    while(App->TextCursor < App->TextCount && 
                          !IsWhiteSpace(App->Text[(App->TextCursor)]))
                    {
                        MoveRight(App);
                        DeleteChar(App);
                    }
                }
                
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
    else if(Input->PlatformWindowIsFocused) WindowBorderColor = Color_Snow2;
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
        // TODO(luca): 
        //1. For each panel 
        // 2. Add split borders for resizing
        // 3. Capture input events for dragging
        // 4. Compute the region for UI and create root ui_box
        // 5. Solve layout for UI
        // 6. Solve UI for each panel
        
        PanelGetRegion(App->FirstPanel, FreeRegion);
        
        // UI for this panel
        {
            App->TitlebarPanel->Root = UI_BoxAlloc(FrameArena);
            ui_box *Root = App->TitlebarPanel->Root;
            
            Root->FixedPosition = App->TitlebarPanel->Region.Min;
            Root->FixedSize = SizeFromRect(App->TitlebarPanel->Region);
            Root->Rec = RectFromSize(Root->FixedPosition, Root->FixedSize);
            
            UI_State = PushStructZero(FrameArena, ui_state);
            UI_InitState(Root, Input, App);
            
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
        
        // Test UI, nothing concrete yet.
        {
            panel *Panel = App->DebugPanel;
            ui_box *Root = Panel->Root;
            
            Root->FixedPosition = Panel->Region.Min;
            Root->FixedSize = SizeFromRect(Panel->Region);
            Root->Rec = RectFromSize(Root->FixedPosition, Root->FixedSize);
            
            UI_State = PushStructZero(FrameArena, ui_state);
            UI_InitState(App->DebugPanel->Root, Input, App);
            
            u32 Flags = (UI_BoxFlag_Clip |
                         UI_BoxFlag_DrawBorders |
                         UI_BoxFlag_DrawBackground |
                         UI_BoxFlag_DrawDisplayString |
                         UI_BoxFlag_CenterTextVertically);
            
            // NOTE(luca): Adding an extra parent like this makes it easy to override defaults
            UI_LayoutAxis(Axis2_Y) 
                UI_SemanticWidth(UI_SizeParent(1.f, 1.f)) UI_SemanticHeight(UI_SizeParent(1.f, 1.f))
                UI_AddBox(S8(""), UI_BoxFlag_Clip);
            
            UI_Push()
            {            
                UI_CornerRadii(V4(0.f, 0.f, 0.f, 0.f))
                    UI_SemanticWidth(UI_SizeParent(1.f, 1.f)) 
                    UI_SemanticHeight(UI_SizeText(2.f, 1.f))
                {
                    local_persist f64 StartTime = OS_GetWallClock();
                    
                    UI_AddBox(Str8Fmt("Time: %.7f###time", OS_GetWallClock() - StartTime), Flags);
                    
                    // TODO(luca): This does not work.
                    UI_LayoutAxis(Axis2_X) UI_SemanticHeight(UI_SizeChildren(1.f)) 
                        UI_AddBox(S8(""), UI_BoxFlag_Clip);
                    UI_Push() UI_SemanticWidth(UI_SizeParent(1.f/2.f, 1.f)) UI_BorderThickness(1.f)
                    {
                        UI_AddBox(Str8Fmt("Count: %lu###Text Count", App->TextCount), Flags);
                        UI_AddBox(Str8Fmt("Cursor: %lu###Cursor Pos", App->TextCursor), Flags);
                    }
                    
                    // Text
                    {                
                        str8 AppText = PushS8(FrameArena, App->TextCount);
                        for EachIndex(Idx, App->TextCount)
                        {
                            AppText.Data[Idx] = (u8)App->Text[Idx];
                        }
                        
                        UI_SemanticHeight(UI_SizeParent(1.f, 0.f))
                            UI_TextColor(Color_Snow0)
                            UI_AddBox(Str8Fmt(S8Fmt "###app text", S8Arg(AppText)), 
                                      (Flags | UI_BoxFlag_TextWrap | UI_BoxFlag_DrawCursor) & 
                                      ~(UI_BoxFlag_CenterTextVertically |
                                        UI_BoxFlag_CenterTextHorizontally));
                    }
                }
            }
            
            UI_ResolveLayout(Root->First);
            
        }
        
    }
    
    // Prune unused boxes
    for EachIndex(Idx, App->UIBoxTableSize)
    {
        ui_box *First = App->UIBoxTable + Idx;
        for(ui_box *Node = First; !UI_IsNilBox(Node); Node = Node->HashNext)
        {
            if(App->FrameIndex > Node->LastTouchedFrameIndex)
            {
                if(!UI_KeyMatch(Node->Key, UI_KeyNull()))
                {
                    Node->Key = UI_KeyNull();
                }
            }
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
