#include <string.h>
#include <allegro5/keycodes.h>

#include "ui/commands.h"

#define CS (CMD_MOD_CTRL | CMD_MOD_SHIFT)

/* Rows appear in their menus in this order. A row with `key` is dispatched from
 * this table; a row with `inject_key` reaches the polled code that owns that key. */
static const CMD_DEF s_table[] =
{
   { CMD_NEW_PROJECT,   "New Project",      MENU_FILE, "Ctrl+Shift+N", ALLEGRO_KEY_N, CS, 0 },
   { CMD_OPEN_PROJECT,  "Open Project",     MENU_FILE, "Ctrl+Shift+O", ALLEGRO_KEY_O, CS, 0 },
   { CMD_CLOSE_PROJECT, "Close Project",    MENU_FILE, "Ctrl+Shift+W", ALLEGRO_KEY_W, CS, 0 },
   { CMD_SAVE,          "Save",             MENU_FILE, "Ctrl+S", ALLEGRO_KEY_S, CMD_MOD_CTRL, 0 },
   { CMD_SAVE_ALL,      "Save All",         MENU_FILE, "Ctrl+Shift+S", ALLEGRO_KEY_S, CS, 0 },
   { CMD_OPEN_PRESET,   "Open Preset",      MENU_FILE, "Ctrl+Shift+P", ALLEGRO_KEY_P, CS, 0 },
   { CMD_EXPORT_ASSETS, "Export Assets",    MENU_FILE, "Ctrl+Shift+A", ALLEGRO_KEY_A, CS, 0 },
   { CMD_PROJECT_SETTINGS, "Project Settings", MENU_FILE, "", 0, 0, 0 },
   { CMD_QUIT,          "Quit",             MENU_FILE, "Esc", 0, 0, 0 },

   { CMD_UNDO,          "Undo",             MENU_EDIT, "Ctrl+Z", 0, 0, 0 },
   { CMD_CUT,           "Cut",              MENU_EDIT, "Ctrl+X", 0, 0, 0 },
   { CMD_COPY,          "Copy",             MENU_EDIT, "Ctrl+C", 0, 0, 0 },
   { CMD_DELETE_SELECTION, "Delete Selection", MENU_EDIT, "Del", 0, 0, ALLEGRO_KEY_DELETE },

   { CMD_MAP_NEW,       "New",              MENU_MAP, "Ctrl+N", ALLEGRO_KEY_N, CMD_MOD_CTRL, 0 },
   { CMD_MAP_CLONE,     "Clone",            MENU_MAP, "Ctrl+D", ALLEGRO_KEY_D, CMD_MOD_CTRL, 0 },
   { CMD_MAP_REVERT,    "Revert to Game Version", MENU_MAP, "", 0, 0, 0 },
   { CMD_MAP_DELETE,    "Delete Map",       MENU_MAP, "Ctrl+Del", ALLEGRO_KEY_DELETE, CMD_MOD_CTRL, 0 },
   { CMD_MAP_REMOVE,    "Remove from Project", MENU_MAP, "", 0, 0, 0 },
   { CMD_MAP_RESTORE_SLOT, "Restore Slot",  MENU_MAP, "", 0, 0, 0 },
   { CMD_MAP_RESTORE_BACKUP, "Restore from Backup", MENU_MAP, "", 0, 0, 0 },
   { CMD_MAP_PROPERTIES, "Properties",      MENU_MAP, "]", 0, 0, ALLEGRO_KEY_CLOSEBRACE },

   { CMD_VIEW_FLOOR1,   "Floor 1",          MENU_VIEW, "F1", 0, 0, ALLEGRO_KEY_F1 },
   { CMD_VIEW_FLOOR2,   "Floor 2",          MENU_VIEW, "F2", 0, 0, ALLEGRO_KEY_F2 },
   { CMD_VIEW_ANIMS,    "Animations",       MENU_VIEW, "F3", 0, 0, ALLEGRO_KEY_F3 },
   { CMD_VIEW_OBJECTS,  "Objects",          MENU_VIEW, "F4", 0, 0, ALLEGRO_KEY_F4 },
   { CMD_VIEW_WALL1,    "Wall 1",           MENU_VIEW, "F5", 0, 0, ALLEGRO_KEY_F5 },
   { CMD_VIEW_WALL2,    "Wall 2",           MENU_VIEW, "F6", 0, 0, ALLEGRO_KEY_F6 },
   { CMD_VIEW_WALL3,    "Wall 3",           MENU_VIEW, "F7", 0, 0, ALLEGRO_KEY_F7 },
   { CMD_VIEW_WALL4,    "Wall 4",           MENU_VIEW, "F8", 0, 0, ALLEGRO_KEY_F8 },
   { CMD_VIEW_SPECIALS, "Specials",         MENU_VIEW, "F9", 0, 0, ALLEGRO_KEY_F9 },
   { CMD_VIEW_PATHS,    "Paths",            MENU_VIEW, "F10", 0, 0, ALLEGRO_KEY_F10 },
   { CMD_VIEW_SHADOWS,  "Shadows",          MENU_VIEW, "F11", 0, 0, ALLEGRO_KEY_F11 },
   { CMD_VIEW_ZOOM_IN,  "Zoom In",          MENU_VIEW, "+", 0, 0, ALLEGRO_KEY_EQUALS },
   { CMD_VIEW_ZOOM_OUT, "Zoom Out",         MENU_VIEW, "-", 0, 0, ALLEGRO_KEY_MINUS },
   { CMD_VIEW_GRID,     "Grid",             MENU_VIEW, "G", 0, 0, ALLEGRO_KEY_G },
   { CMD_VIEW_AREAS,    "Areas Panel",      MENU_VIEW, "[", 0, 0, ALLEGRO_KEY_OPENBRACE },
   { CMD_VIEW_PROPERTIES, "Properties Panel", MENU_VIEW, "]", 0, 0, ALLEGRO_KEY_CLOSEBRACE },

   { CMD_BUILD_MPQ,     "Build MPQ",        MENU_BUILD, "Ctrl+B", ALLEGRO_KEY_B, CMD_MOD_CTRL, 0 },
   { CMD_BUILD_AND_TEST, "Build & Test",    MENU_BUILD, "Ctrl+Shift+B", ALLEGRO_KEY_B, CS, 0 },
   { CMD_LAUNCH_PROFILE, "Launch Profile",  MENU_BUILD, "", 0, 0, 0 },
};

