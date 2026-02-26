internal ui_size
UI_Size(ui_size_kind Kind, f32 Value, f32 Strictness)
{
    ui_size Size = {};
    Size.Kind = Kind;
    Size.Value = Value;
    Size.Strictness = Strictness;
    return Size;
}

#define DeferLoop(Begin, End)        for(int _i_ = ((Begin), 0); !_i_; _i_ += 1, (End))


#define UI_SizePx(Value, Strictness) UI_Size(UI_SizeKind_Pixels, Value, Strictness)
#define UI_SizeText(Value, Strictness) UI_Size(UI_SizeKind_TextContent, Value, Strictness)
#define UI_SizeEm(Value, Strictness) UI_Size(UI_SizeKind_Pixels, Value*HeightPx, Strictness)
#define UI_SizeParent(Value, Strictness) UI_Size(UI_SizeKind_PercentOfParent, Value, Strictness)
#define UI_SizeChildren(Strictness) UI_Size(UI_SizeKind_ChildrenSum, 0.f, Strictness)

internal b32
UI_IsNilBox(ui_box *Box)
{
    b32 Result = (Box == 0) || (Box == UI_NilBox);
    return Result;
}

internal void
UI_PushBox(void)
{
    // NOTE(luca): You may not double push cause it makes no sense
    Assert(!UI_State->AppendToParent);
    
    UI_State->AppendToParent = true;
}

internal ui_key
UI_KeyNull(void)
{
    ui_key Result = {};
    return Result;
}

internal b32
UI_KeyMatch(ui_key A, ui_key B)
{
    b32 Result = (A.U64[0] == B.U64[0]);
    return Result;
}

internal ui_box *
UI_AddBox(str8 String, u32 Flags)
{
    str8 DisplayString = String;
    ui_box *Box;
    ui_key Key = UI_KeyNull();
    b32 FirstTime = true;
    
    if(String.Size)
    {
        u64 HashCutOff = 0;
        for EachIndex(Idx, String.Size - 1)
        {
            if(S8Match(S8FromTo(String, Idx, Idx + 1), S8("##"), true))
            {
                DisplayString = S8To(String, Idx);
                break;
            }
        }
        
        // TODO(luca): Seed with sibling's hash as well?
        u64 AncestorKey = UI_State->Current->Parent->Key.U64[0];
        Key = {U64HashFromSeedStr8(AncestorKey, String)};
        
        // Get the box based on key
        {    
            u64 Slot = (Key.U64[0] % UI_BoxTableSize);
            ui_box *HashBox = UI_BoxTable + Slot;
            ui_box *HashLast = 0;
            
            // There are three scenario's
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
                while(!UI_IsNilBox(HashBox))
                {        
                    if(UI_KeyMatch(HashBox->Key, Key))
                    {
                        break;
                    }
                    
                    HashLast = HashBox;
                    HashBox = HashBox->Next;
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
                    HashLast->HashNext = PushStruct(UI_BoxArena, ui_box);
                    Box = HashLast->HashNext;
                    Box->HashPrev = HashLast;
                }
            }
        }
    }
    else
    {
        Box = PushStruct(FrameArena, ui_box);
    }
    
    Box->First = Box->Last = Box->Next = Box->Prev = Box->Parent = UI_NilBox;
    
    Box->Key = Key;
    Box->DisplayString = DisplayString;
    Box->Flags = Flags;
    Box->LastFrameTouchedIndex = UI_State->FrameIndex;
    
    Box->BackgroundColor = UI_State->BackgroundColorTop->Value;
    Box->BorderColor = UI_State->BorderColorTop->Value;
    Box->TextColor = UI_State->TextColorTop->Value;
    Box->BorderThickness = UI_State->BorderThicknessTop->Value;
    Box->Softness = UI_State->SoftnessTop->Value;
    Box->CornerRadii = UI_State->CornerRadiiTop->Value;
    Box->LayoutAxis = UI_State->LayoutAxisTop->Value;
    Box->SemanticSize[Axis2_X] = UI_State->SemanticWidthTop->Value;
    Box->SemanticSize[Axis2_Y] = UI_State->SemanticHeightTop->Value;
    
    if(!FirstTime)
    {
        v2 MouseP = V2S32(UI_State->Input->MouseX, UI_State->Input->MouseY);
        
        b32 Hovered = IsInsideRectV2(MouseP, RectFromSize(Box->FixedPosition, Box->FixedSize));
        b32 Clicked = (Hovered && (WasPressed(UI_State->Input->MouseButtons[PlatformMouseButton_Left]))); 
        b32 Pressed = (Hovered && UI_State->Input->MouseButtons[PlatformMouseButton_Left].EndedDown);
        
        Box->Clicked = Clicked;
        Box->Hovered = Hovered;
        Box->Pressed = Pressed;
    }
    
    // Add box to the tree
    {    
        if(UI_State->AppendToParent)
        {
            UI_State->Current->First = Box;
            Box->Parent = UI_State->Current;
        }
        else
        {    
            Box->Parent = UI_State->Current->Parent;
            
            if(!UI_IsNilBox(Box->Parent))
            {
                Box->Parent->Last = Box;
            }
            
            Box->Prev = UI_State->Current;
            UI_State->Current->Next = Box;
        }
        
        UI_State->Current = Box;
        
        UI_State->AppendToParent = false;
    }
    
    return Box;
}

