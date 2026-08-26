//~ Misc
internal inline b32
EqualsWithEpsilon(f32 A, f32 B, f32 Epsilon)
{
 b32 Result = (A < (B + Epsilon) && 
               A > (B - Epsilon));
 return Result;
}

//~ Functions
internal void
UI_ConsumeInput(app_input *Input, ui_box *Box)
{
 Input->Consumed = true;
 UI_State->InputConsumerBox = Box;
}

internal b32
UI_IsNilBox(ui_box *Box)
{
 b32 Result = (Box == 0) || (Box == UI_NilBox);
 return Result;
}

internal b32
UI_IsDebugBox(ui_box *Box)
{
 b32 Result = false;
 
#if MUZE_INTERNAL    
 if(!UI_IsNilBox(Box))
 {
  for EachNode(Node, ui_box_node, UI_State->FirstDebugBox)
  {
   if(Node->Box == Box)
   {
    Result = true;
    break;
   }
  }
 }
#endif
 
 return Result;
}

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
 ui_box *Result = PushArrayZero(Arena, ui_box, 1);
 Result->First = Result->Last = Result->Next = Result->Prev = Result->Parent = UI_NilBox;
 Result->HashNext = Result->HashPrev = UI_NilBox;
 
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
 UI_State->AppendToParent = true;
}

internal void
UI_PopBox(void)
{
 UI_State->Current = UI_State->Current->Parent;
}

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

internal void
UI_SetHot(ui_key Key)
{
 UI_State->Hot = Key;
}

internal void
UI_SetActive(ui_key Key)
{
 UI_State->Active = Key;
}

internal ui_box *
UI_BoxFromKey(ui_key Key)
{
 ui_box *Result = UI_NilBox;
 
 if(!UI_KeyMatch(UI_KeyNull(), Key))
 {
  u64 Slot = (Key.U64[0] % UI_State->BoxTableSize);
  ui_box *HashBox = UI_State->BoxTable + Slot;
  
  while(!UI_IsNilBox(HashBox))
  {
   if(UI_KeyMatch(HashBox->Key, Key))
   {
    Result = HashBox;
    break;
   }
   HashBox = HashBox->Next;
  }
 }
 
 return Result;
}

