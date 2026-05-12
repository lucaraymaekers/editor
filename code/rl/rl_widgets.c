#define UI_Row() DeferLoop(UI_PushContainer(Axis2_X), UI_PopBox())
#define UI_Column() DeferLoop(UI_PushContainer(Axis2_Y), UI_PopBox())
#define UI_Padding(Size) DeferLoop(UI_Spacer(Size), UI_Spacer(Size))
#define UI_Center() UI_Padding(UI_SizeParent(1.f, 0.f))

#define UI_PaddingAround(Size) \
UI_Push() \
UI_FillAll() \
UI_Column() UI_Padding(Size) \
UI_Row() UI_Padding(Size)

internal ui_box *
UI_Spacer(ui_size Size)
{
    ui_box *Box = UI_AddBox(S8(""), UI_BoxFlag_Clip);
    axis2 Axis = Box->Parent->LayoutAxis;
    Box->SemanticSize[Axis] = Size;
    Box->SemanticSize[1 - Axis] = UI_SizeParent(1.f, 0.f);
    
    return Box;
}

internal void
UI_PushContainer(axis2 Axis)
{
    UI_LayoutAxis(Axis)
        UI_AddBox((Axis == Axis2_X ? S8("Row") : S8("Column")), UI_BoxFlag_Clip);
    UI_PushBox();
}

internal b32
UI_Checkbox(b32 Toggled, v4 EnabledColor)
{
    b32 Result = false;
    
    ui_box *Box;
    
    f32 CircleWidth = 20.f;
    
    // NOTE(luca): Push the switch onto the text so the text can be centered easily.  This might cause the switch to draw on top of the text...
    // TODO(luca): A better solution would be to allow overflow such that we compute centering as if we own the whole space, but when drawing the text we still clip to the next box?
#if 1
    UI_CornerRadii(V4F32(CircleWidth/2.f))
#endif
    UI_Softness(1.f)
    {         
        UI_BackgroundColor(Toggled ? EnabledColor : Color_Background) 
            UI_SemanticWidth(UI_SizePx(50.f, 1.f))
        {
            Box = UI_AddBox(S8("Checkbox"), (UI_BoxFlag_Clip|
                                             UI_BoxFlag_MouseClickable|
                                             UI_BoxFlag_DrawBackground|
                                             UI_BoxFlag_DrawHotEffects));
            // TODO(luca): This is a hack to have the borders be overlaid and not affected by the smoothness.  Thus they do not become blurry.
            UI_Push() 
            {
                UI_FillAll()
                    UI_BorderColor(Color_Black)
                {
                    UI_AddBox(S8("Borders"), (UI_BoxFlag_Clip|
                                              UI_BoxFlag_DrawBorders));
                }
                
                //- Add padding for border + gap 
                UI_PaddingAround(UI_SizePx(3.f, 1.f))
                {
                    //- Pushes the circle to the end if toggled 
                    if(Toggled)
                    {
                        UI_Spacer(UI_SizeParent(1.f, 0.f));
                    }
                    
                    //- Draw actual circle
                    UI_BackgroundColor(Color_Black)
                        UI_SemanticWidth(UI_SizePx(CircleWidth, 1.f))
#if 1
                    UI_CornerRadii(V4F32(CircleWidth/2.f - 3.f))
#endif
                    UI_AddBox(S8("Circle"), (UI_BoxFlag_Clip|
                                             UI_BoxFlag_DrawBackground|
                                             UI_BoxFlag_AnimatePosX));
                }
            }
        }
    }
    
    Result = Box->WasClicked;
    
    return Result;
}

typedef struct ui_button_params ui_button_params;
struct ui_button_params
{
    str8 Text;
    v4 BackgroundColor;
    v4 TextColor;
    b32 CenterText;
    
    b32 DisableBorder;
    
    b32 Disabled;
    v4 DisabledBackgroundColor;
    
    b32 HasToggle;
    b32 ToggleToggled;
    v4 ToggleToggledColor;
    
    ui_size Padding;
};

