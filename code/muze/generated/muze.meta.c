enum panel_kind
{
PanelKind_Empty,
 PanelKind_Sheet,
 PanelKind_Roll,
 PanelKind_Settings,
 PanelKind_Debug,
 PanelKind_Count
};
typedef enum panel_kind panel_kind;

str8 PanelTypeStrings[] =
{
{(u8 *)"Empty", 5},
{(u8 *)"Sheet", 5},
{(u8 *)"Roll", 4},
{(u8 *)"Settings", 8},
{(u8 *)"Debug", 5},
};

