#define UI_LayoutStacks typedef struct v4_stack_node v4_stack_node; struct v4_stack_node { v4_stack_node *Prev; v4 Value; }; typedef struct f32_stack_node f32_stack_node; struct f32_stack_node { f32_stack_node *Prev; f32 Value; }; typedef struct axis2_stack_node axis2_stack_node; struct axis2_stack_node { axis2_stack_node *Prev; axis2 Value; }; typedef struct ui_size_stack_node ui_size_stack_node; struct ui_size_stack_node { ui_size_stack_node *Prev; ui_size Value; }; typedef struct font_kind_stack_node font_kind_stack_node; struct font_kind_stack_node { font_kind_stack_node *Prev; font_kind Value; }; 
  
#define UI_StateStacks v4_stack_node *BackgroundColorTop; v4_stack_node *TextColorTop; v4_stack_node *BorderColorTop; f32_stack_node *BorderThicknessTop; f32_stack_node *SoftnessTop; v4_stack_node *CornerRadiiTop; axis2_stack_node *LayoutAxisTop; ui_size_stack_node *SemanticWidthTop; ui_size_stack_node *SemanticHeightTop; f32_stack_node *HeightPxTop; font_kind_stack_node *FontKindTop; 
  
#define UI_StackFunctions internal void UI_PushBackgroundColor(v4 BackgroundColor)  { UI_StackPush(v4, BackgroundColor); } internal void UI_PopBackgroundColor(void)                       { UI_StackPop(BackgroundColor); } internal void UI_PushTextColor(v4 TextColor)  { UI_StackPush(v4, TextColor); } internal void UI_PopTextColor(void)                       { UI_StackPop(TextColor); } internal void UI_PushBorderColor(v4 BorderColor)  { UI_StackPush(v4, BorderColor); } internal void UI_PopBorderColor(void)                       { UI_StackPop(BorderColor); } internal void UI_PushBorderThickness(f32 BorderThickness)  { UI_StackPush(f32, BorderThickness); } internal void UI_PopBorderThickness(void)                       { UI_StackPop(BorderThickness); } internal void UI_PushSoftness(f32 Softness)  { UI_StackPush(f32, Softness); } internal void UI_PopSoftness(void)                       { UI_StackPop(Softness); } internal void UI_PushCornerRadii(v4 CornerRadii)  { UI_StackPush(v4, CornerRadii); } internal void UI_PopCornerRadii(void)                       { UI_StackPop(CornerRadii); } internal void UI_PushLayoutAxis(axis2 LayoutAxis)  { UI_StackPush(axis2, LayoutAxis); } internal void UI_PopLayoutAxis(void)                       { UI_StackPop(LayoutAxis); } internal void UI_PushSemanticWidth(ui_size SemanticWidth)  { UI_StackPush(ui_size, SemanticWidth); } internal void UI_PopSemanticWidth(void)                       { UI_StackPop(SemanticWidth); } internal void UI_PushSemanticHeight(ui_size SemanticHeight)  { UI_StackPush(ui_size, SemanticHeight); } internal void UI_PopSemanticHeight(void)                       { UI_StackPop(SemanticHeight); } internal void UI_PushHeightPx(f32 HeightPx)  { UI_StackPush(f32, HeightPx); } internal void UI_PopHeightPx(void)                       { UI_StackPop(HeightPx); } internal void UI_PushFontKind(font_kind FontKind)  { UI_StackPush(font_kind, FontKind); } internal void UI_PopFontKind(void)                       { UI_StackPop(FontKind); } 
  
enum ui_box_flag
{
UI_BoxFlag_None                   = (0 << 0),
UI_BoxFlag_DrawBackground         = (1 << 1),
UI_BoxFlag_DrawBorders            = (1 << 2),
UI_BoxFlag_DrawDebugBorder        = (1 << 3),
UI_BoxFlag_DrawShadow             = (1 << 4),
UI_BoxFlag_DrawDisplayString      = (1 << 5),
UI_BoxFlag_DrawHotEffects         = (1 << 6),
UI_BoxFlag_DrawActiveEffects      = (1 << 7),
UI_BoxFlag_CenterTextHorizontally = (1 << 8),
UI_BoxFlag_CenterTextVertically   = (1 << 9),
UI_BoxFlag_MouseClickable         = (1 << 10),
UI_BoxFlag_FloatingX              = (1 << 11),
UI_BoxFlag_FloatingY              = (1 << 12),
UI_BoxFlag_Clip                   = (1 << 13),
UI_BoxFlag_Scroll                 = (1 << 14),
UI_BoxFlag_AnimatePosX            = (1 << 15),
UI_BoxFlag_AnimatePosY            = (1 << 16),
UI_BoxFlag_MirrorEffects          = (1 << 17),
};
typedef enum ui_box_flag ui_box_flag;
