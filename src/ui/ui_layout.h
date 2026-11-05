/* Rows across the top of the window, from the top down: the menu bar, then one
 * or two HUD rows, then the side panels. Headless renders have no bar, so their
 * golden images don't change. */
#ifndef DS1EDIT_UI_LAYOUT_H
#define DS1EDIT_UI_LAYOUT_H

#include "structs.h"

#define UI_MENU_BAR_H 14

static inline int ui_menu_bar_height(void)
{
   return glb_ds1edit.cmd_line.headless_mode ? 0 : UI_MENU_BAR_H;
}

static inline int ui_top_bar_height(void)
{
   return ui_menu_bar_height() + (glb_ds1edit.show_2nd_row ? 20 : 9);
}

#endif