internal void
UI_PopBox()
{
    UI_State->Current = UI_State->Current->Parent;
}

internal f32
UI_MeasureTextWidth(str8 String)
{
    f32 Result = 0.f;
    
    for EachIndex(Idx, String.Size)
    {
        u8 Char = String.Data[Idx];
        f32 CharWidth = (UI_State->Atlas->PackedChars[Char].xadvance);
        
        Result += CharWidth;
    }
    
    return Result;
}

//- Calculations Start 

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
            if(Axis == Axis2_X)
            {
                Box->FixedSize.e[Axis] = (UI_MeasureTextWidth(Box->DisplayString) + 
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
        ui_box *Child = Box->First;
        
        f32 TotalSize = 0.f;
        while(!UI_IsNilBox(Child))
        {
            b32 IsFirstChild = (Child == Box->First);
            b32 IsPreviousAligned = (Child->Prev->LayoutAxis == Axis); 
            if(IsFirstChild || IsPreviousAligned)
            {                
                TotalSize += Child->FixedSize.e[Axis];
            }
            
            Child = Child->Next;
        }
        
        Box->FixedSize.e[Axis] = TotalSize;
    }
    
}

internal void
UI_CalculateViolations(ui_box *Box, axis2 Axis)
{
    NotImplemented();
    
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
UI_CalculatePositionsAndDrawBoxes(ui_box *Box)
{
    if(Box->Parent->First == Box)
    {
        Box->FixedPosition.X = Box->Parent->FixedPosition.X;
        Box->FixedPosition.Y = Box->Parent->FixedPosition.Y;
    }
    else
    {            
        switch(Box->LayoutAxis)
        {
            default: InvalidPath(); break;
            case Axis2_X:
            {
                Box->FixedPosition.X = (Box->Prev->FixedPosition.X + 
                                        Box->Prev->FixedSize.X);
                Box->FixedPosition.Y = (Box->Prev->FixedPosition.Y);
            } break;
            case Axis2_Y:
            {
                Box->FixedPosition.X = (Box->Prev->FixedPosition.X);
                Box->FixedPosition.Y = (Box->Prev->FixedPosition.Y +
                                        Box->Prev->FixedSize.Y);
                
            } break;
        }
    }
    
    rect Dest = RectFromSize(Box->FixedPosition, Box->FixedSize);
    
    if(Box->Flags & UI_BoxFlag_DrawBackground)
    {    
        rect_instance *Inst = DrawRect(Dest, Box->BackgroundColor, 0.f, 0.f, Box->Softness);
        V4Math Inst->CornerRadii.E = Box->CornerRadii.E;
        
        if(Box->Flags & UI_BoxFlag_AnimateColorOnHover)
        {
            if(Box->Hovered)
            {
                Inst->Color0.W = .2f;
                Inst->Color1.W = .2f;
                
                if(Box->Pressed)
                {
                    Inst->Color0.W = 1.f;
                    Inst->Color1.W = 1.f;
                    Inst->Color2.W = .2f;
                    Inst->Color3.W = .2f;
                }
                
            }
        }
    }
    
    
    if(Box->Flags & UI_BoxFlag_DrawDisplayString)
    {
        v2 TextPos = Box->FixedPosition;
        TextPos.Y += Box->BorderThickness;
        TextPos.X += Box->BorderThickness;
        
        if(Box->Flags & UI_BoxFlag_CenterTextHorizontally)
        {
            f32 TextWidth = UI_MeasureTextWidth(Box->DisplayString);
            TextPos.X += .5f*((Box->FixedSize.X - 2.f*Box->BorderThickness) - TextWidth);
        }
        
        if(Box->Flags & UI_BoxFlag_CenterTextVertically)
        {
            f32 TextHeight = UI_State->Atlas->HeightPx;
            TextPos.Y += .5f*((Box->FixedSize.Y - 2.f*Box->BorderThickness) - TextHeight);
        }
        
        for EachIndex(Idx, Box->DisplayString.Size)
        {
            rune Char = Box->DisplayString.Data[Idx];
            
            f32 CharWidth = (UI_State->Atlas->PackedChars[Char].xadvance);
            
            DrawRectChar(UI_State->Atlas, TextPos, Char, Color_ButtonText);
            
            TextPos.X += CharWidth;
        }
    }
    
    if(Box->Flags & UI_BoxFlag_DrawBorders)
    {    
        rect_instance *Inst = DrawRect(Dest, Box->BorderColor, 0.f, Box->BorderThickness, Box->Softness);
        V4Math Inst->CornerRadii.E = Box->CornerRadii.E;
    }
    
    if(UI_State->RectDebugMode || Box->Flags & UI_BoxFlag_DrawDebugBorder)
    {    
        DrawRect(Dest, V4(1.f, 0.f, 1.f, 1.f), 0.f, 1.f, 0.f);
    }
    
    if(!UI_IsNilBox(Box->First))
    {
        UI_CalculatePositionsAndDrawBoxes(Box->First);
    }
    if(!UI_IsNilBox(Box->Next))
    {
        UI_CalculatePositionsAndDrawBoxes(Box->Next);
    }
}

global_variable u32 UI_DebugIndentation = 0;

internal void
UI_DebugPrintBoxes(ui_box *Box)
{
    Log("%*s" S8Fmt ":\n"
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

//- Calculations End

internal void
UI_InitState(panel *Panel, app_input *Input, app_state *App, b32 DebugMode)
{
    ui_box *Root = PushStructZero(FrameArena, ui_box);
    Root->Parent = Root->First = Root->Next = Root->Last = UI_NilBox;
    Root->FixedPosition = Panel->Region.Min;
    Root->FixedSize = SizeFromRect(Panel->Region);
    
    UI_State->Root = Root;
    UI_State->Current = Root;
    UI_State->AppendToParent = false;
    UI_State->Input = Input;
    UI_State->Atlas = &App->FontAtlas;
    UI_State->FrameIndex = App->FrameIndex;
    UI_State->RectDebugMode = DebugMode;
    
    // Defaults
    UI_PushBackgroundColor(Color_ButtonBackground);
    UI_PushTextColor(Color_ButtonText);
    UI_PushBorderColor(Color_ButtonBorder);
    UI_PushSoftness(.5f);
    UI_PushBorderThickness(2.f);
    UI_PushCornerRadii(V4(5.f, 5.f, 5.f, 5.f));
    UI_PushLayoutAxis(Axis2_X);
    UI_PushSemanticWidth(UI_SizeParent(1.f, 1.f));
    UI_PushSemanticHeight(UI_SizeParent(1.f, 1.f));
    
    UI_PushBox();
}

internal void
UI_ResolveLayout(ui_box *Root)
{
    for EachIndex(Idx, Axis2_Count)
    {        
        axis2 Axis = (axis2)Idx;
        
        UI_CalculateStandaloneSizes(Root->First, Axis);
        UI_CalculateUpwardSizes(Root->First, Axis);
        UI_CalculateDownwardSizes(Root->First, Axis);
        //UI_CalculateViolations(Root->First, Axis);
    }
    UI_CalculatePositionsAndDrawBoxes(Root->First);
    //UI_DebugPrintBoxes(Root->First);
}
