//~ Misc
internal inline b32
EqualsWithEpsilon(f32 A, f32 B, f32 Epsilon)
{
    b32 Result = (A < (B + Epsilon) && 
                  A > (B - Epsilon));
    return Result;
}

//~ Functions
internal ui_size
UI_Size(ui_size_kind Kind, f32 Value, f32 Strictness)
{
    ui_size Size = {0};
    Size.Kind = Kind;
    Size.Value = Value;
    Size.Strictness = Strictness;
    return Size;
}

internal ui_box *
UI_BoxAlloc(arena *Arena)
{
    ui_box *Result = PushStructZero(Arena, ui_box);
    Result->First = Result->Last = Result->Next = Result->Prev = Result->Parent = UI_NilBox;
    Result->HashNext = Result->HashPrev = UI_NilBox;
    
    return Result;
}

internal b32
UI_IsNilBox(ui_box *Box)
{
    b32 Result = (Box == 0) || (Box == UI_NilBox);
    return Result;
}

internal b32
UI_IsFloatingBox(ui_box *Box, axis2 Axis)
{
    b32 Floating = ((Box->Flags & UI_BoxFlag_FloatingX && Axis == Axis2_X) ||
                    (Box->Flags & UI_BoxFlag_FloatingY && Axis == Axis2_Y));
    
    return Floating;
}

internal void
UI_PushBox(void)
{
    // NOTE(luca): You may not double push cause it makes no sense
    Assert(!UI_State->AppendToParent);
    
    UI_State->AppendToParent = true;
}

internal void
UI_PopBox(void)
{
    UI_State->Current = UI_State->Current->Parent;
}

#define UI_Push() DeferLoop(UI_PushBox(), UI_PopBox()) 

internal ui_key
UI_KeyNull(void)
{
    ui_key Result = {0};
    return Result;
}

internal b32
UI_KeyMatch(ui_key A, ui_key B)
{
    b32 Result = (A.U64[0] == B.U64[0]);
    return Result;
}

internal b32
UI_IsActive(ui_box *Box)
{
    b32 Result = UI_KeyMatch(UI_State->Active, Box->Key);
    return Result;
}

internal b32
UI_IsHot(ui_box *Box)
{
    b32 Result = UI_KeyMatch(UI_State->Hot, Box->Key);
    return Result;
}

internal ui_box *
UI_BoxFromKey(ui_key Key)
{
    u64 Slot = (Key.U64[0] % UI_State->BoxTableSize);
    ui_box *HashBox = UI_State->BoxTable + Slot;
    
    while(!UI_KeyMatch(HashBox->Key, Key) &&
          !UI_IsNilBox(HashBox))
    {
        HashBox = HashBox->Next;
    }
    
    return HashBox;
}