#define TABLE_COUNT ((int) (sizeof(s_table) / sizeof(s_table[0])))

static const char * const s_titles[MENU_COUNT] = { "File", "Edit", "Map", "View", "Build" };

/* Titles are this far apart, in the built-in font's 8-pixel cells. */
#define TITLE_PAD_X 8
#define TITLE_GAP   16

CMD_ID commands_find_key(int keycode, int mods)
{
   int i;

   for (i = 0; i < TABLE_COUNT; i++)
      if (s_table[i].key != 0 && s_table[i].key == keycode && s_table[i].mods == mods)
         return s_table[i].id;
   return CMD_NONE;
}

const CMD_DEF *commands_get(CMD_ID id)
{
   int i;

   for (i = 0; i < TABLE_COUNT; i++)
      if (s_table[i].id == id)
         return &s_table[i];
   return NULL;
}

int commands_is_available(CMD_ID id, const CMD_CTX *c)
{
   switch (id)
   {
   case CMD_NEW_PROJECT:
   case CMD_OPEN_PROJECT:
   case CMD_CLOSE_PROJECT:
   case CMD_OPEN_PRESET:
   case CMD_EXPORT_ASSETS:
   case CMD_QUIT:
   case CMD_MAP_NEW:
   case CMD_VIEW_AREAS:
   case CMD_VIEW_PROPERTIES:
   case CMD_MAP_PROPERTIES:
      return 1;

   case CMD_SAVE_ALL:
      return c->any_dirty;

   case CMD_UNDO:
      if (c->tile_mode)
         return c->has_map && !c->paste_active;
      return c->object_mode && c->can_undo_object;

   case CMD_CUT:
   case CMD_COPY:
      return c->tile_mode && c->has_map && !c->paste_active;

   case CMD_DELETE_SELECTION:
      return c->has_map && !c->paste_active && (c->tile_mode || c->object_mode);

   case CMD_MAP_CLONE:
   case CMD_MAP_DELETE:
      return c->has_map && c->has_open_entry;

   /* Step 3 and step 4 implement these. */
   case CMD_MAP_REVERT:
   case CMD_MAP_REMOVE:
   case CMD_MAP_RESTORE_SLOT:
   case CMD_MAP_RESTORE_BACKUP:
   case CMD_PROJECT_SETTINGS:
   case CMD_BUILD_MPQ:
   case CMD_BUILD_AND_TEST:
   case CMD_LAUNCH_PROFILE:
      return 0;

   /* Save and the rest of View need a map on screen. */
   default:
      return c->has_map;
   }
}

