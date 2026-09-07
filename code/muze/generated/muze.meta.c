enum panel_kind
{
PanelKind_Empty,
 PanelKind_Sheet,
 PanelKind_Roll,
 PanelKind_Controls,
 PanelKind_Debug,
 PanelKind_Count
};
typedef enum panel_kind panel_kind;

str8 PanelTypeStrings[] =
{
{(u8 *)"Empty", 5},
{(u8 *)"Sheet", 5},
{(u8 *)"Roll", 4},
{(u8 *)"Controls", 8},
{(u8 *)"Debug", 5},
};