internal ui_box *
UI_AddBox(str8 String, s32 Flags)
{
    str8 DisplayString = String;
    ui_box *Box;
    ui_key Key = UI_KeyNull();
    b32 FirstTime = true;
    
    if(String.Size)
    {
        for EachIndex(Idx, String.Size - 1)
        {
            if(S8Match(S8From(String, Idx), S8("###"), true))
            {
                DisplayString = S8To(String, Idx);
                
                String = S8From(String, Idx + 3);
                break;
            }
            if(S8Match(S8From(String, Idx), S8("##"), true))
            {
                DisplayString = S8To(String, Idx);
                break;
            }
        }
        
        // TODO(luca): Seed with sibling's hash as well?
        // if parent key is null, you sould find top most parent whose key is not null
        
        ui_box *Search = UI_NilBox;
        
        for(Search = UI_State->Current; 
            UI_KeyMatch(Search->Key, UI_KeyNull()) &&
            Search != UI_State->Root;
            Search = Search->Parent);
        
        u64 AncestorKey = Search->Key.U64[0];
        Key.U64[0] = U64HashFromSeedStr8(AncestorKey, String);
        
        // Get the box based on key
        {    
            u64 Slot = (Key.U64[0] % UI_State->BoxTableSize);
            ui_box *HashBox = UI_State->BoxTable + Slot;
            ui_box *HashLast = 0;
            
            // There are three scenarios
            //1. We find the slot empty
            //2. We find the slot occupied
            //2.1 The key matches the slot or one from the linked list
            //2.2 The key did not match
            
            if(UI_KeyMatch(UI_KeyNull(), HashBox->Key))
            {
                // 1.
                Box = HashBox;
                Box->HashNext = Box->HashPrev = UI_NilBox;
            }
            else
            {
                //2.
                
                while(!UI_IsNilBox(HashBox))
                {        
                    if(UI_KeyMatch(HashBox->Key, Key))
                    {
                        break;
                    }
                    
                    HashLast = HashBox;
                    HashBox = HashBox->HashNext;
                }
                
                if(!UI_IsNilBox(HashBox))
                {
                    //2.1
                    Box = HashBox;
                    FirstTime = false;
                }
                else
                {
                    //2.2
                    HashLast->HashNext = PushStruct(UI_State->Arena, ui_box);
                    Box = HashLast->HashNext;
                    Box->HashPrev = HashLast;
                }
            }
        }
    }
    else
    {
        Box = PushStruct(UI_FrameArena, ui_box);
    }
    
    Box->First = Box->Last = Box->Next = Box->Prev = Box->Parent = UI_NilBox;
    
    Box->Key = Key;
    Box->DisplayString = DisplayString;
    Box->Flags = Flags;
    
    Box->BackgroundColor = UI_State->BackgroundColorTop->Value;
    Box->BorderColor = UI_State->BorderColorTop->Value;
    Box->TextColor = UI_State->TextColorTop->Value;
    Box->BorderThickness = UI_State->BorderThicknessTop->Value;
    Box->Softness = UI_State->SoftnessTop->Value;
    Box->CornerRadii = UI_State->CornerRadiiTop->Value;
    Box->LayoutAxis = UI_State->LayoutAxisTop->Value;
    Box->SemanticSize[Axis2_X] = UI_State->SemanticWidthTop->Value;
    Box->SemanticSize[Axis2_Y] = UI_State->SemanticHeightTop->Value;
    Box->HeightPx = UI_State->HeightPxTop->Value;
    Box->FontKind = UI_State->FontKindTop->Value;
    Box->CustomDraw = 0;
    Box->CustomDrawData = 0;
    Box->Clicked = false;
    Box->Hovered = false;
    Box->Pressed = false;
    MemoryZero(&Box->Scroll);
    
    if(!FirstTime)
    {
        app_input *Input = UI_State->Input;
        
        if(!Input->Consumed)
        {        
            if(Box->Flags & UI_BoxFlag_MouseClickable)
            {                
                v2 MouseP = MousePosFromInput(Input);
                
                app_button_state MouseLeft = Input->Mouse.Buttons[PlatformMouseButton_Left];
                
                Box->Hovered = IsInsideRectV2(MouseP, Box->Rec);
                Box->Pressed = (Box->Hovered && MouseLeft.EndedDown);
                
                // Set active and hot state
                {            
                    b32 MouseUp = (!MouseLeft.EndedDown);
                    
                    if(Box->Hovered)
                    {
                        if(UI_IsActive(UI_NilBox) ||
                           UI_IsActive(Box)) 
                        {
                            UI_State->Hot = Box->Key;
                        }
                    }
                    else if(UI_IsHot(Box))
                    {
                        UI_State->Hot = UI_KeyNull();
                    }
                    
                    if(UI_IsActive(Box))
                    {
                        if(MouseUp)
                        {
                            if(UI_IsHot(Box))
                            {
                                Box->Clicked = true;
                            }
                            UI_State->Active = UI_KeyNull();
                        }
                    }
                    else if(UI_IsHot(Box))
                    {
                        if(MouseLeft.EndedDown)
                        {
                            UI_State->Active = Box->Key;
                        }
                    }
                    
                }
                
                f32 Speed = 20.f;
                f32 HotTarget    = UI_IsHot(Box)    ? 1.f : 0.f;
                f32 ActiveTarget = UI_IsActive(Box) ? 1.f : 0.f;
                Box->tActive += (ActiveTarget - Box->tActive) * Speed * Input->dtForFrame;
                Box->tHot += (HotTarget - Box->tHot) * Speed * Input->dtForFrame;
            }
            
            if(Box->Flags & UI_BoxFlag_Scroll)
            {            
                if(Box->Pressed)
                {
                    Box->Scroll.X = (Input->Mouse.X - Input->Mouse.StartX);
                    Box->Scroll.Y = (Input->Mouse.Y - Input->Mouse.StartY);
                }
            }
        }
    }
    
    // Add box to the tree
    {    
        if(UI_State->AppendToParent)
        {
            Box->Parent = UI_State->Current;
            
            Box->Parent->First = Box;
            Box->Parent->Last = Box;
            
            UI_State->AppendToParent = false;
        }
        else
        {
            Box->Parent = UI_State->Current->Parent;
            
            Box->Parent->Last->Next = Box;
            Box->Prev = Box->Parent->Last;
            
            Box->Parent->Last = Box;
        }
        
        UI_State->Current = Box;
    }
    
    return Box;
}