internal ui_box *
UI_AddBox(str8 String, s32 Flags)
{
 str8 DisplayString = String;
 ui_box *Box;
 ui_key Key = UI_KeyNull();
 
 //- Compute the key 
 if(String.Size)
 {
  for EachIndex(Idx, String.Size)
  {
   u64 CharsLeft = String.Size - Idx;
   
   if(CharsLeft > 3 && S8Match(S8From(String, Idx), S8("###"), true))
   {
    DisplayString = S8To(String, Idx);
    String = S8From(String, Idx + 3);
    break;
   }
   
   if(CharsLeft > 2 && S8Match(S8From(String, Idx), S8("##"), true))
   {
    DisplayString = S8To(String, Idx);
    break;
   }
  }
  
  // TODO(luca): Seed with sibling's hash as well?
  
  ui_box *Search = UI_NilBox;
  // Find youngest parent with non-zero key
  {
   for(Search = UI_State->Current; 
       UI_KeyMatch(Search->Key, UI_KeyNull()) &&
       Search != UI_State->Root;
       Search = Search->Parent);
  }
  
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
    //1.
    Box = HashBox;
    
    // NOTE(luca): We need to clear inputs as well since we are erasing the entry.
    Box->WasClicked = Box->Clicked = Box->Hovered = Box->Pressed = false;
   }
   else
   {
    //2. 
    while(!UI_IsNilBox(HashBox))
    {        
     if(UI_KeyMatch(HashBox->Key, Key))
     {
      //2.1
      break;
     }
     
     HashLast = HashBox;
     HashBox = HashBox->HashNext;
    }
    
    if(UI_IsNilBox(HashBox))
    {
     //2.2
     HashBox = HashLast->HashNext = PushArray(UI_State->Arena, ui_box, 1);
     HashBox->HashPrev = HashLast;
    }
    
    Box = HashBox;
   }
  }
 }
 else
 {
  Box = PushArray(UI_State->FrameArenaFront, ui_box, 1);
 }
 
 Box->First = Box->Last = Box->Next = Box->Prev = Box->Parent = UI_NilBox;
 
 Box->Key = Key;
 Box->String = String;
 Box->DisplayString = DisplayString;
 Box->Flags = Flags;
 
 Box->LastTouchedFrameIdx = UI_State->FrameIdx;
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
 Box->Scroll = (v2){0};
 
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
 
 
 if(Box == Box->First ||
    Box == Box->Next || 
    Box == Box->Prev || 
    Box == Box->Last)
 {
  InvalidPath();
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

internal ui_box_rec
UI_BoxDepthFirstPreOrder(ui_box *Box)
{
 ui_box_rec Rec = {0};
 
 if(!UI_IsNilBox(Box->First))
 {
  Rec.Next = Box->First;
  Rec.PushCount = 1;
 }
 else for(ui_box *B = Box; !UI_IsNilBox(B); B = B->Parent)
 {
  if(!UI_IsNilBox(B->Next))
  {
   Rec.Next = B->Next;
   break;
  }
  Rec.PopCount += 1;
 }
 
 return Rec;
}

internal ui_box_rec
UI_BoxDepthFirstPostOrderBegin(ui_box *Root)
{
 ui_box_rec Result = {0};
 
 ui_box *Box = Root;
 while(true)
 {    
  if(!UI_IsNilBox(Box->First))
  {
   Box = Box->First;
   Result.PushCount += 1;
  }
  else if(!UI_IsNilBox(Box->Next))
  {
   Box = Box->Next;
  }
  else
  {
   Result.Next = Box;
   break;
  }
 }
 
 return Result;
}

internal ui_box_rec
UI_BoxDepthFirstPostOrder(ui_box *Box)
{
 ui_box_rec Result = {.Next = UI_NilBox};
 
 if(!UI_IsNilBox(Box->Next))
 {
  Box = Box->Next;
  while(!UI_IsNilBox(Box->First)) 
  {
   Box = Box->First;
   Result.PushCount += 1;
  }
  Result.Next = Box;
 }
 else
 {
  Result.Next = Box->Parent;
  Result.PopCount = 1;
 }
 
 return Result;
}

internal void
UI_DebugCheckWrongBox(ui_box *Root)
{
 for (ui_box *Box = Root;
      !UI_IsNilBox(Box);
      Box = UI_BoxDepthFirstPreOrder(Box).Next)
 {                            
  if(Box == Box->First ||
     Box == Box->Next || 
     Box == Box->Prev || 
     Box == Box->Last)
  {
   InvalidPath();
  }
 }
}

internal void
UI_BeginLayout(ui_box *Root, f32 HeightPx)
{
 Assert(!UI_IsNilBox(Root));
 
 UI_State->Current = Root;
 UI_State->Root = Root;
 
 ArenaSetPos(UI_State->StyleArena, 0);
 
 // Defaults
 // NOTE(luca): This is slightly since what we should be doing here is *setting* and not *pushing*.  But since we don't modify the top item this shouldn't be a problem in practice.t
 UI_PushBackgroundColor(Color_Background);
 UI_PushTextColor(Color_ButtonText);
 UI_PushBorderColor(Color_ButtonBorder);
 UI_PushSoftness(0.f);
 UI_PushBorderThickness(1.f);
 UI_PushCornerRadii(V4F32(0.f));
 UI_PushLayoutAxis(Axis2_X);
 UI_PushSemanticWidth(UI_SizeParent(1.f, 1.f));
 UI_PushSemanticHeight(UI_SizeParent(1.f, 1.f));
 UI_PushHeightPx(HeightPx);
 UI_PushFontKind(FontKind_Text);
 UI_PushBox();
 
 // Input 
 {        
  app_input *Input = UI_State->Input;
  v2 MouseP = MousePosFromInput(Input);
  app_button_state MouseLeft = Input->Mouse.Buttons[PlatformMouseButton_Left];
  b32 MouseUp = (!MouseLeft.EndedDown);
  
  for(ui_box *Box = UI_BoxDepthFirstPostOrderBegin(Root).Next;
      !UI_IsNilBox(Box);
      Box = UI_BoxDepthFirstPostOrder(Box).Next)
  {
   Box->Clicked = false;
   Box->Hovered = false;
   Box->Pressed = false;
   Box->WasClicked = false;
   
   Box->Hovered = IsInsideRectV2(MouseP, Box->Rec);
   
   b32 ReceiveInput = !Input->Consumed;
   if(ReceiveInput)
   {
    if(Box->Flags & UI_BoxFlag_MouseClickable)
    {                
     Box->Pressed = (Box->Hovered && MouseLeft.EndedDown);
     Box->WasClicked = (Box->Hovered && WasPressed(MouseLeft));
     
     // Set active and hot state
     {            
      if(Box->Hovered)
      {
       UI_ConsumeInput(Input, Box);
       
       if(UI_IsActive(UI_NilBox) ||
          UI_IsActive(Box)) 
       {
        UI_SetHot(Box->Key);
       }
      }
      else if(UI_IsHot(Box) && !UI_IsActive(Box))
      {
       UI_SetHot(UI_KeyNull());
      }
      
      if(UI_IsActive(Box))
      {
       if(MouseUp)
       {
        if(UI_IsHot(Box))
        {
         Box->Clicked = true;
        }
        UI_SetActive(UI_KeyNull());
       }
      }
      else if(UI_IsHot(Box))
      {
       if(Box->WasClicked)
       {
        UI_SetActive(Box->Key);
       }
      }
     }
     
     // Update tHot and tActive
     {                
      f32 Speed = UI_State->AnimSpeed;
      f32 HotTarget    = UI_IsHot(Box)    ? 1.f : 0.f;
      f32 ActiveTarget = UI_IsActive(Box) ? 1.f : 0.f;
      Box->tActive += Speed*(ActiveTarget - Box->tActive);
      Box->tHot    += Speed*(HotTarget - Box->tHot);
     }
    }
   }
  }
  
  if(MouseUp)
  {
   UI_SetActive(UI_KeyNull());
  }
 }
 
 
}

internal void
UI_DebugAddBox(ui_box *Box)
{
 ui_box_node *Node = PushArrayZero(UI_State->FrameArenaFront, ui_box_node, 1);
 
 Node->Box = Box;
 Node->Next = UI_State->FirstDebugBox;
 UI_State->FirstDebugBox = Node;
}


internal void
UI_InitState(arena *Arena)
{
 UI_State->Arena = Arena;
 UI_State->BoxTableSize = 4096;
 UI_State->BoxTable = PushArray(UI_State->Arena, ui_box, UI_State->BoxTableSize);
 UI_State->StyleArena = PushArena(UI_State->Arena, KB(64), false);
 UI_State->FrameArenaFront = PushArena(UI_State->Arena, MB(16), false);
 UI_State->FrameArenaBack = PushArena(UI_State->Arena, MB(16), false);
}

// TODO(luca): This can technically also belong in rl_platform.h
internal void
UI_BeginFrame(font_atlas *Atlas, u64 FrameIdx, app_input *Input)
{
 UI_State->Atlas = Atlas;
 UI_State->FrameIdx = FrameIdx;
 UI_State->Input = Input;
 
 /* NOTE(luca): 
Two arena's are used to solve the following situation.

1.

Widget A

2. We insert B

Widget B
Widget A

--- 

Now the boxes of A will be allocated after those of B which will change their pointers.  This is a problem for the boxes that have no key and aren't allocated on the hashtable.
Relying on the fact that this happens a per-frame basis we keep two arena's around which swap at each frame.
This keeps the pointers stable.
*/
 
 Swap(UI_State->FrameArenaFront, UI_State->FrameArenaBack);
 ArenaSetPos(UI_State->FrameArenaFront, 0);
 
 UI_State->FirstDebugBox = 0;
 UI_State->AnimSpeed = (1.f - PowF32(2.f, -60.f*UI_State->Input->dtForFrame));
}

//~ Calculations Start 
internal void
UI_CalculateStandaloneSizes(ui_box *Box, axis2 Axis)
{
 switch(Box->SemanticSize[Axis].Kind)
 {
  default: break;
  case UI_SizeKind_Pixels:
  {
   Box->FixedSize.e[Axis] = Box->SemanticSize[Axis].Value;
  } break;
  case UI_SizeKind_TextContent:
  {
   if(Axis == Axis2_X)
   {
    // NOTE(luca): GPU likes exact pixel coordinates
    Box->FixedSize.e[Axis] = CeilF32(UI_MeasureTextWidth(Box->DisplayString, Box->FontKind) + 
                                     2.f*Box->SemanticSize[Axis].Value);
   }
   else
   {
    Box->FixedSize.e[Axis] = (Box->HeightPx + 
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
 if(UI_IsDebugBox(Box))
 {
  NoOp();
 }
 
 if(Box->SemanticSize[Axis].Kind == UI_SizeKind_ParentPct)
 {
  // NOTE(luca): GPU likes exact pixel coordinates
  Box->FixedSize.e[Axis] = CeilF32(Box->SemanticSize[Axis].Value * 
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
 if(UI_IsDebugBox(Box))
 {
  NoOp();
 }
 
 ui_box *Parent = Box->Parent;
 
 // TODO(luca): This is inefficient since the violations should be solved once per level.
 
 f32 AllowedSize = Box->FixedSize.e[Axis];
 f32 TotalSize = 0.f;
 f32 TotalTakeableSize = 0.f;
 
 {    
  for UI_EachBox(Child, Box->First)
  {
   // NOTE(luca): Only accumulate size if this is the first child or if this box is laying out along the axis.
   b32 IsFirstChild = (Child == Box->First);
   if(IsFirstChild || Box->LayoutAxis == Axis)
   {
    if(!UI_IsFloatingBox(Child, Axis) || IsFirstChild)
    {
     TotalSize += Child->FixedSize.e[Axis];
     TotalTakeableSize += Child->FixedSize.e[Axis] * (1.f - Child->SemanticSize[Axis].Strictness);
    }
   }
  }
  
  f32 ViolationSize = TotalSize - AllowedSize;
  
  if(ViolationSize > 0.f)
  {
   f32 FixupPct = ViolationSize/TotalTakeableSize;
   
   for UI_EachBox(Child, Box->First)
   {
    // TODO(luca): This is a hack because for now I only have strictness of 1.f or 0.f so this handles most cases.
    if(Child->SemanticSize[Axis].Strictness < 1.f)
    {
     Child->FixedSize.e[Axis] -= Child->FixedSize.e[Axis]*FixupPct;
    }
   }
  }
  else for UI_EachBox(Child, Box->First)
  {
   if(Child->SemanticSize[Axis].Kind == UI_SizeKind_ParentPct)
   {
    Child->FixedSize.e[Axis] = Child->SemanticSize[Axis].Value*Box->FixedSize.e[Axis];
   }
  }
  
  if(!UI_IsNilBox(Box->First))
  {
   UI_CalculateViolations(Box->First, Axis);
  }
  
  if(!UI_IsNilBox(Box->Next))
  {
   UI_CalculateViolations(Box->Next, Axis);
  }
 }
}

internal void
UI_CalculatePositions(ui_box *Root)
{
 for(ui_box *Box = Root;
     !UI_IsNilBox(Box);
     Box = UI_BoxDepthFirstPreOrder(Box).Next)
 {        
  ui_box *Parent = Box->Parent;
  
  if(UI_IsDebugBox(Box))
  {
   NoOp();
  }
  
  if(Parent->First == Box)
  {
   Box->FixedPos = V2SubV2(Parent->FixedPos, Parent->Scroll);
  }
  else
  {  
   axis2 Axis = Parent->LayoutAxis;
   axis2 OtherAxis = 1 - Axis;
   
   ui_box *NonFloatingSibling = UI_NilBox;
   for(ui_box *Sibling = Box->Prev; !UI_IsNilBox(Sibling); Sibling = Sibling->Prev)
   {
    if(!UI_IsFloatingBox(Sibling, Axis))
    {
     NonFloatingSibling = Sibling;
     break;
    }
   }
   
   if(!UI_IsNilBox(NonFloatingSibling))
   {
    Box->FixedPos.e[Axis] = (NonFloatingSibling->FixedPos.e[Axis] + 
                             NonFloatingSibling->FixedSize.e[Axis]);
    Box->FixedPos.e[OtherAxis] = (NonFloatingSibling->FixedPos.e[OtherAxis]);
   }
   else
   {
    Box->FixedPos = Parent->FixedPos;
   }
   
   if(UI_IsFloatingBox(Box, Axis2_X))
   {
    Box->FixedPos.X = NonFloatingSibling->FixedPos.X;
   }
   if(UI_IsFloatingBox(Box, Axis2_Y))
   {
    Box->FixedPos.Y = NonFloatingSibling->FixedPos.Y;
   }
  }
  
  // Update animated position
  {    
   for UI_EachAxis(Axis)
   {    
    if(Box->Flags & UI_BoxFlag_AnimatePosX << Axis)
    {
     Box->AnimatedPos.e[Axis] += (Box->FixedPos.e[Axis] - Box->AnimatedPos.e[Axis])*UI_State->AnimSpeed;
    }
    else
    {
     Box->AnimatedPos.e[Axis] = Box->FixedPos.e[Axis];
    }
   }
  }
  
  Box->Rec = RectFromSize(Box->AnimatedPos, Box->FixedSize);
  
  if(Box->Flags & UI_BoxFlag_Clip)
  {
   Box->Rec = RectIntersect(Parent->Rec, Box->Rec);
  }
 }
}

internal void
UI_DrawBoxes(ui_box *Box)
{
 ui_box *Parent = Box->Parent;
 font_atlas *Atlas = UI_State->Atlas;
 v4 Dest = Box->Rec;
 
 if(UI_IsDebugBox(Box))
 {
  NoOp();
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
   
   v4 *Color0 = &Inst->Color0;
   v4 *Color1 = &Inst->Color1;
   v4 *Color2 = &Inst->Color2;
   v4 *Color3 = &Inst->Color3;
   
   if((Box->Flags & UI_BoxFlag_MirrorEffects))
   {
#if 0
    Swap(Color1, Color2);
#else
    Swap(Color0, Color3);
#endif
   }
   
   if(Box->Flags & UI_BoxFlag_DrawHotEffects && UI_IsHot(Box))
   {
    
    V3Math Color0->E *= 1.f - .3f*(Box->tHot - Box->tActive);
    V3Math Color1->E *= 1.f - .3f*(Box->tHot - Box->tActive);
   }
   
   if(Box->Flags & UI_BoxFlag_DrawActiveEffects && UI_IsActive(Box))
   {
    V3Math Color2->E *= 1.f - .3f*(Box->tActive);
    V3Math Color3->E *= 1.f - .3f*(Box->tActive);
   }
  }
  
  if(Box->Flags & UI_BoxFlag_DrawDisplayString)
  {
   v2 TextPos = Box->AnimatedPos;
   V2Math TextPos.E += Box->BorderThickness;
   
   v2 Cur = TextPos;
   
   if(Box->Flags & UI_BoxFlag_CenterTextHorizontally)
   {
    // NOTE(luca): GPU likes exact pixel coordinates
    f32 TextWidth = CeilF32(UI_MeasureTextWidth(Box->DisplayString, Box->FontKind));
    Cur.X += .5f*(Box->FixedSize.X - TextWidth);
   }
   
   if(Box->Flags & UI_BoxFlag_CenterTextVertically)
   {
    f32 TextHeight = Atlas->HeightPx;
    Cur.Y += .5f*(Box->FixedSize.Y - TextHeight);
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
   // NOTE(luca): Highlight border when selected
   if(0)
   {
    b32 Selected = (Box->Flags & UI_BoxFlag_DrawHotEffects &&
                    (UI_IsHot(Box) || UI_IsActive(Box)));
    if(Selected)
    {
     Box->BorderColor = Color_Snow2;
    }
   }
   
   rect_instance *Inst = DrawRect(Dest, Box->BorderColor, 0.f, Box->BorderThickness, Box->Softness);
   V4Math Inst->CornerRadii.E = Box->CornerRadii.E;
  }
  
  if(Box->CustomDraw)
  {
   Box->CustomDraw(Box->CustomDrawData);
  }
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

internal char *
UI_DebugSizeKindString(ui_size_kind Kind)
{
 char *Result = (Kind == UI_SizeKind_Null ? "Null" : 
                 Kind == UI_SizeKind_Pixels ? "Pixels" : 
                 Kind == UI_SizeKind_TextContent ? "Text" : 
                 Kind == UI_SizeKind_ParentPct ? "ParentPct" : 
                 Kind == UI_SizeKind_ChildrenSum ? "Children" : 
                 "???");
 return Result;
}

internal void
UI_DebugPrintBoxes(ui_box *Box)
{
 ui_size *SizeX = Box->SemanticSize + 0;
 ui_size *SizeY = Box->SemanticSize + 1;
 
 Log("%*s\"%S\":\n"
     "%*s %s(%.0f,%0.f),%s(%.0f,%.0f)\n"
     "%*s %.0f,%.0f %.0fx%.0f\n"
     ,
     UI_DebugIndentation, "", Box->DisplayString,
     UI_DebugIndentation, "", 
     UI_DebugSizeKindString(SizeX->Kind), SizeX->Value, SizeX->Strictness,
     UI_DebugSizeKindString(SizeY->Kind), SizeY->Value, SizeY->Strictness,
     UI_DebugIndentation, "", 
     Box->FixedPos.X, Box->FixedPos.Y, Box->FixedSize.X, Box->FixedSize.Y);
 
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
UI_EndLayout(ui_box *Root)
{
 Assert(!UI_IsNilBox(Root));
 
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
 
 // Draw debug borders
 { 
  for(ui_box *Box = Root;
      !UI_IsNilBox(Box);
      Box = UI_BoxDepthFirstPreOrder(Box).Next)
  {
   if(0 && UI_IsDebugBox(Box->Parent))
   {
    UI_DebugAddBox(Box);
   }
   
   b32 DrawDebugBorder = (UI_State->RectDebugMode ||
                          Box->Flags & UI_BoxFlag_DrawDebugBorder ||
                          UI_IsDebugBox(Box));
   if(DrawDebugBorder)
   {
    DrawRect(Box->Rec, V4(1.f, 0.f, 1.f, 1.f), 0.f, 1.f, 0.f);
   }
  }
 }
}

#define UI_Push() DeferLoop(UI_PushBox(), UI_PopBox()) 
#define UI_Layout(Root, HeightPx) DeferLoop(UI_BeginLayout(Root, HeightPx), UI_EndLayout(Root))
