/* props_panel_rules.h -- which row the properties panel works on and which DS1
 * header fields it won't edit. Kept apart from props_panel.c so they can be
 * tested without a display.
 */
#ifndef DS1EDIT_PROPS_PANEL_RULES_H
#define DS1EDIT_PROPS_PANEL_RULES_H

#include "structs.h"

/* The row of the open map, or NULL when no row is open.
 *
 * The panel has to use this and not ab->selected_entry: that is the highlight,
 * which follows the mouse over any group's rows and isn't set when a map is opened
 * directly, as Restore from Backup and Revert do. */
const AREA_DS1_ENTRY_S * props_panel_open_row(const AREA_BROWSER_S * ab);

/* TRUE if a DS1 Header row can't be edited. header_field counts from the section's
 * first row: 0=file 1=version 2=width 3=height 4=act 5=txt_act 6=tag_type 7=walls
 * 8=floors 9=shadows 10=tags 11=objects. */
int props_panel_header_field_read_only(int header_field);

#endif