internal rune 
UI_GetShiftForFont(font_kind Kind)
{
    rune Result = 0;
    
    font_atlas *Atlas = UI_State->Atlas;
    
    switch(Kind)
    {        
        case FontKind_Text:
        {
            Result = 0;
        } break;
        case FontKind_Icon:
        {        
            Result = (Atlas->FirstCodepoint + 
                      Atlas->CodepointsCount -
                      Atlas->IconsFirstCodepoint);
        } break;
        default: break;
    }
    
    return Result;
}

internal f32
UI_MeasureTextWidth(str8 String, font_kind FontKind)
{
    f32 Result = 0.f;
    
    font_atlas *Atlas = UI_State->Atlas;
    
    rune Shift = UI_GetShiftForFont(FontKind) - Atlas->FirstCodepoint;
    
    for EachIndex(Idx, String.Size)
    {
        rune Char = (rune)(String.Data[Idx]) + Shift;
        f32 CharWidth = (Atlas->PackedChars[Char].xadvance);
        
        Result += CharWidth;
    }
    
    return Result;
}

internal void
UI_DefaultState(ui_box *Root, f32 HeightPx)
{
    UI_State->Current = Root;
    UI_State->Root = Root;
    UI_State->AppendToParent = true;
    
    // Defaults
    // NOTE(luca): This is slightly since what we should be doing here is *setting* and not *pushing*.  But since we don't modify the top item this shouldn't be a problem in practice.t
    UI_PushBackgroundColor(Color_Background);
    UI_PushTextColor(Color_ButtonText);
    UI_PushBorderColor(Color_ButtonBorder);
    UI_PushSoftness(0.f);
    UI_PushBorderThickness(2.f);
    UI_PushCornerRadii(V4F32(0.f));
    UI_PushLayoutAxis(Axis2_X);
    UI_PushSemanticWidth(UI_SizeParent(1.f, 1.f));
    UI_PushSemanticHeight(UI_SizeParent(1.f, 1.f));
    UI_PushHeightPx(HeightPx);
    UI_PushFontKind(FontKind_Text);
}

//~ Calculations Start 

internal void
UI_CalculateStandaloneSizes(ui_box *Box, axis2 Axis)
{
    switch(Box->SemanticSize[Axis].Kind)
    {
        default: {}break;
        case UI_SizeKind_Pixels:
        {
            Box->FixedSize.e[Axis] = Box->SemanticSize[Axis].Value;
        } break;
        case UI_SizeKind_TextContent:
        {
            // TODO(luca): This is the only part that needs the UI_State variable, if we can rid of it, we could get totally UI_State agnostic layout computation. 
            if(Axis == Axis2_X)
            {
                Box->FixedSize.e[Axis] = (UI_MeasureTextWidth(Box->DisplayString, Box->FontKind) + 
                                          2.f*Box->SemanticSize[Axis].Value);
            }
            else
            {
                Box->FixedSize.e[Axis] = (UI_State->Atlas->HeightPx + 
                                          2.f*Box->SemanticSize[Axis].Value);
            }
        } break;
    }
    
    if(!UI_IsNilBox(Box->Next))
    {
        UI_CalculateStandaloneSizes(Box->Next, Axis);
    }
    if(!UI_IsNilBox(Box->First))
    {
        UI_CalculateStandaloneSizes(Box->First, Axis);
    }
}

