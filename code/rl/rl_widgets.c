#define UI_Row() DeferLoop(UI_PushContainer(Axis2_X), UI_PopBox())
#define UI_Column() DeferLoop(UI_PushContainer(Axis2_Y), UI_PopBox())
#define UI_Padding(Size) DeferLoop(UI_Spacer(Size), UI_Spacer(Size))

internal void
UI_Spacer(ui_size Size)
{
    ui_box *Box = UI_AddBox(S8(""), 0);
    axis2 Axis = Box->Parent->LayoutAxis;
    Box->SemanticSize[Axis] = Size;
    Box->SemanticSize[1 - Axis] = UI_SizeParent(1.f, 0.f);
}

internal void
UI_PushContainer(axis2 Axis)
{
    UI_LayoutAxis(Axis)
        UI_AddBox(S8(""), 0);
    UI_PushBox();
}

internal b32
UI_ToggleButton(str8 Label, b32 Toggle, v4 EnabledColor)
{
    b32 Result = false;
    UI_BackgroundColor((Toggle ? EnabledColor : Color_ButtonBackground))
    {
        Result = (UI_AddBox(Label, UI_BoxFlag_Clip| 
                            UI_BoxFlag_MouseClickable|
                            UI_BoxFlag_DrawBorders|
                            UI_BoxFlag_DrawBackground|
                            UI_BoxFlag_DrawDisplayString|
                            UI_BoxFlag_DrawHotEffects|
                            UI_BoxFlag_DrawActiveEffects|
                            UI_BoxFlag_CenterTextVertically|
                            UI_BoxFlag_CenterTextHorizontally
                            )->Clicked);
    }
    return Result;
}

internal b32
UI_Button(str8 Label)
{
    b32 Result = UI_ToggleButton(Label, false, V4F32(0.f));
    return Result;
}


internal f32
UI_Slider(f32 MinSize, f32 MaxSize,
          f32 SliderMinPct, 
          f32 Value, 
          str8 DisplayName,
          v4 FillColor, b32 Interactive)
{
    f32 Result = 0.f;
    ui_box *Box = 0;
    
    app_input *Input = UI_State->Input;
    
    f32 Range = MaxSize - MinSize;
    
    
    // Fill level from value
    f32 FillLevel;
    {
        f32 Pct = (Range > 0.f ? (Value - MinSize)/Range : 0.f);
        FillLevel = Pct*(1.f - SliderMinPct) + SliderMinPct;
    }
    
    UI_Softness(1.f) UI_CornerRadii(V4F32(3.f))
    {        
        UI_FillWidth()
            Box = UI_AddBox(Str8Fmt(S8Fmt "Slider", S8Arg(DisplayName)),
                            (UI_BoxFlag_Clip|
                             UI_BoxFlag_MouseClickable|
                             UI_BoxFlag_DrawBackground|
                             UI_BoxFlag_Scroll));
        
        UI_Push()
            UI_FillAll()
            UI_SemanticWidth(UI_SizeParent(FillLevel, 0.f))
            UI_BackgroundColor(FillColor)
            UI_AddBox(S8("FillColor"), (UI_BoxFlag_Clip|
                                        UI_BoxFlag_DrawBackground));
        
        v4 Color = (Interactive && (UI_IsHot(Box) || UI_IsActive(Box)) ?
                    Color_Snow2 : Color_ButtonBorder);
        
        UI_SemanticWidth(UI_SizeText(2.f, 1.f))
            UI_BorderColor(Color)
            UI_AddBox(Str8Fmt(S8Fmt ": %.0f###Label" S8Fmt, 
                              S8Arg(DisplayName), Value,
                              S8Arg(DisplayName)), 
                      (UI_BoxFlag_Clip|
                       UI_BoxFlag_DrawDisplayString|
                       UI_BoxFlag_CenterTextHorizontally|
                       UI_BoxFlag_CenterTextVertically|
                       UI_BoxFlag_DrawBorders|
                       UI_BoxFlag_FloatingX|
                       UI_BoxFlag_FloatingY));
    }
    
    app_button_state MouseLeft = Input->Mouse.Buttons[PlatformMouseButton_Left];
    
    // NOTE(luca): Proper dragging.
    
    if(!Input->Consumed && 
       (UI_IsActive(Box) || UI_IsHot(Box)) && 
       MouseLeft.EndedDown)
    {
        f32 PctOnXAxis = (((f32)Input->Mouse.X - Box->FixedPosition.X)/Box->FixedSize.X);
        FillLevel = Clamp(SliderMinPct, PctOnXAxis, 1.f);
        
        Input->Consumed = true;
    }
    
    // Value from FillLevel
    {    
        f32 New = MinSize + (Range*(FillLevel - SliderMinPct)/(1.f - SliderMinPct));
        Result = roundf(New);
    }
    
    return Result;
}