int commands_conflict_count(void)
{
   int i, j, n = 0;

   for (i = 0; i < TABLE_COUNT; i++)
      for (j = i + 1; j < TABLE_COUNT; j++)
         if (s_table[i].key != 0 && s_table[i].key == s_table[j].key &&
             s_table[i].mods == s_table[j].mods)
            n++;
   return n;
}

int commands_menu_item_count(int menu)
{
   int i, n = 0;

   for (i = 0; i < TABLE_COUNT; i++)
      if (s_table[i].menu == menu)
         n++;
   return n;
}

CMD_ID commands_menu_item(int menu, int index)
{
   int i, n = 0;

   for (i = 0; i < TABLE_COUNT; i++)
      if (s_table[i].menu == menu && n++ == index)
         return s_table[i].id;
   return CMD_NONE;
}

void menu_state_open(MENU_STATE *st, int menu)
{
   st->menu = menu;
   st->item = 0;
}

CMD_ID menu_state_key(MENU_STATE *st, int keycode, const CMD_CTX *ctx)
{
   int count;
   CMD_ID id;

   if (st->menu < 0)
      return CMD_NONE;
   count = commands_menu_item_count(st->menu);

   switch (keycode)
   {
   case ALLEGRO_KEY_ESCAPE:
      st->menu = -1;
      break;
   case ALLEGRO_KEY_UP:
      st->item = (st->item + count - 1) % count;
      break;
   case ALLEGRO_KEY_DOWN:
      st->item = (st->item + 1) % count;
      break;
   case ALLEGRO_KEY_LEFT:
      menu_state_open(st, (st->menu + MENU_COUNT - 1) % MENU_COUNT);
      break;
   case ALLEGRO_KEY_RIGHT:
      menu_state_open(st, (st->menu + 1) % MENU_COUNT);
      break;
   case ALLEGRO_KEY_ENTER:
   case ALLEGRO_KEY_PAD_ENTER:
      id = commands_menu_item(st->menu, st->item);
      if (commands_is_available(id, ctx))
      {
         st->menu = -1;
         return id;
      }
      break;
   default:
      break;
   }
   return CMD_NONE;
}

const char *menu_title(int menu)
{
   return s_titles[menu];
}

int menu_title_x(int menu)
{
   int i, x = TITLE_PAD_X;

   for (i = 0; i < menu; i++)
      x += (int) strlen(s_titles[i]) * 8 + TITLE_GAP;
   return x;
}

int menu_title_at(int mx, int my, int bar_h)
{
   int i;

   if (my < 0 || my >= bar_h)
      return -1;
   for (i = 0; i < MENU_COUNT; i++)
   {
      int x = menu_title_x(i);
      if (mx >= x - TITLE_GAP / 2 &&
          mx < x + (int) strlen(s_titles[i]) * 8 + TITLE_GAP / 2)
         return i;
   }
   return -1;
}

int menu_item_at(int menu, int mx, int my, int bar_h)
{
   int x, row;

   if (menu < 0)
      return -1;
   x = menu_title_x(menu);
   if (mx < x || mx >= x + MENU_DROP_W || my < bar_h)
      return -1;
   row = (my - bar_h) / MENU_ITEM_H;
   return row < commands_menu_item_count(menu) ? row : -1;
}