#define Button(...) \
Button_((ui_button_params){ \
.Padding = UI_SizePx(0.f, 1.f), \
.TextColor = Color_ButtonText, \
.BackgroundColor = Color_ButtonBackground, \
.DisabledBackgroundColor = {U32ToV4Arg(0xFF616E88)}, \
##__VA_ARGS__})

typedef struct ui_button_result ui_button_result;
struct ui_button_result
{
    b32 Pressed;
    b32 Released;
    b32 Toggled;
    b32 OneClicked;
};

internal ui_button_result 
Button_(ui_button_params Params)
{
    ui_button_result Result = {0};
    
    s32 ButtonFlags = (UI_BoxFlag_Clip|
                       UI_BoxFlag_MouseClickable|
                       UI_BoxFlag_DrawBackground|
                       UI_BoxFlag_DrawHotEffects|
                       UI_BoxFlag_DrawActiveEffects);
    
    if(!Params.DisableBorder)
    {
        ButtonFlags |= UI_BoxFlag_DrawBorders;
    }
    
    UI_BackgroundColor(Params.Disabled ? 
                       Params.DisabledBackgroundColor : 
                       Params.BackgroundColor)
    {
        ui_box *Box = UI_AddBox(Params.Text, ButtonFlags);
        Result.Pressed = Box->WasClicked;
        Result.Released = Box->Clicked;
    }
    
    UI_PaddingAround(Params.Padding)
    {
        s32 TextFlags = UI_BoxFlag_Clip|UI_BoxFlag_DrawDisplayString|UI_BoxFlag_CenterTextVertically;
        
        if(Params.CenterText)
        {
            TextFlags |= UI_BoxFlag_CenterTextHorizontally;
        }
        
        UI_TextColor(Params.TextColor)
            UI_AddBox(Params.Text, TextFlags);
        
        if(Params.HasToggle)
        {
            Result.Toggled = UI_Checkbox(Params.ToggleToggled, Params.ToggleToggledColor);
        }
    }
    
    Result.OneClicked = (Result.Toggled | Result.Pressed);
    
    return Result;
}

internal ui_button_result
UI_ButtonWithToggle(str8 Text, b32 Enabled,
                    ui_size Padding, 
                    v4 EnabledColor)
{
    ui_button_result Result = Button(.Text = Text, 
                                     .HasToggle = true, 
                                     .Padding = Padding, 
                                     .ToggleToggled = Enabled,
                                     .ToggleToggledColor = EnabledColor);
    
    return Result;
}

internal f32
UI_Slider(f32 Value, f32 Min, f32 Max, f32 StepSize, char *Format, b32 Interactive)
{
    f32 Result = Value;
    
    f32 Range = Max-Min;
    f32 Step = StepSize/Range;
    f32 Pct = (Value - Min)/Range;
    f32 Rounded = RoundF32(Pct/Step)*(Step);
    
    Pct = Clamp(0.f, Rounded, 1.f);
    
    if(Format)
    {    
        ui_box *Box;
        UI_SemanticWidth(UI_SizeText(1.f, 1.f))
            Box = UI_AddBox(S8("Out"), 
                            UI_BoxFlag_Clip|
                            UI_BoxFlag_DrawDisplayString|
                            UI_BoxFlag_CenterTextVertically|
                            UI_BoxFlag_CenterTextHorizontally);
        Box->DisplayString = Str8Fmt(Format, Value);
    }
    
    f32 ThumbWidth = 20.f;
    f32 Padding = 3.f;
    ui_box *Slider;
    ui_box *DragArea;
    
    UI_Softness(1.f)
#if 1
    UI_CornerRadii(V4F32(ThumbWidth/2.f))
#endif
    {
        
        UI_LayoutAxis(Axis2_X)
            Slider = UI_AddBox(S8("Slider"), (UI_BoxFlag_Clip|
                                              UI_BoxFlag_DrawBackground|
                                              UI_BoxFlag_DrawHotEffects|
                                              UI_BoxFlag_CenterTextHorizontally|
                                              UI_BoxFlag_CenterTextVertically));
        if(Interactive) Slider->Flags |= UI_BoxFlag_MouseClickable;
        
        UI_Push()
        {
            UI_AddBox(S8("Borders"), (UI_BoxFlag_Clip|
                                      UI_BoxFlag_DrawBorders));
            
            UI_PaddingAround(UI_SizePx(Padding, 1.f))
            {
                UI_FillAll()
                    DragArea = UI_AddBox(S8("DragArea"), UI_BoxFlag_Clip);
                UI_Push()
                    
#if 1
                UI_CornerRadii(V4F32(ThumbWidth/2.f - Padding))
#endif
                {
                    f32 SpacerSize = FloorF32(Pct*(DragArea->FixedSize.X - ThumbWidth));
                    UI_SemanticWidth(UI_SizePx(SpacerSize, 1.f))
                        UI_AddBox(S8(""), 0);
                    
                    UI_SemanticWidth(UI_SizePx(ThumbWidth, 1.f))
                        UI_BackgroundColor(Color_Black)
                        UI_AddBox(S8("Thumb"),
                                  UI_BoxFlag_Clip|
                                  UI_BoxFlag_DrawBackground);
                }
            }
        }
        
    }
    
    v2 MouseP = MousePosFromInput(UI_State->Input);
    app_button_state MouseLeft = UI_State->Input->Mouse.Buttons[PlatformMouseButton_Left];
    
    if(Interactive && UI_IsActive(Slider))
    {
        f32 Start = DragArea->Rec.Min.X + ThumbWidth/2.f;
        f32 End = DragArea->Rec.Max.X - ThumbWidth/2.f;
        
        f32 X = Clamp(Start, MouseP.X, End) - Start;
        
        Pct = X/(End - Start);
        
        Result = Pct*Range + Min;
        Result = RoundF32(Result/StepSize)*StepSize;
    }
    
    return Result;
}

internal f32
UI_Scrollbar(axis2 Axis, f32 TotalSize, f32 Scroll)
{
    f32 Result = 0.f;
    
    f32 ScrollbarSize = 16.f;
    f32 ThumbSize = 32.f;
    f32 HalfThumbSize = .5f*ThumbSize;
    f32 Padding = 2.f;
    
    ui_box *Scrollbar;
    ui_box *Thumb;
    ui_box *Spacer;
    
    app_input *Input = UI_State->Input;
    
    UI_BackgroundColor(Color_Night3)
        UI_LayoutAxis(Axis)
        UI_SemanticSizeOnAxis(Axis, UI_SizeParent(1.f, 0.f))
        UI_SemanticSizeOnAxis(1 - Axis, UI_SizePx(ScrollbarSize, 1.f))
        UI_AddBox(S8("ScrollbarParent"), UI_BoxFlag_Clip|UI_BoxFlag_DrawBackground);
    UI_PaddingAround(UI_SizePx(Padding, 1.f))
    {
        UI_LayoutAxis(Axis)
            UI_FillAll()
            Scrollbar = UI_AddBox(S8("Scrollbar"), UI_BoxFlag_MouseClickable);
        UI_Push()
        {                                                    
            f32 ScrollableSize = Scrollbar->FixedSize.e[Axis] - ThumbSize;
            f32 Factor = TotalSize/ScrollableSize;
            //Factor = 1.f;
            
            Spacer = UI_AddBox(S8("Spacer"), 0);
            
            UI_FillOnAxis(1 - Axis)
                UI_SemanticSizeOnAxis(Axis, UI_SizePx(ThumbSize, 1.f))
                UI_BorderColor(Color_Black)
                UI_BackgroundColor(Color_Orange)
                Thumb = UI_AddBox(S8("Thumb"), 
                                  UI_BoxFlag_Clip|
                                  UI_BoxFlag_DrawBackground|
                                  UI_BoxFlag_DrawBorders|
                                  UI_BoxFlag_MouseClickable|
                                  UI_BoxFlag_DrawHotEffects|
                                  UI_BoxFlag_DrawActiveEffects);
            
            f32 ScrollP = Scroll;
            
            if(UI_IsActive(Thumb) || UI_IsActive(Scrollbar))
            {                                                                            
                f32 MouseP = (f32)Input->Mouse.Pos.e[Axis];
                f32 RelP = MouseP - Scrollbar->FixedPos.e[Axis];
                RelP -= HalfThumbSize;
                RelP = Clamp(0, RelP, ScrollableSize);
                
                ScrollP = RelP*Factor;
            }
            
            Result = ScrollP;
            
            f32 SpaceBeforeThumb = ScrollP/Factor;
            
            Spacer->SemanticSize[Axis] = UI_SizePx(SpaceBeforeThumb, 1.f);
            Spacer->SemanticSize[1 - Axis] = UI_SizeParent(1.f, 0.f);
        }
        
    }
    
    return Result;
}