internal void
UI_CalculateUpwardSizes(ui_box *Box, axis2 Axis)
{
    if(Box->SemanticSize[Axis].Kind == UI_SizeKind_PercentOfParent)
    {
        
        if(Box == UI_State->DebugBox);
        
        Box->FixedSize.e[Axis] = (Box->SemanticSize[Axis].Value * 
                                  Box->Parent->FixedSize.e[Axis]);
    }
    
    if(!UI_IsNilBox(Box->First))
    {
        UI_CalculateUpwardSizes(Box->First, Axis);
    }
    if(!UI_IsNilBox(Box->Next))
    {
        UI_CalculateUpwardSizes(Box->Next, Axis);
    }
}

internal void
UI_CalculateDownwardSizes(ui_box *Box, axis2 Axis)
{
    if(!UI_IsNilBox(Box->First))
    {
        UI_CalculateDownwardSizes(Box->First, Axis);
    }
    if(!UI_IsNilBox(Box->Next))
    {
        UI_CalculateDownwardSizes(Box->Next, Axis);
    }
    
    if(Box->SemanticSize[Axis].Kind == UI_SizeKind_ChildrenSum)
    {
        f32 TotalSize = 0.f;
        
        ui_box *FirstNonFloatingChild = UI_NilBox;
        for UI_EachBox(Child, Box->First)
        {
            if(!UI_IsFloatingBox(Child, Axis))
            {
                FirstNonFloatingChild = Child;
                break;
            }
        }
        
        for UI_EachBox(Child, Box->First)
        {
            b32 IsFirstChild = (Child == FirstNonFloatingChild);
            b32 IsAligned = (Box->LayoutAxis == Axis);
            
            if(IsFirstChild)
            {
                TotalSize += Child->FixedSize.e[Axis];
            }
            else if(!UI_IsFloatingBox(Child, Axis))
            {
                if(IsAligned)
                {
                    TotalSize += Child->FixedSize.e[Axis];
                }
                else
                {
                    TotalSize = Max(TotalSize, Child->FixedSize.e[Axis]);
                }
            }
        }
        
        Box->FixedSize.e[Axis] = TotalSize;
    }
}

internal void
UI_CalculateViolations(ui_box *Box, axis2 Axis)
{
    
    ui_box *Parent = Box->Parent;
    
    f32 ParentSize = Parent->FixedSize.e[Axis];
    
    // TODO(luca): This is inefficient since the violations should be solved once per level.
    f32 TotalSize = 0.f;
    {    
        for UI_EachBox(Child, Parent->First)
        {
            if(Child->Parent->LayoutAxis == Axis)
            {            
                if(!UI_IsFloatingBox(Child, Axis))
                {
                    TotalSize += Child->FixedSize.e[Axis];
                }
            }
        }
    }
    
    f32 ViolationSize = TotalSize - ParentSize;
    
    if(ViolationSize > 0.f)
    {
        // NOTE(luca): Take size from all the siblings which have strictnes < 1.f
        
        ui_box *Child = Parent->First;
        // TODO(luca): epsilon compare?
        while(!UI_IsNilBox(Child) && ViolationSize != 0.f)
        {
            if(!UI_IsFloatingBox(Child, Axis))
            {                
                f32 Strictness = Child->SemanticSize[Axis].Strictness;
                f32 AllowedSizeToTake = ((1.f - Strictness)*Child->FixedSize.e[Axis]);
                f32 TakenSize = Min(ViolationSize, AllowedSizeToTake);
                
                Child->FixedSize.e[Axis] = Child->FixedSize.e[Axis] - TakenSize;
                ViolationSize -= TakenSize;
            }
            
            Child = Child->Next;
        }
        
        if(!EqualsWithEpsilon(ViolationSize, 0.f, .001f))
        {
#if 0
            DebugBreak();
            ErrorLog("Clipping");
#endif
        }
        
    }
    
    if(!UI_IsNilBox(Box->Next))
    {
        UI_CalculateViolations(Box->Next, Axis);
    }
    
    if(!UI_IsNilBox(Box->First))
    {
        UI_CalculateViolations(Box->First, Axis);
    }
}


