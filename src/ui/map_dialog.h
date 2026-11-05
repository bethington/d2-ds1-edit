#ifndef DS1EDIT_MAP_DIALOG_H
#define DS1EDIT_MAP_DIALOG_H

typedef struct MAP_DIALOG_S
{
   int  is_clone;       /* hides width and height */
   int  group_idx;      /* in: the default area. out: the chosen one */
   int  def;            /* in: the default row. out: the chosen one */
   int  width, height;  /* in: defaults. out: chosen */
   char file_name[64];  /* in: suggested name. out: checked, with .ds1 */
} MAP_DIALOG_S;

/* Ask for a new map's name, area, tileset and size. Returns 1 for OK, with every
 * rule checked, or 0 for cancel. A window close cancels and stays pending. */
int map_dialog_show(MAP_DIALOG_S * d);

#endif