internal b32
UI_Checkbox(str8 Label, b32 Toggled)
{
    b32 Clicked = false;
    
        f32 VerticalPadding = 8.f;
    
    f32 HeightPx = UI_State->HeightPxTop->Value;
    f32 BoxHeight = HeightPx + 2.f*VerticalPadding;
        
    UI_LayoutAxis(Axis2_X)
        UI_SemanticWidth(UI_SizeChildren(1.f))
            UI_SemanticHeight(UI_SizePx(BoxHeight, 1.f))
            UI_BackgroundColor(Color_ButtonBackground)
            UI_AddBox(S8("Checkbox"), (UI_BoxFlag_Clip|
                                       UI_BoxFlag_DrawBackground|
                                       UI_BoxFlag_DrawBorders));
        {                            
            UI_Push()
                UI_FillHeight() UI_SemanticWidth(UI_SizeChildren(1.f))
            UI_Column() UI_Padding(UI_SizePx(5.f, 1.f))
             UI_Row() UI_Padding(UI_SizePx(5.f, 1.f))
            {
            UI_SemanticWidth(UI_SizeText(2.f, 1.f))
                UI_AddBox(Label, (UI_BoxFlag_Clip|
                                  UI_BoxFlag_CenterTextVertically|
                                      UI_BoxFlag_DrawDisplayString));
                
                f32 CircleWidth = 20.f;
                
                UI_CornerRadii(V4F32(CircleWidth/2.f))
                    UI_Softness(1.f)
                {                                
                    ui_box *Box;
                    UI_BackgroundColor(Toggled ? Color_Red : Color_Background) 
                        UI_SemanticWidth(UI_SizePx(50.f, 1.f))
                        Box = UI_AddBox(S8("Background"), (UI_BoxFlag_Clip|
                                                           UI_BoxFlag_MouseClickable|
                                                           UI_BoxFlag_DrawBackground|
                                                           UI_BoxFlag_DrawHotEffects));
                    // Input
                    // NOTE(luca): Overwrite Hot and Active -ness to match checkbox behavior
                    if(UI_IsHot(Box) && Box->WasClicked)
                    {
                        Clicked = true;
                    }
                    
                    UI_Push() 
                    {
                        UI_FillAll()
                            UI_BorderColor(Color_Black)
                        {
                            UI_AddBox(S8("Borders"), (UI_BoxFlag_Clip|
                                                      UI_BoxFlag_DrawBorders));
                        }
                        
                        //- Add padding for border 
                        ui_size Padding = UI_SizePx(UI_BorderThicknessTop() + 2.f, 1.f);
                        UI_Push()
                            UI_FillAll() UI_Column() UI_Padding(Padding)
                            UI_FillAll() UI_Row() UI_Padding(Padding)
                        {
                            //- Pushes the circle to the end if toggled 
                            if(Toggled)
                            {
                                UI_Spacer(UI_SizeParent(1.f, 0.f));
                            }
                            
                            f32 CircleHeight = BoxHeight - 2.f*VerticalPadding - 2.f*UI_BorderThicknessTop() - 2.f*2.f; 
                            
                            //- Draw actual circle
                            UI_BackgroundColor(Color_Black)
                                UI_SemanticWidth(UI_SizePx(CircleWidth, 1.f))
                                UI_CornerRadii(V4F32(CircleHeight/2.f))
                                UI_AddBox(S8("Circle"), (UI_BoxFlag_Clip|
                                                         UI_BoxFlag_DrawBackground|
                                                         UI_BoxFlag_AnimatePosX));
                        }
                    }
                }
            }
    }
    
    return Clicked;
}