internal void
UI_CalculatePositions(ui_box *Box)
{
    ui_box *Parent = Box->Parent;
    
    // TODO(luca): This won't work since the root box will never be passed only the first child of that box, so its index will never be set.
    Box->LastTouchedFrameIdx = UI_State->FrameIdx;
    
    if(Parent->First == Box)
    {
        Box->FixedPosition.X = Parent->FixedPosition.X;
        Box->FixedPosition.Y = Parent->FixedPosition.Y;
    }
    else
    {  
        axis2 Axis = Parent->LayoutAxis;
        Assert(Axis == Axis2_X || Axis == Axis2_Y);
        axis2 OtherAxis = 1 - Axis;
        
        ui_box *NonFloatingSibling = UI_NilBox;
        for(ui_box *Sibling = Box->Prev; !UI_IsNilBox(Sibling); Sibling = Sibling->Prev)
        {
            if(!UI_IsFloatingBox(Sibling, Axis2_X))
            {
                NonFloatingSibling = Sibling;
                break;
            }
        }
        
        if(!UI_IsNilBox(NonFloatingSibling))
        {
            Box->FixedPosition.e[Axis] = (NonFloatingSibling->FixedPosition.e[Axis] + 
                                          NonFloatingSibling->FixedSize.e[Axis]);
            Box->FixedPosition.e[OtherAxis] = (NonFloatingSibling->FixedPosition.e[OtherAxis]);
        }
        else
        {
            Box->FixedPosition = Parent->FixedPosition;
        }
        
        if(UI_IsFloatingBox(Box, Axis2_X))
        {
            Box->FixedPosition.X = NonFloatingSibling->FixedPosition.X;
        }
        if(UI_IsFloatingBox(Box, Axis2_Y))
        {
            Box->FixedPosition.Y = NonFloatingSibling->FixedPosition.Y;
        }
    }
    
    Box->Rec = RectFromSize(Box->FixedPosition, Box->FixedSize);
    
    if(!UI_IsNilBox(Box->First))
    {
        UI_CalculatePositions(Box->First);
    }
    if(!UI_IsNilBox(Box->Next))
    {
        UI_CalculatePositions(Box->Next);
    }
}

