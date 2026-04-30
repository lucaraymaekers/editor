
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

#define UI_Row() DeferLoop(UI_PushContainer(Axis2_X), UI_PopBox())
#define UI_Column() DeferLoop(UI_PushContainer(Axis2_Y), UI_PopBox())
#define UI_Padding(Size) DeferLoop(UI_Spacer(Size), UI_Spacer(Size))