internal void
UI_DrawBoxes(ui_box *Box)
{
    ui_box *Parent = Box->Parent;
    
    font_atlas *Atlas = UI_State->Atlas;
    
    v4 Dest = Box->Rec;
    
    if(Box->Flags & UI_BoxFlag_Clip)
    {
        Dest = RectIntersect(Parent->Rec, Dest);
        Box->Rec = Dest;
    }
    
    if(RectValid(Dest))
    {    
        if(Box->Flags & UI_BoxFlag_DrawShadow)
        {
            f32 ShadowSize = 4.f;
            v4 ShadowDest = RectV2(V2AddF32(Dest.Min, ShadowSize), 
                                   V2AddF32(Dest.Max, ShadowSize)); 
            rect_instance *Inst = DrawRect(ShadowDest, Color_Black, 0.f, ShadowSize, .5f*ShadowSize);
            Inst->CornerRadii = Box->CornerRadii;
        }
        
        if(Box->Flags & UI_BoxFlag_DrawBackground)
        {    
            rect_instance *Inst = DrawRect(Dest, Box->BackgroundColor, 0.f, 0.f, Box->Softness);
            Inst->CornerRadii = Box->CornerRadii;
            
            if(Box->Flags & UI_BoxFlag_DrawHotEffects)
            {
                V3Math Inst->Color0.E *= 1.f - .4f*(Box->tHot - Box->tActive);
                V3Math Inst->Color1.E *= 1.f - .4f*(Box->tHot - Box->tActive);
            }
            
            if(Box->Flags & UI_BoxFlag_DrawActiveEffects)
            {
                V3Math Inst->Color2.E *= 1.f - .4f*(Box->tActive);
                V3Math Inst->Color3.E *= 1.f - .4f*(Box->tActive);
            }
        }
        
        if(Box->Flags & UI_BoxFlag_DrawDisplayString)
        {
            v2 TextPos = Box->FixedPosition;
            TextPos.Y += Box->BorderThickness;
            TextPos.X += Box->BorderThickness;
            
            v2 Cur = TextPos;
            
            if(Box->Flags & UI_BoxFlag_CenterTextHorizontally)
            {
                f32 TextWidth = UI_MeasureTextWidth(Box->DisplayString, Box->FontKind);
                Cur.X += .5f*((Box->FixedSize.X - 2.f*Box->BorderThickness) - TextWidth);
            }
            
            if(Box->Flags & UI_BoxFlag_CenterTextVertically)
            {
                f32 TextHeight = Atlas->HeightPx;
                Cur.Y += .5f*((Box->FixedSize.Y - 2.f*Box->BorderThickness) - TextHeight);
            }
            
            rune Shift = UI_GetShiftForFont(Box->FontKind);
            
            for EachIndex(Idx, Box->DisplayString.Size)
            {
                rune Char = (rune)(Box->DisplayString.Data[Idx]) + Shift;
                f32 CharWidth = (Atlas->PackedChars[Char - Atlas->FirstCodepoint].xadvance);
                f32 CharHeight = (Atlas->HeightPx);
                
                v2 CurMax = V2AddV2(Cur, V2(CharWidth, CharHeight));
                if(IsInsideRectV2(Cur, Dest) &&
                   IsInsideRectV2(CurMax, Dest))
                {
                    DrawRectChar(Atlas, Cur, Char, Box->TextColor);
                    
                    Cur.X += CharWidth;
                }
            }
        }
        
        if(Box->Flags & UI_BoxFlag_DrawBorders)
        {
            b32 Selected = (Box->Flags & UI_BoxFlag_DrawHotEffects &&
                            (UI_IsHot(Box) || UI_IsActive(Box)));
            
            if(Selected)
            {
                Box->BorderColor = Color_Snow2;
            }
            rect_instance *Inst = DrawRect(Dest, Box->BorderColor, 0.f, Box->BorderThickness, Box->Softness);
            V4Math Inst->CornerRadii.E = Box->CornerRadii.E;
        }
        
        
        if(Box->CustomDraw)
        {
            Box->CustomDraw(Box->CustomDrawData);
        }
    }
    
    b32 RectDebugMode = false;
    if(RectDebugMode || Box->Flags & UI_BoxFlag_DrawDebugBorder)
    {    
        DrawRect(Dest, V4(1.f, 0.f, 1.f, 1.f), 0.f, 1.f, 0.f);
    }
    
    if(!UI_IsNilBox(Box->First))
    {
        UI_DrawBoxes(Box->First);
    }
    if(!UI_IsNilBox(Box->Next))
    {
        UI_DrawBoxes(Box->Next);
    }
}

global_variable s32 UI_DebugIndentation = 0;

internal void
UI_DebugPrintBoxes(ui_box *Box)
{
    Log("%*s\"" S8Fmt "\":\n"
        "%*s %d,%d\n"
        "%*s %.0f,%.0f %.0fx%.0f\n"
        ,
        UI_DebugIndentation, "", S8Arg(Box->DisplayString),
        UI_DebugIndentation, "", Box->SemanticSize[0].Kind, Box->SemanticSize[1].Kind, 
        UI_DebugIndentation, "", 
        Box->FixedPosition.X, Box->FixedPosition.Y, Box->FixedSize.X, Box->FixedSize.Y);
    
    if(!UI_IsNilBox(Box->First))
    {
        UI_DebugIndentation += 1;
        UI_DebugPrintBoxes(Box->First);
        UI_DebugIndentation -= 1;
    }
    
    if(!UI_IsNilBox(Box->Next))
    {
        UI_DebugPrintBoxes(Box->Next);
    }
}

//~ Calculations End

internal void
UI_ResolveLayout(ui_box *Root)
{
    if(!UI_IsNilBox(Root))
    { 
        Root->LastTouchedFrameIdx = UI_State->FrameIdx;
        
        for EachIndex(Idx, (s32)Axis2_Count)
        {        
            axis2 Axis = (axis2)Idx;
            
            UI_CalculateStandaloneSizes(Root, Axis);
            UI_CalculateUpwardSizes(Root, Axis);
            UI_CalculateDownwardSizes(Root, Axis);
            UI_CalculateViolations(Root, Axis);
        }
        UI_CalculatePositions(Root);
        UI_DrawBoxes(Root);
    }
}
